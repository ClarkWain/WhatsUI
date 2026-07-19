#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "wui/node.h"
#include "wui/state.h"

namespace wui {

struct TextStyleToken;
class TextInput;

enum class LabelSize { Small, Medium, Large };

class Label : public Node {
public:
    explicit Label(std::string text = {});
    Label& text(std::string text);
    [[nodiscard]] const std::string& text() const noexcept;
    void setText(std::string text);
    void setSize(LabelSize size) noexcept;
    [[nodiscard]] LabelSize size() const noexcept;
    void setRequired(bool required) noexcept;
    [[nodiscard]] bool isRequired() const noexcept;
    // Associates the visual label and accessible name with one input. The
    // caller keeps both nodes alive in the same UI tree.
    void setForControl(TextInput* control) noexcept;
    [[nodiscard]] TextInput* forControl() const noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;
    EventResult onPointerEvent(const PointerEvent& event, EventContext& context) override;

private:
    std::string text_;
    LabelSize size_{LabelSize::Medium};
    bool required_{false};
    TextInput* control_{nullptr};
};

namespace detail {
class ImageResource;
class ImageTexture;
}

// Immutable, interned RGBA image data. Constructing equivalent sources reuses
// the same backing resource, so declaratively rebuilt Image nodes do not keep
// duplicate pixel buffers or backend textures alive.
class ImageSource {
public:
    ImageSource() = default;
    ImageSource(std::vector<unsigned char> rgbaPixels, int pixelWidth, int pixelHeight);

    [[nodiscard]] int pixelWidth() const noexcept;
    [[nodiscard]] int pixelHeight() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] bool operator==(const ImageSource& other) const noexcept;
    [[nodiscard]] bool operator!=(const ImageSource& other) const noexcept { return !(*this == other); }

private:
    explicit ImageSource(std::shared_ptr<detail::ImageResource> resource) noexcept;
    std::shared_ptr<detail::ImageResource> resource_;

    friend class Image;
};

// Text wraps only at explicit line breaks unless Word wrapping is enabled.
// When a maximum line count drops content, Ellipsis appends a fitted "..." to
// the final visible line (where the available width permits it).
enum class TextWrap {
    NoWrap,
    Word,
};

enum class TextOverflow {
    Clip,
    Ellipsis,
};

// Semantic role is deliberately independent from the visual style: a heading
// is exposed to accessibility clients as a heading, while the author selects
// its Fluent typography token explicitly.
enum class TextRole { Span, Paragraph, Heading, Code };
enum class TextAlign { Start, Center, End };

class Text : public Node {
public:
    explicit Text(std::string value = {});

    [[nodiscard]] const std::string& value() const noexcept;
    Text& value(std::string value);
    void setValue(std::string value);

    [[nodiscard]] float fontSize() const noexcept;
    void setFontSize(float size) noexcept;
    [[nodiscard]] int fontWeight() const noexcept;
    void setFontWeight(int weight) noexcept;
    [[nodiscard]] float lineHeight() const noexcept;
    void setLineHeight(float height) noexcept;
    [[nodiscard]] const std::string& fontFamily() const noexcept;
    void setFontFamily(std::string family);
    // Applies a named Fluent typography token atomically. Explicit setters
    // remain available for application-specific text.
    void setTextStyle(const TextStyleToken& style);
    void setRole(TextRole role) noexcept;
    [[nodiscard]] TextRole role() const noexcept;
    void setAlignment(TextAlign alignment) noexcept;
    [[nodiscard]] TextAlign alignment() const noexcept;
    void setUnderline(bool value) noexcept;
    [[nodiscard]] bool isUnderlined() const noexcept;
    void setStrikethrough(bool value) noexcept;
    [[nodiscard]] bool isStrikethrough() const noexcept;

    [[nodiscard]] TextWrap wrap() const noexcept;
    void setWrap(TextWrap wrap) noexcept;
    [[nodiscard]] std::size_t maxLines() const noexcept;
    // Zero means unlimited lines.
    void setMaxLines(std::size_t lines) noexcept;
    [[nodiscard]] TextOverflow overflow() const noexcept;
    void setOverflow(TextOverflow overflow) noexcept;

