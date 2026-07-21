#include "wui/rating.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <utility>
#include <vector>

#include "wui/text_metrics.h"
#include "wui/theme.h"

namespace wui {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kItemGap = 2.0f;

float itemSize(RatingSize size) noexcept
{
    switch (size) {
    case RatingSize::Small: return 12.0f;
    case RatingSize::Medium: return 16.0f;
    case RatingSize::Large: return 20.0f;
    case RatingSize::ExtraLarge: return 28.0f;
    }
    return 16.0f;
}

float rowHeight(RatingSize size) noexcept
{
    return std::max(16.0f, itemSize(size));
}

float itemOffset(int index, RatingSize size) noexcept
{
    return static_cast<float>(index) * (itemSize(size) + kItemGap);
}

float itemsWidth(int count, RatingSize size) noexcept
{
    if (count <= 0) return 0.0f;
    return itemSize(size) * static_cast<float>(count) +
        kItemGap * static_cast<float>(count - 1);
}

float labelGap(RatingSize size) noexcept
{
    switch (size) {
    case RatingSize::Small:
    case RatingSize::Medium: return 4.0f;
    case RatingSize::Large: return 6.0f;
    case RatingSize::ExtraLarge: return 8.0f;
    }
    return 4.0f;
}

float textRunWidth(std::string_view text, float size, int weight) noexcept
{
    if (const auto* measurer = textMeasurer()) {
        return measurer
            ->measureText(std::string(text), size, weight,
                          theme().typography.familyBase)
            .width;
    }
    return static_cast<float>(text.size()) * size * 0.56f;
}

Color selectedColor(const Theme& current, RatingColor color) noexcept
{
    switch (color) {
    case RatingColor::Brand: return current.colors.brandForeground1;
    case RatingColor::Marigold: return {234, 163, 0, 255};
    case RatingColor::Neutral: return current.colors.neutralForeground1;
    }
    return current.colors.neutralForeground1;
}

std::vector<PointF> starPoints(const RectF& item)
{
    std::vector<PointF> points;
    points.reserve(10);
    const float cx = item.x + item.width * 0.5f;
    const float cy = item.y + item.height * 0.5f;
    const float outer = std::min(item.width, item.height) * 0.43f;
    const float inner = outer * 0.45f;
    for (int index = 0; index < 10; ++index) {
        const float radius = index % 2 == 0 ? outer : inner;
        const float angle = -kPi * 0.5f + static_cast<float>(index) * kPi / 5.0f;
        points.push_back({cx + std::cos(angle) * radius, cy + std::sin(angle) * radius});
    }
    return points;
}

void fillShape(PaintContext& context, const RectF& item, RatingShape shape, Color color)
{
    const float inset = std::max(1.0f, item.width * 0.1f);
    const RectF icon{item.x + inset, item.y + inset,
                     std::max(0.0f, item.width - inset * 2.0f),
                     std::max(0.0f, item.height - inset * 2.0f)};
    switch (shape) {
    case RatingShape::Star:
        context.fillPolygon(starPoints(item), color);
        break;
    case RatingShape::Circle:
        context.fillRoundRect(icon, theme().radius.circular, color);
        break;
    case RatingShape::Square:
        context.fillRect(icon, color);
        break;
    }
}

void strokeShape(PaintContext& context, const RectF& item, RatingShape shape, Color color)
{
    const float inset = std::max(1.0f, item.width * 0.1f);
    const float stroke = context.snapStrokeWidth(theme().stroke.thin);
    const RectF outer{item.x + inset, item.y + inset,
                      std::max(0.0f, item.width - inset * 2.0f),
                      std::max(0.0f, item.height - inset * 2.0f)};
    switch (shape) {
    case RatingShape::Star:
        context.strokePolygon(starPoints(item), stroke, color);
        break;
    case RatingShape::Circle:
        context.strokeRoundRect(outer, theme().radius.circular, stroke, color);
        break;
    case RatingShape::Square:
        context.strokeRoundRect(outer, theme().radius.none, stroke, color);
        break;
    }
}

void drawItem(PaintContext& context, const RectF& item, RatingShape shape,
              float fraction, Color active, bool interactive)
{
    const Theme& current = theme();
    const RectF renderedItem = context.snapRectEdges(item);
    if (interactive) {
        strokeShape(context, renderedItem, shape, active);
    } else {
        fillShape(context, renderedItem, shape,
                  current.colors.neutralBackground3.pressed);
    }
    fraction = std::clamp(fraction, 0.0f, 1.0f);
    if (fraction <= 0.0f) return;
    if (fraction >= 1.0f) {
        fillShape(context, renderedItem, shape, active);
        return;
    }
    const int checkpoint = context.save();
    const float clipRight = context.snapToPhysicalPixel(
        renderedItem.x + renderedItem.width * fraction);
    context.clipRect(
        {renderedItem.x, renderedItem.y,
         std::max(0.0f, clipRight - renderedItem.x),
         renderedItem.height});
    fillShape(context, renderedItem, shape, active);
    context.restoreTo(checkpoint);
}

void drawFocusOutline(PaintContext& context, const RectF& bounds)
{
    const Theme& current = theme();
    const float inset = current.controls.focusInset;
    const RectF aligned = context.snapRectEdges(bounds);
    const std::vector<PointF> points{
        {context.snapToPhysicalPixel(aligned.x - inset),
         context.snapToPhysicalPixel(aligned.y - inset)},
        {context.snapToPhysicalPixel(aligned.x + aligned.width + inset),
         context.snapToPhysicalPixel(aligned.y - inset)},
        {context.snapToPhysicalPixel(aligned.x + aligned.width + inset),
         context.snapToPhysicalPixel(aligned.y + aligned.height + inset)},
        {context.snapToPhysicalPixel(aligned.x - inset),
         context.snapToPhysicalPixel(aligned.y + aligned.height + inset)},
    };
    context.strokePolygon(points, context.snapStrokeWidth(
                                      current.controls.focusWidth +
                                      current.stroke.thick),
                          current.colors.strokeFocusOuter);
    context.strokePolygon(
        points, context.snapStrokeWidth(current.controls.focusWidth),
        current.colors.strokeFocusInner);
}

std::string formatNumber(float value)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1) << value;
    std::string result = stream.str();
    if (result.size() >= 2 && result.substr(result.size() - 2) == ".0") {
        result.resize(result.size() - 2);
    }
    return result;
}

