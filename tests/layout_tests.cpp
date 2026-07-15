#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "wui/wui.h"

namespace {

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool approx(float a, float b, float epsilon = 0.01f)
{
    return std::fabs(a - b) < epsilon;
}

// --- Row tests ---

void testRowMeasureNoChildren()
{
    wui::Row row;
    auto size = row.measure(wui::Constraints{0.0f, 400.0f, 0.0f, 300.0f});
    expect(size.width == 0.0f, "Empty Row should measure to zero width");
    expect(size.height == 0.0f, "Empty Row should measure to zero height");
}

void testRowMeasureWithGap()
{
    wui::Row row;
    row.setGap(10.0f);
    row.appendChild(std::make_unique<wui::Spacer>(wui::SizeF{20.0f, 30.0f}));
    row.appendChild(std::make_unique<wui::Spacer>(wui::SizeF{40.0f, 20.0f}));

    auto size = row.measure(wui::Constraints{0.0f, 500.0f, 0.0f, 500.0f});
    expect(approx(size.width, 70.0f), "Row width should be 20 + 10(gap) + 40 = 70");
    expect(approx(size.height, 30.0f), "Row height should be max child height = 30");
}

void testRowMeasureWithPadding()
{
    wui::Row row;
    row.setPadding(wui::InsetsF{5.0f, 10.0f, 15.0f, 20.0f});
    row.appendChild(std::make_unique<wui::Spacer>(wui::SizeF{50.0f, 40.0f}));

    auto size = row.measure(wui::Constraints{0.0f, 500.0f, 0.0f, 500.0f});
    expect(approx(size.width, 70.0f), "Row width should be 50 + 5(left) + 15(right) = 70");
    expect(approx(size.height, 70.0f), "Row height should be 40 + 10(top) + 20(bottom) = 70");
}

void testRowGapSkipsZeroSizeChildren()
{
    wui::Row row;
    row.setGap(10.0f);
    auto first = std::make_unique<wui::Spacer>(wui::SizeF{20.0f, 20.0f});
    auto hidden = std::make_unique<wui::Spacer>();
    auto last = std::make_unique<wui::Spacer>(wui::SizeF{30.0f, 20.0f});
    auto* hiddenPtr = hidden.get();
    auto* lastPtr = last.get();
    row.appendChild(std::move(first));
    row.appendChild(std::move(hidden));
    row.appendChild(std::move(last));

    const auto size = row.measure({0.0f, 200.0f, 0.0f, 100.0f});
    expect(approx(size.width, 60.0f), "Zero-size Row children must not reserve an extra gap");
    row.layout({0.0f, 0.0f, size.width, size.height});
    expect(approx(hiddenPtr->bounds().width, 0.0f), "Zero-size Row child should remain collapsed");
    expect(approx(lastPtr->bounds().x, 30.0f), "Visible Row children should have exactly one gap");
}

void testRowCollapsedGapEdgesAndFlexContract()
{
    wui::Row row;
    row.setGap(5.0f);
    auto leading = std::make_unique<wui::Spacer>();
    auto first = std::make_unique<wui::Spacer>(wui::SizeF{10.0f, 10.0f});
    auto middleA = std::make_unique<wui::Spacer>();
    auto middleB = std::make_unique<wui::Spacer>();
    auto last = std::make_unique<wui::Spacer>(wui::SizeF{10.0f, 10.0f});
    auto trailing = std::make_unique<wui::Spacer>();
    auto* lastPtr = last.get();
    row.appendChild(std::move(leading));
    row.appendChild(std::move(first));
    row.appendChild(std::move(middleA));
    row.appendChild(std::move(middleB));
    row.appendChild(std::move(last));
    row.appendChild(std::move(trailing));
    const auto size = row.measure({0.0f, 100.0f, 0.0f, 100.0f});
    expect(approx(size.width, 25.0f), "Leading, trailing and consecutive collapsed Row children must not add gaps");
    row.layout({0.0f, 0.0f, size.width, size.height});
    expect(approx(lastPtr->bounds().x, 15.0f), "Collapsed Row edges must preserve one visible-child gap");

    wui::Row flexRow;
    flexRow.setGap(5.0f);
    auto fixed = std::make_unique<wui::Spacer>(wui::SizeF{10.0f, 10.0f});
    auto flex = std::make_unique<wui::Spacer>();
    flex->setFlex(1.0f);
    auto* flexPtr = flex.get();
    flexRow.appendChild(std::move(fixed));
    flexRow.appendChild(std::move(flex));
    flexRow.layout({0.0f, 0.0f, 15.0f, 10.0f});
    expect(approx(flexPtr->bounds().width, 0.0f) && approx(flexPtr->bounds().x, 15.0f),
           "A zero-allocation flex child remains an intentional layout participant after its gap");
}

void testRowLayoutFlexDistribution()
{
    wui::Row row;
    auto spacer1 = std::make_unique<wui::Spacer>(wui::SizeF{50.0f, 20.0f});
    auto flex1 = std::make_unique<wui::Spacer>();
    flex1->setFlex(1.0f);
    auto flex2 = std::make_unique<wui::Spacer>();
    flex2->setFlex(2.0f);

    wui::Spacer* pFlex1 = flex1.get();
    wui::Spacer* pFlex2 = flex2.get();

    row.appendChild(std::move(spacer1));
    row.appendChild(std::move(flex1));
    row.appendChild(std::move(flex2));

    row.layout({0.0f, 0.0f, 300.0f, 100.0f});

    // Remaining = 300 - 50 = 250; flex1 gets 250*(1/3) ~83.33, flex2 gets 250*(2/3) ~166.67
    expect(approx(pFlex1->bounds().width, 250.0f / 3.0f, 0.5f),
           "Flex 1 child should get 1/3 of remaining space");
    expect(approx(pFlex2->bounds().width, 500.0f / 3.0f, 0.5f),
           "Flex 2 child should get 2/3 of remaining space");
}

void testRowLayoutAlignCenter()
{
    wui::Row row;
    row.setAlign(wui::Alignment::Center);
    auto child = std::make_unique<wui::Spacer>(wui::SizeF{30.0f, 20.0f});
    wui::Spacer* pChild = child.get();
    row.appendChild(std::move(child));

    row.layout({0.0f, 0.0f, 200.0f, 100.0f});

    // Cross-axis centering: (100 - 20) / 2 = 40
    expect(approx(pChild->bounds().y, 40.0f),
           "Center-aligned child should be vertically centered");
}

void testRowLayoutAlignEnd()
{
    wui::Row row;
    row.setAlign(wui::Alignment::End);
    auto child = std::make_unique<wui::Spacer>(wui::SizeF{30.0f, 20.0f});
    wui::Spacer* pChild = child.get();
    row.appendChild(std::move(child));

    row.layout({0.0f, 0.0f, 200.0f, 100.0f});

    expect(approx(pChild->bounds().y, 80.0f),
           "End-aligned child should be at bottom (100 - 20 = 80)");
}

void testRowLayoutAlignStretch()
{
    wui::Row row;
    row.setAlign(wui::Alignment::Stretch);
    auto child = std::make_unique<wui::Spacer>(wui::SizeF{30.0f, 20.0f});
    wui::Spacer* pChild = child.get();
    row.appendChild(std::move(child));

    row.layout({0.0f, 0.0f, 200.0f, 100.0f});

    expect(approx(pChild->bounds().height, 100.0f),
           "Stretch-aligned child should fill cross-axis height");
}

void testRowMeasureClampsToConstraints()
{
    wui::Row row;
    row.appendChild(std::make_unique<wui::Spacer>(wui::SizeF{500.0f, 30.0f}));

    auto size = row.measure(wui::Constraints{0.0f, 200.0f, 0.0f, 50.0f});
    expect(size.width <= 200.0f, "Row measure should clamp width to maxWidth constraint");
}

// --- Column tests ---

void testColumnMeasureNoChildren()
{
    wui::Column col;
    auto size = col.measure(wui::Constraints{0.0f, 400.0f, 0.0f, 300.0f});
    expect(size.width == 0.0f, "Empty Column should measure to zero width");
    expect(size.height == 0.0f, "Empty Column should measure to zero height");
}

void testColumnMeasureWithGap()
{
    wui::Column col;
    col.setGap(5.0f);
    col.appendChild(std::make_unique<wui::Spacer>(wui::SizeF{20.0f, 30.0f}));
    col.appendChild(std::make_unique<wui::Spacer>(wui::SizeF{40.0f, 20.0f}));
    col.appendChild(std::make_unique<wui::Spacer>(wui::SizeF{10.0f, 15.0f}));

    auto size = col.measure(wui::Constraints{0.0f, 500.0f, 0.0f, 500.0f});
    expect(approx(size.width, 40.0f), "Column width should be max child width = 40");
    expect(approx(size.height, 75.0f), "Column height should be 30+5+20+5+15 = 75");
}

void testColumnMeasureWithPadding()
{
    wui::Column col;
    col.setPadding(wui::InsetsF{8.0f, 12.0f, 8.0f, 12.0f});
    col.appendChild(std::make_unique<wui::Spacer>(wui::SizeF{60.0f, 50.0f}));

    auto size = col.measure(wui::Constraints{0.0f, 500.0f, 0.0f, 500.0f});
    expect(approx(size.width, 76.0f), "Column width should be 60 + 8 + 8 = 76");
    expect(approx(size.height, 74.0f), "Column height should be 50 + 12 + 12 = 74");
}

void testColumnGapSkipsZeroSizeChildren()
{
    wui::Column column;
    column.setGap(10.0f);
    auto first = std::make_unique<wui::Spacer>(wui::SizeF{20.0f, 20.0f});
    auto hidden = std::make_unique<wui::Spacer>();
    auto last = std::make_unique<wui::Spacer>(wui::SizeF{20.0f, 30.0f});
    auto* hiddenPtr = hidden.get();
    auto* lastPtr = last.get();
    column.appendChild(std::move(first));
    column.appendChild(std::move(hidden));
    column.appendChild(std::move(last));

    const auto size = column.measure({0.0f, 100.0f, 0.0f, 200.0f});
    expect(approx(size.height, 60.0f), "Zero-size Column children must not reserve an extra gap");
    column.layout({0.0f, 0.0f, size.width, size.height});
    expect(approx(hiddenPtr->bounds().height, 0.0f), "Zero-size Column child should remain collapsed");
    expect(approx(lastPtr->bounds().y, 30.0f), "Visible Column children should have exactly one gap");
}

void testColumnCollapsedGapEdgesAndDynamicVisibility()
{
    wui::Column column;
    column.setGap(5.0f);
    auto leading = std::make_unique<wui::Spacer>();
    auto first = std::make_unique<wui::Spacer>(wui::SizeF{10.0f, 10.0f});
    auto middle = std::make_unique<wui::Spacer>();
    auto last = std::make_unique<wui::Spacer>(wui::SizeF{10.0f, 10.0f});
    auto trailing = std::make_unique<wui::Spacer>();
    auto* middlePtr = middle.get();
    auto* lastPtr = last.get();
    column.appendChild(std::move(leading));
    column.appendChild(std::move(first));
    column.appendChild(std::move(middle));
    column.appendChild(std::move(last));
    column.appendChild(std::move(trailing));
    auto size = column.measure({0.0f, 100.0f, 0.0f, 100.0f});
    expect(approx(size.height, 25.0f), "Collapsed Column edge children must not add gaps");
    column.layout({0.0f, 0.0f, size.width, size.height});
    expect(approx(lastPtr->bounds().y, 15.0f), "Collapsed middle Column child must preserve one gap");

    middlePtr->setSize({10.0f, 10.0f});
    size = column.measure({0.0f, 100.0f, 0.0f, 100.0f});
    expect(approx(size.height, 40.0f), "A formerly collapsed Column child must rejoin layout with two gaps");
    column.layout({0.0f, 0.0f, size.width, size.height});
    expect(approx(lastPtr->bounds().y, 30.0f), "Dynamic visibility must update subsequent Column placement");

    middlePtr->setSize({0.0f, 0.0f});
    size = column.measure({0.0f, 100.0f, 0.0f, 100.0f});
    expect(approx(size.height, 25.0f), "A dynamically collapsed Column child must release its gaps again");
}

void testColumnLayoutFlexDistribution()
{
    wui::Column col;
    auto fixed = std::make_unique<wui::Spacer>(wui::SizeF{30.0f, 50.0f});
    auto flex1 = std::make_unique<wui::Spacer>();
    flex1->setFlex(1.0f);
    auto flex2 = std::make_unique<wui::Spacer>();
    flex2->setFlex(3.0f);

    wui::Spacer* pFlex1 = flex1.get();
    wui::Spacer* pFlex2 = flex2.get();

    col.appendChild(std::move(fixed));
    col.appendChild(std::move(flex1));
    col.appendChild(std::move(flex2));

    col.layout({0.0f, 0.0f, 100.0f, 200.0f});

    // Remaining = 200 - 50 = 150; flex1 gets 150*(1/4) = 37.5, flex2 gets 150*(3/4) = 112.5
    expect(approx(pFlex1->bounds().height, 37.5f),
           "Column flex 1 child should get 1/4 of remaining space");
    expect(approx(pFlex2->bounds().height, 112.5f),
           "Column flex 3 child should get 3/4 of remaining space");
}

void testColumnLayoutAlignCenter()
{
    wui::Column col;
    col.setAlign(wui::Alignment::Center);
    auto child = std::make_unique<wui::Spacer>(wui::SizeF{40.0f, 30.0f});
    wui::Spacer* pChild = child.get();
    col.appendChild(std::move(child));

    col.layout({0.0f, 0.0f, 200.0f, 100.0f});

    // Cross-axis centering: (200 - 40) / 2 = 80
    expect(approx(pChild->bounds().x, 80.0f),
           "Center-aligned Column child should be horizontally centered");
}

void testColumnLayoutAlignStretch()
{
    wui::Column col;
    col.setAlign(wui::Alignment::Stretch);
    auto child = std::make_unique<wui::Spacer>(wui::SizeF{40.0f, 30.0f});
    wui::Spacer* pChild = child.get();
    col.appendChild(std::move(child));

    col.layout({0.0f, 0.0f, 200.0f, 100.0f});

    expect(approx(pChild->bounds().width, 200.0f),
           "Stretch-aligned Column child should fill cross-axis width");
}

// --- Constraints tests ---

void testConstraintsClamp()
{
    wui::Constraints c{10.0f, 100.0f, 20.0f, 80.0f};
    auto s1 = c.clamp({5.0f, 15.0f});
    expect(s1.width == 10.0f, "Clamp should enforce minWidth");
    expect(s1.height == 20.0f, "Clamp should enforce minHeight");

    auto s2 = c.clamp({200.0f, 150.0f});
    expect(s2.width == 100.0f, "Clamp should enforce maxWidth");
    expect(s2.height == 80.0f, "Clamp should enforce maxHeight");

    auto s3 = c.clamp({50.0f, 50.0f});
    expect(s3.width == 50.0f, "Clamp should pass through in-range width");
    expect(s3.height == 50.0f, "Clamp should pass through in-range height");
}

// --- Container tests ---

void testContainerSingleChild()
{
    wui::Container container;
    auto child = std::make_unique<wui::Spacer>(wui::SizeF{80.0f, 60.0f});
    wui::Spacer* pChild = child.get();
    container.appendChild(std::move(child));

    auto size = container.measure(wui::Constraints{0.0f, 200.0f, 0.0f, 200.0f});
    expect(approx(size.width, 80.0f), "Container should pass through child width");
    expect(approx(size.height, 60.0f), "Container should pass through child height");

    container.layout({10.0f, 20.0f, 200.0f, 200.0f});
    expect(approx(pChild->bounds().x, 10.0f), "Container child should get parent bounds x");
    expect(approx(pChild->bounds().y, 20.0f), "Container child should get parent bounds y");
}

void testRowLayoutAlignBaseline()
{
    wui::setTextMeasurer(nullptr);
    wui::Row row;
    row.setAlign(wui::Alignment::Baseline);
    auto large = std::make_unique<wui::Text>("Title");
    large->setFontSize(24.0f);
    large->setLineHeight(32.0f);
    auto small = std::make_unique<wui::Text>("Meta");
    small->setFontSize(12.0f);
    small->setLineHeight(18.0f);
    auto* pLarge = large.get();
    auto* pSmall = small.get();
    row.appendChild(std::move(large));
    row.appendChild(std::move(small));
    row.layout({0.0f, 0.0f, 200.0f, 40.0f});
    expect(approx(pLarge->bounds().y + pLarge->baselineOffset(),
                  pSmall->bounds().y + pSmall->baselineOffset()),
           "Baseline Row should align different text sizes");
}

void testContainerOverlaysAllChildren()
{
    wui::Container container;
    container.setPadding({5.0f, 10.0f, 15.0f, 20.0f});
    auto small = std::make_unique<wui::Spacer>(wui::SizeF{30.0f, 40.0f});
    auto large = std::make_unique<wui::Spacer>(wui::SizeF{80.0f, 60.0f});
    auto* pSmall = small.get();
    auto* pLarge = large.get();
    container.appendChild(std::move(small));
    container.appendChild(std::move(large));

    const auto size = container.measure({0.0f, 200.0f, 0.0f, 200.0f});
    expect(approx(size.width, 100.0f), "Container width should use widest child plus horizontal padding");
    expect(approx(size.height, 90.0f), "Container height should use tallest child plus vertical padding");

    container.layout({10.0f, 20.0f, 120.0f, 100.0f});
    for (const auto* child : {static_cast<wui::Node*>(pSmall), static_cast<wui::Node*>(pLarge)}) {
        expect(approx(child->bounds().x, 15.0f), "Every Container child should share the padded x");
        expect(approx(child->bounds().y, 30.0f), "Every Container child should share the padded y");
        expect(approx(child->bounds().width, 100.0f), "Every Container child should fill padded width");
        expect(approx(child->bounds().height, 70.0f), "Every Container child should fill padded height");
    }
}

void testImageIntrinsicMeasurement()
{
    std::vector<unsigned char> pixels(4u * 3u * 2u, 255u);
    wui::Image image(std::move(pixels), 3, 2);

    expect(image.hasSource(), "Image should report a valid RGBA source");
    expect(approx(image.intrinsicSize().width, 3.0f), "Image intrinsic width should use pixel width");
    expect(approx(image.intrinsicSize().height, 2.0f), "Image intrinsic height should use pixel height");
    const auto constrained = image.measure({0.0f, 2.0f, 0.0f, 1.0f});
    expect(approx(constrained.width, 2.0f), "Image measurement should clamp width");
    expect(approx(constrained.height, 1.0f), "Image measurement should clamp height");

    image.setAlignment(-1.0f, 2.0f);
    expect(approx(image.alignment().x, 0.0f) && approx(image.alignment().y, 1.0f),
           "Image alignment should clamp to normalized coordinates");
}

void testTextMeasurementBaselineAndHitTest()
{
    wui::setTextMeasurer(nullptr);
    wui::Text text("Hello");
    text.setFontSize(20.0f);
    text.setLineHeight(30.0f);

    const auto size = text.measure({0.0f, 200.0f, 0.0f, 100.0f});
    expect(approx(size.width, 50.0f), "Text fallback width should be deterministic");
    expect(approx(size.height, 30.0f), "Text should honor its explicit line height");
    expect(approx(text.baselineOffset(), 21.0f), "Text baseline should center glyph metrics in line height");

    text.layout({10.0f, 20.0f, size.width, size.height});
    expect(text.hitTest({10.0f, 20.0f}) == &text, "Text should hit test inside its laid-out bounds");
    expect(text.hitTest({9.0f, 20.0f}) == nullptr, "Text should reject points outside its bounds");
}

void testTextWrapMaxLinesEllipsisAndUnicode()
{
    wui::setTextMeasurer(nullptr);
    wui::Text text("hello world");
    text.setFontSize(10.0f);
    text.setWrap(wui::TextWrap::Word);
    const auto wrapped = text.resolvedLines(25.0f);
    expect(wrapped.size() == 2 && wrapped[0] == "hello" && wrapped[1] == "world",
           "Word wrap should break at whitespace and omit the break whitespace");
    const auto size = text.measure({0.0f, 25.0f, 0.0f, 100.0f});
    expect(approx(size.width, 25.0f) && approx(size.height, 25.0f),
           "Wrapped Text should measure the widest resolved line and every line height");

    text.setValue("one two three");
    text.setMaxLines(2);
    text.setOverflow(wui::TextOverflow::Ellipsis);
    const auto truncated = text.resolvedLines(25.0f);
    expect(truncated.size() == 2 && truncated[1] == "tw...",
           "maxLines with Ellipsis should preserve a fitted suffix on the final visible line");

    wui::Text multilingual("\xE4\xBD\xA0\xE5\xA5\xBD\xE4\xB8\x96\xE7\x95\x8C");
    multilingual.setFontSize(10.0f);
    multilingual.setWrap(wui::TextWrap::Word);
    const auto unicodeLines = multilingual.resolvedLines(5.0f);
    expect(unicodeLines.size() == 4 && unicodeLines[0] == "\xE4\xBD\xA0" && unicodeLines[3] == "\xE7\x95\x8C",
           "Fallback wrapping must split UTF-8 at codepoint boundaries, not bytes");

    auto built = std::move(wui::ui::Text("builder").wrap().maxLines(3).ellipsis()).intoNode();
    auto* builtText = dynamic_cast<wui::Text*>(built.get());
    expect(builtText && builtText->wrap() == wui::TextWrap::Word && builtText->maxLines() == 3
               && builtText->overflow() == wui::TextOverflow::Ellipsis,
           "Text builder should expose wrapping, line limit, and ellipsis configuration");
}

void testFoundationalContainerHitTestOrder()
{
    wui::Container container;
    auto back = std::make_unique<wui::Column>();
    back->appendChild(std::make_unique<wui::Text>("Back"));
    auto front = std::make_unique<wui::Row>();
    auto image = std::make_unique<wui::Image>(std::vector<unsigned char>(4u, 255u), 1, 1);
    auto* frontImage = image.get();
    front->appendChild(std::move(image));
    container.appendChild(std::move(back));
    container.appendChild(std::move(front));

    container.layout({0.0f, 0.0f, 80.0f, 40.0f});
    expect(container.hitTest({0.5f, 0.5f}) == frontImage,
           "Overlapping foundational widgets should hit test in reverse paint order");
    expect(container.hitTest({81.0f, 1.0f}) == nullptr,
           "Foundational widget tree should reject points outside its root bounds");
}

// --- Row layout with gap + flex ---

void testRowGapWithFlex()
{
    wui::Row row;
    row.setGap(10.0f);
    auto fixed1 = std::make_unique<wui::Spacer>(wui::SizeF{20.0f, 30.0f});
    auto flex = std::make_unique<wui::Spacer>();
    flex->setFlex(1.0f);
    auto fixed2 = std::make_unique<wui::Spacer>(wui::SizeF{30.0f, 30.0f});

    wui::Spacer* pFixed2 = fixed2.get();

    row.appendChild(std::move(fixed1));
    row.appendChild(std::move(flex));
    row.appendChild(std::move(fixed2));

    row.layout({0.0f, 0.0f, 200.0f, 50.0f});

    // fixed1=20, gap=10, flex=?, gap=10, fixed2=30
    // fixedWidth = 20 + 10 + 10 + 30 = 70, remaining = 130
    // flex gets 130, last child at 20+10+130+10 = 170
    expect(approx(pFixed2->bounds().x, 170.0f),
           "Last fixed child should be positioned after flex+gaps");
}

void testConstraintsDeflate()
{
    const wui::Constraints constraints{20.0f, 100.0f, 30.0f, 90.0f};
    const auto inner = constraints.deflate({5.0f, 10.0f, 15.0f, 20.0f});
    expect(approx(inner.minWidth, 0.0f) && approx(inner.maxWidth, 80.0f),
           "Deflated constraints should remove horizontal insets");
    expect(approx(inner.minHeight, 0.0f) && approx(inner.maxHeight, 60.0f),
           "Deflated constraints should remove vertical insets");
}

void testContainerContentAlignment()
{
    wui::Container container;
    container.setContentAlignment(wui::Alignment::Center, wui::Alignment::Center);
    auto child = std::make_unique<wui::Spacer>(wui::SizeF{20.0f, 10.0f});
    auto* pChild = child.get();
    container.appendChild(std::move(child));
    container.layout({10.0f, 20.0f, 100.0f, 50.0f});
    expect(approx(pChild->bounds().x, 50.0f) && approx(pChild->bounds().y, 40.0f),
           "Centered Container content should have precise position");
    expect(approx(pChild->bounds().width, 20.0f) && approx(pChild->bounds().height, 10.0f),
           "Centered Container content should preserve intrinsic size");
}

void testContainerExplicitSize()
{
    wui::Container container;
    container.setWidth(120.0f);
    container.setHeight(48.0f);
    container.appendChild(std::make_unique<wui::Spacer>(wui::SizeF{10.0f, 10.0f}));
    const auto size = container.measure({0.0f, 200.0f, 0.0f, 200.0f});
    expect(approx(size.width, 120.0f) && approx(size.height, 48.0f),
           "Container explicit size should override intrinsic content size");
}

void testNestedLayoutSnapshotAndDirtyClear()
{
    wui::Container root;
    root.setPadding({10.0f, 5.0f, 10.0f, 5.0f});
    root.setContentAlignment(wui::Alignment::Start, wui::Alignment::Start);
    auto column = std::make_unique<wui::Column>();
    column->setGap(4.0f);
    auto first = std::make_unique<wui::Spacer>(wui::SizeF{30.0f, 20.0f});
    auto row = std::make_unique<wui::Row>();
    row->setGap(3.0f);
    auto left = std::make_unique<wui::Spacer>(wui::SizeF{10.0f, 8.0f});
    auto right = std::make_unique<wui::Spacer>(wui::SizeF{15.0f, 8.0f});
    auto* columnPtr = column.get();
    auto* firstPtr = first.get();
    auto* rowPtr = row.get();
    auto* leftPtr = left.get();
    auto* rightPtr = right.get();
    row->appendChild(std::move(left));
    row->appendChild(std::move(right));
    column->appendChild(std::move(first));
    column->appendChild(std::move(row));
    root.appendChild(std::move(column));

    root.layout({20.0f, 30.0f, 100.0f, 80.0f});

    expect(approx(columnPtr->bounds().x, 30.0f) && approx(columnPtr->bounds().width, 30.0f),
           "Snapshot: start-aligned Column should retain intrinsic width in Container content bounds");
    expect(approx(firstPtr->bounds().x, 30.0f) && approx(firstPtr->bounds().y, 35.0f),
           "Snapshot: first Column child should begin at padded content origin");
    expect(approx(rowPtr->bounds().x, 30.0f) && approx(rowPtr->bounds().y, 59.0f),
           "Snapshot: second Column child should follow first child and gap");
    expect(approx(leftPtr->bounds().x, 30.0f) && approx(rightPtr->bounds().x, 43.0f),
           "Snapshot: nested Row should place children in main-axis order with its gap");
    for (const wui::Node* node : {static_cast<wui::Node*>(&root), static_cast<wui::Node*>(columnPtr),
                                  static_cast<wui::Node*>(firstPtr), static_cast<wui::Node*>(rowPtr),
                                  static_cast<wui::Node*>(leftPtr), static_cast<wui::Node*>(rightPtr)}) {
        expect(!node->isDirty(wui::DirtyFlag::Layout),
               "Completed composite layout should clear layout dirtiness for the entire laid-out subtree");
    }
}

void testConstrainedNestedLayoutSnapshot()
{
    wui::Row row;
    row.setPadding({4.0f, 2.0f, 4.0f, 2.0f});
    row.setGap(6.0f);
    auto fixed = std::make_unique<wui::Spacer>(wui::SizeF{20.0f, 10.0f});
    auto flex = std::make_unique<wui::Column>();
    flex->setFlex(1.0f);
    flex->setAlign(wui::Alignment::Stretch);
    auto inner = std::make_unique<wui::Spacer>(wui::SizeF{5.0f, 7.0f});
    auto* fixedPtr = fixed.get();
    auto* flexPtr = flex.get();
    auto* innerPtr = inner.get();
    flex->appendChild(std::move(inner));
    row.appendChild(std::move(fixed));
    row.appendChild(std::move(flex));

    row.layout({0.0f, 0.0f, 80.0f, 30.0f});

    expect(approx(fixedPtr->bounds().x, 4.0f) && approx(fixedPtr->bounds().width, 20.0f),
           "Snapshot: fixed Row child should honor leading padding");
    expect(approx(flexPtr->bounds().x, 30.0f) && approx(flexPtr->bounds().width, 46.0f),
           "Snapshot: flex child should receive constrained remaining main-axis space");
    expect(approx(innerPtr->bounds().width, 46.0f) && approx(innerPtr->bounds().height, 7.0f),
           "Snapshot: stretched nested Column should constrain its child to allocated cross-axis width");
}

} // namespace

