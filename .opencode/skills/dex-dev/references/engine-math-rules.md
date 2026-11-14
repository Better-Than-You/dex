# Dex Engine, TOML & Spoken Math Specification

> Phase 2.1 adds an **on-demand** `llama.cpp` translator for noisy maths and
> calculus transcripts, a vector/ML-aware evaluator, and a symbolic
> differentiator. The deterministic path still handles every maths query when
> the SLM is unavailable (no llama.cpp build, no GGUF, disabled config), so the
> engine stays fully functional offline without the model.

## 1. Config Files (`~/.config/dex/`)
- `config.toml`:
  - `[general]`: `daemon = true`, `confirm_by_default = false`, `auto_hide_ms = 2200`
  - `[asr]`: `provider = "sherpa"` (`"sherpa"` | `"stub"`), `model_dir = "~/.local/share/dex/models"`, `vad_model = ""`, `threads = 4`, `silence_timeout_ms = 800`, `vad_threshold = 0.5`
  - `[math_slm]`: `enabled = true`, `model_path = "~/.local/share/dex/models/Qwen3.5-0.8B-Q4_K_M.gguf"`, `threads = 4` (clamped 1–16), `idle_unload_sec = 60` (clamped 0–3600; `0` keeps the model resident)
  - `[ui]`: `anchor = "top"`, `margin_top = 48`, `width = 460`
- `commands.toml`:
  - Array of `[[commands]]` tables with `name`, `patterns` (regex array with named groups `(?<query>.+)`), `type` (`"url"` | `"exec"`), `exec`, `args`, and optional `confirm = true|false`.
- `defaultSlmModelPath()` resolves to `$XDG_DATA_HOME/dex/models/Qwen3.5-0.8B-Q4_K_M.gguf` (or `~/.local/share/dex/models/...`).

## 2. Pipeline Order (`CommandEngine::process`)
1. **Deterministic fast path (0 ms):** normalise, then run `commands.toml` rules. A matched desktop command never loads or consults the SLM.
2. **Maths/calculus gate:** `TextNormalizer::derivative(raw)`, or `MathEvaluator::isMathTrigger(raw) && MathEvaluator::hasMathContent(raw)`. `hasMathContent` requires a digit, a known function/constant or a number word, so trigger words alone ("turn the power off") stop here and return `OutcomeKind::None`.
3. **Optional SLM:** `LlamaMathTranslator::translate(raw)` -> `{"mode":"eval|diff","expr":...,"var":...}`.
   - `eval` is evaluated by `MathEvaluator::evaluate`; `diff` by `SymbolicDifferentiator::differentiate`.
   - Any load failure, timeout, bad JSON, or unusable expression falls through to step 4 (logged via `dex.slm`).
4. **Deterministic fallback:** `SymbolicDifferentiator` for derivative requests, otherwise `MathEvaluator::evaluateUtterance(normalized)`.

`Outcome` also carries diagnostics: `usedSlm`, `slmMode`, `slmExpression`.

## 3. TextNormalizer Order of Operations
1. Trim and lowercase; remove fillers (`uh`, `uhm`, `um`, `umm`, `erm`).
2. Build spoken vectors first (`softness function for the vector one, zero point one comma minus three` -> `softmax([1, 0.1, -3])`).
3. Apply spoken-math replacements BEFORE dot stripping so `l.n.e` is preserved:
   - `\bminus\s*[.,]?\s*(?:is|equals|equal to)\b` -> `-` (ASR splits the sign: `minus. is 0.8` -> `-0.8`)
   - `\bsigma\b` -> `sigmoid`, `\bsoftness\b` -> `softmax`
   - `\by\s+hat\b` -> `y_hat`, `\by\s+bar\b` -> `y_bar`
   - `\bl\.?n\.?\s*\(\s*e\s*\)` -> `ln(e)`, `\bl\.?n\.?e\b` -> `ln(e)`, `\bl\.?n\.?\b` -> `ln`
   - `\blog\b` -> `log`, `\bexp\b` -> `exp`
   - `\b(?:multiplied by|times|into)\b` -> `*`
   - `\b(?:divided by|over|by)\b` -> `/`
   - `\bplus\b` -> `+`
   - `\b(?:minus|negative)\b` -> `-`
   - `\b(?:to the power of|power|raised to)\b` -> `^`
   - `\bsquared\b` -> `^2`, `\bcubed\b` -> `^3`
   - `\b(?:square root of|root)\b` -> `sqrt`
   - `… whole square` -> `(…)^2` (groups `y_hat minus y` before exponentiation)
   - `\bpi\b` -> `3.141592653589793`
   - `\be\b` -> `2.718281828459045` (guarded so the `e` in `ln(e)` is untouched)
   - `ln2` -> `ln(2)` (bare ASR tokens become explicit calls)
