# Testing & Stub Verification Guide

## 1. Automated Unit Tests (`QtTest`)
Run via `ctest --test-dir build --output-on-failure`:
1. `tst_normalizer`:
   - `"Hey Dex, search for Arch Linux Wiki."` -> `"search for arch linux wiki"`
   - `"minus 2 into l.n.e"` -> `"-2 * ln(e)"`
   - `"3.14 plus 2.5"` -> `"3.14 + 2.5"`
2. `tst_math`:
   - `"minus 2 multiplied by l.n.e"` -> `-2.0`
   - `"minus 2 into l.n.e"` -> `-2.0`
   - `"10 plus 5 times 2"` -> `20.0`
   - `"sqrt 16 plus 2 power 3"` -> `12.0`
   - Division by zero -> `std::nullopt`
3. `tst_router`:
   - `"search C++20 concepts"` -> matches `web_search`, URL encodes `C%2B%2B20%20concepts`
   - `"launch kitty"` -> matches `launch_app`, program `kitty`
   - `"workspace 4"` -> matches `hypr_workspace`, args `["dispatch", "workspace", "4"]`

## 2. Manual IPC Smoke Test
Start `./build/dex` and send commands over `/run/user/$(id -u)/dex.sock`:
- `printf "SHOW\n" | socat - UNIX-CONNECT:/run/user/$(id -u)/dex.sock`
- `printf "PARTIAL:minus 2 into\n" | socat - UNIX-CONNECT:/run/user/$(id -u)/dex.sock`
- `printf "FINAL:minus 2 into l.n.e\n" | socat - UNIX-CONNECT:/run/user/$(id -u)/dex.sock`
- `printf "FINAL:search hyprland layer shell\n" | socat - UNIX-CONNECT:/run/user/$(id -u)/dex.sock`