std::string formatCount(std::uint64_t value)
{
    try {
        std::ostringstream stream;
        stream.imbue(std::locale(""));
        stream << value;
        return stream.str();
    } catch (...) {
        // A stripped-down deployment may not have the OS locale installed.
        // Preserve a readable deterministic fallback instead of failing paint.
        std::string result = std::to_string(value);
        for (std::ptrdiff_t index = static_cast<std::ptrdiff_t>(result.size()) - 3;
             index > 0; index -= 3) {
            result.insert(static_cast<std::size_t>(index), ",");
        }
        return result;
    }
}

bool primary(const PointerEvent& event) noexcept
{
    return event.button == MouseButton::Left;
}

} // namespace

Rating::Rating(float value, int maximum)
{
    setMaximum(maximum);
    value_ = normalized(value);
}

float Rating::value() const noexcept
{
    return normalized(hasBinding_ ? binding_->get() : value_);
}

Rating& Rating::value(float value)
{
    setValue(value);
    return *this;
}

void Rating::setValue(float value)
{
    const float next = normalized(value);
    if (hasBinding_) {
        if (binding_->get() != next) binding_->set(next);
    } else if (value_ != next) {
        value_ = next;
        markDirty(DirtyFlag::Paint);
    }
}

int Rating::maximum() const noexcept { return maximum_; }

void Rating::setMaximum(int maximum)
{
    maximum_ = std::max(2, maximum);
    setValue(value());
    markDirty(DirtyFlag::Layout);
}

