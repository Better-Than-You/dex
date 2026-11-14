---
name: dex-dev
description: Architect, implement, test, and debug Dex — a deterministic C++20/Qt6/QML Material 3 voice command launcher for Arch Linux and Hyprland using layer-shell-qt, toml++, recursive-descent spoken math evaluation, and Unix socket IPC without LLMs. Use whenever working on Dex C++20 modules, QML Material 3 UI, TOML command routing, spoken math parsing, or QtTest/IPC verification.
---

# Dex Development Skill

**Dex** is an offline, deterministic voice command launcher for Arch Linux / Hyprland (`C++20`, `Qt6`, `QML`, `layer-shell-qt`, `toml++`).
**Strict Rule:** Zero LLMs in the runtime pipeline. All text normalization, spoken math evaluation, and command dispatch are 100% deterministic.

## Current Phase: Phase 1 (UI + Deterministic Engine + Stubs)
- Do **not** link `whisper.cpp` or capture live microphone audio yet.
- Keep ASR abstracted behind `ITranscriptProvider` using `StubTranscriptProvider` + Unix socket IPC (`/run/user/$UID/dex.sock`).

## Build & Verify Commands
- Configure: `cmake -B build -G Ninja -DBUILD_TESTING=ON`
- Build: `cmake --build build`
- Unit tests: `ctest --test-dir build --output-on-failure`
- IPC smoke test: `bash .opencode/skills/dex-dev/scripts/ipc-smoke.sh`

## Privilege Escalation
- Whenever elevated privileges are needed (installing packages, writing to system paths), use `pkexec` — never `sudo`.
  Example: `pkexec pacman -S --needed --noconfirm layer-shell-qt`

## Module Layout & Boundaries
1. `src/core/` — `Config` (`toml++` loader for `config.toml`), `AppController` (daemon vs. one-shot state machine), `IpcServer` (`QLocalServer`).
2. `src/engine/` — `CommandEngine`:
   - `TextNormalizer`: lowercase, spoken-math canonicalization (`"into"` -> `*`, `"l.n.e"` -> `ln(e)`), decimal-safe punctuation stripping.
   - `MathEvaluator`: C++20 recursive-descent / shunting-yard parser (`+ - * / ^ %`, `ln`, `log`, `sin`, `cos`, `tan`, `sqrt`, `pi`, `e`, unary minus, implicit multiplication).
   - `RuleRouter`: top-to-bottom `QRegularExpression` matching against `commands.toml` with named slot extraction (`%{query}`, `%{app}`, `%{num}`).
   - `ActionDispatcher`: `QProcess::startDetached` (`$BROWSER` fallback `xdg-open`, app launch) + `wl-copy` clipboard integration.
3. `src/provider/` — `ITranscriptProvider` interface + `StubTranscriptProvider`.
4. `qml/` — `Main.qml` (`LayerShellQt` overlay), `Theme.qml` (M3 dark tonal singleton), `components/` (`StatusPill.qml`, `MathResultCard.qml`, `ConfirmBar.qml`).

## Coding & Security Rules
- **C++20 / Qt6:** Use `Q_OBJECT` / `QML_ELEMENT` with `qt_add_qml_module(dex URI Dex VERSION 1.0 ...)`. Prefer `std::optional`, `std::string_view`, and smart pointers.
- **QML:** Strict property types (`property string`, `required property`). Keep all parsing/routing in C++; QML only binds to `AppController` properties/signals.
- **Injection Safety:** Always percent-encode URL query slots via `QUrl::toPercentEncoding()`. Never invoke `/bin/sh -c` unless explicitly flagged in `commands.toml`.

## On-Demand Reference Files
Load only the reference file matching the active subtask to keep context minimal:
- `references/engine-math-rules.md` — TOML schemas, spoken-math regex table, parser edge cases, confirmation gate.
- `references/ui-m3-hyprland.md` — `LayerShellQt` window flags, keyboard interactivity switching, M3 color/shape tokens, QML states.
- `references/ipc-and-testing.md` — Unix socket protocol specification and `QtTest` test cases.