int main()
{
    try {
    testRowMeasureNoChildren();
    testRowMeasureWithGap();
    testRowMeasureWithPadding();
    testRowGapSkipsZeroSizeChildren();
    testRowCollapsedGapEdgesAndFlexContract();
    testRowLayoutFlexDistribution();
    testRowLayoutAlignCenter();
    testRowLayoutAlignEnd();
    testRowLayoutAlignStretch();
    testRowMeasureClampsToConstraints();
    testRowLayoutAlignBaseline();
    testColumnMeasureNoChildren();
    testColumnMeasureWithGap();
    testColumnMeasureWithPadding();
    testColumnGapSkipsZeroSizeChildren();
    testColumnCollapsedGapEdgesAndDynamicVisibility();
    testColumnLayoutFlexDistribution();
    testColumnLayoutAlignCenter();
    testColumnLayoutAlignStretch();
    testConstraintsClamp();
    testContainerSingleChild();
    testContainerOverlaysAllChildren();
    testImageIntrinsicMeasurement();
    testTextMeasurementBaselineAndHitTest();
    testTextWrapMaxLinesEllipsisAndUnicode();
    testFoundationalContainerHitTestOrder();
    testRowGapWithFlex();
    testConstraintsDeflate();
    testContainerContentAlignment();
    testContainerExplicitSize();
    testNestedLayoutSnapshotAndDirtyClear();
    testConstrainedNestedLayoutSnapshot();
    } catch (const std::exception& error) {
        std::cerr << "Layout test failure: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
