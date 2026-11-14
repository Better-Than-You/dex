# Deterministic Command & Math Engine Spec (`DexEngine`)

## 1. Pipeline Execution Order
When `utteranceFinalized(rawText)` arrives:

### Step 0: Normalise
`TextNormalizer::normalize` produces a canonical lowercase string (see §2) and
`TextNormalizer::derivative` extracts a `DerivativeRequest` when the utterance
carries a `… in respect to <var>` / `… wrt <var>` marker.

### Step 1: `RuleRouter` — always first
`commands.toml` rules are evaluated top-to-bottom against the normalised text.
A match extracts named capture groups (`%{query}`, `%{app}`, `%{num}`), resolves
the action and returns immediately. **Desktop commands never invoke the SLM**,
so the fast path stays at 0 ms.

### Step 2: Maths/calculus gate
Only if no rule matched, a derivative request **or** (`isMathTrigger(raw)`
**and** `hasMathContent(raw)`) decides whether the utterance is maths at all.
`hasMathContent` requires a digit, a known function/constant, or a number word,
so phrases such as "turn the power off" (which contain the trigger word
`power`) stop here and return `OutcomeKind::None` without waking the SLM.

### Step 3: Optional maths SLM
When `LlamaMathTranslator::available()` (built with `llama.cpp`,
`math_slm.enabled = true`, GGUF present), the raw transcript is translated into
one compact JSON command:

```json
{"mode":"eval","expr":"sigmoid(ln(2))"}
{"mode":"diff","expr":"0.5*(y_hat-y)^2","var":"y_hat"}
```

- `mode = "eval"` → `MathEvaluator::evaluate(expr)`
- `mode = "diff"` → `SymbolicDifferentiator::differentiate(expr, var)`

The model loads on the first query and is freed after
`math_slm.idle_unload_sec`. See `docs/ARCHITECTURE.md` §5 for the lifecycle.

### Step 4: Deterministic fallback
If the SLM is unavailable, times out, or returns an unusable command:

1. Derivative request → `SymbolicDifferentiator::differentiate` on the
   normalised expression and variable.
2. Otherwise `MathEvaluator::evaluateUtterance(normalized)`.

On success the result is formatted, copied with `wl-copy`, and shown in
`MathResultCard`; `AppController` may dispatch a routed action instead.

## 2. `TextNormalizer`
1. Trim whitespace and lowercase input.
2. Remove fillers: `uh`, `uhm`, `um`, `umm`, `erm`.
3. Build vectors from spoken phrases (before other rules):
   `softness function for the vector one, zero point one comma minus three`
   → `softmax([1, 0.1, -3])`.
4. Convert spoken math phrases BEFORE stripping punctuation so `l.n.e` is preserved:
   - `\bminus\s*[.,]?\s*(?:is|equals|equal to)\b` → `-` (`minus. is 0.8` → `-0.8`)
   - `\bsigma\b` → `sigmoid`, `\bsoftness\b` → `softmax`
   - `\by\s+hat\b` → `y_hat`, `\by\s+bar\b` → `y_bar`
   - `\bl\.?n\.?\s*\(\s*e\s*\)` → `ln(e)`
   - `\bl\.?n\.?e\b` → `ln(e)`
   - `\bl\.?n\.?\b` → `ln`
   - `\blog\b` → `log`, `\bexp\b` → `exp`
   - `\b(?:multiplied by|times|into)\b` → `*`
   - `\b(?:divided by|over|by)\b` → `/`
   - `\b(?:plus)\b` → `+`
   - `\b(?:minus|negative)\b` → `-`
   - `\b(?:to the power of|power|raised to)\b` → `^`
   - `\bsquared\b` → `^2`, `\bcubed\b` → `^3`
   - `\b(?:square root of|root)\b` → `sqrt`
   - `<expr> whole square` → `(<expr>)^2` for `a minus b` style phrases
   - `\bpi\b` → `3.141592653589793`
   - `\be\b` → `2.718281828459045` (standalone `e` only; `ln(e)` is protected)
   - `\bfunction\s+(?:of|for)\b` → removed
   - `\bln\s*([0-9]+(\.[0-9]+)?)\b` → `ln(<n>)` (`ln2` → `ln(2)`)
