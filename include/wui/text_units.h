#pragma once

// Grapheme-aware walker for UTF-8 text, used by the accessibility text
// model to advance by TextUnit::Character in a way that Narrator and other
// screen readers can announce as one perceived character.
//
// The walker is intentionally simple for stage 1 of the UIA TextRange
// rollout (see doc/whatsui/UIA_TEXT_PATTERN_DESIGN.md): it advances by
// code point and merges the common combining-mark, Regional Indicator,
// emoji modifier, and ZWJ sequences. Full Unicode 15 grapheme-cluster
// segmentation is deferred; the walker's contract is that a Narrator
// user never stalls on a boundary and the visible caret always lands on
// a code-point boundary.
//
// All positions are UTF-8 byte offsets into the input string_view. A
// position past the last byte is always a valid boundary.

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace wui {

// Decode the code point starting at `offset` (must be a byte-boundary).
// Returns U+FFFD when the sequence is truncated or malformed. On success
// (or replacement) `bytesConsumed` reports how many bytes were consumed.
[[nodiscard]] std::uint32_t decodeUtf8CodePoint(std::string_view text, std::size_t offset,
                                                std::size_t& bytesConsumed) noexcept;

// Byte offset of the next code-point boundary strictly greater than
// `offset`. Returns text.size() when there is no next boundary or the
// walker cannot advance (invalid input caps the caret at end-of-text).
[[nodiscard]] std::size_t nextCodePointBoundary(std::string_view text,
                                                std::size_t offset) noexcept;

// Byte offset of the previous code-point boundary strictly less than
// `offset`. Returns 0 when there is no previous boundary.
[[nodiscard]] std::size_t previousCodePointBoundary(std::string_view text,
                                                    std::size_t offset) noexcept;

// Byte offset of the next grapheme-cluster boundary strictly greater than
// `offset`. Common combining marks (Mn class subset), Regional Indicator
// pairs, emoji-modifier sequences, and ZWJ joiners advance as one grapheme
// with the preceding base code point. Returns text.size() at end-of-text.
[[nodiscard]] std::size_t nextGraphemeBoundary(std::string_view text,
                                               std::size_t offset) noexcept;

// Byte offset of the previous grapheme-cluster boundary strictly less than
// `offset`. Returns 0 when there is no previous boundary.
[[nodiscard]] std::size_t previousGraphemeBoundary(std::string_view text,
                                                   std::size_t offset) noexcept;

// True if `codePoint` is part of the combining set the stage 1 walker
// treats as continuing the preceding base grapheme (nonspacing marks,
// ZWJ, Regional Indicator, emoji modifiers, variation selectors).
[[nodiscard]] bool isGraphemeExtendingCodePoint(std::uint32_t codePoint) noexcept;

} // namespace wui