    // Resolves explicit breaks, wrapping and optional truncation in logical
    // coordinates. It is useful to custom renderers that need to mirror Text's
    // layout decisions.
    [[nodiscard]] std::vector<std::string> resolvedLines(float availableWidth) const;

    void setColor(Color color) noexcept;
    void clearColor() noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    [[nodiscard]] float baselineOffset() const noexcept override;
    void paint(PaintContext& context) override;

private:
    [[nodiscard]] std::vector<std::string> layoutLines(float availableWidth) const;
    [[nodiscard]] float textWidth(const std::string& value) const;
    [[nodiscard]] float effectiveLineHeight() const noexcept;
    std::string value_;
    float fontSize_{16.0f};
    int fontWeight_{400};
    float lineHeight_{0.0f};
    std::string fontFamily_{"Segoe UI"};
    TextRole role_{TextRole::Span};
    TextAlign alignment_{TextAlign::Start};
    bool underline_{false};
    bool strikethrough_{false};
    TextWrap wrap_{TextWrap::NoWrap};
    TextOverflow overflow_{TextOverflow::Clip};
    std::size_t maxLines_{0};
    Color color_{};
    bool hasColor_{false};
};

enum class ImageFit {
    Default,
    None,
    Center,
    Fill,
    Contain,
    Cover,
};

enum class ImageShape {
    Square,
    Circular,
    Rounded,
};

class Image : public Node {
public:
    Image();
    Image(std::vector<unsigned char> rgbaPixels, int pixelWidth, int pixelHeight);
    explicit Image(ImageSource source);
    ~Image() override;

    Image& source(std::vector<unsigned char> rgbaPixels, int pixelWidth, int pixelHeight);
    Image& source(ImageSource source);
    void setSource(std::vector<unsigned char> rgbaPixels, int pixelWidth, int pixelHeight);
    void setSource(ImageSource source);
    void clearSource() noexcept;
    Image& fallback(std::vector<unsigned char> rgbaPixels, int pixelWidth, int pixelHeight);
    Image& fallback(ImageSource source);
    void setFallback(ImageSource source);
    void clearFallback() noexcept;

    [[nodiscard]] const ImageSource imageSource() const noexcept;

    Image& fit(ImageFit fit) noexcept;
    void setFit(ImageFit fit) noexcept;
    [[nodiscard]] ImageFit fit() const noexcept;

    Image& align(float x, float y) noexcept;
    void setAlignment(float x, float y) noexcept;
    [[nodiscard]] PointF alignment() const noexcept;
    Image& shape(ImageShape shape) noexcept;
    void setShape(ImageShape shape) noexcept;
    [[nodiscard]] ImageShape shape() const noexcept;
    Image& bordered(bool bordered = true) noexcept;
    void setBordered(bool bordered) noexcept;
    [[nodiscard]] bool isBordered() const noexcept;
    Image& shadow(bool shadow = true) noexcept;
    void setShadow(bool shadow) noexcept;
    [[nodiscard]] bool hasShadow() const noexcept;
    Image& block(bool block = true) noexcept;
    void setBlock(bool block) noexcept;
    [[nodiscard]] bool isBlock() const noexcept;
    Image& alt(std::string description);
    void setAlt(std::string description);
    [[nodiscard]] const std::string& alt() const noexcept;
    Image& decorative(bool decorative = true) noexcept;
    void setDecorative(bool decorative) noexcept;
    [[nodiscard]] bool isDecorative() const noexcept;
    [[nodiscard]] SizeF intrinsicSize() const noexcept;
    [[nodiscard]] bool hasSource() const noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void prepare(PaintContext& context) override;
    void paint(PaintContext& context) override;

private:
    [[nodiscard]] const ImageSource& effectiveSource() const noexcept;
    ImageSource source_;
    ImageSource fallback_;
    ImageFit fit_{ImageFit::Default};
    PointF alignment_{0.5f, 0.5f};
    ImageShape shape_{ImageShape::Square};
    bool bordered_{false};
    bool shadow_{false};
    bool block_{false};
    bool decorative_{false};
    std::string alt_;
    // ImageSource is immutable shared CPU data. GPU texture state belongs to
    // this widget and one Canvas context only.
    std::unique_ptr<detail::ImageTexture> texture_;
};