float Rating::step() const noexcept { return step_; }
Rating& Rating::step(float step) { setStep(step); return *this; }
void Rating::setStep(float step)
{
    step_ = std::abs(step - 0.5f) < 0.001f ? 0.5f : 1.0f;
    setValue(value());
    markDirty(DirtyFlag::Paint);
}
RatingColor Rating::color() const noexcept { return color_; }
void Rating::setColor(RatingColor color) noexcept { color_ = color; markDirty(DirtyFlag::Paint); }
RatingSize Rating::size() const noexcept { return size_; }
void Rating::setSize(RatingSize size) noexcept { size_ = size; markDirty(DirtyFlag::Layout); }
RatingShape Rating::shape() const noexcept { return shape_; }
void Rating::setShape(RatingShape shape) noexcept { shape_ = shape; markDirty(DirtyFlag::Paint); }
bool Rating::isReadOnly() const noexcept { return readOnly_; }
void Rating::setReadOnly(bool value) noexcept
{
    readOnly_ = value;
    hoveredValue_.reset();
    setVisualState(ControlVisualState::Pressed, false);
    markDirty(DirtyFlag::Paint);
}
const std::string& Rating::accessibleLabel() const noexcept { return accessibleLabel_; }
void Rating::setAccessibleLabel(std::string label)
{
    if (accessibleLabel_ != label) { accessibleLabel_ = std::move(label); markDirty(DirtyFlag::Style); }
}
Rating& Rating::itemLabel(ItemLabelHandler handler) { setItemLabel(std::move(handler)); return *this; }
void Rating::setItemLabel(ItemLabelHandler handler)
{
    itemLabel_ = std::move(handler);
    markDirty(DirtyFlag::Style);
}
std::string Rating::labelForValue(float value) const
{
    return itemLabel_ ? itemLabel_(normalized(value)) : formatNumber(normalized(value));
}
std::string Rating::accessibleValueText() const
{
    return formatNumber(value()) + " out of " + std::to_string(maximum_);
}

Rating& Rating::bind(State<float>& state)
{
    binding_.emplace(state);
    hasBinding_ = true;
    value_ = normalized(state.get());
    subscription_.subscribe(state, [this](const float& value) {
        const float canonical = normalized(value);
        value_ = canonical;
        markDirty(DirtyFlag::Paint);
        // Binding remains the single source of truth.  External writes can be
        // off-step, non-finite or outside max; canonicalize them immediately
        // so every observer sees the same value the control renders.
        if (hasBinding_ && binding_->get() != canonical) binding_->set(canonical);
    });
    setValue(value_);
    return *this;
}

Rating& Rating::onChange(ChangeHandler handler) { onChange_ = std::move(handler); return *this; }

float Rating::normalized(float value) const noexcept
{
    if (!std::isfinite(value)) value = 0.0f;
    value = std::clamp(value, 0.0f, static_cast<float>(maximum_));
    value = std::round(value / step_) * step_;
    return std::clamp(value, 0.0f, static_cast<float>(maximum_));
}

float Rating::valueAt(float x) const noexcept
{
    const float size = itemSize(size_);
    const float stride = size + kItemGap;
    const float local = std::clamp(
        x - bounds().x, 0.0f,
        std::max(0.0f, itemsWidth(maximum_, size_) - 0.001f));
    const int item =
        std::min(maximum_ - 1, static_cast<int>(local / stride));
    const float withinItem =
        std::clamp(local - static_cast<float>(item) * stride, 0.0f, size);
    if (step_ == 0.5f && withinItem < size * 0.5f) {
        return static_cast<float>(item) + 0.5f;
    }
    return static_cast<float>(item + 1);
}

void Rating::commit(float value)
{
    const float before = this->value();
    setValue(value);
    if (this->value() != before && onChange_) onChange_(this->value());
}

SizeF Rating::measure(const Constraints& constraints) const
{
    return constraints.clamp(
        {itemsWidth(maximum_, size_), rowHeight(size_)});
}

void Rating::paint(PaintContext& context)
{
    const Theme& current = theme();
    const float size = itemSize(size_);
    const float y = bounds().y + (bounds().height - size) * 0.5f;
    const float shown = hoveredValue_.value_or(value());
    Color active = selectedColor(current, color_);
    if (!isEnabled()) active = current.colors.neutralForegroundDisabled;
    for (int index = 0; index < maximum_; ++index) {
        const float fraction = std::clamp(shown - static_cast<float>(index), 0.0f, 1.0f);
        drawItem(context,
                 {bounds().x + itemOffset(index, size_), y, size, size},
                 shape_, fraction, active, true);
    }
    if ((visualStates() & toMask(ControlVisualState::Focused)) != 0) {
        drawFocusOutline(
            context,
            {bounds().x, bounds().y, itemsWidth(maximum_, size_),
             rowHeight(size_)});
    }
    clearDirty(DirtyFlag::Paint);
}

