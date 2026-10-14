"""Snapper's voice worker: speaks lines with Qwen3-TTS or Chatterbox.

Snapper starts this once and keeps it running, so a model loads only on
its first job. Each engine has its own Python (their libraries clash),
so Snapper runs this script in the Python of the engine a job needs. It
reads one JSON job per line on stdin and answers with JSON lines on
stdout:

  {"id": 1, "kind": "clone", "text": "...", "language": "English",
   "sample": "/path/sample.wav", "words": "what the sample says",
   "out": "/path/out.wav"}
  {"id": 2, "kind": "clone", "engine": "chatterbox", "emotion": 0.5,
   "text": "...", "language": "English", "sample": "/path/sample.wav",
   "out": "/path/out.wav"}
  {"id": 3, "kind": "design", "text": "...", "language": "English",
   "describe": "a cheerful young boy", "out": "/path/out.wav"}
  {"id": 4, "kind": "transcribe", "sample": "/path/sample.wav"}

  {"id": 1, "state": "loading"}   the model is loading (first job only)
  {"id": 1, "state": "speaking"}
  {"id": 1, "state": "done", "seconds": 2.4}
  {"id": 4, "state": "done", "text": "what the sample says"}
  {"id": 1, "state": "error", "why": "..."}

Qwen with an empty "words" clones from the sound of the sample alone,
which is less close; Chatterbox never needs the words. "emotion" is
Chatterbox's exaggeration, 0.25 (flat) to 2 (wild), 0.5 plain. Anything
the libraries print goes to stderr, never stdout.
"""

import json
import sys

kModels = {
    "clone": "Qwen/Qwen3-TTS-12Hz-1.7B-Base",
    "design": "Qwen/Qwen3-TTS-12Hz-1.7B-VoiceDesign",
    "transcribe": "openai/whisper-large-v3-turbo",
}

# Steps Chatterbox takes to turn its sound plan into sound: 10 by
# default; more is a little cleaner and costs little time.
kChatterboxSteps = 20

# The languages Snapper offers, as Chatterbox's language codes.
kChatterboxLanguages = {
    "Chinese": "zh", "Japanese": "ja", "Korean": "ko", "German": "de",
    "French": "fr", "Russian": "ru", "Portuguese": "pt", "Spanish": "es",
    "Italian": "it",
}

# The libraries print progress; only answers may reach Snapper.
kAnswers = sys.stdout
sys.stdout = sys.stderr


def Say(answer):
    kAnswers.write(json.dumps(answer) + "\n")
    kAnswers.flush()


def Load(name):
    import torch
    if name == "transcribe":
        from transformers import pipeline
        return pipeline("automatic-speech-recognition", model=kModels[name],
                        torch_dtype=torch.float16, device="cuda:0")
    if name in ("chatterbox", "chatterbox_multi"):
        if name == "chatterbox":
            from chatterbox.tts import ChatterboxTTS as Model
        else:
            from chatterbox.mtl_tts import ChatterboxMultilingualTTS as Model
        model = Model.from_pretrained(device="cuda")
        # generate() can't pass the step count, so it rides on the call.
        import functools
        model.s3gen.inference = functools.partial(
            model.s3gen.inference, n_cfm_timesteps=kChatterboxSteps)
        return model
    from qwen_tts import Qwen3TTSModel
    return Qwen3TTSModel.from_pretrained(
        kModels[name], device_map="cuda:0", dtype=torch.bfloat16)


class Speaker:
    """Holds the loaded models, one per name, so each loads once."""

    def __init__(self):
        self.models = {}

    def Model(self, name, job_id):
        if name not in self.models:
            Say({"id": job_id, "state": "loading"})
            try:
                self.models[name] = Load(name)
            except Exception as problem:
                if "out of memory" not in str(problem).lower():
                    raise
                # The card is full: the other models go, this one loads
                # alone. They load again when next needed.
                self.Forget()
                self.models[name] = Load(name)
        return self.models[name]

    def Forget(self):
        import gc
        import torch
        self.models.clear()
        gc.collect()
        torch.cuda.empty_cache()

    def Transcribe(self, job):
        listener = self.Model("transcribe", job["id"])
        Say({"id": job["id"], "state": "speaking"})
        heard = listener(job["sample"], return_timestamps=True)
        return heard["text"].strip()

    def Chatterbox(self, job):
        """Returns the waveform and its rate."""
        code = kChatterboxLanguages.get(job.get("language") or "Auto")
        model = self.Model("chatterbox_multi" if code else "chatterbox",
                           job["id"])
        Say({"id": job["id"], "state": "speaking"})
        emotion = min(2.0, max(0.25, float(job.get("emotion", 0.5))))
        # Strong feeling speaks too fast unless the guidance eases off.
        guidance = 0.5 if emotion <= 0.7 else 0.3
        extra = {"language_id": code} if code else {}
        wav = model.generate(job["text"], audio_prompt_path=job["sample"],
                             exaggeration=emotion, cfg_weight=guidance,
                             **extra)
        return wav.squeeze(0).cpu().numpy(), model.sr

    def Qwen(self, job):
        """Returns the waveform and its rate."""
        kind = job["kind"]
        model = self.Model(kind, job["id"])
        Say({"id": job["id"], "state": "speaking"})
        language = job.get("language") or "Auto"
        if kind == "clone":
            words = (job.get("words") or "").strip()
            wavs, rate = model.generate_voice_clone(
                text=job["text"], language=language,
                ref_audio=job["sample"], ref_text=words or None,
                x_vector_only_mode=not words)
        else:
            wavs, rate = model.generate_voice_design(
                text=job["text"], language=language,
                instruct=job["describe"])
        return wavs[0], rate

    def Run(self, job):
        kind = job.get("kind")
        if kind not in ("clone", "design"):
            raise ValueError(f"unknown job kind {kind!r}")
        is_chatterbox = kind == "clone" and job.get("engine") == "chatterbox"
        speak = self.Chatterbox if is_chatterbox else self.Qwen
        try:
            wav, rate = speak(job)
        except Exception as problem:
            if "out of memory" not in str(problem).lower():
                raise
            # Speaking ran out of room: once more with only its model.
            self.Forget()
            wav, rate = speak(job)
        import soundfile
        soundfile.write(job["out"], wav, rate)
        return len(wav) / rate


def main():
    speaker = Speaker()
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        job_id = None
        try:
            job = json.loads(line)
            job_id = job.get("id")
            if job.get("kind") == "transcribe":
                text = speaker.Transcribe(job)
                Say({"id": job_id, "state": "done", "text": text})
            else:
                seconds = speaker.Run(job)
                Say({"id": job_id, "state": "done", "seconds": seconds})
        except Exception as problem:  # Snapper shows why, the worker lives on.
            Say({"id": job_id, "state": "error", "why": str(problem)})


if __name__ == "__main__":
    main()