class Spacer : public Node {
public:
    explicit Spacer(SizeF size = {}) noexcept;

    [[nodiscard]] SizeF size() const noexcept;
    void setSize(SizeF size) noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;

private:
    SizeF size_{};
};

class Container : public ContainerNode {
public:
    Container& child(std::unique_ptr<Node> child);

    void setBackground(Color color) noexcept;
    void setRadius(float radius) noexcept;
    void setPadding(InsetsF padding) noexcept;
    void setContentAlignment(Alignment horizontal, Alignment vertical) noexcept;
    void setWidth(float width) noexcept;
    void clearWidth() noexcept;
    void setHeight(float height) noexcept;
    void clearHeight() noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;
    void paint(PaintContext& context) override;

private:
    Color background_{0, 0, 0, 0};
    float radius_{0.0f};
    InsetsF padding_{};
    Alignment horizontalAlignment_{Alignment::Stretch};
    Alignment verticalAlignment_{Alignment::Stretch};
    std::optional<float> width_;
    std::optional<float> height_;
};

enum class CardAppearance { Filled, FilledAlternative, Outline, Subtle };
enum class CardSize { Small, Medium, Large };
enum class CardOrientation { Vertical, Horizontal };

// A semantic Fluent surface. Cards own layout padding and elevation so apps
// do not recreate visually inconsistent Box combinations on every screen.
class Card : public ControlNode {
public:
    using ChangeHandler = std::function<void(bool)>;
    Card& child(std::unique_ptr<Node> child);
    void setAppearance(CardAppearance appearance) noexcept;
    [[nodiscard]] CardAppearance appearance() const noexcept;
    void setSize(CardSize size) noexcept;
    [[nodiscard]] CardSize size() const noexcept;
    void setOrientation(CardOrientation orientation) noexcept;
    [[nodiscard]] CardOrientation orientation() const noexcept;
    void setSelected(bool selected) noexcept;
    [[nodiscard]] bool isSelected() const noexcept;
    Card& selectable(bool value = true) noexcept;
    [[nodiscard]] bool isSelectable() const noexcept;
    Card& onSelectionChange(ChangeHandler handler);

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;
    void paint(PaintContext& context) override;
    bool onPointerEvent(const PointerEvent& event) override;
    bool onKeyEvent(const KeyEvent& event) override;
    [[nodiscard]] AccessibilityActionCapabilities accessibilityActions() const noexcept override;
    AccessibilityActionStatus performAccessibilityAction(
        AccessibilityActionKind kind, std::string_view value) override;

private:
    [[nodiscard]] InsetsF padding() const noexcept;
    CardAppearance appearance_{CardAppearance::Filled};
    CardSize size_{CardSize::Medium};
    CardOrientation orientation_{CardOrientation::Vertical};
    bool selected_{false};
    bool selectable_{false};
    ChangeHandler onSelectionChange_;
};

class CardHeader : public ContainerNode {
public:
    CardHeader(std::string title = {}, std::string description = {});
    void setTitle(std::string title);
    void setDescription(std::string description);
    [[nodiscard]] const std::string& title() const noexcept;
    [[nodiscard]] const std::string& description() const noexcept;
    // Slots have a stable visual order (media, text, action) regardless of
    // the order in which callers configure them. Passing nullptr clears the
    // respective slot; a later non-null value replaces it safely even while
    // the header is attached to a live tree.
    CardHeader& media(std::unique_ptr<Node> media);
    CardHeader& action(std::unique_ptr<Node> action);
    void setMedia(std::unique_ptr<Node> media);
    void setAction(std::unique_ptr<Node> action);
    [[nodiscard]] Node* media() const noexcept;
    [[nodiscard]] Node* action() const noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;
    void paint(PaintContext& context) override;

private:
    [[nodiscard]] std::size_t actionIndex() const noexcept;
    std::string title_;
    std::string description_;
    bool hasMedia_{false};
    bool hasAction_{false};
};

class CardPreview : public ContainerNode {
public:
    CardPreview& child(std::unique_ptr<Node> child);
    void setHeight(float value) noexcept;
    [[nodiscard]] float height() const noexcept;
    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;
    void paint(PaintContext& context) override;
private:
    float height_{0.0f};
};

