# Dex IPC Socket Protocol & QtTest Matrix

## 1. Unix Domain Socket (`/run/user/$UID/dex.sock`)
Implemented via `QLocalServer` (`Qt6::Network`), newline-delimited commands:
- `SHOW` / `HIDE` / `TOGGLE` — Overlay visibility in daemon mode
- `PARTIAL:<text>` — Feed simulated live partial transcript
- `FINAL:<text>` — Feed finalized utterance into `CommandEngine`
- `SET_MODEL:<parakeet|stub>` — Select ASR backend
- `CONFIRM` / `CANCEL` — Resolve `Confirming` state
- `QUIT` — Terminate daemon cleanly

Stub mode (`--stub` or `asr.provider = "stub"`) exercises the same socket
without a microphone or model files.

## 2. Required `QtTest` Suites (`tests/`, 8 offline + `tst_live_input`)
- `tst_normalizer`:
  - `"Hey Dex, search for Arch Linux Wiki."` -> `"search for arch linux wiki"`
  - `"minus 2 into l.n.e"` -> `"-2 * ln(e)"`
  - `"3.14 plus 2.5"` -> `"3.14 + 2.5"`
- `tst_math` (21 cases):
  - `"minus 2 multiplied by l.n.e"` -> `-2.0`
  - `"minus 2 into l.n.e"` -> `-2.0`
  - `"10 plus 5 times 2"` -> `20.0`
  - `"sqrt 16 plus 2 power 3"` -> `12.0`
  - `"minus. is zero point eight"` -> `-0.8`; `"sigma function of ln2"` -> `0.666...`
  - `sigmoid(ln(2))` -> `0.666667`, `relu(-2)` -> `0`, `softmax([1, 0.1, -3])` -> `[0.7018, 0.2853, 0.0128]`
  - derivatives: `0.5*(y_hat-y)^2` wrt `y_hat` -> `y_hat - y`, `sigmoid(x)` -> `sigmoid(x) * (1 - sigmoid(x))`
  - division by zero -> `std::nullopt`
- `tst_router`:
  - `"search C++20 concepts"` -> matches `web_search`, encodes query as `C%2B%2B20%20concepts`
  - `"launch kitty"` -> matches `launch_app`, program `kitty`
  - `"workspace 4"` -> matches `hypr_workspace`, args `["dispatch", "workspace", "4"]`
- `tst_config`: shipped `config.toml`, defaults, `~` expansion, clamping
  (incl. `[math_slm]`), broken TOML.
- `tst_engine`: router-first flow; `spokenMath` proves the deterministic fallback
  (`usedSlm == false`), `nonMathTrigger` proves trigger words alone never reach
  the SLM, and `slmTranslatesSpeech` asserts real translations when the GGUF is
  present and skips otherwise.
- `tst_ipc`: control commands, `PARTIAL:`/`FINAL:`, `SET_MODEL:`, buffered lines.
- `tst_provider`: factory/flag selection, aliases, Parakeet layout, missing-file rejection.
- `tst_audio`: SPSC ring-buffer round-trip, wrap-around, drop count, reset.
- `tst_live_input` (`live` label): default input -> VAD -> sherpa-onnx ->
  `CommandEngine`; prints `[partial]`, `[final]`, `[slm]`, `[result]`.
  `DEX_TEST_MODE=asr|engine` (default `engine`), `DEX_TEST_EXPECT_RESULT`.

## 3. Verification Recipes
- Unit: `ctest --test-dir build --output-on-failure -LE live`
- Llama-free build: `cmake -B build-nollama -G Ninja -DBUILD_TESTING=ON
  -DCMAKE_DISABLE_FIND_PACKAGE_LlamaCpp=ON`, then build + ctest.
- Stub IPC:
  ```sh
  ./build/dex --stub --socket /tmp/dex-stub.sock
  printf 'SHOW\nSET_MODEL:parakeet\nFINAL:minus 2 into l.n.e\nQUIT\n' \
    | socat - UNIX-CONNECT:/tmp/dex-stub.sock
  ```
- Model selection: send `SET_MODEL:parakeet` or `SET_MODEL:stub` and check
  the status response.
- End-to-end ASR: route the default source to the speaker monitor, play a
  16 kHz mono sample, and check `Dex ASR: final "..."`. Restore the source
  afterwards. See `docs/TESTING.md` for the full script.
- Spoken maths: `./build/tests/tst_live_input` (engine mode) and speak, e.g.
  "minus two into l n e"; watch `[slm]` and `[result]`.
- Debug partials with `QT_LOGGING_RULES='dex.asr.debug=true'`; debug SLM JSON
  with `dex.slm.debug=true`. Keep Qt logs on stderr with
  `QT_ASSUME_STDERR_HAS_CONSOLE=1`.
