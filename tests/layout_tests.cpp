#include <cmath>
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

} // namespace

int main()
{
    testRowMeasureNoChildren();
    testRowMeasureWithGap();
    testRowMeasureWithPadding();
    testRowLayoutFlexDistribution();
    testRowLayoutAlignCenter();
    testRowLayoutAlignEnd();
    testRowLayoutAlignStretch();
    testRowMeasureClampsToConstraints();
    testColumnMeasureNoChildren();
    testColumnMeasureWithGap();
    testColumnMeasureWithPadding();
    testColumnLayoutFlexDistribution();
    testColumnLayoutAlignCenter();
    testColumnLayoutAlignStretch();
    testConstraintsClamp();
    testContainerSingleChild();
    testRowGapWithFlex();
    return 0;
}
