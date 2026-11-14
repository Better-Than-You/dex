# Testing & Stub Verification Guide

## 1. Automated Unit Tests (`QtTest`)
Run via `ctest --test-dir build --output-on-failure -LE live` (8 suites; the
9th, `tst_live_input`, carries the `live` label and is described in §5):

1. `tst_normalizer`:
   - `"Hey Dex, search for Arch Linux Wiki."` → `"search for arch linux wiki"`
   - `"minus 2 into l.n.e"` → `"-2 * ln(e)"`
   - `"3.14 plus 2.5"` → `"3.14 + 2.5"`
2. `tst_math` (spoken-math rows plus precedence, implicit multiplication,
   functions, ML helpers, softmax vectors, derivatives, rejection and
   formatting):
   - `"minus 2 multiplied by l.n.e"` → `-2.0`
   - `"minus 2 into l.n.e"` → `-2.0`
   - `"10 plus 5 times 2"` → `20.0`
   - `"sqrt 16 plus 2 power 3"` → `12.0`
   - `"minus. is zero point eight"` → `-0.8`, `"sigma function of ln2"` → `0.666…`
   - ML helpers: `sigmoid(ln(2))` → `0.666667`, `relu(-2)` → `0`, `exp(0) + relu(3)` → `4`
   - `softmax([1, 0.1, -3])` → `[0.7018, 0.2853, 0.0128]` (vector `MathResult`)
   - `SymbolicDifferentiator`: `0.5 * (y_hat - y)^2` wrt `y_hat` → `y_hat - y`,
     `3*x^2 + 2*x` → `2 + 6 * x`, `sigmoid(x)` → `sigmoid(x) * (1 - sigmoid(x))`,
     `x^2` wrt `t` → `0`
   - Division by zero, modulo by zero, trailing operators, and desktop text → `std::nullopt`
3. `tst_router`:
   - `"search C++20 concepts"` → matches `web_search`, URL encodes `C%2B%2B20%20concepts`
   - `"launch kitty"` → matches `launch_app`, program `kitty`
   - `"workspace 4"` → matches `hypr_workspace`, args `["dispatch", "workspace", "4"]`
4. `tst_config`:
   - shipped `config/config.toml` parses with the single-shot `[asr]` and
     `[math_slm]` defaults
   - missing keys keep defaults (`provider = "sherpa"`,
     `math_slm.enabled = true`, `idle_unload_sec = 60`)
   - `[math_slm]` overrides: `enabled`, `~` expansion, `threads` clamped to 16,
     `idle_unload_sec = 0`
   - clamping (`threads` 1–16, `vad_threshold` 0.05–0.95) and broken TOML rejection
5. `tst_engine`: end-to-end normalise → route → maths/calculus decisions.
   - `spokenMath` rows run with **no translator** and assert `usedSlm == false`,
     proving the deterministic fallback.
   - `nonMathTrigger` rows ("turn the power off", "what is the weather") assert
     `OutcomeKind::None` with `usedSlm == false`, both with and without a
     translator attached.
   - `slmTranslatesSpeech` rows use a real `LlamaMathTranslator` when the GGUF
     exists and assert `usedSlm`, the JSON mode and the final result; the whole
     data set soft-skips when the GGUF is absent or the build has no llama.cpp.
6. `tst_ipc`: control commands, `PARTIAL:`/`FINAL:`, `SET_MODEL:`, and
   buffered/partial lines.
7. `tst_provider`:
   - factory defaults/`--stub` override and unknown-provider fallback
   - canonical profile aliases (`parakeet-v2`/`accurate` → `parakeet`,
     `stub` → `stub`) and rejection of `moonshine`/`whisper`
   - Parakeet bundle discovery (`encoder.int8.onnx`, `decoder.int8.onnx`,
     `joiner.int8.onnx`, `tokens.txt`, `silero_vad.onnx`)
   - missing VAD/joiner rejection, stub creation, missing-model failure
8. `tst_audio`: SPSC ring-buffer round-trip, wrap-around, overflow drop count,
   and reset.