class CardFooter : public ContainerNode {
public:
    CardFooter& child(std::unique_ptr<Node> child);
    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;
};

class Row : public ContainerNode {
public:
    Row& child(std::unique_ptr<Node> child);
    Row& gap(float gap) noexcept;
    void setGap(float gap) noexcept;
    [[nodiscard]] float gap() const noexcept;

    Row& padding(InsetsF padding) noexcept;
    void setPadding(InsetsF padding) noexcept;
    [[nodiscard]] InsetsF padding() const noexcept;

    Row& align(Alignment align) noexcept;
    void setAlign(Alignment align) noexcept;
    [[nodiscard]] Alignment align() const noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;

private:
    float gap_{0.0f};
    InsetsF padding_{};
    Alignment align_{Alignment::Start};
};

class Column : public ContainerNode {
public:
    Column& child(std::unique_ptr<Node> child);
    Column& gap(float gap) noexcept;
    void setGap(float gap) noexcept;
    [[nodiscard]] float gap() const noexcept;

    Column& padding(InsetsF padding) noexcept;
    void setPadding(InsetsF padding) noexcept;
    [[nodiscard]] InsetsF padding() const noexcept;

    Column& align(Alignment align) noexcept;
    void setAlign(Alignment align) noexcept;
    [[nodiscard]] Alignment align() const noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;

private:
    float gap_{0.0f};
    InsetsF padding_{};
    Alignment align_{Alignment::Start};
};

enum class ScrollAxis {
    Vertical,
    Horizontal,
    Both,
};

// A single-child clipped viewport. Its content is unbounded only on enabled
// axes; offsets are logical pixels and are clamped after every layout/content
// change. A wheel handler consumes the part it can apply and leaves the
// remainder in EventContext for ancestor ScrollViews during bubbling.
class ScrollView : public ContainerNode {
public:
    ScrollView& child(std::unique_ptr<Node> child);
    ScrollView& setAxis(ScrollAxis axis) noexcept;
    [[nodiscard]] ScrollAxis axis() const noexcept;
    void setScrollOffset(float offset) noexcept;
    void setScrollOffset(PointF offset) noexcept;
    [[nodiscard]] float scrollOffset() const noexcept;
    [[nodiscard]] float scrollOffsetX() const noexcept;
    [[nodiscard]] float scrollOffsetY() const noexcept;
    [[nodiscard]] float maxScrollOffset() const noexcept;
    [[nodiscard]] float maxScrollOffsetX() const noexcept;
    [[nodiscard]] float maxScrollOffsetY() const noexcept;
    [[nodiscard]] SizeF contentSize() const noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;
    void paint(PaintContext& context) override;
    [[nodiscard]] Node* hitTest(PointF point) override;
    EventResult onPointerEvent(const PointerEvent& event, EventContext& context) override;
    bool onPointerEvent(const PointerEvent& event) override;

private:
    void clampOffset() noexcept;
    SizeF contentSize_{};
    PointF scrollOffset_{};
    ScrollAxis axis_{ScrollAxis::Vertical};
};

// A window-sized modal surface. Dialog owns exactly one content subtree,
// centers it in the available window bounds, paints a dimming scrim behind
// it, and consumes backdrop pointer input. UiWindow supplies Escape handling
// and focus restoration through showDialog()/dismissDialog().
class Dialog : public ContainerNode {
public:
    using DismissHandler = std::function<void()>;

    Dialog& content(std::unique_ptr<Node> content);
    void setMaxWidth(float width) noexcept;
    [[nodiscard]] float maxWidth() const noexcept;
    void setBackdropDismissEnabled(bool enabled) noexcept;
    [[nodiscard]] bool backdropDismissEnabled() const noexcept;
    void onDismiss(DismissHandler handler);

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;
    void paint(PaintContext& context) override;
    [[nodiscard]] Node* hitTest(PointF point) override;
    bool onPointerEvent(const PointerEvent& event) override;
    void dismiss();

private:
    // UiWindow installs its lifecycle handler separately so author callbacks
    // registered with onDismiss() are not overwritten.
    friend class UiWindow;
    void setWindowDismissHandler(DismissHandler handler);
    float maxWidth_{420.0f};
    bool backdropDismissEnabled_{false};
    DismissHandler onDismiss_;
    DismissHandler windowDismiss_;
};

