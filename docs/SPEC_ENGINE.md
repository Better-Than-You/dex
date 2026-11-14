# Deterministic Command & Math Engine Spec (`DexEngine`)

## 1. Pipeline Execution Order
When `utteranceFinalized(rawText)` arrives:

### Step 1: `TextNormalizer`
1. Trim whitespace and lowercase input.
2. Convert spoken math phrases BEFORE stripping punctuation so `l.n.e` is preserved:
   - `\bl\.?n\.?\s*\(?e\)?\b` -> `ln(e)`
   - `\bl\.?n\.?\b` -> `ln`
   - `\blog\b` -> `log`
   - `\b(?:multiplied by|times|into)\b` -> `*`
   - `\b(?:divided by|over|by)\b` -> `/`
   - `\b(?:plus)\b` -> `+`
   - `\b(?:minus|negative)\b` -> `-`
   - `\b(?:to the power of|power|raised to)\b` -> `^`
   - `\bsquared\b` -> `^2`
   - `\bcubed\b` -> `^3`
   - `\b(?:square root of|root)\b` -> `sqrt`
   - `\bpi\b` -> `3.141592653589793`
   - `\be\b` -> `2.718281828459045` (standalone `e` only, not inside words)
3. Strip trailing punctuation (`.`, `?`, `!`, `,`) while preserving decimal dots between digits (`3.14`).
4. Strip optional wake prefixes: `^(?:hey dex|dex|please|can you)\s+`.

### Step 2: Math Detection & `MathEvaluator`
Trigger math evaluation if normalized text starts with `calculate `, `what is `, `solve `, OR consists solely of digits, math operators (`+ - * / ^ % ( )`), and functions (`ln`, `log`, `sin`, `cos`, `tan`, `sqrt`).
- Implement a clean recursive-descent or shunting-yard parser in C++20.
- Support unary minus (`-2 * ln(e)` -> `-2`) and implicit multiplication (`2pi`, `-2ln(e)`).
- Return `std::optional<double>` (`std::nullopt` on syntax error or division by zero).
- On success: format cleanly (strip trailing `.000000`), copy to clipboard via `wl-copy`, and display `MathResultCard`.

### Step 3: `RuleRouter` & `ActionDispatcher`
1. Evaluate `commands.toml` rules top-to-bottom using `QRegularExpression`.
2. Extract named capture groups (`%{query}`, `%{app}`, `%{num}`).
3. If `type == "url"`, percent-encode captured slots via `QString::fromUtf8(QUrl::toPercentEncoding(val))`.
4. Resolve `$BROWSER` via `qEnvironmentVariable("BROWSER")`, falling back to `"xdg-open"`.
5. Effective confirm = `rule.confirm.value_or(config.general.confirm_by_default)`.
   - If `true`: enter `State::Confirming` and await `CONFIRM` (`Enter`) or `CANCEL` (`Esc`).
   - If `false`: execute immediately via `QProcess::startDetached(program, args)`.
