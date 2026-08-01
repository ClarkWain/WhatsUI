#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "wui/icons.h"
#include "wui/interaction.h"
#include "wui/node.h"
#include "wui/state.h"

namespace wui {

struct TextStyleToken;
class TextFieldNode;

enum class LabelSize { Small, Medium, Large };

class LabelNode : public Node {
public:
    explicit LabelNode(std::string text = {});
    LabelNode& text(std::string text);
    [[nodiscard]] const std::string& text() const noexcept;
    void setText(std::string text);
    void setSize(LabelSize size) noexcept;
    [[nodiscard]] LabelSize size() const noexcept;
    void setRequired(bool required) noexcept;
    [[nodiscard]] bool isRequired() const noexcept;
    // Associates the visual label and accessible name with one input. The
    // caller keeps both nodes alive in the same UI tree.
    void setForControl(TextFieldNode* control) noexcept;
    [[nodiscard]] TextFieldNode* forControl() const noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;
    EventResult onPointerEvent(const PointerEvent& event, EventContext& context) override;

private:
    std::string text_;
    LabelSize size_{LabelSize::Medium};
    bool required_{false};
    TextFieldNode* control_{nullptr};
};

namespace detail {
class ImageResource;
class ImageTexture;
}

// Immutable, interned RGBA image data. Constructing equivalent sources reuses
// the same backing resource, so declaratively rebuilt ImageNode nodes do not keep
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

    friend class ImageNode;
};

// TextNode wraps only at explicit line breaks unless Word wrapping is enabled.
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

class TextNode : public Node {
public:
    explicit TextNode(std::string value = {});

    [[nodiscard]] const std::string& value() const noexcept;
    TextNode& value(std::string value);
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
    // Block text can use the finite layout width directly instead of
    // resolving an expensive intrinsic width. This is particularly useful
    // for large documents hosted by a vertical ScrollViewNode.
    void setFillAvailableWidth(bool fill) noexcept;
    [[nodiscard]] bool fillsAvailableWidth() const noexcept;

    // Resolves explicit breaks, wrapping and optional truncation in logical
    // coordinates. It is useful to custom renderers that need to mirror TextNode's
    // layout decisions.
    [[nodiscard]] std::vector<std::string> resolvedLines(float availableWidth) const;

    void setColor(Color color) noexcept;
    void clearColor() noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    [[nodiscard]] float baselineOffset() const noexcept override;
    void paint(PaintContext& context) override;

private:
    struct ExplicitLine {
        std::size_t start{0};
        std::size_t length{0};
    };

    [[nodiscard]] const std::vector<ExplicitLine>& explicitLines() const;
    void invalidateLineCache() noexcept;
    [[nodiscard]] std::vector<std::string> layoutLines(float availableWidth) const;
    [[nodiscard]] float textWidth(const std::string& value) const;
    [[nodiscard]] float effectiveLineHeight() const noexcept;
    std::string value_;
    float fontSize_{16.0f};
    int fontWeight_{400};
    float lineHeight_{0.0f};
    // Resolved in TextNode's constructor from the active Theme. Leaving this
    // empty here avoids freezing the legacy Segoe UI family before a caller
    // has selected its Windows typography token set.
    std::string fontFamily_{};
    TextRole role_{TextRole::Span};
    TextAlign alignment_{TextAlign::Start};
    bool underline_{false};
    bool strikethrough_{false};
    TextWrap wrap_{TextWrap::NoWrap};
    TextOverflow overflow_{TextOverflow::Clip};
    std::size_t maxLines_{0};
    bool fillAvailableWidth_{false};
    Color color_{};
    bool hasColor_{false};
    mutable bool explicitLineCacheValid_{false};
    mutable std::vector<ExplicitLine> explicitLineCache_;
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

class ImageNode : public Node {
public:
    ImageNode();
    ImageNode(std::vector<unsigned char> rgbaPixels, int pixelWidth, int pixelHeight);
    explicit ImageNode(ImageSource source);
    ~ImageNode() override;

