#!/bin/sh
# Installs the voice worker's two Pythons and their models where Snapper
# looks for them: Qwen3-TTS and Whisper in venv (about 11 GB), Chatterbox
# in chatterbox_venv (about 4 GB; its libraries clash with Qwen's, so it
# has its own). Needs uv and an NVIDIA GPU. Run again to update.
home="${XDG_DATA_HOME:-$HOME/.local/share}/Snapper/Snapper/voice_gen"
mkdir -p "$home" || exit 1
cd "$home" || exit 1
if [ ! -x venv/bin/python ]; then
  uv venv --python 3.12 venv || exit 1
fi
VIRTUAL_ENV="$home/venv" uv pip install -U qwen-tts soundfile || exit 1
venv/bin/hf download Qwen/Qwen3-TTS-12Hz-1.7B-Base || exit 1
venv/bin/hf download Qwen/Qwen3-TTS-12Hz-1.7B-VoiceDesign || exit 1
venv/bin/hf download openai/whisper-large-v3-turbo || exit 1
if [ ! -x chatterbox_venv/bin/python ]; then
  uv venv --python 3.11 chatterbox_venv || exit 1
fi
# Its watermarker still imports pkg_resources, gone from setuptools 81.
VIRTUAL_ENV="$home/chatterbox_venv" \
  uv pip install -U chatterbox-tts soundfile "setuptools<81" || exit 1
venv/bin/hf download ResembleAI/chatterbox || exit 1
echo "Voice gen is ready: $home"
