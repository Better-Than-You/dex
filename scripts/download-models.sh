#!/usr/bin/env bash
set -euo pipefail

MODEL_DIR="${DEX_MODEL_DIR:-$HOME/.local/share/dex/models}"
BASE_URL="https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models"
SLM_URL="${DEX_SLM_URL:-https://huggingface.co/unsloth/Qwen3.5-0.8B-GGUF/resolve/main/Qwen3.5-0.8B-Q4_K_M.gguf}"
SLM_FILE="Qwen3.5-0.8B-Q4_K_M.gguf"
SKIP_SLM=0

for arg in "$@"; do
  case "$arg" in
    --dir=*) MODEL_DIR="${arg#*=}" ;;
    --skip-slm) SKIP_SLM=1 ;;
    -h|--help)
      echo "Usage: $0 [--dir=/path/to/models] [--skip-slm]"
      echo ""
      echo "Downloads:"
      echo "  - silero_vad.onnx"
      echo "  - sherpa-onnx-nemo-parakeet-tdt-0.6b-v2-int8 (single-shot transducer)"
      echo "  - Qwen3.5-0.8B-Q4_K_M.gguf (maths SLM; skip with --skip-slm)"
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

# 2. NVIDIA Parakeet TDT 0.6B v2 INT8 (~650 MB)
download_bundle "sherpa-onnx-nemo-parakeet-tdt-0.6b-v2-int8"

# 3. Phase 2.1 maths/calculus SLM: Qwen3.5 0.8B Q4_K_M (~510 MB).
# It is loaded on demand by LlamaMathTranslator, never at startup.
if [[ "$SKIP_SLM" -eq 1 ]]; then
  echo "==> --skip-slm set, skipping $SLM_FILE."
elif [[ -f "$SLM_FILE" ]]; then
  echo "==> $SLM_FILE already exists, skipping."
else
  echo "==> Downloading $SLM_FILE (~510 MB)..."
  curl -fL --progress-bar -o "${SLM_FILE}.tmp" "$SLM_URL"
  mv "${SLM_FILE}.tmp" "$SLM_FILE"
fi

echo ""
echo "==> Required files:"
files=(
  "silero_vad.onnx"
  "sherpa-onnx-nemo-parakeet-tdt-0.6b-v2-int8/encoder.int8.onnx"
  "sherpa-onnx-nemo-parakeet-tdt-0.6b-v2-int8/decoder.int8.onnx"
  "sherpa-onnx-nemo-parakeet-tdt-0.6b-v2-int8/joiner.int8.onnx"
  "sherpa-onnx-nemo-parakeet-tdt-0.6b-v2-int8/tokens.txt"
)

if [[ "$SKIP_SLM" -eq 0 ]]; then
  files+=("$SLM_FILE")
fi

for f in "${files[@]}"; do
  if [[ -f "$f" ]]; then
    echo "  [ok] $f"
  else
    echo "  [missing] $f"
  fi
done

echo ""
echo "==> Models are ready in: $MODEL_DIR"
du -sh "$MODEL_DIR"/* 2>/dev/null || true
