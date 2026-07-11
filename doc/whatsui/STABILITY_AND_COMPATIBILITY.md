# Source stability and compatibility

WhatsUI is currently a Windows-first developer preview. Its public headers are
designed to be reviewed and exercised by external applications, but no binary
ABI compatibility is promised before version 1.0.

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
matrix have passed their release checks. At that point the release notes will
state an explicit source and ABI policy, supported compiler/CRT combinations,
and deprecation process.

## Consumer guidance

Use CMake targets rather than copying include paths or linking raw archives.
Pin an exact release tag for production use, keep WhatsCanvas/GLFW integration
in the same source build until the packaged integration is published, and run
the external-consumer smoke test during upgrades.