    ImageNode& source(std::vector<unsigned char> rgbaPixels, int pixelWidth, int pixelHeight);
    ImageNode& source(ImageSource source);
    void setSource(std::vector<unsigned char> rgbaPixels, int pixelWidth, int pixelHeight);
    void setSource(ImageSource source);
    void clearSource() noexcept;
    ImageNode& fallback(std::vector<unsigned char> rgbaPixels, int pixelWidth, int pixelHeight);
    ImageNode& fallback(ImageSource source);
    void setFallback(ImageSource source);
    void clearFallback() noexcept;

    [[nodiscard]] const ImageSource imageSource() const noexcept;

    ImageNode& fit(ImageFit fit) noexcept;
    void setFit(ImageFit fit) noexcept;
    [[nodiscard]] ImageFit fit() const noexcept;

    ImageNode& align(float x, float y) noexcept;
    void setAlignment(float x, float y) noexcept;
    [[nodiscard]] PointF alignment() const noexcept;
    ImageNode& shape(ImageShape shape) noexcept;
    void setShape(ImageShape shape) noexcept;
    [[nodiscard]] ImageShape shape() const noexcept;
    ImageNode& bordered(bool bordered = true) noexcept;
    void setBordered(bool bordered) noexcept;
    [[nodiscard]] bool isBordered() const noexcept;
    ImageNode& shadow(bool shadow = true) noexcept;
    void setShadow(bool shadow) noexcept;
    [[nodiscard]] bool hasShadow() const noexcept;
    ImageNode& block(bool block = true) noexcept;
    void setBlock(bool block) noexcept;
    [[nodiscard]] bool isBlock() const noexcept;
    ImageNode& alt(std::string description);
    void setAlt(std::string description);
    [[nodiscard]] const std::string& alt() const noexcept;
    ImageNode& decorative(bool decorative = true) noexcept;
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

class SpacerNode : public Node {
public:
    explicit SpacerNode(SizeF size = {}) noexcept;

    [[nodiscard]] SizeF size() const noexcept;
    void setSize(SizeF size) noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;

private:
    SizeF size_{};
};

class BoxNode : public ContainerNode {
public:
    BoxNode& child(std::unique_ptr<Node> child);

    void setBackground(Color color) noexcept;
    void setRadius(float radius) noexcept;
    void setPadding(InsetsF padding) noexcept;
    void setContentAlignment(Alignment horizontal, Alignment vertical) noexcept;
    void setWidth(float width) noexcept;
    void clearWidth() noexcept;
    void setHeight(float height) noexcept;
    void clearHeight() noexcept;

    // Attach an InteractionArea on demand. The container stays a pure layout
    // shell until any interaction setter is called; from that point on it
    // participates in pointer/keyboard/accessibility routing exactly like a
    // Fluent control, and its paint pass picks up hover/pressed backgrounds.
    void setOnClick(std::function<void()> handler);
    void setOnPointerDown(std::function<bool(const PointerEvent&)> handler);
    void setOnPointerMove(std::function<bool(const PointerEvent&)> handler);
    void setOnPointerUp(std::function<bool(const PointerEvent&)> handler);
    void setOnHoverChange(std::function<void(bool)> handler);
    void setOnFocusChange(std::function<void(bool)> handler);
    void setOnKey(std::function<bool(const KeyEvent&)> handler);
    void setHoverBackground(Color color) noexcept;
    void setPressedBackground(Color color) noexcept;
    void setAccessibleRole(AccessibilityRole role) noexcept;
    void setAccessibleLabel(std::string label);
    [[nodiscard]] const InteractionArea* interaction() const noexcept
    {
        return interaction_.get();
    }

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;
    void paint(PaintContext& context) override;
    EventResult onPointerEvent(const PointerEvent& event,
                               EventContext& context) override;
    bool onPointerEvent(const PointerEvent& event) override;
    bool onKeyEvent(const KeyEvent& event) override;
    [[nodiscard]] AccessibilityActionCapabilities accessibilityActions() const noexcept override;
    AccessibilityActionStatus performAccessibilityAction(
        AccessibilityActionKind kind, std::string_view value) override;

private:
    InteractionArea& ensureInteraction();

