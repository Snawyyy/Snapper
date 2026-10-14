#!/bin/sh
# Installs the voice worker's Python and its Qwen3-TTS models (about
# 9 GB) where Snapper looks for them. Needs uv and an NVIDIA GPU.
# Run again to update.
home="${XDG_DATA_HOME:-$HOME/.local/share}/Snapper/Snapper/voice_gen"
mkdir -p "$home" || exit 1
cd "$home" || exit 1
if [ ! -x venv/bin/python ]; then
  uv venv --python 3.12 venv || exit 1
fi
VIRTUAL_ENV="$home/venv" uv pip install -U qwen-tts soundfile || exit 1
venv/bin/hf download Qwen/Qwen3-TTS-12Hz-1.7B-Base || exit 1
venv/bin/hf download Qwen/Qwen3-TTS-12Hz-1.7B-VoiceDesign || exit 1
echo "Voice gen is ready: $home"
