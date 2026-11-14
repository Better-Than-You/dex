# Dex IPC Socket Protocol & QtTest Matrix

## 1. Unix Domain Socket (`/run/user/$UID/dex.sock`)
Implemented via `QLocalServer` (`Qt6::Network`), newline-delimited commands:
- `SHOW` / `HIDE` / `TOGGLE` — Overlay visibility in daemon mode
- `PARTIAL:<text>` — Feed simulated live partial transcript
- `FINAL:<text>` — Feed finalized utterance into `CommandEngine`
- `CONFIRM` / `CANCEL` — Resolve `Confirming` state
- `QUIT` — Terminate daemon cleanly

## 2. Required `QtTest` Suites (`tests/`)
- `tst_normalizer`:
  - `"Hey Dex, search for Arch Linux Wiki."` -> `"search for arch linux wiki"`
  - `"minus 2 into l.n.e"` -> `"-2 * ln(e)"`
  - `"3.14 plus 2.5"` -> `"3.14 + 2.5"`
- `tst_math`:
  - `"minus 2 multiplied by l.n.e"` -> `-2.0`
  - `"minus 2 into l.n.e"` -> `-2.0`
  - `"10 plus 5 times 2"` -> `20.0`
  - `"sqrt 16 plus 2 power 3"` -> `12.0`
  - Division by zero -> `std::nullopt`
- `tst_router`:
  - `"search C++20 concepts"` -> matches `web_search`, encodes query as `C%2B%2B20%20concepts`
  - `"launch kitty"` -> matches `launch_app`, program `kitty`
  - `"workspace 4"` -> matches `hypr_workspace`, args `["dispatch", "workspace", "4"]`
