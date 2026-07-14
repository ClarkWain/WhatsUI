# Proposed 1.0 source, binary, and deprecation policy

## Status and approval boundary

This is the proposed policy for the first Windows 1.0 release. It is a
release-candidate input, not a claim that the current developer preview has a
stable ABI. It becomes binding only when all of the following are recorded for
the tagged release:

- the release owner approves this document without outstanding compatibility
  exceptions;
- the release notes name the supported Windows, architecture, MSVC toolset,
  Windows SDK, and CRT tuple used to build the artifacts; and
- the release checklist's source/archive, engineering, accessibility, and
  legal/SBOM gates are approved.

Until then, the preview rules in
[Source stability and compatibility](STABILITY_AND_COMPATIBILITY.md) apply.
No checkbox in the release roadmap may be inferred from publishing this draft.

## Scope and versioning

Once approved, this policy applies to the stable-API manifest published with
the release, the documented CMake targets and package profiles listed below,
and the package configuration they install. A header being copied from
`include/wui` is not by itself a stability promise: the 1.0 candidate must
publish the manifest and identify every experimental extension header before
approval. Examples, tests, internal `src` headers, generated files, and
third-party APIs are not part of the WhatsUI public contract.

WhatsUI uses semantic-versioning intent after 1.0:

- A **patch** release must preserve supported source and binary compatibility.
  A correction to documented erroneous behavior is allowed, but must be called
  out in release notes when consumers could observe it.
- A **minor** release may add APIs and behavior without requiring a source or
  binary change from a conforming consumer. It may deprecate a public API.
- A **major** release may remove deprecated APIs or make another incompatible
  source, behavior, package, or ABI change. Every such change requires an
  upgrade note and migration path when practical.

Source compatibility means that a consumer using only supported public APIs
continues to compile after a clean CMake reconfigure in a supported build tuple.
It does not promise unchanged pixels for documented visual bug fixes, preserve
undefined behavior, or make experimental APIs stable.

## Windows package profiles and binary compatibility boundary

The proposed 1.0 package contract is intentionally limited to **prebuilt
static Release archives** plus the equivalent source build. Shared DLLs are
not a 1.0 package profile: the current build does not define an explicit
Windows export/import surface, so no DLL ABI promise may be inferred from
`BUILD_SHARED_LIBS`. Debug, sanitizer, `/MT`, and custom feature builds are
source-build configurations, not binary-compatible distributions.

Every approved binary artifact must name exactly one of these profiles in its
file name, package metadata, and release evidence:

| Profile | Required CMake values and exported targets | Boundary |
| --- | --- | --- |
| `core-static` | `BUILD_SHARED_LIBS=OFF`, `WHATSUI_WITH_WHATSCANVAS=OFF`; exports `WhatsUI::WhatsUI`. | No renderer, text adapter, or `WhatsUI::Glfw` target is present. |
| `glfw-static` | `BUILD_SHARED_LIBS=OFF`, `WHATSUI_WITH_WHATSCANVAS=ON`, `WHATSUI_ENABLE_ADVANCED_TEXT=ON`; exports the core target, WhatsCanvas dependencies, and `WhatsUI::Glfw`. | WhatsCanvas, GLFW, FreeType, HarfBuzz, and the selected renderer/text feature set are one inseparable package tuple. |

An artifact using another option combination (including advanced text disabled)
may be built from source, but is outside the prebuilt 1.0 ABI promise until a
later release adds a named profile and its evidence. `WhatsUI::Glfw` is thus an
optional profile target, not a guarantee that every `find_package(WhatsUI)`
installation provides it.

Within an approved profile, the following is the proposed supported binary
boundary:

| Dimension | Proposed 1.0 boundary | Not covered unless a later release says so |
| --- | --- | --- |
| Architecture | Windows x64 only. | x86, ARM64, ARM64EC, and cross-architecture linking. |
| Compiler | Visual Studio 2022 generator, x64 architecture, Microsoft C++ toolset v143 in C++17 mode. The exact `cl.exe` version, CMake generator, `-A` architecture, Windows SDK, and toolset are recorded in release notes. | Clang-cl, MinGW, older MSVC toolsets, an unrecorded toolset update, and toolset mixing. |
| CRT | Release-only dynamic MSVC CRT: `CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL` (`/MD`). Consumers and WhatsUI use the same Release configuration and CRT mode. | `/MDd`, `/MT`, `/MTd`, Debug/Release mixing, `/clr`, or independently replaced CRT/STL binaries. |
| Standard library/runtime options | `/EHsc`, `/GR`, and `_ITERATOR_DEBUG_LEVEL=0`; the candidate records its effective compile flags. | A different iterator-debug level, disabled exceptions/RTTI, or another ABI-changing compiler switch. |
| Dependencies | The exact WhatsCanvas, GLFW, FreeType, HarfBuzz, and other package revisions exported with the WhatsUI package. | Linking a WhatsUI binary against separately built or substituted dependency binaries. |

The public C++ API exposes standard-library and ownership types. Therefore this
is a deliberately narrow binary boundary: a consumer must not assume ABI
compatibility merely because another Windows compiler can link the library.
Do not mix object files, libraries, or exported CMake packages from different
WhatsUI release tags or profiles. Use the installed CMake targets so their
declared transitive dependencies stay coherent.

Before the owner approves a binary package, the candidate must pass a clean
external-consumer build and run for every profile it intends to list. The
candidate evidence must record the CMake generator, MSVC toolset and `cl.exe`
version, Windows SDK, architecture, configuration,
`CMAKE_MSVC_RUNTIME_LIBRARY`, effective exception/RTTI/iterator settings, all
profile options, and dependency revisions. A package change that broadens or
narrows this table is a release-note and compatibility review event.

## Deprecation and removal

For supported public APIs, a deprecation must:

1. be annotated in the public header with `[[deprecated("replacement")]]` when
   practical, and be documented in the release notes and upgrade guide;
2. name a supported replacement or explain why no replacement exists;
3. remain available for at least one subsequent minor release **and** six
   months from the first release carrying the deprecation, whichever is
   longer; and
4. be removed only in the first major release that begins after both retention
   conditions are met, with a migration note.

The release owner may approve a shorter emergency path only for a security,
data-loss, memory-safety, or severe correctness defect. The release notes must
identify the exception, affected versions, and required consumer action. An
undocumented rename, removal, or incompatible semantic change is not a valid
deprecation process.

## Experimental APIs

An API is experimental only when the stable-API manifest excludes its header or
symbol and its public header comment and documentation explicitly say
`Experimental`, or it is placed in an explicitly documented experimental
extension surface. Experimental APIs are excluded from the
source/ABI/deprecation guarantees above and may change or disappear in a minor
release. They still require a changelog entry when changed materially, must not
silently replace a stable API, and must not be used as the sole path for a
claimed 1.0 Windows capability.

Adding an API to the stable surface requires its header, ownership/lifetime
contract, thread and error behavior, supported build boundary, tests, and
release-note classification to be reviewed. A stable API must not expose an
unversioned third-party type unless that third-party ABI is part of the same
documented package boundary.

## Release-owner record (intentionally unapproved)

| Required record | Value |
| --- | --- |
| Release tag/version | _Pending_ |
| Release owner name | _Pending_ |
| Owner approval date and evidence link | _Pending_ |
| Approved source/ABI exceptions | _None recorded_ |
| Approved package profiles and compiler/CRT tuples | _Pending candidate validation_ |
| Stable-API manifest and experimental-header inventory | _Pending candidate validation_ |
| Legal/SBOM approver and evidence link | _Pending_ |

Do not replace these placeholders with an automated test result. Approval is a
human release-governance decision and remains a separate M5 gate.