    Color background_{0, 0, 0, 0};
    float radius_{0.0f};
    InsetsF padding_{};
    Alignment horizontalAlignment_{Alignment::Stretch};
    Alignment verticalAlignment_{Alignment::Stretch};
    std::optional<float> width_;
    std::optional<float> height_;
    std::unique_ptr<InteractionArea> interaction_;
};

enum class CardAppearance { Filled, FilledAlternative, Outline, Subtle };
enum class CardSize { Small, Medium, Large };
enum class CardOrientation { Vertical, Horizontal };

// A semantic Fluent surface. Cards own layout padding and elevation so apps
// do not recreate visually inconsistent Box combinations on every screen.
class CardNode : public ControlNode {
public:
    using ChangeHandler = std::function<void(bool)>;
    CardNode& child(std::unique_ptr<Node> child);
    void setAppearance(CardAppearance appearance) noexcept;
    [[nodiscard]] CardAppearance appearance() const noexcept;
    void setSize(CardSize size) noexcept;
    [[nodiscard]] CardSize size() const noexcept;
    void setOrientation(CardOrientation orientation) noexcept;
    [[nodiscard]] CardOrientation orientation() const noexcept;
    void setSelected(bool selected) noexcept;
    [[nodiscard]] bool isSelected() const noexcept;
    CardNode& selectable(bool value = true) noexcept;
    [[nodiscard]] bool isSelectable() const noexcept;
    CardNode& onSelectionChange(ChangeHandler handler);

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

class CardHeaderNode : public ContainerNode {
public:
    CardHeaderNode(std::string title = {}, std::string description = {});
    void setTitle(std::string title);
    void setDescription(std::string description);
    [[nodiscard]] const std::string& title() const noexcept;
    [[nodiscard]] const std::string& description() const noexcept;
    // Slots have a stable visual order (media, text, action) regardless of
    // the order in which callers configure them. Passing nullptr clears the
    // respective slot; a later non-null value replaces it safely even while
    // the header is attached to a live tree.
    CardHeaderNode& media(std::unique_ptr<Node> media);
    CardHeaderNode& action(std::unique_ptr<Node> action);
    void setMedia(std::unique_ptr<Node> media);
    void setAction(std::unique_ptr<Node> action);
    [[nodiscard]] Node* media() const noexcept;
    [[nodiscard]] Node* action() const noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;
    void paint(PaintContext& context) override;

protected:
    void validateChildInsertion(
        const Node& child,
        std::size_t index,
        std::size_t resultingCount) const override;

private:
    [[nodiscard]] std::size_t actionIndex() const noexcept;
    std::string title_;
    std::string description_;
    bool hasMedia_{false};
    bool hasAction_{false};
    bool acceptingSlotMutation_{false};
};

class CardPreviewNode : public ContainerNode {
public:
    CardPreviewNode& child(std::unique_ptr<Node> child);
    void setHeight(float value) noexcept;
    [[nodiscard]] float height() const noexcept;
    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;
    void paint(PaintContext& context) override;
private:
    float height_{0.0f};
};

class CardFooterNode : public ContainerNode {
public:
    CardFooterNode& child(std::unique_ptr<Node> child);
    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;
};

class RowNode : public ContainerNode {
public:
    RowNode& child(std::unique_ptr<Node> child);
    RowNode& gap(float gap) noexcept;
    void setGap(float gap) noexcept;
    [[nodiscard]] float gap() const noexcept;

    RowNode& padding(InsetsF padding) noexcept;
    void setPadding(InsetsF padding) noexcept;
    [[nodiscard]] InsetsF padding() const noexcept;

    RowNode& align(Alignment align) noexcept;
    void setAlign(Alignment align) noexcept;
    [[nodiscard]] Alignment align() const noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;

private:
    float gap_{0.0f};
    InsetsF padding_{};
    Alignment align_{Alignment::Start};
};

class ColumnNode : public ContainerNode {
public:
    ColumnNode& child(std::unique_ptr<Node> child);
    ColumnNode& gap(float gap) noexcept;
    void setGap(float gap) noexcept;
    [[nodiscard]] float gap() const noexcept;

    ColumnNode& padding(InsetsF padding) noexcept;
    void setPadding(InsetsF padding) noexcept;
    [[nodiscard]] InsetsF padding() const noexcept;