enum class ButtonVariant {
    // Compatibility names for the original WhatsUI button API. New code uses
    // ButtonAppearance so every state belongs to a Fluent semantic role.
    Primary,
    Ghost,
    Danger,
};

enum class ButtonAppearance {
    Secondary,
    Primary,
    Outline,
    Subtle,
    Transparent,
    Danger,
};

enum class ButtonSize {
    Small,
    Medium,
    Large,
};

enum class ButtonShape {
    Rounded,
    Circular,
    Square,
};

class Button : public ControlNode {
public:
    using ClickHandler = std::function<void()>;

    explicit Button(std::string label = {});

    [[nodiscard]] const std::string& label() const noexcept;
    Button& label(std::string label);
    void setLabel(std::string label);

    Button& onClick(ClickHandler handler);

    void setVariant(ButtonVariant variant) noexcept;
    [[nodiscard]] ButtonVariant variant() const noexcept;
    void setAppearance(ButtonAppearance appearance) noexcept;
    [[nodiscard]] ButtonAppearance appearance() const noexcept;
    void setSize(ButtonSize size) noexcept;
    [[nodiscard]] ButtonSize size() const noexcept;
    void setShape(ButtonShape shape) noexcept;
    [[nodiscard]] ButtonShape shape() const noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;
    bool onPointerEvent(const PointerEvent& event) override;
    [[nodiscard]] AccessibilityActionCapabilities accessibilityActions() const noexcept override;
    AccessibilityActionStatus performAccessibilityAction(
        AccessibilityActionKind kind, std::string_view value) override;

private:
    std::string label_;
    ClickHandler onClick_;
    ButtonVariant variant_{ButtonVariant::Primary};
    ButtonAppearance appearance_{ButtonAppearance::Primary};
    ButtonSize size_{ButtonSize::Medium};
    ButtonShape shape_{ButtonShape::Rounded};
};

// Fluent's two-line command button. The secondary content is descriptive
// text, not a second action: the whole surface invokes one command.
class CompoundButton : public ControlNode {
public:
    using ClickHandler = std::function<void()>;

    CompoundButton(std::string label = {}, std::string secondaryContent = {});
    [[nodiscard]] const std::string& label() const noexcept;
    [[nodiscard]] const std::string& secondaryContent() const noexcept;
    CompoundButton& label(std::string value);
    CompoundButton& secondaryContent(std::string value);
    void setLabel(std::string value);
    void setSecondaryContent(std::string value);
    CompoundButton& onClick(ClickHandler handler);
    void setAppearance(ButtonAppearance value) noexcept;
    [[nodiscard]] ButtonAppearance appearance() const noexcept;
    void setSize(ButtonSize value) noexcept;
    [[nodiscard]] ButtonSize size() const noexcept;
    void setShape(ButtonShape value) noexcept;
    [[nodiscard]] ButtonShape shape() const noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;
    bool onPointerEvent(const PointerEvent& event) override;
    [[nodiscard]] AccessibilityActionCapabilities accessibilityActions() const noexcept override;
    AccessibilityActionStatus performAccessibilityAction(
        AccessibilityActionKind kind, std::string_view value) override;

private:
    std::string label_;
    std::string secondaryContent_;
    ClickHandler onClick_;
    ButtonAppearance appearance_{ButtonAppearance::Secondary};
    ButtonSize size_{ButtonSize::Medium};
    ButtonShape shape_{ButtonShape::Rounded};
};

// A command-style two-state control. Unlike Checkbox it is rendered as a
// button surface, which makes it appropriate for formatting and view toggles.
class ToggleButton : public ControlNode {
public:
    using ChangeHandler = std::function<void(bool)>;

    explicit ToggleButton(std::string label = {}, bool checked = false);