bool Rating::onPointerEvent(const PointerEvent& event)
{
    if (!isEnabled() || readOnly_) return false;
    switch (event.action) {
    case PointerAction::Enter:
    case PointerAction::Move:
        setVisualState(ControlVisualState::Hovered, bounds().contains(event.position));
        hoveredValue_ = bounds().contains(event.position)
            ? std::optional<float>(valueAt(event.position.x)) : std::nullopt;
        markDirty(DirtyFlag::Paint);
        return true;
    case PointerAction::Leave:
        setVisualState(ControlVisualState::Hovered, false);
        hoveredValue_.reset();
        markDirty(DirtyFlag::Paint);
        return true;
    case PointerAction::Down:
        if (!primary(event)) return false;
        setVisualState(ControlVisualState::Pressed, true);
        hoveredValue_ = valueAt(event.position.x);
        return true;
    case PointerAction::Up:
        if (!primary(event)) return false;
        if ((visualStates() & toMask(ControlVisualState::Pressed)) != 0 && bounds().contains(event.position)) {
            commit(valueAt(event.position.x));
        }
        setVisualState(ControlVisualState::Pressed, false);
        hoveredValue_.reset();
        return true;
    case PointerAction::Cancel:
        setVisualState(ControlVisualState::Pressed, false);
        hoveredValue_.reset();
        markDirty(DirtyFlag::Paint);
        return true;
    default:
        return false;
    }
}

bool Rating::onKeyEvent(const KeyEvent& event)
{
    if (!isEnabled() || readOnly_ || event.action != KeyAction::Down) return false;
    float next = value();
    switch (event.keyCode) {
    case 37: case 40: next = std::max(step_, next - step_); break;
    case 38: case 39: next = next == 0.0f ? step_ : next + step_; break;
    case 36: next = step_; break;
    case 35: next = static_cast<float>(maximum_); break;
    case 32: case 13: case 257: next = next == 0.0f ? step_ : next; break;
    default: return false;
    }
    commit(next);
    return true;
}

AccessibilityActionCapabilities Rating::accessibilityActions() const noexcept
{
    AccessibilityActionCapabilities actions;
    actions.setValue = !readOnly_;
    actions.valueReadOnly = readOnly_;
    return actions;
}

AccessibilityActionStatus Rating::performAccessibilityAction(
    AccessibilityActionKind kind, std::string_view value)
{
    if (kind != AccessibilityActionKind::SetValue) return AccessibilityActionStatus::NotSupported;
    if (!isEnabled()) return AccessibilityActionStatus::ElementNotEnabled;
    if (readOnly_) return AccessibilityActionStatus::NotSupported;
    try {
        std::size_t parsed = 0;
        const std::string text(value);
        const float requested = std::stof(text, &parsed);
        if (parsed != text.size() || !std::isfinite(requested) || requested < 0.0f || requested > maximum_ ||
            std::abs(normalized(requested) - requested) > 0.001f) {
            return AccessibilityActionStatus::InvalidValue;
        }
        commit(requested);
        return AccessibilityActionStatus::Succeeded;
    } catch (...) {
        return AccessibilityActionStatus::InvalidValue;
    }
}

RatingDisplay::RatingDisplay(std::optional<float> value, int maximum)
    : value_(value)
{
    setMaximum(maximum);
    setValue(value);
}

