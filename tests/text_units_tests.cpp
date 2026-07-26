// Unit tests for the UTF-8 grapheme walker used by the accessibility text
// model. See doc/whatsui/UIA_TEXT_PATTERN_DESIGN.md for the pattern
// contract this walker underpins.

#include "wui/text_units.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

namespace {

int g_failures = 0;

void reportFailure(const char* file, int line, const char* expression, const std::string& detail)
{
    ++g_failures;
    std::fprintf(stderr, "%s(%d): FAIL: %s [%s]\n", file, line, expression, detail.c_str());
}

#define EXPECT_EQ(actual, expected)                                                            \
    do {                                                                                       \
        const auto _actual = (actual);                                                         \
        const auto _expected = (expected);                                                     \
        if (!(_actual == _expected)) {                                                         \
            reportFailure(__FILE__, __LINE__, #actual " == " #expected,                        \
                          std::string("actual=") + std::to_string(_actual)                     \
                              + ", expected=" + std::to_string(_expected));                    \
        }                                                                                      \
    } while (false)

void testAsciiCodePointAndGrapheme()
{
    const std::string text = "abc";
    EXPECT_EQ(wui::nextCodePointBoundary(text, 0), 1u);
    EXPECT_EQ(wui::nextCodePointBoundary(text, 1), 2u);
    EXPECT_EQ(wui::nextCodePointBoundary(text, 2), 3u);
    EXPECT_EQ(wui::nextCodePointBoundary(text, 3), 3u);
    EXPECT_EQ(wui::previousCodePointBoundary(text, 3), 2u);
    EXPECT_EQ(wui::previousCodePointBoundary(text, 2), 1u);
    EXPECT_EQ(wui::previousCodePointBoundary(text, 1), 0u);
    EXPECT_EQ(wui::previousCodePointBoundary(text, 0), 0u);
    EXPECT_EQ(wui::nextGraphemeBoundary(text, 0), 1u);
    EXPECT_EQ(wui::previousGraphemeBoundary(text, 3), 2u);
}

void testCjkAdvancesOneCodePointPerGrapheme()
{
    // "汉字" — each character is a 3-byte UTF-8 sequence.
    const std::string text = "\xE6\xB1\x89\xE5\xAD\x97";
    EXPECT_EQ(text.size(), 6u);
    EXPECT_EQ(wui::nextCodePointBoundary(text, 0), 3u);
    EXPECT_EQ(wui::nextCodePointBoundary(text, 3), 6u);
    EXPECT_EQ(wui::previousCodePointBoundary(text, 6), 3u);
    EXPECT_EQ(wui::previousCodePointBoundary(text, 3), 0u);
    EXPECT_EQ(wui::nextGraphemeBoundary(text, 0), 3u);
    EXPECT_EQ(wui::nextGraphemeBoundary(text, 3), 6u);
    EXPECT_EQ(wui::previousGraphemeBoundary(text, 6), 3u);
}

void testCombiningMarkAttachesToBase()
{
    // "e" (U+0065) + COMBINING ACUTE ACCENT (U+0301) + "s"
    const std::string text = "e\xCC\x81s";
    EXPECT_EQ(text.size(), 4u);
    // Code-point walker treats them as two separate steps.
    EXPECT_EQ(wui::nextCodePointBoundary(text, 0), 1u);
    EXPECT_EQ(wui::nextCodePointBoundary(text, 1), 3u);
    // Grapheme walker collapses the combining mark into the base.
    EXPECT_EQ(wui::nextGraphemeBoundary(text, 0), 3u);
    EXPECT_EQ(wui::nextGraphemeBoundary(text, 3), 4u);
    EXPECT_EQ(wui::previousGraphemeBoundary(text, 4), 3u);
    EXPECT_EQ(wui::previousGraphemeBoundary(text, 3), 0u);
}

void testZwjJoinsAdjacentGlyphsAsOneGrapheme()
{
    // "a" + ZWJ (U+200D) + "b". ZWJ is a grapheme-extending code point so
    // it attaches to the preceding base, producing two graphemes:
    // "a\xE2\x80\x8D" (base + extend) and "b" (standalone).
    // Real emoji ZWJ sequences (e.g. family emoji) join glyphs on both
    // sides via UAX #29 GB11; a stage 1 walker only handles the
    // extending-onto-base half, which is enough for Narrator to advance
    // without stalling.
    // Split the ZWJ escape from the trailing "b" so MSVC does not fold
    // the 'b' into the hex escape as a fourth digit.
    const std::string text = "a\xE2\x80\x8D" "b";
    EXPECT_EQ(text.size(), 5u);
    EXPECT_EQ(wui::nextGraphemeBoundary(text, 0), 4u);
    EXPECT_EQ(wui::nextGraphemeBoundary(text, 4), 5u);
    EXPECT_EQ(wui::previousGraphemeBoundary(text, 5), 4u);
    EXPECT_EQ(wui::previousGraphemeBoundary(text, 4), 0u);
}

void testRegionalIndicatorPairIsSingleGrapheme()
{
    // Regional Indicator "C" (U+1F1E8) + Regional Indicator "N" (U+1F1F3) — the
    // Chinese flag emoji as a Regional Indicator pair. Each RI is 4 bytes UTF-8.
    const std::string text = "\xF0\x9F\x87\xA8\xF0\x9F\x87\xB3";
    EXPECT_EQ(text.size(), 8u);
    EXPECT_EQ(wui::nextGraphemeBoundary(text, 0), 8u);
    EXPECT_EQ(wui::previousGraphemeBoundary(text, 8), 0u);
    // Three RIs in a row should split as 2 + 1.
    const std::string threeRis = text + "\xF0\x9F\x87\xA8";
    EXPECT_EQ(threeRis.size(), 12u);
    EXPECT_EQ(wui::nextGraphemeBoundary(threeRis, 0), 8u);
    EXPECT_EQ(wui::nextGraphemeBoundary(threeRis, 8), 12u);
    EXPECT_EQ(wui::previousGraphemeBoundary(threeRis, 12), 8u);
}

void testEmojiModifierAttachesToBase()
{
    // Waving hand (U+1F44B) + fitzpatrick modifier type-4 (U+1F3FD).
    const std::string text = "\xF0\x9F\x91\x8B\xF0\x9F\x8F\xBD";
    EXPECT_EQ(text.size(), 8u);
    EXPECT_EQ(wui::nextGraphemeBoundary(text, 0), 8u);
    EXPECT_EQ(wui::previousGraphemeBoundary(text, 8), 0u);
}

void testWalkerNeverStallsOnInvalidUtf8()
{
    // A truncated 3-byte sequence followed by an ASCII byte. The walker
    // must always advance so a Narrator caret can never get stuck.
    const char rawBytes[] = {'\xE6', '\xB1', 'x', '\0'}; // U+FFFD replacement + 'x'
    const std::string text(rawBytes, 3);
    std::size_t consumed = 0;
    const std::uint32_t code = wui::decodeUtf8CodePoint(text, 0, consumed);
    EXPECT_EQ(code, 0xFFFDu);
    EXPECT_EQ(consumed, 1u);
    EXPECT_EQ(wui::nextCodePointBoundary(text, 0), 1u);
    EXPECT_EQ(wui::nextGraphemeBoundary(text, 0), 1u);
    // Past-end input returns end-of-text without spinning.
    EXPECT_EQ(wui::nextCodePointBoundary(text, text.size() + 4), text.size());
    EXPECT_EQ(wui::previousCodePointBoundary(text, text.size() + 4), text.size() - 1);
}

void testExtendingClassifierIncludesCoreRanges()
{
    // Sanity check the classifier: base ASCII letters are not extending;
    // the combining set we care about is.
    if (wui::isGraphemeExtendingCodePoint(0x0061u)) {
        reportFailure(__FILE__, __LINE__, "!isGraphemeExtendingCodePoint('a')", "");
    }
    if (!wui::isGraphemeExtendingCodePoint(0x0301u)) { // combining acute
        reportFailure(__FILE__, __LINE__, "isGraphemeExtendingCodePoint(U+0301)", "");
    }
    if (!wui::isGraphemeExtendingCodePoint(0x200Du)) { // ZWJ
        reportFailure(__FILE__, __LINE__, "isGraphemeExtendingCodePoint(U+200D)", "");
    }
    if (!wui::isGraphemeExtendingCodePoint(0x1F3FCu)) { // emoji modifier
        reportFailure(__FILE__, __LINE__, "isGraphemeExtendingCodePoint(U+1F3FC)", "");
    }
    if (!wui::isGraphemeExtendingCodePoint(0xFE0Fu)) { // VS-16
        reportFailure(__FILE__, __LINE__, "isGraphemeExtendingCodePoint(U+FE0F)", "");
    }
}

} // namespace

int main()
{
    testAsciiCodePointAndGrapheme();
    testCjkAdvancesOneCodePointPerGrapheme();
    testCombiningMarkAttachesToBase();
    testZwjJoinsAdjacentGlyphsAsOneGrapheme();
    testRegionalIndicatorPairIsSingleGrapheme();
    testEmojiModifierAttachesToBase();
    testWalkerNeverStallsOnInvalidUtf8();
    testExtendingClassifierIncludesCoreRanges();
    if (g_failures != 0) {
        std::fprintf(stderr, "text_units_tests: %d failure(s)\n", g_failures);
        return 1;
    }
    return 0;
}