    [[nodiscard]] const std::string& label() const noexcept;
    ToggleButton& label(std::string value);
    void setLabel(std::string value);
    [[nodiscard]] bool isChecked() const noexcept;
    ToggleButton& checked(bool value);
    void setChecked(bool value);
    ToggleButton& bind(State<bool>& state);
    ToggleButton& onChange(ChangeHandler handler);
    void setSize(ButtonSize value) noexcept;
    [[nodiscard]] ButtonSize size() const noexcept;
    void setShape(ButtonShape value) noexcept;
    [[nodiscard]] ButtonShape shape() const noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;
    bool onPointerEvent(const PointerEvent& event) override;
    bool onKeyEvent(const KeyEvent& event) override;
    [[nodiscard]] AccessibilityActionCapabilities accessibilityActions() const noexcept override;
    AccessibilityActionStatus performAccessibilityAction(
        AccessibilityActionKind kind, std::string_view value) override;

private:
    void toggle();

    std::string label_;
    bool checked_{false};
    std::optional<Binding<bool>> binding_;
    bool hasBinding_{false};
    ChangeHandler onChange_;
    ButtonSize size_{ButtonSize::Medium};
    ButtonShape shape_{ButtonShape::Rounded};
};

enum class CheckboxState {
    Unchecked,
    Checked,
    Mixed,
};

enum class CheckboxSize {
    Medium,
    Large,
};

enum class CheckboxShape {
    Square,
    Circular,
};

enum class CheckboxLabelPosition {
    After,
    Before,
};

// A Fluent tri-state form control. Checkbox owns its state unless bound to a
// State<bool>; a bool binding deliberately supports only checked/unchecked.
class Checkbox : public ControlNode {
public:
    using ChangeHandler = std::function<void(bool)>;
    using StateChangeHandler = std::function<void(CheckboxState)>;

    explicit Checkbox(std::string label = {}, bool checked = false);

    [[nodiscard]] const std::string& label() const noexcept;
    Checkbox& label(std::string label);
    void setLabel(std::string label);

    // An optional semantic name for compact controls whose visible task title
    // is rendered by a neighbouring Text node rather than by the checkbox.
    // It never affects layout or painting.
    [[nodiscard]] const std::string& accessibleLabel() const noexcept;
    Checkbox& accessibleLabel(std::string label);
    void setAccessibleLabel(std::string label);

    [[nodiscard]] bool isChecked() const noexcept;
    [[nodiscard]] bool isMixed() const noexcept;
    [[nodiscard]] CheckboxState state() const noexcept;
    Checkbox& checked(bool value);
    void setChecked(bool value);
    Checkbox& mixed(bool value = true);
    void setMixed(bool value = true);
    Checkbox& checkState(CheckboxState value);
    void setCheckState(CheckboxState value);
    Checkbox& bind(State<bool>& state);
    Checkbox& onChange(ChangeHandler handler);
    Checkbox& onStateChange(StateChangeHandler handler);

    Checkbox& size(CheckboxSize value) noexcept;
    void setSize(CheckboxSize value) noexcept;
    [[nodiscard]] CheckboxSize size() const noexcept;
    Checkbox& shape(CheckboxShape value) noexcept;
    void setShape(CheckboxShape value) noexcept;
    [[nodiscard]] CheckboxShape shape() const noexcept;
    Checkbox& labelPosition(CheckboxLabelPosition value) noexcept;
    void setLabelPosition(CheckboxLabelPosition value) noexcept;
    [[nodiscard]] CheckboxLabelPosition labelPosition() const noexcept;
    Checkbox& required(bool value = true) noexcept;
    void setRequired(bool value = true) noexcept;
    [[nodiscard]] bool isRequired() const noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;
    bool onPointerEvent(const PointerEvent& event) override;
    bool onKeyEvent(const KeyEvent& event) override;
    [[nodiscard]] AccessibilityActionCapabilities accessibilityActions() const noexcept override;
    AccessibilityActionStatus performAccessibilityAction(
        AccessibilityActionKind kind, std::string_view value) override;

private:
    void toggle();
    std::string label_;
    std::string accessibleLabel_;
    bool checked_{false};
    bool mixed_{false};
    std::optional<Binding<bool>> binding_;
    StateSubscription<bool> subscription_;
    bool hasBinding_{false};
    ChangeHandler onChange_;
    StateChangeHandler onStateChange_;
    CheckboxSize size_{CheckboxSize::Medium};
    CheckboxShape shape_{CheckboxShape::Square};
    CheckboxLabelPosition labelPosition_{CheckboxLabelPosition::After};
    bool required_{false};
};

} // namespace wui
