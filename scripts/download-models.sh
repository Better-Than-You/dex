#!/usr/bin/env bash
set -euo pipefail

MODEL_DIR="${DEX_MODEL_DIR:-$HOME/.local/share/dex/models}"
BASE_URL="https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models"

# Dex uses sherpa-onnx's offline Moonshine v2 bundle and produces low-latency
# partial transcripts by re-decoding the active VAD buffer ("simulated
# streaming"). The Hugging Face `moonshine-streaming-medium` export is not a
# sherpa-onnx model format and is intentionally not downloaded here.

for arg in "$@"; do
  case "$arg" in
    --dir=*) MODEL_DIR="${arg#*=}" ;;
    -h|--help)
      echo "Usage: $0 [--dir=/path/to/models]"
      echo ""
      echo "Downloads:"
      echo "  - silero_vad.onnx"
      echo "  - sherpa-onnx-moonshine-base-en-quantized-2026-02-27 (default)"
      echo "  - sherpa-onnx-nemo-parakeet-tdt-0.6b-v2-int8 (alternative)"
      exit 0
      ;;
    *)
      echo "Unknown argument: $arg" >&2
      exit 2
      ;;
  esac
done

mkdir -p "$MODEL_DIR"
cd "$MODEL_DIR"

echo "==> Target directory: $MODEL_DIR"

download_bundle() {
  local name="$1"
  if [[ -d "$name" ]]; then
    echo "==> $name already exists, skipping."
    return
  fi
  echo "==> Downloading $name..."
  curl -fL --progress-bar -o "${name}.tar.bz2" "$BASE_URL/${name}.tar.bz2"
  tar -xjf "${name}.tar.bz2"
  rm -f "${name}.tar.bz2"
}

# 1. Silero VAD (~0.6 MB)
if [[ ! -f "silero_vad.onnx" ]]; then
  echo "==> Downloading silero_vad.onnx..."
  curl -fL --progress-bar -o "silero_vad.onnx.tmp" "$BASE_URL/silero_vad.onnx"
  mv "silero_vad.onnx.tmp" "silero_vad.onnx"
else
  echo "==> silero_vad.onnx already exists, skipping."
fi

# 2. Default profile: Moonshine Base EN quantized (offline v2, ~140 MB)
download_bundle "sherpa-onnx-moonshine-base-en-quantized-2026-02-27"

# 3. Alternative profile: NVIDIA Parakeet TDT 0.6B v2 INT8 (~650 MB)
download_bundle "sherpa-onnx-nemo-parakeet-tdt-0.6b-v2-int8"

echo ""
echo "==> Required files:"
for f in \
  "silero_vad.onnx" \
  "sherpa-onnx-moonshine-base-en-quantized-2026-02-27/encoder_model.ort" \
  "sherpa-onnx-moonshine-base-en-quantized-2026-02-27/decoder_model_merged.ort" \
  "sherpa-onnx-moonshine-base-en-quantized-2026-02-27/tokens.txt" \
  "sherpa-onnx-nemo-parakeet-tdt-0.6b-v2-int8/encoder.int8.onnx" \
  "sherpa-onnx-nemo-parakeet-tdt-0.6b-v2-int8/decoder.int8.onnx" \
  "sherpa-onnx-nemo-parakeet-tdt-0.6b-v2-int8/joiner.int8.onnx" \
  "sherpa-onnx-nemo-parakeet-tdt-0.6b-v2-int8/tokens.txt"; do
  if [[ -f "$f" ]]; then
    echo "  [ok] $f"
  else
    echo "  [missing] $f"
  fi
done

echo ""
echo "==> Models are ready in: $MODEL_DIR"
du -sh "$MODEL_DIR"/* 2>/dev/null || true
