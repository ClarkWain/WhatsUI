# Changelog

All notable changes are recorded here. Versions before a 1.0 release remain
source-compatible only within their documented preview contract.

## Unreleased

- Changed preview class layouts and virtual interfaces for accessibility,
  platform titles, text metrics, styled text, and input routing. Pre-1.0 ABI
  compatibility is not provided; all WhatsUI/WhatsCanvas consumers must fully
  rebuild rather than reuse previously compiled objects or binaries.
- Drafted the post-1.0 source, ABI, experimental-API, and deprecation policy.
  It is explicitly pending release-owner approval and candidate-specific
  compiler/CRT validation; the preview ABI contract is unchanged.
- Windows Fluent Todo reference: local recoverable storage, validation,
  controller/undo interaction foundation, deterministic visual coverage.
- Fluent component set, dark/local themes, accessibility semantics, command
  overlays, virtual list, and UI Inspector reference application.
- Windows external CMake package validation for core and GLFW targets with
  WhatsCanvas Software/OpenGL and advanced text dependencies.
- Runtime lifecycle, IME, text shaping, diagnostics, sanitizer, benchmark,
  and visual-regression improvements.