4. Number words are mapped **only in maths context** (`zero point one` -> `0.1`, `half` -> `0.5`), so desktop queries such as "search two towers" are not rewritten.
5. Strip trailing punctuation (`.`, `?`, `!`, `,`) while preserving decimal dots between digits (`3.14`).
6. Strip wake prefixes: `^(?:hey dex|dex|please|can you)[\s,]+`.
7. Canonical spacing: remove unary gaps (`- 2` -> `-2`), tighten calls (`ln (2)` -> `ln(2)`), space `^`.

`TextNormalizer::derivative(raw)` returns `DerivativeRequest{expression, variable}` for `… in respect to <var>` / `… wrt <var>`; a leading `d/d<var>` in the expression part is stripped.

## 4. MathEvaluator
- Public API: `std::optional<MathResult> evaluate(QString)`, `evaluateUtterance(QString)`, `looksLikeMath()`, `isMathTrigger()`, `hasMathContent()`, `format(double)`, `format(MathResult)`.
- `MathResult` is either a scalar or a vector (`softmax([...])`).
- Scalar functions: `ln`, `log` (base 10), `sin`, `cos`, `tan`, `sqrt`, `abs`, `sigmoid` (alias `sigma`), `relu`, `exp`.
- Vector functions: `softmax([a, b, c])` using the numerically stable max-shifted exponential.
- Grammars: unary minus, implicit multiplication (`2pi`, `-2ln(e)`, `3(4+1)`), right-associative `^`, `%` as `fmod`.
- `std::nullopt` on syntax errors, division/modulo by zero, unknown identifiers, and non-finite results.
- Formatting: scalars use 6 significant digits and collapse integer fuzz (`0.1 + 0.2` -> `0.3`); vectors truncate (not round) to 4 decimals and trim zeros, e.g. `[0.7018, 0.2853, 0.0128]`.

## 5. SymbolicDifferentiator
- Parses `+ - * / ^`, parentheses, constants, variables (`x`, `y_hat`, …), and `ln`, `exp`, `sin`, `cos`, `sigmoid`, `sqrt` (`sqrt` is lowered to `^0.5`).
- Rules: sum, difference, product, quotient, power (constant and general exponent), and chain rule for the unary functions.
- Simplifier: constant folding, `0 + x -> x`, `x - x -> 0`, `0 * x -> 0`, `1 * x -> x`, `x^1 -> x`, `x^0 -> 1`, and flattening of chained sums/products.
- `differentiate("0.5 * (y_hat - y)^2", "y_hat")` -> `y_hat - y`.
- Variables other than the differentiation variable are treated as constants (`x^2` wrt `t` -> `0`).
- Returns `std::optional<QString>`; `std::nullopt` means "not parseable".

## 6. LlamaMathTranslator (Phase 2.1)
- Optional at build time: `cmake/FindLlamaCpp.cmake` sets `DEX_HAVE_LLAMA=1` when `llama.h` + `libllama.so` are found. Without it, the class compiles to unavailable stubs and the deterministic path is used.
- Optional at runtime: `available()` requires `math_slm.enabled`, a non-empty path, and an existing GGUF. Nothing is loaded at startup.
- Cold start on the first maths query; `idle_unload_sec` then frees the model so idle memory returns to zero.
- Prompt: system rules + `Input:`/`Output:` few-shot pairs in the **user** turn, with the transcript as the final `Input:`. Keeping the examples out of the assistant turn avoids the model echoing the previous example.
- Assistant turn is pre-filled with `<think\n</think\n` to suppress Qwen reasoning.
- Output is constrained by a GBNF grammar: `{"mode":"eval|diff","expr":"…"}` (+ optional `"var"`). Generation stops at the first `}`.
- Qwen3.5 is a hybrid attention/recurrent model, so a context is created per query (`ContextGuard`) instead of being reset; only the model stays memory-mapped.
- Debugging: `QT_LOGGING_RULES='dex.slm.debug=true'` prints load timing and the raw JSON.

## 7. RuleRouter & Confirmation Gate
- Match normalized text against `commands.toml` `QRegularExpression` rules in order.
- URL-encode slots with `QString::fromUtf8(QUrl::toPercentEncoding(val))` when `type == "url"`.
- Resolve `$BROWSER` via `qEnvironmentVariable("BROWSER")`, fallback `"xdg-open"`.
- `effectiveConfirm = rule.confirm.value_or(config.general.confirm_by_default)`.
  - `true` -> `State::Confirming` (wait for `CONFIRM` or `CANCEL`).
  - `false` -> `QProcess::startDetached(program, args)` immediately.
