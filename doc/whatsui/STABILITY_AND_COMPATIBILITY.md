# Source stability and compatibility

WhatsUI is currently a Windows-first developer preview. Its public headers are
designed to be reviewed and exercised by external applications, but no binary
ABI compatibility is promised before version 1.0.

The proposed post-1.0 policy is documented separately in
[Proposed 1.0 source, binary, and deprecation policy](COMPATIBILITY_POLICY_1_0_DRAFT.md).
It deliberately remains pending release-owner approval and does not change the
preview contract in this document.

## Before 1.0

- Public source APIs follow semantic versioning intent: incompatible source
  changes are recorded in the changelog and release notes.
- Consumers must rebuild against the exact WhatsUI release they use. Do not
  mix object files or prebuilt libraries from different preview releases.
- Experimental facilities are marked in their header comments or live under a
  dedicated extension header. They may change or be removed between previews.
- The Software backend and deterministic test captures are the reference
  contract for layout and paint behavior. GPU/platform integration may have
  documented limitations until it has an equivalent validation matrix.

## 1.0 release gate

The project will only declare 1.0 after Windows package consumers, the Todo,
Settings, and Inspector reference applications, and the published support
matrix have passed their release checks. The release owner must approve the
proposed 1.0 policy, record the exact supported compiler/CRT combinations in
the release notes, and complete the deprecation and ABI evidence required by
the release checklist.

## Consumer guidance

Use CMake targets rather than copying include paths or linking raw archives.
Pin an exact release tag for production use, keep WhatsCanvas/GLFW integration
in the same source build until the packaged integration is published, and run
the external-consumer smoke test during upgrades.