    ColumnNode& align(Alignment align) noexcept;
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
class ScrollViewNode : public ContainerNode {
public:
    ScrollViewNode& content(std::unique_ptr<Node> content);
    [[nodiscard]] Node* content() const noexcept;
    ScrollViewNode& setAxis(ScrollAxis axis) noexcept;
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
    [[nodiscard]] PointF mapPointToContent(PointF point) const noexcept override;
    EventResult onPointerEvent(const PointerEvent& event, EventContext& context) override;
    bool onPointerEvent(const PointerEvent& event) override;

private:
    void validateChildInsertion(
        const Node& child,
        std::size_t index,
        std::size_t resultingCount) const override;
    void clampOffset() noexcept;
    SizeF contentSize_{};
    PointF scrollOffset_{};
    ScrollAxis axis_{ScrollAxis::Vertical};
};

// A window-sized modal surface. DialogNode owns exactly one content subtree,
// centers it in the available window bounds, paints a dimming scrim behind
// it, and consumes backdrop pointer input. UiWindow supplies Escape handling
// and focus restoration through showDialog()/dismissDialog().
class DialogNode : public ContainerNode {
public:
    using DismissHandler = std::function<void()>;

    DialogNode& content(std::unique_ptr<Node> content);
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

enum class ButtonIconPosition {
    Before,
    After,
};

class ButtonNode : public ControlNode {
public:
    using ClickHandler = std::function<void()>;

    explicit ButtonNode(std::string label = {});

    [[nodiscard]] const std::string& label() const noexcept;
    ButtonNode& label(std::string label);
    void setLabel(std::string label);

    ButtonNode& onClick(ClickHandler handler);

    void setAppearance(ButtonAppearance appearance) noexcept;
    [[nodiscard]] ButtonAppearance appearance() const noexcept;
    void setSize(ButtonSize size) noexcept;
    [[nodiscard]] ButtonSize size() const noexcept;
    void setShape(ButtonShape shape) noexcept;
    [[nodiscard]] ButtonShape shape() const noexcept;
    ButtonNode& icon(IconName value) noexcept;
    ButtonNode& iconStyle(IconStyle value) noexcept;
    ButtonNode& iconPosition(ButtonIconPosition value) noexcept;
    ButtonNode& iconOnly(bool value = true) noexcept;
    ButtonNode& clearIcon() noexcept;
    void setIcon(std::optional<IconName> value) noexcept;
    void setIconStyle(IconStyle value) noexcept;
    void setIconPosition(ButtonIconPosition value) noexcept;
    void setIconOnly(bool value) noexcept;
    [[nodiscard]] std::optional<IconName> icon() const noexcept;
    [[nodiscard]] IconStyle iconStyle() const noexcept;
    [[nodiscard]] ButtonIconPosition iconPosition() const noexcept;
    [[nodiscard]] bool isIconOnly() const noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;
    bool onPointerEvent(const PointerEvent& event) override;
    [[nodiscard]] AccessibilityActionCapabilities accessibilityActions() const noexcept override;
    AccessibilityActionStatus performAccessibilityAction(
        AccessibilityActionKind kind, std::string_view value) override;

private:
    std::string label_;
    ClickHandler onClick_;
    ButtonAppearance appearance_{ButtonAppearance::Secondary};
    ButtonSize size_{ButtonSize::Medium};
    ButtonShape shape_{ButtonShape::Rounded};
    std::optional<IconName> icon_;
    IconStyle iconStyle_{IconStyle::Regular};
    ButtonIconPosition iconPosition_{ButtonIconPosition::Before};
    bool iconOnly_{false};
};

// Fluent's two-line command button. The secondary content is descriptive
// text, not a second action: the whole surface invokes one command.
class CompoundButtonNode : public ControlNode {
public:
    using ClickHandler = std::function<void()>;

    CompoundButtonNode(std::string label = {}, std::string secondaryContent = {});
    [[nodiscard]] const std::string& label() const noexcept;
    [[nodiscard]] const std::string& secondaryContent() const noexcept;
    CompoundButtonNode& label(std::string value);
    CompoundButtonNode& secondaryContent(std::string value);
    void setLabel(std::string value);
    void setSecondaryContent(std::string value);
    CompoundButtonNode& onClick(ClickHandler handler);
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

// A command-style two-state control. Unlike CheckboxNode it is rendered as a
// button surface, which makes it appropriate for formatting and view toggles.
class ToggleButtonNode : public ControlNode {
public:
    using ChangeHandler = std::function<void(bool)>;

