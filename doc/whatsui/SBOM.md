# Preliminary software bill of materials

Status: inventory for the MIT-licensed WhatsUI source tree, not legal advice
and not a substitute for artifact-specific attribution review.
It identifies source-tree dependencies that a release owner must verify at the
exact tagged revisions. It intentionally does **not** assert license terms or
select a project license. See [Release checklist](RELEASE_CHECKLIST.md).

## First-party inventory

| Component | Location | Distribution status | License/notice decision |
| --- | --- | --- | --- |
| WhatsUI runtime, public headers, examples, tests, documentation | Repository root excluding `third_party/` | Core and Windows renderer/GLFW CMake exports are validated. | MIT; see root `LICENSE`. |
| Todo local-store format | `examples/todo_app/` | Example/application code; not a separate package. | MIT as first-party WhatsUI source; see root `LICENSE`. |

## Known third-party source dependencies

| Component | Relationship / activation | Source location or upstream pointer | License evidence to review | Release note |
| --- | --- | --- | --- | --- |
| WhatsCanvas | Git submodule; required by `WHATSUI_WITH_WHATSCANVAS=ON` | `third_party/WhatsCanvas`, declared in root `.gitmodules` | `third_party/WhatsCanvas/LICENSE` | Windows package export is validated; record the submodule commit in the release manifest. |
| FreeType | WhatsCanvas vendored/submodule dependency; used by advanced text when configured | `third_party/WhatsCanvas/third_party/freetype` | `LICENSE.TXT` in that directory | Inclusion depends on the WhatsCanvas build configuration. |
| HarfBuzz | WhatsCanvas vendored/submodule dependency; requested by advanced text by default | `third_party/WhatsCanvas/third_party/harfbuzz` | `COPYING` in that directory | Inclusion depends on the WhatsCanvas build configuration. |
| GLFW | WhatsCanvas submodule dependency; used for the interactive host | `third_party/WhatsCanvas/third_party/glfw` | `LICENSE.md` in that directory | Not part of the headless core export. |
| GLAD | WhatsCanvas bundled OpenGL loader source | `third_party/WhatsCanvas/third_party/glad` | Verify upstream metadata at the pinned source revision. | Used by the OpenGL integration; attribution still needs release review. |
| GLM | WhatsCanvas submodule dependency | `third_party/WhatsCanvas/third_party/glm` | `copying.txt` in that directory | Used by WhatsCanvas build paths; verify exact inclusion. |
| stb | WhatsCanvas submodule dependency | `third_party/WhatsCanvas/third_party/stb` | `LICENSE` in that directory | Verify which headers/assets are compiled into a chosen artifact. |
| polyline2d | WhatsCanvas bundled dependency | `third_party/WhatsCanvas/third_party/polyline2d` | `LICENSE.md` in that directory | Verify whether the selected backend links it. |

The WhatsCanvas repository itself warns that its third-party directory may
contain additional test data or nested dependencies. The table above is not
exhaustive for every transitive build tool, system SDK, font, or asset. Before
release, run the selected build configuration from a fresh checkout and record
the exact Git/submodule revisions and generated dependency graph.

## Build and environment dependencies

| Dependency | Purpose | Shipped in WhatsUI archive? | Follow-up |
| --- | --- | --- | --- |
| CMake 3.20+ | Configure/build/package generation | No | Record release-builder version. |
| C++17 compiler and Windows SDK/CRT | Build Windows binaries | No | State supported compiler/CRT combinations in final release notes. |
| Windows IMM32 | Windows GLFW IME bridge | System component | Review platform redistribution requirements separately; this SBOM makes no assertion. |
| Windows fonts and installed IME | Text fallback and manual IME validation | System/user environment | Not bundled; document test environment rather than treating a local font as an artifact dependency. |

## Required release completion

Before publishing, the release owner must replace this draft or accompany it
with a generated, version-pinned SBOM that names each shipped artifact,
component version/revision, source URL, checksum, license/notice disposition,
and build inclusion. The final document must be reviewed with the release
archive; this draft alone is not proof of license compliance.
