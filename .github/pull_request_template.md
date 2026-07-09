## Summary

-

## Validation

- [ ] `cmake --build build --config Debug`
- [ ] `ctest --test-dir build -C Debug --output-on-failure`
- [ ] If touched WhatsCanvas integration: `cmake -S . -B build-wsc -DWHATSUI_WITH_WHATSCANVAS=ON -DWHATSUI_BUILD_TESTS=OFF`

## Notes

-