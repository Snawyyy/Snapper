"""Snapper's voice worker: speaks lines with Qwen3-TTS.

Snapper starts this once and keeps it running, so a model loads only on
its first job. It reads one JSON job per line on stdin and answers with
JSON lines on stdout:

  {"id": 1, "kind": "clone", "text": "...", "language": "English",
   "sample": "/path/sample.wav", "words": "what the sample says",
   "out": "/path/out.wav"}
  {"id": 2, "kind": "design", "text": "...", "language": "English",
   "describe": "a cheerful young boy", "out": "/path/out.wav"}
  {"id": 3, "kind": "transcribe", "sample": "/path/sample.wav"}

  {"id": 1, "state": "loading"}   the model is loading (first job only)
  {"id": 1, "state": "speaking"}
  {"id": 1, "state": "done", "seconds": 2.4}
  {"id": 3, "state": "done", "text": "what the sample says"}
  {"id": 1, "state": "error", "why": "..."}

An empty "words" clones from the sound of the sample alone, which is
less close. Anything the libraries print goes to stderr, never stdout.
"""

import json
import sys

kModels = {
    "clone": "Qwen/Qwen3-TTS-12Hz-1.7B-Base",
    "design": "Qwen/Qwen3-TTS-12Hz-1.7B-VoiceDesign",
    "transcribe": "openai/whisper-large-v3-turbo",
}

# The libraries print progress; only answers may reach Snapper.
kAnswers = sys.stdout
sys.stdout = sys.stderr


def Say(answer):
    kAnswers.write(json.dumps(answer) + "\n")
    kAnswers.flush()


class Speaker:
    """Holds the loaded models, one per kind, so each loads once."""

    def __init__(self):
        self.models = {}

    def Model(self, kind, job_id):
        if kind not in self.models:
            Say({"id": job_id, "state": "loading"})
            import torch
            if kind == "transcribe":
                from transformers import pipeline
                self.models[kind] = pipeline(
                    "automatic-speech-recognition", model=kModels[kind],
                    torch_dtype=torch.float16, device="cuda:0")
            else:
                from qwen_tts import Qwen3TTSModel
                self.models[kind] = Qwen3TTSModel.from_pretrained(
                    kModels[kind], device_map="cuda:0", dtype=torch.bfloat16)
        return self.models[kind]

    def Transcribe(self, job):
        listener = self.Model("transcribe", job["id"])
        Say({"id": job["id"], "state": "speaking"})
        heard = listener(job["sample"], return_timestamps=True)
        return heard["text"].strip()

    def Run(self, job):
        kind = job.get("kind")
        if kind not in kModels or kind == "transcribe":
            raise ValueError(f"unknown job kind {kind!r}")
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
        import soundfile
        soundfile.write(job["out"], wavs[0], rate)
        return len(wavs[0]) / rate


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
