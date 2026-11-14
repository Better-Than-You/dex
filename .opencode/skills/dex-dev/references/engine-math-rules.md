# Dex Engine, TOML & Spoken Math Specification

## 1. Config Files (`~/.config/dex/`)
- `config.toml`:
  - `[general]`: `daemon = true`, `confirm_by_default = false`, `auto_hide_ms = 2200`
  - `[asr]`: `provider = "sherpa"` (`"sherpa"` | `"stub"`), `model_profile = "moonshine"` (`"moonshine"` | `"parakeet"`; legacy `light`/`balanced` -> `moonshine`, `accurate` -> `parakeet`), `model_dir = "~/.local/share/dex/models"`, `vad_model = ""`, `threads = 4`, `silence_timeout_ms = 800`, `partial_interval_ms = 400`, `vad_threshold = 0.5`
  - `[ui]`: `anchor = "top"`, `margin_top = 48`, `width = 460`
- `commands.toml`:
  - Array of `[[commands]]` tables with `name`, `patterns` (regex array with named groups `(?<query>.+)`), `type` (`"url"` | `"exec"`), `exec`, `args`, and optional `confirm = true|false`.

## 2. TextNormalizer Order of Operations
1. Trim and lowercase raw text.
2. Apply spoken-math replacements BEFORE dot stripping so `l.n.e` is preserved:
   - `\bl\.?n\.?\s*\(\s*e\s*\)` -> `ln(e)` (parenthesised form; exact regex in SPEC_ENGINE leaves a stray `)` on `ln(e)`)
   - `\bl\.?n\.?e\b` -> `ln(e)` (bare `l.n.e` form)
   - `\bl\.?n\.?\b` -> `ln`
   - `\blog\b` -> `log`
   - `\b(?:multiplied by|times|into)\b` -> `*`
   - `\b(?:divided by|over|by)\b` -> `/`
   - `\bplus\b` -> `+`
   - `\b(?:minus|negative)\b` -> `-`
   - `\b(?:to the power of|power|raised to)\b` -> `^`
   - `\bsquared\b` -> `^2`
   - `\bcubed\b` -> `^3`
   - `\b(?:square root of|root)\b` -> `sqrt`
   - `\bpi\b` -> `3.141592653589793`
   - `\be\b` -> `2.718281828459045`
3. Strip trailing punctuation (`.`, `?`, `!`, `,`) while preserving decimal dots between digits (`3.14`).
4. Strip wake prefixes: `^(?:hey dex|dex|please|can you)\s+`.

## 3. MathEvaluator Rules
- Trigger when text starts with `calculate `, `what is `, `solve `, OR contains only numbers, operators (`+ - * / ^ % ( )`), and functions (`ln`, `log`, `sin`, `cos`, `tan`, `sqrt`).
- Support unary minus (`-2 * ln(e)` -> `-2`) and implicit multiplication (`2pi`, `-2ln(e)`).
- Return `std::optional<double>` (`std::nullopt` on invalid syntax or division by zero).
- Copy formatted result to clipboard via `wl-copy` and transition `AppController` to `MathResult`.

## 4. RuleRouter & Confirmation Gate
- Match normalized text against `commands.toml` `QRegularExpression` rules in order.
- URL-encode slots with `QString::fromUtf8(QUrl::toPercentEncoding(val))` when `type == "url"`.
- Resolve `$BROWSER` via `qEnvironmentVariable("BROWSER")`, fallback `"xdg-open"`.
- `effectiveConfirm = rule.confirm.value_or(config.general.confirm_by_default)`.
  - `true` -> `State::Confirming` (wait for `CONFIRM` or `CANCEL`).
  - `false` -> `QProcess::startDetached(program, args)` immediately.
