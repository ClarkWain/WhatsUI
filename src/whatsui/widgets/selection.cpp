#include "wui/selection.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <memory>
#include <utility>

#include "wui/runtime.h"
#include "wui/icons.h"
#include "wui/text_metrics.h"
#include "wui/theme.h"

namespace wui {
namespace {

constexpr float kOptionPadding = 12.0f;
constexpr float kListPadding = 4.0f;
constexpr float kMinimumListWidth = 180.0f;

[[nodiscard]] bool isPrimary(const PointerEvent& event) noexcept
{
    return event.button == MouseButton::Left;
}

[[nodiscard]] bool isActivationKey(const KeyEvent& event) noexcept
{
    return event.action == KeyAction::Down &&
           (event.keyCode == 13 || event.keyCode == 32 || event.keyCode == 257);
}

[[nodiscard]] bool isEscape(const KeyEvent& event) noexcept
{
    return event.action == KeyAction::Down && (event.keyCode == 27 || event.keyCode == 256);
}

[[nodiscard]] std::string lowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

[[nodiscard]] float textWidth(const std::string& text, float size) noexcept
{
    if (const auto* measurer = textMeasurer()) return measurer->measureText(text, size).width;
    std::size_t characters = 0;
    for (const auto c : text) if ((static_cast<unsigned char>(c) & 0xc0u) != 0x80u) ++characters;
    return static_cast<float>(characters) * size * 0.55f;
}

void drawFocusRing(PaintContext& context, const RectF& rect, const Theme& current, bool focused)
{
    if (!focused) return;
    const float inset = current.controls.focusInset;
    context.strokeRoundRect(
        {rect.x - inset, rect.y - inset,
         rect.width + inset * 2.0f, rect.height + inset * 2.0f},
        current.radius.medium + inset, current.stroke.thick,
        current.colors.strokeFocusOuter);
    const float inner =
        std::max(0.0f, inset - current.stroke.thin * 0.5f);
    context.strokeRoundRect(
        {rect.x - inner, rect.y - inner,
         rect.width + inner * 2.0f, rect.height + inner * 2.0f},
        current.radius.medium + inner, current.stroke.thin,
        current.colors.strokeFocusInner);
}

void drawChevron(PaintContext& context, float cx, float cy, Color color, bool up)
{
    drawIcon(context, up ? IconName::ChevronUp : IconName::ChevronDown,
             {cx - 8.0f, cy - 8.0f, 16.0f, 16.0f}, color,
             IconSize::Size16);
}

[[nodiscard]] std::unique_ptr<Popup> makeListPopup(
    const RectF& anchor, float width, std::unique_ptr<ListBox> list, Popup::DismissHandler dismiss)
{
    auto popup = std::make_unique<Popup>();
    popup->anchor(anchor)
        .placement(PopupPlacement::BelowStart)
        .preferredSize({width, 0.0f})
        .onDismiss(std::move(dismiss));
    popup->content(std::move(list));
    return popup;
}

} // namespace

ListBox::ListBox(std::vector<Option> options)
    : options_(std::move(options))
{
    activeIndex_ = nextSelectable(-1, 1);
}

ListBox& ListBox::addOption(Option option)
{
    options_.push_back(std::move(option));
    if (activeIndex_ < 0) activeIndex_ = nextSelectable(-1, 1);
    markDirty(DirtyFlag::Layout);
    return *this;
}

ListBox& ListBox::setOptions(std::vector<Option> options)
{
    options_ = std::move(options);
    selected_.erase(std::remove_if(selected_.begin(), selected_.end(), [this](int i) { return !selectable(i); }), selected_.end());
    activeIndex_ = selectable(activeIndex_) ? activeIndex_ : nextSelectable(-1, 1);
    hoveredIndex_ = pressedIndex_ = -1;
    setScrollOffset(scrollOffset_);
    markDirty(DirtyFlag::Layout);
    return *this;
}

ListBox& ListBox::clearOptions()
{
    options_.clear(); selected_.clear(); activeIndex_ = hoveredIndex_ = pressedIndex_ = -1;
    scrollOffset_ = 0.0f;
    markDirty(DirtyFlag::Layout);
    return *this;
}

const std::vector<Option>& ListBox::options() const noexcept { return options_; }
ListBox& ListBox::selectionMode(ListBoxSelectionMode value) noexcept { setSelectionMode(value); return *this; }
void ListBox::setSelectionMode(ListBoxSelectionMode value) noexcept
{
    if (selectionMode_ == value) return;
    selectionMode_ = value;
    if (selectionMode_ == ListBoxSelectionMode::Single && selected_.size() > 1) selected_.resize(1);
    markDirty(DirtyFlag::Paint);
}
ListBoxSelectionMode ListBox::selectionMode() const noexcept { return selectionMode_; }
int ListBox::selectedIndex() const noexcept { return selected_.empty() ? -1 : selected_.front(); }
const std::vector<int>& ListBox::selectedIndices() const noexcept { return selected_; }
ListBox& ListBox::selectedIndex(int index) { setSelectedIndex(index); return *this; }
void ListBox::setSelectedIndex(int index)
{
    selected_.clear();
    if (selectable(index)) { selected_.push_back(index); activeIndex_ = index; }
    markDirty(DirtyFlag::Paint);
}
ListBox& ListBox::selectedIndices(std::vector<int> indices) { setSelectedIndices(std::move(indices)); return *this; }
void ListBox::setSelectedIndices(std::vector<int> indices)
{
    selected_.clear();
    for (const int index : indices) {
        if (!selectable(index) || std::find(selected_.begin(), selected_.end(), index) != selected_.end()) continue;
        selected_.push_back(index);
        if (selectionMode_ == ListBoxSelectionMode::Single) break;
    }
    if (!selected_.empty()) activeIndex_ = selected_.front();
    markDirty(DirtyFlag::Paint);
}
int ListBox::activeIndex() const noexcept { return activeIndex_; }
void ListBox::setActiveIndex(int index) { activeIndex_ = selectable(index) ? index : -1; markDirty(DirtyFlag::Paint); }
ListBox& ListBox::onSelectionChanged(SelectionHandler handler) { onSelectionChanged_ = std::move(handler); return *this; }
ListBox& ListBox::accessibleLabel(std::string value) { setAccessibleLabel(std::move(value)); return *this; }
void ListBox::setAccessibleLabel(std::string value) { accessibleLabel_ = std::move(value); markDirty(DirtyFlag::Paint); }
const std::string& ListBox::accessibleLabel() const noexcept { return accessibleLabel_; }
ListBox& ListBox::maxVisibleOptions(std::size_t value) noexcept { setMaxVisibleOptions(value); return *this; }
void ListBox::setMaxVisibleOptions(std::size_t value) noexcept { maxVisibleOptions_ = std::max<std::size_t>(1, value); markDirty(DirtyFlag::Layout); }
std::size_t ListBox::maxVisibleOptions() const noexcept { return maxVisibleOptions_; }
float ListBox::scrollOffset() const noexcept { return scrollOffset_; }
void ListBox::setScrollOffset(float value) noexcept
{
    const float finite = std::isfinite(value) ? value : 0.0f;
    const float next = std::clamp(finite, 0.0f, maximumScrollOffset());
    if (scrollOffset_ != next) { scrollOffset_ = next; markDirty(DirtyFlag::Paint); }
}
float ListBox::maximumScrollOffset() const noexcept
{
    const float viewport = std::max(0.0f, bounds().height - kListPadding * 2.0f - theme().stroke.thin * 2.0f);
    return std::max(0.0f, rowHeight() * static_cast<float>(options_.size()) - viewport);
}

std::vector<ListBoxOptionAccessibility> ListBox::accessibilityOptions() const
{
    std::vector<ListBoxOptionAccessibility> result;
    result.reserve(options_.size());
    for (std::size_t index = 0; index < options_.size(); ++index) {
        ListBoxOptionAccessibility semantic;
        semantic.index = index;
        semantic.properties.role = AccessibilityRole::Option;
        semantic.properties.label = options_[index].text;
        semantic.properties.description = options_[index].secondaryText;
        semantic.properties.value = options_[index].value;
        semantic.properties.enabled = isEnabled() && options_[index].enabled;
        semantic.properties.checked = isSelected(static_cast<int>(index));
        semantic.properties.actions.invoke = semantic.properties.enabled;
        const RectF row = optionBounds(static_cast<int>(index));
        if (row.y + row.height > bounds().y && row.y < bounds().y + bounds().height) semantic.properties.bounds = row;
        result.push_back(std::move(semantic));
    }
    return result;
}

float ListBox::rowHeight() const noexcept
{
    const bool hasSecondary = std::any_of(options_.begin(), options_.end(), [](const Option& option) {
        return !option.secondaryText.empty();
    });
    // Fluent's secondary-content option is a two-line row. Use a uniform
    // height for the list so keyboard/pointer hit regions remain stable.
    return std::max(hasSecondary ? 52.0f : 32.0f, theme().controls.height);
}
float ListBox::preferredWidth() const noexcept
{
    float width = kMinimumListWidth;
    for (const auto& option : options_) {
        width = std::max(width, textWidth(option.text, theme().typography.body1.size) + kOptionPadding * 2.0f + 20.0f);
        if (!option.secondaryText.empty()) width = std::max(width, textWidth(option.secondaryText, theme().typography.caption1.size) + kOptionPadding * 2.0f + 20.0f);
    }
    return width;
}
SizeF ListBox::measure(const Constraints& constraints) const
{
    const std::size_t visible = std::min(options_.size(), maxVisibleOptions_);
    return constraints.clamp({preferredWidth(), kListPadding * 2.0f + rowHeight() * static_cast<float>(visible)});
}
void ListBox::layout(const RectF& bounds)
{
    Node::layout(bounds);
    // A selected value must be discoverable the instant a popup opens.  The
    // viewport dimensions are only known after layout, so doing this here
    // avoids Dropdown/TimePicker lists opening at 00:00 while their committed
    // selection is several pages away.
    scrollActiveIntoView();
    clearLayoutDirtyRecursively();
}

void ListBox::paint(PaintContext& context)
{
    const auto& current = theme();
    const bool focused = (visualStates() & toMask(ControlVisualState::Focused)) != 0;
    drawFocusRing(context, bounds(), current, focused);
    const RectF content{bounds().x + current.stroke.thin, bounds().y + current.stroke.thin,
                        std::max(0.0f, bounds().width - current.stroke.thin * 2.0f),
                        std::max(0.0f, bounds().height - current.stroke.thin * 2.0f)};
    context.fillStrokeRoundRect(bounds(), current.radius.medium,
                                current.stroke.thin,
                                current.colors.surfaceRaised,
                                current.colors.neutralStroke1);
    const int checkpoint = context.save(); context.clipRect(content);
    const int first = std::max(0, static_cast<int>(std::floor(scrollOffset_ / rowHeight())));
    for (std::size_t i = static_cast<std::size_t>(first); i < options_.size(); ++i) {
        const RectF row = optionBounds(static_cast<int>(i));
        if (row.y >= content.y + content.height) break;
        const bool enabled = isEnabled() && options_[i].enabled;
        const bool selected = isSelected(static_cast<int>(i));
        Color fill = current.colors.neutralBackground1.rest;
        if (selected) fill = current.colors.neutralBackground1.selected;
        if (enabled && static_cast<int>(i) == hoveredIndex_) fill = selected ? current.colors.neutralBackground2.hover : current.colors.neutralBackground1.hover;
        if (enabled && static_cast<int>(i) == pressedIndex_) fill = selected ? current.colors.neutralBackground2.pressed : current.colors.neutralBackground1.pressed;
        if (static_cast<int>(i) == activeIndex_ && focused) fill = selected ? current.colors.neutralBackground2.hover : current.colors.neutralBackground1.hover;
        context.fillRoundRect(row, current.radius.medium, fill);
        const Color fg = enabled ? current.colors.neutralForeground1
                                 : current.colors.neutralForegroundDisabled;
        // PaintContext::drawText takes the text baseline.  The two-line
        // option therefore needs explicit baseline positions rather than a
        // top-edge offset, otherwise the first glyph is clipped by the list
        // viewport at 150% DPI.
        if (options_[i].secondaryText.empty()) {
            context.drawText(
                options_[i].text, row.x + kOptionPadding,
                context.centeredTextBottom(
                    options_[i].text, row,
                    current.typography.body1.size,
                    current.typography.body1.weight,
                    current.typography.body1.family),
                current.typography.body1.size, fg,
                current.typography.body1.weight,
                current.typography.body1.family);
        } else {
            const float blockHeight =
                current.typography.body1.lineHeight +
                current.typography.caption1.lineHeight;
            const float top =
                row.y + std::max(0.0f, (row.height - blockHeight) * 0.5f);
            const RectF primaryBox{
                row.x, top, row.width,
                current.typography.body1.lineHeight};
            const RectF secondaryBox{
                row.x, top + current.typography.body1.lineHeight,
                row.width, current.typography.caption1.lineHeight};
            context.drawText(
                options_[i].text, row.x + kOptionPadding,
                context.centeredTextBottom(
                    options_[i].text, primaryBox,
                    current.typography.body1.size,
                    current.typography.body1.weight,
                    current.typography.body1.family),
                current.typography.body1.size, fg,
                current.typography.body1.weight,
                current.typography.body1.family);
            context.drawText(
                options_[i].secondaryText, row.x + kOptionPadding,
                context.centeredTextBottom(
                    options_[i].secondaryText, secondaryBox,
                    current.typography.caption1.size,
                    current.typography.caption1.weight,
                    current.typography.caption1.family),
                current.typography.caption1.size,
                current.colors.neutralForeground2,
                current.typography.caption1.weight,
                current.typography.caption1.family);
        }
        if (selected)
            drawIcon(context, IconName::Checkmark,
                     {row.x + row.width - 28.0f,
                      row.y + (row.height - 20.0f) * 0.5f, 20.0f, 20.0f},
                     current.colors.brandForeground1, IconSize::Size20);
    }
    context.restoreTo(checkpoint);
    clearDirty(DirtyFlag::Paint);
}

bool ListBox::onPointerEvent(const PointerEvent& event)
{
    if (!isEnabled()) return false;
    const int option = optionAt(event.position);
    switch (event.action) {
    case PointerAction::Enter: case PointerAction::Move: hoveredIndex_ = option; markDirty(DirtyFlag::Paint); return true;
    case PointerAction::Leave: hoveredIndex_ = -1; markDirty(DirtyFlag::Paint); return true;
    case PointerAction::Down:
        if (!isPrimary(event)) return false; pressedIndex_ = option; setVisualState(ControlVisualState::Pressed, true); markDirty(DirtyFlag::Paint); return true;
    case PointerAction::Up:
        if (!isPrimary(event)) return false;
        { const int pressed = pressedIndex_; pressedIndex_ = -1; setVisualState(ControlVisualState::Pressed, false);
          if (pressed >= 0 && pressed == option && selectable(option)) choose(option, selectionMode_ == ListBoxSelectionMode::Multiple); }
        markDirty(DirtyFlag::Paint); return true;
    case PointerAction::Cancel: pressedIndex_ = -1; setVisualState(ControlVisualState::Pressed, false); markDirty(DirtyFlag::Paint); return true;
    case PointerAction::Scroll:
        setScrollOffset(scrollOffset_ - event.scrollDelta.y);
        return true;
    default: return false;
    }
}

bool ListBox::onKeyEvent(const KeyEvent& event)
{
    if (!isEnabled() || event.action != KeyAction::Down) return false;
    int next = -1;
    switch (event.keyCode) {
    case 38: case 265: next = nextSelectable(activeIndex_ < 0 ? static_cast<int>(options_.size()) : activeIndex_, -1); break;
    case 40: case 264: next = nextSelectable(activeIndex_, 1); break;
    case 36: case 268: next = nextSelectable(-1, 1); break;
    case 35: case 269: next = nextSelectable(static_cast<int>(options_.size()), -1); break;
    case 32: case 13: case 257:
        if (selectable(activeIndex_)) choose(activeIndex_, selectionMode_ == ListBoxSelectionMode::Multiple && event.keyCode == 32);
        return true;
    default:
        if (event.keyCode >= 33 && event.keyCode <= 126 && !event.modifiers) {
            updateTypeAhead(static_cast<char>(event.keyCode));
            return true;
        }
        return false;
    }
    if (next >= 0) { setActiveIndex(next); scrollActiveIntoView(); }
    return true;
}

AccessibilityActionCapabilities ListBox::accessibilityActions() const noexcept
{
    AccessibilityActionCapabilities actions; actions.setValue = true; return actions;
}
AccessibilityActionStatus ListBox::performAccessibilityAction(AccessibilityActionKind kind, std::string_view value)
{
    if (!isEnabled()) return AccessibilityActionStatus::ElementNotEnabled;
    if (kind != AccessibilityActionKind::SetValue) return AccessibilityActionStatus::NotSupported;
    for (std::size_t i = 0; i < options_.size(); ++i) if (options_[i].value == value) { choose(static_cast<int>(i)); return AccessibilityActionStatus::Succeeded; }
    return AccessibilityActionStatus::InvalidValue;
}
bool ListBox::selectable(int index) const noexcept { return index >= 0 && static_cast<std::size_t>(index) < options_.size() && options_[static_cast<std::size_t>(index)].enabled; }
int ListBox::nextSelectable(int from, int delta) const noexcept
{
    for (int i = from + delta; i >= 0 && static_cast<std::size_t>(i) < options_.size(); i += delta) if (selectable(i)) return i;
    return -1;
}
int ListBox::optionAt(PointF point) const noexcept
{
    if (!bounds().contains(point)) return -1;
    const int i = static_cast<int>((point.y - bounds().y - theme().stroke.thin - kListPadding + scrollOffset_) / rowHeight());
    return selectable(i) ? i : -1;
}
bool ListBox::isSelected(int index) const noexcept { return std::find(selected_.begin(), selected_.end(), index) != selected_.end(); }
RectF ListBox::optionBounds(int index) const noexcept
{
    const float inset = theme().stroke.thin;
    return {bounds().x + inset + kListPadding,
            bounds().y + inset + kListPadding + rowHeight() * static_cast<float>(index) - scrollOffset_,
            std::max(0.0f, bounds().width - inset * 2.0f - kListPadding * 2.0f), rowHeight()};
}
void ListBox::scrollActiveIntoView() noexcept
{
    if (!selectable(activeIndex_)) return;
    const float viewportTop = bounds().y + theme().stroke.thin + kListPadding;
    const float viewportBottom = bounds().y + bounds().height - theme().stroke.thin - kListPadding;
    const RectF row = optionBounds(activeIndex_);
    if (row.y < viewportTop) setScrollOffset(scrollOffset_ - (viewportTop - row.y));
    else if (row.y + row.height > viewportBottom) setScrollOffset(scrollOffset_ + (row.y + row.height - viewportBottom));
}
void ListBox::updateTypeAhead(char character)
{
    const auto now = std::chrono::steady_clock::now();
    const char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    if (now - lastTypeAhead_ > std::chrono::milliseconds(700)) typeAheadPrefix_.clear();
    if (typeAheadPrefix_.size() == 1 && typeAheadPrefix_.front() == lower) {
        // Repeating a letter cycles matching options, as native listboxes do.
    } else typeAheadPrefix_.push_back(lower);
    lastTypeAhead_ = now;
    if (options_.empty()) return;
    const int count = static_cast<int>(options_.size());
    for (int offset = 1; offset <= count; ++offset) {
        const int index = (std::max(activeIndex_, -1) + offset) % count;
        if (selectable(index) && lowerAscii(options_[static_cast<std::size_t>(index)].text).rfind(typeAheadPrefix_, 0) == 0) {
            setActiveIndex(index); scrollActiveIntoView(); return;
        }
    }
}
void ListBox::choose(int index, bool toggle)
{
    if (!selectable(index)) return;
    activeIndex_ = index;
    if (selectionMode_ == ListBoxSelectionMode::Multiple && toggle) {
        const auto it = std::find(selected_.begin(), selected_.end(), index);
        if (it == selected_.end()) selected_.push_back(index); else selected_.erase(it);
    } else { selected_.assign(1, index); }
    scrollActiveIntoView();
    markDirty(DirtyFlag::Paint);
    if (onSelectionChanged_) onSelectionChanged_(index, options_[static_cast<std::size_t>(index)]);
}

Combobox::Combobox(std::string placeholder) : TextInput(std::move(placeholder))
{
    TextInput::onChange([this](const std::string& value) {
        if (!updatingText_) refreshPopup();
        if (userOnChange_) userOnChange_(value);
    });
}
Combobox::~Combobox() { closePopup(); }
Combobox& Combobox::addOption(Option option) { options_.push_back(std::move(option)); refreshPopup(); return *this; }
Combobox& Combobox::setOptions(std::vector<Option> options) { options_ = std::move(options); selectedIndices_.erase(std::remove_if(selectedIndices_.begin(), selectedIndices_.end(), [this](int index) { return index < 0 || static_cast<std::size_t>(index) >= options_.size() || !options_[static_cast<std::size_t>(index)].enabled; }), selectedIndices_.end()); selectedIndex_ = selectedIndices_.empty() ? -1 : selectedIndices_.front(); refreshPopup(); return *this; }
Combobox& Combobox::clearOptions() { options_.clear(); selectedIndex_ = -1; selectedIndices_.clear(); closePopup(); return *this; }
const std::vector<Option>& Combobox::options() const noexcept { return options_; }
int Combobox::selectedIndex() const noexcept { return selectedIndex_; }
const std::vector<int>& Combobox::selectedIndices() const noexcept { return selectedIndices_; }
Combobox& Combobox::selectedIndex(int index) { setSelectedIndex(index); return *this; }
void Combobox::setSelectedIndex(int index) { if (index >= 0 && static_cast<std::size_t>(index) < options_.size() && options_[static_cast<std::size_t>(index)].enabled) commit(index); else if (index < 0) { selectedIndex_ = -1; selectedIndices_.clear(); } }
Combobox& Combobox::multiselect(bool value) noexcept { setMultiselect(value); return *this; }
void Combobox::setMultiselect(bool value) noexcept
{
    if (multiselect_ == value) return;
    multiselect_ = value;
    if (!multiselect_ && selectedIndices_.size() > 1) selectedIndices_.resize(1);
    selectedIndex_ = selectedIndices_.empty() ? -1 : selectedIndices_.front();
    markDirty(DirtyFlag::Paint);
}
bool Combobox::isMultiselect() const noexcept { return multiselect_; }
Combobox& Combobox::selectedIndices(std::vector<int> indices) { setSelectedIndices(std::move(indices)); return *this; }
void Combobox::setSelectedIndices(std::vector<int> indices)
{
    std::vector<int> accepted;
    for (const int index : indices) {
        if (index < 0 || static_cast<std::size_t>(index) >= options_.size() || !options_[static_cast<std::size_t>(index)].enabled || std::find(accepted.begin(), accepted.end(), index) != accepted.end()) continue;
        accepted.push_back(index); if (!multiselect_) break;
    }
    const int changed = accepted.empty() ? -1 : accepted.front();
    commitSelection(std::move(accepted), changed);
}
bool Combobox::isOpen() const noexcept { return open_; }
Combobox& Combobox::bindOverlayHost(OverlayHost& host) noexcept { overlayHost_ = &host; return *this; }
Combobox& Combobox::onSelectionChanged(SelectionHandler handler) { onSelectionChanged_ = std::move(handler); return *this; }
Combobox& Combobox::onChange(ChangeHandler handler) { userOnChange_ = std::move(handler); return *this; }
Combobox& Combobox::openOnFocus(bool value) noexcept { setOpenOnFocus(value); return *this; }
void Combobox::setOpenOnFocus(bool value) noexcept { openOnFocus_ = value; }
EventResult Combobox::onPointerEvent(const PointerEvent& event, EventContext& context)
{
    const EventResult handled = TextInput::onPointerEvent(event, context);
    if (event.action == PointerAction::Down && isPrimary(event) && bounds().contains(event.position)) openPopup();
    return handled;
}
bool Combobox::onKeyEvent(const KeyEvent& event)
{
    if (isEscape(event) && open_) { closePopup(); return true; }
    if (event.action == KeyAction::Down && (event.keyCode == 40 || event.keyCode == 264 || event.keyCode == 38 || event.keyCode == 265)) { openPopup(); return true; }
    return TextInput::onKeyEvent(event);
}
bool Combobox::onTextInput(const TextInputEvent& event) { const bool result = TextInput::onTextInput(event); if (result) refreshPopup(); return result; }
AccessibilityActionCapabilities Combobox::accessibilityActions() const noexcept { auto actions = TextInput::accessibilityActions(); actions.expandCollapse = overlayHost_ != nullptr && !options_.empty(); return actions; }
AccessibilityActionStatus Combobox::performAccessibilityAction(AccessibilityActionKind kind, std::string_view value)
{
    if (kind == AccessibilityActionKind::Expand) { openPopup(); return open_ ? AccessibilityActionStatus::Succeeded : AccessibilityActionStatus::Failed; }
    if (kind == AccessibilityActionKind::Collapse) { closePopup(); return AccessibilityActionStatus::Succeeded; }
    return TextInput::performAccessibilityAction(kind, value);
}
std::vector<int> Combobox::filteredIndices() const
{
    const std::string filter = lowerAscii(controller().text()); std::vector<int> result;
    for (std::size_t i = 0; i < options_.size(); ++i) {
        const auto& option = options_[i];
        if (!option.enabled) continue;
        if (filter.empty() || lowerAscii(option.text).find(filter) != std::string::npos || lowerAscii(option.secondaryText).find(filter) != std::string::npos) result.push_back(static_cast<int>(i));
    }
    return result;
}
int Combobox::sourceIndexForVisible(int visibleIndex) const noexcept { return visibleIndex >= 0 && static_cast<std::size_t>(visibleIndex) < visibleIndices_.size() ? visibleIndices_[static_cast<std::size_t>(visibleIndex)] : -1; }
void Combobox::openPopup()
{
    if (open_ || overlayHost_ == nullptr || options_.empty()) return;
    visibleIndices_ = filteredIndices();
    auto list = std::make_unique<ListBox>(); ListBox* raw = list.get();
    for (const int index : visibleIndices_) list->addOption(options_[static_cast<std::size_t>(index)]);
    const auto selected = std::find(visibleIndices_.begin(), visibleIndices_.end(), selectedIndex_);
    list->setSelectionMode(multiselect_ ? ListBoxSelectionMode::Multiple : ListBoxSelectionMode::Single);
    std::vector<int> visibleSelected;
    for (std::size_t visible = 0; visible < visibleIndices_.size(); ++visible)
        if (std::find(selectedIndices_.begin(), selectedIndices_.end(), visibleIndices_[visible]) != selectedIndices_.end()) visibleSelected.push_back(static_cast<int>(visible));
    if (multiselect_) list->setSelectedIndices(std::move(visibleSelected));
    else list->setSelectedIndex(selected == visibleIndices_.end() ? -1 : static_cast<int>(selected - visibleIndices_.begin()));
    list->setAccessibleLabel(accessibleLabel());
    list->onSelectionChanged([this, raw](int visible, const Option&) {
        const int source = sourceIndexForVisible(visible); if (source < 0) return;
        if (multiselect_) {
            std::vector<int> selections;
            for (const int row : raw->selectedIndices()) { const int sourceRow = sourceIndexForVisible(row); if (sourceRow >= 0) selections.push_back(sourceRow); }
            commitSelection(std::move(selections), source);
        } else { commit(source); closePopup(); }
    });
    open_ = true; overlayId_ = overlayHost_->show(makeListPopup(bounds(), std::max(bounds().width, kMinimumListWidth), std::move(list), [this] { closePopup(); }));
    overlayHost_->focus(raw); markDirty(DirtyFlag::Paint);
}
void Combobox::closePopup()
{
    if (!open_) return; OverlayHost* host = overlayHost_; const auto id = overlayId_; open_ = false; overlayId_ = 0;
    if (host && id) { (void)host->dismiss(id); host->focus(this); } markDirty(DirtyFlag::Paint);
}
void Combobox::refreshPopup() { if (open_) { closePopup(); openPopup(); } }
void Combobox::commit(int sourceIndex)
{
    if (sourceIndex < 0 || static_cast<std::size_t>(sourceIndex) >= options_.size() || !options_[static_cast<std::size_t>(sourceIndex)].enabled) return;
    commitSelection({sourceIndex}, sourceIndex);
}
void Combobox::commitSelection(std::vector<int> sourceIndices, int changedSourceIndex)
{
    selectedIndices_ = std::move(sourceIndices);
    if (!multiselect_ && selectedIndices_.size() > 1) selectedIndices_.resize(1);
    selectedIndex_ = selectedIndices_.empty() ? -1 : selectedIndices_.front();
    updatingText_ = true;
    if (!multiselect_ && selectedIndex_ >= 0) TextInput::text(options_[static_cast<std::size_t>(selectedIndex_)].text);
    else if (multiselect_) TextInput::text({});
    updatingText_ = false;
    if (changedSourceIndex >= 0 && static_cast<std::size_t>(changedSourceIndex) < options_.size() && onSelectionChanged_)
        onSelectionChanged_(changedSourceIndex, options_[static_cast<std::size_t>(changedSourceIndex)]);
    markDirty(DirtyFlag::Paint);
}
Dropdown::Dropdown(std::string placeholder) : placeholder_(std::move(placeholder)) {}
Dropdown::~Dropdown() { closePopup(); }
Dropdown& Dropdown::addOption(Option option) { options_.push_back(std::move(option)); markDirty(DirtyFlag::Layout); return *this; }
Dropdown& Dropdown::setOptions(std::vector<Option> options) { options_ = std::move(options); selectedIndices_.erase(std::remove_if(selectedIndices_.begin(), selectedIndices_.end(), [this](int index) { return !selectable(index); }), selectedIndices_.end()); selectedIndex_ = selectedIndices_.empty() ? -1 : selectedIndices_.front(); markDirty(DirtyFlag::Layout); return *this; }
Dropdown& Dropdown::clearOptions() { options_.clear(); selectedIndex_ = -1; selectedIndices_.clear(); closePopup(); markDirty(DirtyFlag::Layout); return *this; }
const std::vector<Option>& Dropdown::options() const noexcept { return options_; }
int Dropdown::selectedIndex() const noexcept { return selectedIndex_; }
const std::vector<int>& Dropdown::selectedIndices() const noexcept { return selectedIndices_; }
Dropdown& Dropdown::selectedIndex(int index) { setSelectedIndex(index); return *this; }
void Dropdown::setSelectedIndex(int index) { if (selectable(index)) commit(index); else if (index < 0) { selectedIndex_ = -1; selectedIndices_.clear(); markDirty(DirtyFlag::Paint); } }
Dropdown& Dropdown::multiselect(bool value) noexcept { setMultiselect(value); return *this; }
void Dropdown::setMultiselect(bool value) noexcept { if (multiselect_ == value) return; multiselect_ = value; if (!multiselect_ && selectedIndices_.size() > 1) selectedIndices_.resize(1); selectedIndex_ = selectedIndices_.empty() ? -1 : selectedIndices_.front(); markDirty(DirtyFlag::Paint); }
bool Dropdown::isMultiselect() const noexcept { return multiselect_; }
Dropdown& Dropdown::selectedIndices(std::vector<int> indices) { setSelectedIndices(std::move(indices)); return *this; }
void Dropdown::setSelectedIndices(std::vector<int> indices)
{
    std::vector<int> accepted;
    for (const int index : indices) { if (!selectable(index) || std::find(accepted.begin(), accepted.end(), index) != accepted.end()) continue; accepted.push_back(index); if (!multiselect_) break; }
    commitSelection(std::move(accepted), -1);
}
const std::string& Dropdown::value() const noexcept { return selectable(selectedIndex_) ? options_[static_cast<std::size_t>(selectedIndex_)].value : placeholder_; }
const std::string& Dropdown::placeholder() const noexcept { return placeholder_; }
bool Dropdown::isOpen() const noexcept { return open_; }
Dropdown& Dropdown::bindOverlayHost(OverlayHost& host) noexcept { overlayHost_ = &host; return *this; }
Dropdown& Dropdown::onSelectionChanged(SelectionHandler handler) { onSelectionChanged_ = std::move(handler); return *this; }
Dropdown& Dropdown::accessibleLabel(std::string value) { setAccessibleLabel(std::move(value)); return *this; }
void Dropdown::setAccessibleLabel(std::string value) { accessibleLabel_ = std::move(value); }
const std::string& Dropdown::accessibleLabel() const noexcept { return accessibleLabel_; }
SizeF Dropdown::measure(const Constraints& constraints) const
{
    const float width = std::max(
        160.0f,
        textWidth(value(), theme().typography.body1.size) + 48.0f);
    return constraints.clamp({width, theme().controls.height});
}
void Dropdown::paint(PaintContext& context)
{
    const auto& current = theme(); const bool focused = (visualStates() & toMask(ControlVisualState::Focused)) != 0;
    drawFocusRing(context, bounds(), current, focused);
    Color fill = current.colors.neutralBackground1.rest;
    if (!isEnabled()) fill = current.colors.neutralBackground3.rest;
    else if (open_ || (visualStates() & toMask(ControlVisualState::Pressed))) fill = current.colors.neutralBackground1.pressed;
    else if (visualStates() & toMask(ControlVisualState::Hovered)) fill = current.colors.neutralBackground1.hover;
    context.fillStrokeRoundRect(bounds(), current.radius.medium,
                                current.stroke.thin, fill,
                                isEnabled()
                                    ? current.colors.neutralStroke1
                                    : current.colors.neutralStrokeDisabled);
    const bool selected = !selectedIndices_.empty();
    const Color fg = !isEnabled()
        ? current.colors.neutralForegroundDisabled
        : selected ? current.colors.neutralForeground1
                   : current.colors.neutralForeground3;
    std::string display = placeholder_;
    if (selected) {
        display = options_[static_cast<std::size_t>(selectedIndices_.front())].text;
        if (multiselect_ && selectedIndices_.size() > 1) display += " +" + std::to_string(selectedIndices_.size() - 1);
    }
    context.drawText(
        display, bounds().x + kOptionPadding,
        context.centeredTextBottom(
            display, bounds(), current.typography.body1.size,
            current.typography.body1.weight,
            current.typography.body1.family),
        current.typography.body1.size, fg,
        current.typography.body1.weight,
        current.typography.body1.family);
    drawChevron(context, bounds().x + bounds().width - 18.0f, bounds().y + bounds().height * .5f, fg, open_); clearDirty(DirtyFlag::Paint);
}
bool Dropdown::onPointerEvent(const PointerEvent& event)
{
    if (!isEnabled()) return false;
    if (event.action == PointerAction::Enter) { setVisualState(ControlVisualState::Hovered, true); markDirty(DirtyFlag::Paint); return true; }
    if (event.action == PointerAction::Leave) { setVisualState(ControlVisualState::Hovered, false); markDirty(DirtyFlag::Paint); return true; }
    if (event.action == PointerAction::Down && isPrimary(event)) { setVisualState(ControlVisualState::Pressed, true); markDirty(DirtyFlag::Paint); return true; }
    if (event.action == PointerAction::Up && isPrimary(event)) { setVisualState(ControlVisualState::Pressed, false); if (bounds().contains(event.position)) openPopup(); markDirty(DirtyFlag::Paint); return true; }
    if (event.action == PointerAction::Cancel) { setVisualState(ControlVisualState::Pressed, false); markDirty(DirtyFlag::Paint); return true; }
    return false;
}
bool Dropdown::onKeyEvent(const KeyEvent& event)
{
    if (!isEnabled()) return false;
    if (isEscape(event) && open_) { closePopup(); return true; }
    if (isActivationKey(event) || (event.action == KeyAction::Down && (event.keyCode == 40 || event.keyCode == 264))) { openPopup(); return true; }
    if (event.action == KeyAction::Down && (event.keyCode == 38 || event.keyCode == 265)) { const int next = nextSelectable(selectedIndex_ < 0 ? static_cast<int>(options_.size()) : selectedIndex_, -1); if (next >= 0) commit(next); return true; }
    return false;
}
AccessibilityActionCapabilities Dropdown::accessibilityActions() const noexcept { AccessibilityActionCapabilities actions; actions.expandCollapse = overlayHost_ != nullptr && !options_.empty(); return actions; }
AccessibilityActionStatus Dropdown::performAccessibilityAction(AccessibilityActionKind kind, std::string_view value)
{
    if (!isEnabled()) return AccessibilityActionStatus::ElementNotEnabled;
    if (kind == AccessibilityActionKind::Expand) { openPopup(); return open_ ? AccessibilityActionStatus::Succeeded : AccessibilityActionStatus::Failed; }
    if (kind == AccessibilityActionKind::Collapse) { closePopup(); return AccessibilityActionStatus::Succeeded; }
    if (kind == AccessibilityActionKind::SetValue) { for (std::size_t i = 0; i < options_.size(); ++i) if (options_[i].value == value) { commit(static_cast<int>(i)); return AccessibilityActionStatus::Succeeded; } return AccessibilityActionStatus::InvalidValue; }
    return AccessibilityActionStatus::NotSupported;
}
void Dropdown::openPopup()
{
    if (open_ || overlayHost_ == nullptr || options_.empty()) return;
    auto list = std::make_unique<ListBox>(options_); ListBox* raw = list.get(); list->setSelectionMode(multiselect_ ? ListBoxSelectionMode::Multiple : ListBoxSelectionMode::Single); if (multiselect_) list->setSelectedIndices(selectedIndices_); else list->setSelectedIndex(selectedIndex_); list->setAccessibleLabel(accessibleLabel_);
    list->onSelectionChanged([this, raw](int index, const Option&) { if (multiselect_) commitSelection(raw->selectedIndices(), index); else { commit(index); closePopup(); } });
    open_ = true; setVisualState(ControlVisualState::Pressed, true);
    overlayId_ = overlayHost_->show(makeListPopup(bounds(), std::max(bounds().width, kMinimumListWidth), std::move(list), [this] { closePopup(); })); overlayHost_->focus(raw); markDirty(DirtyFlag::Paint);
}
void Dropdown::closePopup()
{
    if (!open_) return; OverlayHost* host = overlayHost_; const auto id = overlayId_; open_ = false; overlayId_ = 0; setVisualState(ControlVisualState::Pressed, false); if (host && id) { (void)host->dismiss(id); host->focus(this); } markDirty(DirtyFlag::Paint);
}
void Dropdown::commit(int index)
{
    if (!selectable(index)) return; commitSelection({index}, index);
}
void Dropdown::commitSelection(std::vector<int> indices, int changedIndex)
{
    selectedIndices_ = std::move(indices); if (!multiselect_ && selectedIndices_.size() > 1) selectedIndices_.resize(1);
    selectedIndex_ = selectedIndices_.empty() ? -1 : selectedIndices_.front();
    if (changedIndex >= 0 && static_cast<std::size_t>(changedIndex) < options_.size() && onSelectionChanged_) onSelectionChanged_(changedIndex, options_[static_cast<std::size_t>(changedIndex)]);
    markDirty(DirtyFlag::Paint);
}
bool Dropdown::selectable(int index) const noexcept { return index >= 0 && static_cast<std::size_t>(index) < options_.size() && options_[static_cast<std::size_t>(index)].enabled; }
int Dropdown::nextSelectable(int from, int delta) const noexcept { for (int i = from + delta; i >= 0 && static_cast<std::size_t>(i) < options_.size(); i += delta) if (selectable(i)) return i; return -1; }

} // namespace wui