5. Map number words **only when the utterance is in maths context**
   (`calculate`, `solve`, `plus`, `times`, `derivative`, `sigmoid`, `softmax`,
   …): `zero point one` → `0.1`, `half` → `0.5`, `quarter` → `0.25`. Desktop
   queries such as `search two towers` are left untouched.
6. Strip trailing punctuation (`.`, `?`, `!`, `,`) while preserving decimal dots
   between digits (`3.14`).
7. Strip optional wake prefixes: `^(?:hey dex|dex|please|can you)[\s,]+`.
8. Canonicalise spacing: remove unary gaps (`- 2` → `-2`), tighten function
   calls (`ln (2)` → `ln(2)`), and space the power operator (`5 ^ 2`).

## 3. `MathEvaluator`
Entry points:

- `evaluate(expression)` — bare expression.
- `evaluateUtterance(normalized)` — applies the trigger rules first.
- `looksLikeMath(expression)` — every word is a known function/constant.
- `isMathTrigger(text)` — cheap pre-filter used to decide whether the SLM is
  worth invoking.
- `hasMathContent(text)` — true when the text holds a digit, a known
  function/constant, or a number word. The engine gates on
  `isMathTrigger && hasMathContent` so ordinary phrases that merely contain a
  trigger word ("turn the power off") never reach the SLM.
- `format(double)` / `format(MathResult)`.

Grammar:

- Operators `+ - * / ^ %`, parentheses, unary minus, implicit multiplication
  (`2pi`, `-2ln(e)`, `3(4+1)`).
- Scalar functions `ln`, `log` (base 10), `sin`, `cos`, `tan`, `sqrt`, `abs`,
  `sigmoid` (alias `sigma`), `relu`, `exp`.
- Vectors `[a, b, c]` and `softmax([...])` using a numerically stable,
  max-shifted exponential.
- Constants `pi` and `e`.

Results:

- `MathResult::Kind::Scalar` or `Kind::Vector`.
- `std::nullopt` on syntax errors, division/modulo by zero, unknown identifiers,
  and non-finite values.
- Scalars format with 6 significant digits, collapsing integer fuzz
  (`0.1 + 0.2` → `0.3`, `sigmoid(ln(2))` → `0.666667`).
- Vectors truncate (not round) to 4 decimals and trim trailing zeros, e.g.
  `softmax([1, 0.1, -3])` → `[0.7018, 0.2853, 0.0128]`.

## 4. `SymbolicDifferentiator`
- `differentiate(expression, variable)` → simplified derivative string.
- `simplify(expression)` → simplified expression.

Parsing supports constants, variables (including `y_hat`), `+ - * / ^`,
parentheses, and `ln`, `exp`, `sin`, `cos`, `sigmoid`, `sqrt` (`sqrt u` becomes
`u ^ 0.5`).

Rules:

- Sum, difference, product, quotient.
- Power rule with the chain rule for constant exponents; the general
  `(u^v)' = u^v (v' ln u + v u' / u)` otherwise.
- `ln u`, `exp u`, `sin u`, `cos u`, `sigmoid u` with the chain rule.

Simplification: constant folding, `0 + x → x`, `x - x → 0`, `0 * x → 0`,
`1 * x → x`, `x^1 → x`, `x^0 → 1`, and flattening of chained sums/products.

Examples:

| Input | Variable | Output |
| --- | --- | --- |
| `0.5 * (y_hat - y)^2` | `y_hat` | `y_hat - y` |
| `3 * x^2 + 2 * x` | `x` | `2 + 6 * x` |
| `sigmoid(x)` | `x` | `sigmoid(x) * (1 - sigmoid(x))` |
| `x^2` | `t` | `0` |

## 5. `RuleRouter` & `ActionDispatcher`
1. Evaluate `commands.toml` rules top-to-bottom using `QRegularExpression`.
2. Extract named capture groups (`%{query}`, `%{app}`, `%{num}`).
3. If `type == "url"`, percent-encode captured slots via
   `QString::fromUtf8(QUrl::toPercentEncoding(val))`.
4. Resolve `$BROWSER` via `qEnvironmentVariable("BROWSER")`, falling back to
   `"xdg-open"`.
5. Effective confirm = `rule.confirm.value_or(config.general.confirm_by_default)`.
   - If `true`: enter `State::Confirming` and await `CONFIRM` (`Enter`) or
     `CANCEL` (`Esc`).
   - If `false`: execute immediately via `QProcess::startDetached(program, args)`.