9. `tst_live_input` (labelled `live`): end-to-end microphone test through the
   production `AudioCapture` → Silero VAD → single-shot Parakeet path using the system
   default input. It has no fixed input to assert on, so it soft-skips when
   there is no input device, the model bundle is missing, or no speech arrives
   within the timeout. See §5.

## 2. Stub Smoke Test (no microphone required)
Run Dex with the text-injection backend and a private socket:

```sh
./build/dex --stub --socket /tmp/dex-stub.sock
printf 'SHOW\nSET_MODEL:stub\nPARTIAL:minus 2 into\nFINAL:minus 2 into l.n.e\nQUIT\n' \
  | socat - UNIX-CONNECT:/tmp/dex-stub.sock
```

`--stub` also works without a model directory, so it is the fastest way to
exercise the UI and command engine on a machine without models.

## 3. Manual IPC Smoke Test (live backend)
Start `./build/dex` and send commands over `/run/user/$(id -u)/dex.sock`:

- `printf "SHOW\n" | socat - UNIX-CONNECT:/run/user/$(id -u)/dex.sock`
- `printf "SET_MODEL:parakeet\n" | socat - UNIX-CONNECT:/run/user/$(id -u)/dex.sock`
- `printf "SET_MODEL:stub\n" | socat - UNIX-CONNECT:/run/user/$(id -u)/dex.sock`
- `printf "PARTIAL:minus 2 into\n" | socat - UNIX-CONNECT:/run/user/$(id -u)/dex.sock`
- `printf "FINAL:minus 2 into l.n.e\n" | socat - UNIX-CONNECT:/run/user/$(id -u)/dex.sock`
- `printf "FINAL:search hyprland layer shell\n" | socat - UNIX-CONNECT:/run/user/$(id -u)/dex.sock`
- `printf "QUIT\n" | socat - UNIX-CONNECT:/run/user/$(id -u)/dex.sock`

`FINAL:` on the live backend still exercises the command engine; it does not
replace microphone input.

## 4. End-to-End ASR Check (PipeWire monitor source)
Requires `scripts/download-models.sh` to have populated the models and a
16 kHz mono speech sample.

```sh
# 1. Download a known sample (16 kHz mono).
curl -fL -o /tmp/speech.wav \
  https://huggingface.co/csukuangfj/sherpa-onnx-whisper-tiny/resolve/main/test_wavs/0.wav

# 2. Route the default input to the speaker monitor.
ORIG=$(pactl get-default-source)
pactl set-default-source alsa_output.pci-0000_00_1f.3.analog-stereo.monitor

# 3. Run Dex and play the sample.
QT_ASSUME_STDERR_HAS_CONSOLE=1 QT_QPA_PLATFORM=offscreen \
  ./build/dex --socket /tmp/dex-e2e.sock > /tmp/dex-e2e.log 2>&1 &
sleep 4
pw-play /tmp/speech.wav
sleep 5
printf 'QUIT\n' | socat - UNIX-CONNECT:/tmp/dex-e2e.sock
sleep 2

# 4. Restore the original source.
pactl set-default-source "$ORIG"
grep -E 'Dex ASR' /tmp/dex-e2e.log
```

Expected final transcript (punctuation and casing may vary):

```
After early nightfall, the yellow lamps would light up here and there the squalid quarter of the brothels.
```

Useful logging:
- `QT_ASSUME_STDERR_HAS_CONSOLE=1` keeps Qt logs on stderr.
- The provider logs `Dex ASR: loaded parakeet ...`, `Dex ASR: final "..."`
  and any dropped-sample warnings at info level; there is no separate
  per-word debug category.

Notes:
- `QT_QPA_PLATFORM=offscreen` is only for headless verification; layer-shell is skipped.
- Single-shot mode has no partial decodes: the whole VAD segment, including
  its trailing silence, is decoded exactly once when the segment closes.

## 5. Live Microphone Test (`tst_live_input`)
The fastest way to see the real pipeline transcribe and evaluate: run the test
and talk.

```sh
./build/tests/tst_live_input
```