    explicit ToggleButtonNode(std::string label = {}, bool checked = false);

    [[nodiscard]] const std::string& label() const noexcept;
    ToggleButtonNode& label(std::string value);
    void setLabel(std::string value);
    [[nodiscard]] bool isChecked() const noexcept;
    ToggleButtonNode& checked(bool value);
    void setChecked(bool value);
    ToggleButtonNode& bind(State<bool>& state);
    ToggleButtonNode& onChange(ChangeHandler handler);
    void setSize(ButtonSize value) noexcept;
    [[nodiscard]] ButtonSize size() const noexcept;
    void setShape(ButtonShape value) noexcept;
    [[nodiscard]] ButtonShape shape() const noexcept;
    void setAppearance(ButtonAppearance value) noexcept;
    [[nodiscard]] ButtonAppearance appearance() const noexcept;
    ToggleButtonNode& icon(IconName value) noexcept;
    ToggleButtonNode& iconStyle(IconStyle value) noexcept;
    ToggleButtonNode& iconPosition(ButtonIconPosition value) noexcept;
    ToggleButtonNode& iconOnly(bool value = true) noexcept;
    ToggleButtonNode& clearIcon() noexcept;
    void setIcon(std::optional<IconName> value) noexcept;
    void setIconStyle(IconStyle value) noexcept;
    void setIconPosition(ButtonIconPosition value) noexcept;
    void setIconOnly(bool value) noexcept;
    [[nodiscard]] std::optional<IconName> icon() const noexcept;
    [[nodiscard]] IconStyle iconStyle() const noexcept;
    [[nodiscard]] ButtonIconPosition iconPosition() const noexcept;
    [[nodiscard]] bool isIconOnly() const noexcept;

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
    ButtonAppearance appearance_{ButtonAppearance::Secondary};
    ButtonSize size_{ButtonSize::Medium};
    ButtonShape shape_{ButtonShape::Rounded};
    std::optional<IconName> icon_;
    IconStyle iconStyle_{IconStyle::Regular};
    ButtonIconPosition iconPosition_{ButtonIconPosition::Before};
    bool iconOnly_{false};
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

// A Fluent tri-state form control. CheckboxNode owns its state unless bound to a
// State<bool>; a bool binding deliberately supports only checked/unchecked.
class CheckboxNode : public ControlNode {
public:
    using ChangeHandler = std::function<void(bool)>;
    using StateChangeHandler = std::function<void(CheckboxState)>;

    explicit CheckboxNode(std::string label = {}, bool checked = false);

    [[nodiscard]] const std::string& label() const noexcept;
    CheckboxNode& label(std::string label);
    void setLabel(std::string label);

    // An optional semantic name for compact controls whose visible task title
    // is rendered by a neighbouring TextNode node rather than by the checkbox.
    // It never affects layout or painting.
    [[nodiscard]] const std::string& accessibleLabel() const noexcept;
    CheckboxNode& accessibleLabel(std::string label);
    void setAccessibleLabel(std::string label);

    [[nodiscard]] bool isChecked() const noexcept;
    [[nodiscard]] bool isMixed() const noexcept;
    [[nodiscard]] CheckboxState state() const noexcept;
    CheckboxNode& checked(bool value);
    void setChecked(bool value);
    CheckboxNode& mixed(bool value = true);
    void setMixed(bool value = true);
    CheckboxNode& checkState(CheckboxState value);
    void setCheckState(CheckboxState value);
    CheckboxNode& bind(State<bool>& state);
    CheckboxNode& onChange(ChangeHandler handler);
    CheckboxNode& onStateChange(StateChangeHandler handler);

    CheckboxNode& size(CheckboxSize value) noexcept;
    void setSize(CheckboxSize value) noexcept;
    [[nodiscard]] CheckboxSize size() const noexcept;
    CheckboxNode& shape(CheckboxShape value) noexcept;
    void setShape(CheckboxShape value) noexcept;
    [[nodiscard]] CheckboxShape shape() const noexcept;
    CheckboxNode& labelPosition(CheckboxLabelPosition value) noexcept;
    void setLabelPosition(CheckboxLabelPosition value) noexcept;
    [[nodiscard]] CheckboxLabelPosition labelPosition() const noexcept;
    CheckboxNode& required(bool value = true) noexcept;
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