std::optional<float> RatingDisplay::value() const noexcept { return value_; }
RatingDisplay& RatingDisplay::value(float value) { setValue(value); return *this; }
void RatingDisplay::setValue(std::optional<float> value) noexcept
{
    // A rendered RatingDisplay always has a visible and spoken value. Callers
    // with no result should omit the component rather than creating an empty
    // image semantic that assistive technology cannot describe.
    if (!value || !std::isfinite(*value)) value = 0.0f;
    *value = std::clamp(*value, 0.0f, static_cast<float>(maximum_));
    value_ = value;
    markDirty(DirtyFlag::Layout);
}
int RatingDisplay::maximum() const noexcept { return maximum_; }
void RatingDisplay::setMaximum(int maximum) noexcept
{
    maximum_ = std::max(2, maximum);
    setValue(value_);
    markDirty(DirtyFlag::Layout);
}
std::optional<std::uint64_t> RatingDisplay::count() const noexcept { return count_; }
void RatingDisplay::setCount(std::optional<std::uint64_t> count) noexcept { count_ = count; markDirty(DirtyFlag::Layout); }
RatingDisplay& RatingDisplay::countFormatter(CountFormatter formatter)
{
    setCountFormatter(std::move(formatter));
    return *this;
}
void RatingDisplay::setCountFormatter(CountFormatter formatter)
{
    countFormatter_ = std::move(formatter);
    markDirty(DirtyFlag::Layout);
}
bool RatingDisplay::isCompact() const noexcept { return compact_; }
void RatingDisplay::setCompact(bool compact) noexcept { compact_ = compact; markDirty(DirtyFlag::Layout); }
RatingColor RatingDisplay::color() const noexcept { return color_; }
void RatingDisplay::setColor(RatingColor color) noexcept { color_ = color; markDirty(DirtyFlag::Paint); }
RatingSize RatingDisplay::size() const noexcept { return size_; }
void RatingDisplay::setSize(RatingSize size) noexcept { size_ = size; markDirty(DirtyFlag::Layout); }
RatingShape RatingDisplay::shape() const noexcept { return shape_; }
void RatingDisplay::setShape(RatingShape shape) noexcept { shape_ = shape; markDirty(DirtyFlag::Paint); }
const std::string& RatingDisplay::accessibleLabel() const noexcept { return accessibleLabel_; }
void RatingDisplay::setAccessibleLabel(std::string label)
{
    if (accessibleLabel_ != label) { accessibleLabel_ = std::move(label); markDirty(DirtyFlag::Style); }
}

std::string RatingDisplay::valueText() const { return value_ ? formatNumber(*value_) : std::string{}; }
std::string RatingDisplay::countText() const
{
    if (!count_) return {};
    return countFormatter_ ? countFormatter_(*count_) : formatCount(*count_);
}
std::string RatingDisplay::generatedAccessibleLabel() const
{
    if (!accessibleLabel_.empty()) return accessibleLabel_;
    if (!value_) return {};
    std::string result = valueText() + " out of " + std::to_string(maximum_);
    if (count_) result += ", " + countText() + (*count_ == 1 ? " rating" : " ratings");
    return result;
}

SizeF RatingDisplay::measure(const Constraints& constraints) const
{
    const Theme& current = theme();
    const float size = itemSize(size_);
    const int items = compact_ ? 1 : maximum_;
    const float labelSize = size_ == RatingSize::Large ? current.typography.fontSizeBase300
        : size_ == RatingSize::ExtraLarge ? current.typography.fontSizeBase400
        : current.typography.fontSizeBase200;
    float width = itemsWidth(items, size_) + labelGap(size_);
    width += textRunWidth(valueText(), labelSize,
                          current.typography.weightSemibold);
    if (count_) {
        width += labelGap(size_) +
            textRunWidth("· " + countText(), labelSize,
                         current.typography.weightRegular);
    }
    return constraints.clamp(
        {width, std::max(rowHeight(size_), labelSize * 1.35f)});
}

void RatingDisplay::paint(PaintContext& context)
{
    const Theme& current = theme();
    const float size = itemSize(size_);
    const int items = compact_ ? 1 : maximum_;
    const float shown = compact_ ? 1.0f : std::round(*value_ * 2.0f) * 0.5f;
    const Color active = selectedColor(current, color_);
    for (int index = 0; index < items; ++index) {
        const float fraction = compact_ ? 1.0f
            : std::clamp(shown - static_cast<float>(index), 0.0f, 1.0f);
        drawItem(context, {bounds().x + itemOffset(index, size_),
                           bounds().y + (bounds().height - size) * 0.5f, size, size},
                 shape_, fraction, active, false);
    }
    const float labelSize = size_ == RatingSize::Large ? current.typography.fontSizeBase300
        : size_ == RatingSize::ExtraLarge ? current.typography.fontSizeBase400
        : current.typography.fontSizeBase200;
    float textX =
        bounds().x + itemsWidth(items, size_) + labelGap(size_);
    const float textBottom = context.centeredTextBottom(valueText(), bounds(), labelSize,
                                                         current.typography.weightSemibold);
    context.drawText(valueText(), textX, textBottom, labelSize,
                     current.colors.neutralForeground1, current.typography.weightSemibold);
    textX += textRunWidth(valueText(), labelSize,
                          current.typography.weightSemibold);
    if (count_) {
        textX += labelGap(size_);
        const std::string count = "· " + countText();
        context.drawText(count, textX, textBottom, labelSize, current.colors.neutralForeground1);
    }
    clearDirty(DirtyFlag::Paint);
}

} // namespace wui