It opens the **system default input**, loads the configured bundle, and prints
each final transcript and engine result to stdout as it is decoded. The default
`engine` mode also pushes each final through the production `CommandEngine`
(including the on-demand maths SLM):

```
Live ASR test: input='Usb Audio Device Mono' model='parakeet' mode='engine'
Listening - speak 1 utterance(s) now (25 s timeout)...
  [final]   minus 2 into ln e
  [slm]     {"mode":"eval","expr":"-2*ln(e)"}
  [result]  -2 * ln(e) = -2
Captured 1 utterance(s): minus 2 into ln e
```

The `[partial]` hook still exists for backends that emit live hypotheses, but
single-shot Parakeet never does: `partialTextChanged` stays empty.

Environment overrides:

| Variable | Default | Meaning |
| --- | --- | --- |
| `DEX_TEST_MODE` | `engine` | `asr` (raw transcripts) or `engine` (also evaluate) |
| `DEX_TEST_MODEL_DIR` | `defaultModelDir()` | model base directory |
| `DEX_TEST_SPEECH_TIMEOUT_MS` | `25000` | how long to wait for speech |
| `DEX_TEST_UTTERANCES` | `1` | how many finals to collect |
| `DEX_TEST_EXPECT` | unset | substring the joined transcript must contain |
| `DEX_TEST_EXPECT_RESULT` | unset | substring the joined engine results must contain |

Example: check a spoken sigmoid without any typing.

```sh
DEX_TEST_UTTERANCES=2 DEX_TEST_EXPECT_RESULT='0.666667' ./build/tests/tst_live_input
```

Add `QT_LOGGING_RULES='dex.slm.debug=true'` to get the SLM's raw JSON in the
failure output.

Behaviour:
- Skips (not fails) when there is no default input device, the model bundle is
  incomplete, or nothing was said before the timeout — so `ctest` stays green
  on a quiet machine.
- Also skips immediately when stdout is not a terminal (e.g. under `ctest`), so
  a normal test run is not delayed by the speech timeout. Set
  `DEX_TEST_SPEECH_TIMEOUT_MS` to force it in non-interactive runs.
- `engine` mode soft-skips the SLM checks when the GGUF is absent; the
  deterministic path is still exercised.
- Fails on real regressions: device open errors, a provider that never reaches
  `Listening`, worker errors, or a transcript/result that does not contain
  `DEX_TEST_EXPECT` / `DEX_TEST_EXPECT_RESULT`.

Because it needs hardware and speech, it carries the `live` CTest label:

```sh
ctest --test-dir build --output-on-failure -LE live   # everything else
ctest --test-dir build --output-on-failure -L live    # only this
```

For a fully scripted check (monitor source + known wav, no speaking), use the
recipe in §4.

## 6. Maths SLM Verification
The SLM is optional, so verify both shapes:

**With `llama.cpp` + GGUF** — `tst_engine` runs four real translations
(`sigma`, `softmax` vector, derivative, decimal) and asserts `usedSlm`. To watch
the raw model output:

```sh
QT_LOGGING_RULES='dex.slm.debug=true' ./build/tests/tst_engine
# dex.slm: Loaded ".../Qwen3.5-0.8B-Q4_K_M.gguf" in 341 ms
# dex.slm: raw: "{\"mode\":\"eval\",\"expr\":\"sigmoid(ln(2))\"}"
```

**Without `llama.cpp`** — build a second tree and run the same suite. The
deterministic `spokenMath` rows must still pass and the SLM rows skip:

```sh
cmake -B build-nollama -G Ninja -DBUILD_TESTING=ON \
  -DCMAKE_DISABLE_FIND_PACKAGE_LlamaCpp=ON
cmake --build build-nollama
ctest --test-dir build-nollama --output-on-failure -LE live
# "Dex: llama.cpp not found - maths SLM disabled, deterministic maths only"
```

**Without the GGUF** — either rebuild as above or point
`math_slm.model_path` at a missing file (or set `math_slm.enabled = false`).
`LlamaMathTranslator::available()` returns false, so `CommandEngine` never
constructs a translator and every maths query uses the deterministic path.
