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
archive location, and SHA-256. The script intentionally validates only the
headless package; it must not be cited as evidence that the renderer/GLFW
package export exists.
