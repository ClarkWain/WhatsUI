#include "long_text_page.h"

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "view/components/page_header.h"
#include "view/components/preview_surface.h"
#include "view/components/responsive_layouts.h"
#include "wui/theme.h"
#include "wui/ui.h"

using namespace wui::ui;

namespace whatsui::gallery::view::pages {
namespace {

constexpr std::size_t kLineCount = 10000;
constexpr float kLineHeight = 24.0f;
constexpr float kViewportHeight = 480.0f;

constexpr std::array<std::string_view, 12> kMultilingualSamples{{
    "中文 · 雨后的上海，霓虹倒映在安静的街道上。",
    "English · A quick brown fox studies typography at dawn.",
    "日本語 · 静かな朝、文字が画面の上を軽やかに流れる。",
    "한국어 · 만 줄의 문장이 부드럽고 빠르게 스크롤됩니다.",
    "العربية · مرحبًا بالعالم، تتدفق الكلمات بوضوح وسلاسة.",
    "हिन्दी · नमस्ते दुनिया, दस हज़ार पंक्तियाँ सहजता से चलती हैं।",
    "ไทย · สวัสดีโลก ตัวอักษรหนึ่งหมื่นบรรทัดเลื่อนได้อย่างลื่นไหล",
    "Ελληνικά · Η τυπογραφία παραμένει καθαρή σε κάθε γραμμή.",
    "Русский · Десять тысяч строк остаются быстрыми и читаемыми.",
    "Español · Tipografía, ritmo y rendimiento conviven aquí.",
    "עברית · עשרת אלפים שורות נשארות ברורות ומהירות.",
    "Mixed · 東京 → القاهرة → Αθήνα → 서울 → मुंबई · 🌍 10K",
}};

std::string lineNumber(std::size_t index)
{
    std::string value = std::to_string(index + 1);
    if (value.size() < 5) value.insert(0, 5 - value.size(), '0');
    return value;
}

std::string lineAt(std::size_t index)
{
    return lineNumber(index) + "  │  "
        + std::string(kMultilingualSamples[index % kMultilingualSamples.size()]);
}

std::string buildDocument()
{
    std::string document;
    document.reserve(kLineCount * 96);
    for (std::size_t index = 0; index < kLineCount; ++index) {
        if (index != 0) document.push_back('\n');
        document += lineAt(index);
    }
    return document;
}

std::unique_ptr<wui::Node> buildCapabilityCard()
{
    auto badges = std::make_unique<view::components::ResponsiveFlow>();
    badges->gap(8.0f);
    badges->appendChild(Badge("10,000 LINES")
        .appearance(wui::BadgeAppearance::Tint)
        .color(wui::BadgeColor::Brand)
        .intoNode());
    badges->appendChild(Badge("1 TEXT NODE")
        .appearance(wui::BadgeAppearance::Tint)
        .color(wui::BadgeColor::Informative)
        .intoNode());
    badges->appendChild(Badge("12 LANGUAGES")
        .appearance(wui::BadgeAppearance::Tint)
        .color(wui::BadgeColor::Success)
        .intoNode());

    return Card()
        .appearance(wui::CardAppearance::FilledAlternative)
        .size(wui::CardSize::Medium)
        .children(
            Column()
            .gap(10.0f)
            .align(wui::Alignment::Stretch)
            .children(
                std::move(badges),
                Text("One Text node owns the complete UTF-8 document. Explicit line "
                     "breaks, font fallback, shaping and mixed-direction scripts are "
                     "rendered inside a clipped ScrollView.")
                    .size(12.0f)
                    .lineHeight(19.0f)
                    .wrap()
                    .color(wui::theme().colors.textMuted)
            )
        )
        .intoNode();
}

std::unique_ptr<wui::Node> buildTextViewport()
{
    auto document = std::make_unique<wui::Text>(buildDocument());
    document->setFontSize(14.0f);
    document->setLineHeight(kLineHeight);
    document->setWrap(wui::TextWrap::NoWrap);
    document->setFillAvailableWidth(true);
    document->setColor(wui::theme().colors.text);
    document->setAccessibilityId("gallery.long-text.document");

    auto scroller = std::make_unique<wui::ScrollView>();
    scroller->setAxis(wui::ScrollAxis::Vertical);
    scroller->setAccessibilityId("gallery.long-text.viewport");
    auto* const textViewport = scroller.get();
    scroller->child(std::move(document));

    auto actions = std::make_unique<view::components::ResponsiveFlow>();
    actions->gap(8.0f);
    actions->appendChild(Button("Line 1")
        .appearance(wui::ButtonAppearance::Subtle)
        .onClick([textViewport] { textViewport->setScrollOffset(0.0f); })
        .intoNode());
    actions->appendChild(Button("Line 5,000")
        .appearance(wui::ButtonAppearance::Subtle)
        .onClick([textViewport] {
            textViewport->setScrollOffset(kLineHeight * 4999.0f);
        })
        .intoNode());
    actions->appendChild(Button("Line 10,000")
        .appearance(wui::ButtonAppearance::Subtle)
        .onClick([textViewport] {
            textViewport->setScrollOffset(textViewport->maxScrollOffset());
        })
        .intoNode());

    auto viewport = Box()
        .height(kViewportHeight)
        .background(wui::theme().colors.neutralBackground1.rest)
        .radius(wui::theme().radius.md)
        .children(std::move(scroller))
        .intoNode();

    return view::components::buildPreviewSurface(
        {"Multilingual Text document",
         "Single Text node · explicit line breaks · 24 DIP line height",
         "10K LINES",
         590.0f,
         true},
        Column()
            .gap(12.0f)
            .align(wui::Alignment::Stretch)
            .children(std::move(actions), std::move(viewport))
            .intoNode());
}

} // namespace

std::unique_ptr<wui::Node> buildLongTextPage()
{
    return ScrollView()
        .children(
            Column()
            .gap(20.0f)
            .padding({32.0f, 32.0f, 40.0f, 32.0f})
            .align(wui::Alignment::Stretch)
            .children(
                view::components::buildPageHeader({
                    "TEXT ENGINE",
                    "10,000-line text lab",
                    "A multilingual shaping, fallback and scrolling stress test "
                    "rendered by one continuous Text document.",
                    {}}),
                buildCapabilityCard(),
                buildTextViewport()
            )
        )
        .intoNode();
}

} // namespace whatsui::gallery::view::pages
