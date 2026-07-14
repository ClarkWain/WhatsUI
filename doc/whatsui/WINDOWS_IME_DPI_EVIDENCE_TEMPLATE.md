# Windows IME and DPI evidence template

Copy this file into the release-candidate evidence directory. Fill every row
from an interactive Windows session; do not treat an automated test pass as
native candidate-window evidence.

## Environment

| Field | Recorded value |
| --- | --- |
| Candidate tag / commit | |
| Tester and date | |
| Windows edition and build | |
| GPU and driver version | |
| WhatsUI build command / configuration | |
| Installed IME and version (for example Microsoft Pinyin) | |
| Monitor model, native resolution, and arrangement | |

## Per-DPI matrix

For each DPI value, place the caret at the start, middle, and end of a longer
line. Capture at least one screenshot or short recording while the candidate
window is visible, then record its repository-relative evidence path below.

| Windows scale | Monitor / app window position | Start / middle / end candidate placement | Commit exactly once | Cancel / focus-loss clears pre-edit | Modal / Alt+Tab recovery | Evidence path | Pass / fail / blocked | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 100% | | | | | | | | |
| 150% | | | | | | | | |
| 200% | | | | | | | | |

## Cross-DPI movement and text editing

| Check | Actual result | Evidence path / notes |
| --- | --- | --- |
| Move the focused window between monitors with different scale values, then refocus and begin a new pre-edit phrase. | | |
| Shift+Arrow and mouse selection retain logical text order after an IME commit. | | |
| Ctrl+C, Ctrl+X, Ctrl+V, Ctrl+Z, and Ctrl+Y operate on committed text after a commit and after a cancelled pre-edit. | | |
| Todo composer and Settings reference input were each checked. | | |

## Sign-off

| Role | Name | Date | Decision / unresolved issue |
| --- | --- | --- | --- |
| Tester | | | |
| Windows release owner | | | |
