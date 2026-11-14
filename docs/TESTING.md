# Testing & Stub Verification Guide

## 1. Automated Unit Tests (`QtTest`)
Run via `ctest --test-dir build --output-on-failure` (9 suites):

1. `tst_normalizer`:
   - `"Hey Dex, search for Arch Linux Wiki."` → `"search for arch linux wiki"`
   - `"minus 2 into l.n.e"` → `"-2 * ln(e)"`
   - `"3.14 plus 2.5"` → `"3.14 + 2.5"`
2. `tst_math`:
   - `"minus 2 multiplied by l.n.e"` → `-2.0`
   - `"minus 2 into l.n.e"` → `-2.0`
   - `"10 plus 5 times 2"` → `20.0`
   - `"sqrt 16 plus 2 power 3"` → `12.0`
   - Division by zero → `std::nullopt`
3. `tst_router`:
   - `"search C++20 concepts"` → matches `web_search`, URL encodes `C%2B%2B20%20concepts`
   - `"launch kitty"` → matches `launch_app`, program `kitty`
   - `"workspace 4"` → matches `hypr_workspace`, args `["dispatch", "workspace", "4"]`
4. `tst_config`:
   - shipped `config/config.toml` parses with the Phase 2 `[asr]` defaults
   - missing keys keep defaults (`provider = "sherpa"`, `model_profile = "moonshine"`)
   - `~` expansion, clamping (`threads` 1–16, `vad_threshold` 0.05–0.95), and
     broken TOML rejection
5. `tst_engine`: end-to-end normalise → math/route → dispatch decisions.
6. `tst_ipc`: control commands, `PARTIAL:`/`FINAL:`, `SET_MODEL:`, and
   buffered/partial lines.
7. `tst_provider`:
   - factory defaults/`--stub` override and unknown-profile fallback
   - legacy alias canonicalisation (`balanced` → `moonshine`)
   - Moonshine and Parakeet bundle discovery incl. required files
   - missing VAD/joiner rejection, stub creation, missing-model failure
8. `tst_audio`: SPSC ring-buffer round-trip, wrap-around, overflow drop count,
   and reset.
9. `tst_live_input` (labelled `live`): end-to-end microphone test through the
   production `AudioCapture` → Silero VAD → sherpa-onnx path using the system
   default input. It has no fixed input to assert on, so it soft-skips when
   there is no input device, the model bundle is missing, or no speech arrives
   within the timeout. See §5.

## 2. Stub Smoke Test (no microphone required)
Run Dex with the text-injection backend and a private socket:

```sh
./build/dex --stub --socket /tmp/dex-stub.sock
printf 'SHOW\nSET_MODEL:parakeet\nPARTIAL:minus 2 into\nFINAL:minus 2 into l.n.e\nQUIT\n' \
  | socat - UNIX-CONNECT:/tmp/dex-stub.sock
```

`--stub` also works without a model directory, so it is the fastest way to
exercise the UI and command engine on a machine without models.

## 3. Manual IPC Smoke Test (live backend)
Start `./build/dex` and send commands over `/run/user/$(id -u)/dex.sock`:

- `printf "SHOW\n" | socat - UNIX-CONNECT:/run/user/$(id -u)/dex.sock`
- `printf "SET_MODEL:parakeet\n" | socat - UNIX-CONNECT:/run/user/$(id -u)/dex.sock`
- `printf "SET_MODEL:moonshine\n" | socat - UNIX-CONNECT:/run/user/$(id -u)/dex.sock`
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

Expected final transcript:

```
After early nightfall, the yellow lamps would light up here and there the squalid quarter of the brothels.
```

Useful logging:
- `QT_ASSUME_STDERR_HAS_CONSOLE=1` keeps Qt logs on stderr.
- `QT_LOGGING_RULES='dex.asr.debug=true'` prints partial transcripts
  (`dex.asr` category).

Notes:
- `--qpa offscreen` is only for headless verification; layer-shell is skipped.
- Partial decodes are padded with 350 ms of silence so Moonshine does not loop
  on the last word; finals use the full VAD segment including trailing silence.

## 5. Live Microphone Test (`tst_live_input`)
The fastest way to see the real pipeline transcribe: run the test and talk.

```sh
./build/tests/tst_live_input
```

It opens the **system default input**, loads the configured bundle, and streams
partials and finals to stdout as they are decoded:

```
Live ASR test: input='Usb Audio Device Mono' model='moonshine'
Listening - speak 1 utterance(s) now (25 s timeout)...
  [partial] search arch linux wiki
  [final]   search arch linux wiki
Captured 1 utterance(s): search arch linux wiki
```

Environment overrides:

| Variable | Default | Meaning |
| --- | --- | --- |
| `DEX_TEST_PROFILE` | `moonshine` | `moonshine` or `parakeet` |
| `DEX_TEST_MODEL_DIR` | `defaultModelDir()` | model base directory |
| `DEX_TEST_SPEECH_TIMEOUT_MS` | `25000` | how long to wait for speech |
| `DEX_TEST_UTTERANCES` | `1` | how many finals to collect |
| `DEX_TEST_EXPECT` | unset | substring the joined transcript must contain |

Add `QT_LOGGING_RULES='dex.asr.debug=true'` to also get the `dex.asr` category
logs in the failure output.

Behaviour:
- Skips (not fails) when there is no default input device, the model bundle is
  incomplete, or nothing was said before the timeout — so `ctest` stays green
  on a quiet machine.
- Also skips immediately when stdout is not a terminal (e.g. under `ctest`), so
  a normal test run is not delayed by the speech timeout. Set
  `DEX_TEST_SPEECH_TIMEOUT_MS` to force it in non-interactive runs.
- Fails on real regressions: device open errors, a provider that never reaches
  `Listening`, worker errors, or a transcript that does not contain
  `DEX_TEST_EXPECT`.

Because it needs hardware and speech, it carries the `live` CTest label:

```sh
ctest --test-dir build --output-on-failure -LE live   # everything else
ctest --test-dir build --output-on-failure -L live    # only this
```

For a fully scripted check (monitor source + known wav, no speaking), use the
recipe in §4.
