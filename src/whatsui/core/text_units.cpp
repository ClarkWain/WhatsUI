#include "wui/text_units.h"

#include <array>
#include <utility>

namespace wui {
namespace {

// Nonspacing / spacing / enclosing combining marks the stage 1 walker
// recognises. Ranges cover the practically-used subset of Unicode 15
// General_Category = M* plus the joiner code points and variation
// selectors that Narrator treats as extending the previous grapheme.
// Complete Unicode 15 grapheme-cluster segmentation is deferred; see
// doc/whatsui/UIA_TEXT_PATTERN_DESIGN.md for the rollout plan.
constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 26>
    kExtendingRanges{{
        {0x0300u, 0x036Fu}, // Combining Diacritical Marks
        {0x0483u, 0x0489u}, // Cyrillic
        {0x0591u, 0x05BDu}, // Hebrew
        {0x05BFu, 0x05BFu},
        {0x05C1u, 0x05C2u},
        {0x05C4u, 0x05C5u},
        {0x05C7u, 0x05C7u},
        {0x0610u, 0x061Au}, // Arabic
        {0x064Bu, 0x065Fu},
        {0x0670u, 0x0670u},
        {0x06D6u, 0x06DCu},
        {0x06DFu, 0x06E4u},
        {0x06E7u, 0x06E8u},
        {0x06EAu, 0x06EDu},
        {0x0711u, 0x0711u},
        {0x0730u, 0x074Au},
        {0x0E31u, 0x0E31u}, // Thai
        {0x0E34u, 0x0E3Au},
        {0x0E47u, 0x0E4Eu},
        {0x1AB0u, 0x1AFFu}, // Combining Diacritical Marks Extended
        {0x1DC0u, 0x1DFFu}, // Combining Diacritical Marks Supplement
        {0x200Du, 0x200Du}, // Zero Width Joiner
        {0x20D0u, 0x20FFu}, // Combining Diacritical Marks for Symbols
        {0xFE00u, 0xFE0Fu}, // Variation Selectors
        {0xFE20u, 0xFE2Fu}, // Combining Half Marks
        {0x1F3FBu, 0x1F3FFu}, // Emoji modifier fitzpatrick
    }};

// Regional Indicator characters combine in pairs to form flag emoji. The
// stage 1 walker handles pairs greedily: two adjacent RI code points are
// one grapheme, but a third RI starts a new one.
constexpr bool isRegionalIndicator(std::uint32_t codePoint) noexcept
{
    return codePoint >= 0x1F1E6u && codePoint <= 0x1F1FFu;
}

// Decode without advancing; used for the previous-boundary walkers so
// grapheme-extending checks can look at the current code point without
// mutating a cursor.
std::uint32_t decodeAt(std::string_view text, std::size_t offset,
                       std::size_t& bytesConsumed) noexcept
{
    return decodeUtf8CodePoint(text, offset, bytesConsumed);
}

} // namespace

std::uint32_t decodeUtf8CodePoint(std::string_view text, std::size_t offset,
                                  std::size_t& bytesConsumed) noexcept
{
    bytesConsumed = 0;
    if (offset >= text.size()) return 0xFFFDu;
    const auto* data = reinterpret_cast<const unsigned char*>(text.data()) + offset;
    const std::size_t remaining = text.size() - offset;
    const unsigned char lead = data[0];
    if (lead < 0x80u) {
        bytesConsumed = 1;
        return lead;
    }
    std::size_t sequenceLength = 0;
    std::uint32_t codePoint = 0;
    if ((lead & 0xE0u) == 0xC0u) {
        sequenceLength = 2;
        codePoint = lead & 0x1Fu;
    } else if ((lead & 0xF0u) == 0xE0u) {
        sequenceLength = 3;
        codePoint = lead & 0x0Fu;
    } else if ((lead & 0xF8u) == 0xF0u) {
        sequenceLength = 4;
        codePoint = lead & 0x07u;
    } else {
        // Invalid lead byte; treat as a single-byte U+FFFD so the walker
        // still advances by one and the caret never gets stuck.
        bytesConsumed = 1;
        return 0xFFFDu;
    }
    if (remaining < sequenceLength) {
        bytesConsumed = 1;
        return 0xFFFDu;
    }
    for (std::size_t i = 1; i < sequenceLength; ++i) {
        const unsigned char cont = data[i];
        if ((cont & 0xC0u) != 0x80u) {
            bytesConsumed = 1;
            return 0xFFFDu;
        }
        codePoint = (codePoint << 6) | (cont & 0x3Fu);
    }
    bytesConsumed = sequenceLength;
    return codePoint;
}

std::size_t nextCodePointBoundary(std::string_view text, std::size_t offset) noexcept
{
    if (offset >= text.size()) return text.size();
    std::size_t consumed = 0;
    (void)decodeUtf8CodePoint(text, offset, consumed);
    if (consumed == 0) return text.size();
    const std::size_t next = offset + consumed;
    return next > text.size() ? text.size() : next;
}

std::size_t previousCodePointBoundary(std::string_view text, std::size_t offset) noexcept
{
    if (offset == 0 || text.empty()) return 0;
    if (offset > text.size()) offset = text.size();
    std::size_t cursor = offset - 1;
    // Walk back over UTF-8 continuation bytes (10xxxxxx) up to 3 bytes.
    while (cursor > 0) {
        const unsigned char byte = static_cast<unsigned char>(text[cursor]);
        if ((byte & 0xC0u) != 0x80u) break;
        if (offset - cursor >= 4) break; // capped by max UTF-8 length
        --cursor;
    }
    return cursor;
}

bool isGraphemeExtendingCodePoint(std::uint32_t codePoint) noexcept
{
    for (const auto& range : kExtendingRanges) {
        if (codePoint < range.first) return false;
        if (codePoint <= range.second) return true;
    }
    return false;
}

std::size_t nextGraphemeBoundary(std::string_view text, std::size_t offset) noexcept
{
    if (offset >= text.size()) return text.size();
    std::size_t cursor = nextCodePointBoundary(text, offset);
    if (cursor >= text.size()) return text.size();
    // Decode the base code point to decide whether we are opening a
    // Regional Indicator pair; consecutive extending marks and a single
    // trailing RI are then folded into the current grapheme.
    std::size_t baseConsumed = 0;
    const std::uint32_t base = decodeAt(text, offset, baseConsumed);
    (void)baseConsumed;
    const bool baseIsRi = isRegionalIndicator(base);
    bool absorbedTrailingRi = false;
    while (cursor < text.size()) {
        std::size_t consumed = 0;
        const std::uint32_t next = decodeAt(text, cursor, consumed);
        if (consumed == 0) break;
        if (isGraphemeExtendingCodePoint(next)) {
            cursor += consumed;
            continue;
        }
        if (baseIsRi && !absorbedTrailingRi && isRegionalIndicator(next)) {
            cursor += consumed;
            absorbedTrailingRi = true;
            continue;
        }
        break;
    }
    return cursor;
}

std::size_t previousGraphemeBoundary(std::string_view text, std::size_t offset) noexcept
{
    if (offset == 0 || text.empty()) return 0;
    if (offset > text.size()) offset = text.size();
    // Walk back past extending marks first. When we consume a base code
    // point that is itself a Regional Indicator, use UAX #29 GB12/GB13:
    // count how many consecutive RIs precede this position (including this
    // one) and merge with the previous RI when the count is even, so pairs
    // of RIs form one flag grapheme but an odd trailing RI stands alone.
    std::size_t cursor = offset;
    while (cursor > 0) {
        const std::size_t previous = previousCodePointBoundary(text, cursor);
        std::size_t consumed = 0;
        const std::uint32_t codePoint = decodeAt(text, previous, consumed);
        (void)consumed;
        cursor = previous;
        if (isGraphemeExtendingCodePoint(codePoint)) continue;
        if (isRegionalIndicator(codePoint)) {
            std::size_t riCount = 1;
            std::size_t scan = cursor;
            while (scan > 0) {
                const std::size_t p = previousCodePointBoundary(text, scan);
                std::size_t c = 0;
                const std::uint32_t cp = decodeAt(text, p, c);
                (void)c;
                if (!isRegionalIndicator(cp)) break;
                ++riCount;
                scan = p;
            }
            if (riCount % 2 == 0) {
                cursor = previousCodePointBoundary(text, cursor);
            }
        }
        break;
    }
    return cursor;
}

} // namespace wui
