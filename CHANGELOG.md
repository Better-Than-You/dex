# Changelog

All notable changes to Dex are documented in this file. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and versions follow
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.0.0] - 2026-11-20

### Added

- Single-shot offline ASR: 16 kHz capture, Silero VAD and NVIDIA Parakeet TDT
  0.6B v2 INT8 recognition on a dedicated worker thread.
- Deterministic command routing from `commands.toml` with URL encoding and an
  explicit confirmation gate.
- Maths engine: scalars, vectors, `softmax`, `sigmoid`, `relu`, `exp` and
  symbolic differentiation, all with deterministic fallbacks.
- Optional on-demand `llama.cpp` maths/calculus translator with GBNF-constrained
  JSON and idle unload.
- Material Design 3 layer-shell overlay with a live audio-level pulse.
- Nine QtTest suites, GitHub Actions CI on every push and pull request, and
  automated releases on `v*` tags.

### Notes

- The deterministic pipeline works without `llama.cpp` and without the GGUF.
- Stub mode (`--stub`) needs no microphone or model bundle.
