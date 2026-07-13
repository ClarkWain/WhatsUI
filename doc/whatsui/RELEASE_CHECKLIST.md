# Release checklist

This is a release gate, not a statement that the current developer preview is
ready to ship. A checked box requires recorded evidence from the release
candidate revision.

## Release identity and source

- [ ] Tag/version is approved and matches the intended release notes.
- [ ] `git status --porcelain` is empty before the source archive is created.
- [ ] The source archive was produced from the tagged commit, and its SHA-256
  plus Git/submodule revisions are stored with the release artifacts.
- [ ] The archive contains the source needed by the supported distribution, or
  its manifest clearly identifies every required submodule revision.
- [ ] A clean network-only clone can initialize every pinned submodule revision
  from its configured remote. Local object caches do not count as evidence.
- [ ] Release owner has reviewed generated artifact names, sizes, hashes, and
  retention location.

## Legal and supply-chain gate

- [x] Project license selected: MIT; see root `LICENSE`.
- [ ] A release owner has reviewed all third-party license/notice obligations
  for the exact dependency revisions in the artifact.
- [ ] `LICENSE`, `NOTICE`, and the release-specific SBOM/attribution material
  have been approved by the appropriate project/legal owner.

The MIT decision covers WhatsUI first-party source. Third-party attribution and
artifact-specific notice review remain release-owner gates.

## Current reproducibility status

The current parent revision (`712dc2a`) and WhatsCanvas submodule pin
(`1434f1a…`) are reachable from their configured remotes. On 2026-07-13, a
fresh `--no-local` clone with global configuration disabled completed recursive
network-only initialization for WhatsCanvas, FreeType, dlg, GLFW, GLM,
HarfBuzz, and stb. The headless archive and GLFW package consumers also passed
from fresh output directories.

This establishes source retrievability for the recorded revision. It does not
approve a release: an approved version tag, candidate-specific artifacts, and
the remaining manual/legal gates above are still required.

## Engineering gates

- [ ] Clean headless configure/build/test succeeds on Windows/MSVC.
- [ ] MSVC AddressSanitizer headless gate succeeds.
- [ ] Headless install/export plus an external `find_package` consumer smoke
  test succeeds.
- [ ] WhatsCanvas Software build and deterministic visual regression succeeds.
- [ ] Todo responsive captures are reviewed by a human at narrow, regular, and
  wide sizes; hashes are updated only for an intentional visual decision.
- [ ] GLFW Todo, Settings, and Command Palette reference applications launch on
  the release Windows version.
- [ ] Text input/IME manual matrix has evidence at 100%, 150%, and 200% DPI.
- [ ] Open bugs, unsupported APIs, and release caveats are included in release
  notes.

## Recorded automated evidence (not release approval)

The records below are reproducible engineering evidence from this workspace.
They do **not** check a release gate: this checkout was not a tagged, clean
release candidate and none of the manual/legal rows above may be inferred from
an automated pass.

| Date / environment | Command or scenario | Observed result | What it does not prove |
| --- | --- | --- | --- |
| 2026-07-12, Windows/MSVC Debug | `cmake --build build --config Debug` then `ctest --test-dir build -C Debug --output-on-failure` | **18/18** CTest tests passed (including runtime, controls, storage, Todo interaction, virtual-list, inspector, and benchmark targets). | ASan, renderer/GLFW package, native window launch, IME/DPI, release archive, or legal review. |
| 2026-07-12, Windows/MSVC Debug with AddressSanitizer | `cmake --build build-asan --config Debug` then `ctest --test-dir build-asan -C Debug --output-on-failure` | **18/18** CTest tests passed after a high-churn virtual-list recycle regression and polymorphic `Node` ownership regression were added. | A tagged candidate, renderer/GLFW package behavior, native window launch, IME/DPI, or release/legal approval. |
| 2026-07-12, Windows/MSVC Debug with WhatsCanvas Software | Built `WhatsUITodoApp` and `WhatsUITodoGlfw`, then ran `whatsui_todo_visual_regression` and `whatsui_todo_visual_review`. | **2/2** passed; deterministic pixels and three 2x responsive Todo captures (360×720, 640×720, 1180×760) were accepted after visual review. | Native GLFW interaction, IME/DPI, accessibility bridge, a tagged artifact, or release approval. |
| 2026-07-12, Windows/MSVC Release | Fresh `WHATSUI_WITH_WHATSCANVAS=ON` package configure/build/install with default advanced text, then fresh `tests/package_consumer` configured with `-DWHATSUI_PACKAGE_TARGET=Glfw`. | External consumer configured through `find_package(WhatsUI)`, linked `WhatsUI::Glfw`, and its smoke executable exited **0**. The installed prefix included WhatsUI, WhatsCanvas Software/OpenGL, GLFW, FreeType, and HarfBuzz exports. | Opening a GLFW window, GPU/driver behavior, a redistributable/signed release artifact, or third-party notice approval. |
| 2026-07-12, Windows/MSVC Release | Fresh Software-only WhatsCanvas package, then its external `WhatsCanvas::Software` consumer smoke. | Install, `find_package(WhatsCanvas)`, link, and consumer executable all succeeded. | WhatsUI package behavior, GLFW/OpenGL, or platform UI behavior. |

Retain the command logs with the candidate tag before changing any engineering
row above to checked. Re-run the matrix from a fresh output directory whenever
the package/export graph, compiler toolset, or submodule revision changes.

## Explicitly blocked M5 items

The following are not complete merely because a headless archive or demo was
created:

- Windows UI Automation or equivalent native accessibility bridge and
  screen-reader sign-off.
- Final ABI/source compatibility and deprecation policy.
- Release archive, NOTICE/SBOM/legal review, changelog, and hashes approved by
  the release owner.

## Reproducible release-groundwork command

Run this only from a clean, committed checkout. It creates an archive/hash and
validates the headless exported package plus its external consumer. By default
it fails while required release metadata such as `NOTICE` is absent; use the
override only for a rehearsal and never as release approval.

```powershell
cmake `
  -DWHATSUI_SOURCE_DIR=$PWD `
  -DWHATSUI_RELEASE_OUTPUT_DIR=$PWD\release-check `
  -DWHATSUI_RELEASE_ALLOW_MISSING_METADATA=ON `
  -P cmake\WhatsUIReleaseCheck.cmake
```

The output `SOURCE_MANIFEST.txt` records the Git revision, submodule status,
archive location, and SHA-256. The script validates the headless package. The
separate renderer/GLFW external-consumer command in the recorded-evidence table
must be re-run from the tagged candidate; neither command replaces manual
launch, IME/DPI, legal, or artifact-review gates.
