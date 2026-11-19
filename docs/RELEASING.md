# Releasing Dex

Dex uses [Semantic Versioning](https://semver.org/spec/v2.0.0.html): `MAJOR.MINOR.PATCH`.

- **MAJOR** — incompatible change to config, IPC commands or command-rule syntax.
- **MINOR** — new provider, engine capability or UI state, backwards compatible.
- **PATCH** — bug fix, documentation or dependency bump with no behaviour change.

## Single source of truth

`VERSION` holds the canonical version. `CMakeLists.txt` reads it for both the
CMake project version and the `DEX_VERSION` compile definition used by
`dex --version`. `CHANGELOG.md` must contain a matching `## [X.Y.Z]` section;
CI verifies this with `scripts/version.sh`.

The QML module version (`qt_add_qml_module(... VERSION 1.0)`) is an API version
for the `Dex` URI and changes only when the QML module's public surface changes.
It is intentionally independent of the application release version.

## Cutting a release

```sh
bash scripts/bump-version.sh 1.2.3       # updates VERSION + CHANGELOG
# edit the new CHANGELOG section: replace the placeholder with real notes
git add VERSION CHANGELOG.md
git commit -m "chore(release): bump version to 1.2.3"
git tag -a v1.2.3 -m "Dex 1.2.3"
git push origin main
git push origin v1.2.3
```

Pushing the `v*` tag starts the `release` workflow, which runs the full test
suite and then publishes a GitHub release with generated notes. Do not create
the release by hand; the workflow is the only publisher.

## Branch policy

`main` and `showcase` are protected: changes arrive through pull requests and
the `build-and-test` check must pass. `showcase` is the public, project-only
snapshot — agent/AI tooling (`AGENTS.md`, `opencode.json`, `.opencode/`,
`.agent/`) is excluded by CI policy on that branch. Keep `docs/` there: those
files describe the project itself.

To promote a release to `showcase`, open a PR from `main` into `showcase` and
remove the tooling paths in that PR.
