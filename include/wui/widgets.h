#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "wui/node.h"
#include "wui/state.h"

namespace wui {

namespace detail {
class ImageResource;
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

class Text : public Node {
public:
    explicit Text(std::string value = {});

    [[nodiscard]] const std::string& value() const noexcept;
    Text& value(std::string value);
    void setValue(std::string value);

    [[nodiscard]] float fontSize() const noexcept;
    void setFontSize(float size) noexcept;
    [[nodiscard]] float lineHeight() const noexcept;
    void setLineHeight(float height) noexcept;

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
    float lineHeight_{0.0f};
    TextWrap wrap_{TextWrap::NoWrap};
    TextOverflow overflow_{TextOverflow::Clip};
    std::size_t maxLines_{0};
    Color color_{};
    bool hasColor_{false};
};

enum class ImageFit {
    Fill,
    Contain,
    Cover,
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

    [[nodiscard]] const ImageSource imageSource() const noexcept;

    Image& fit(ImageFit fit) noexcept;
    void setFit(ImageFit fit) noexcept;
    [[nodiscard]] ImageFit fit() const noexcept;

    Image& align(float x, float y) noexcept;
    void setAlignment(float x, float y) noexcept;
    [[nodiscard]] PointF alignment() const noexcept;
    [[nodiscard]] SizeF intrinsicSize() const noexcept;
    [[nodiscard]] bool hasSource() const noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void prepare(PaintContext& context) override;
    void paint(PaintContext& context) override;

private:
    ImageSource source_;
    ImageFit fit_{ImageFit::Contain};
    PointF alignment_{0.5f, 0.5f};

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
    Primary,
    Ghost,
    Danger,
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

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;
    bool onPointerEvent(const PointerEvent& event) override;

private:
    std::string label_;
    ClickHandler onClick_;
    ButtonVariant variant_{ButtonVariant::Primary};
};

// A two-state form control. Checkbox owns its checked value unless bound to a
// State<bool>; the binding remains the source of truth for external changes.
class Checkbox : public ControlNode {
public:
    using ChangeHandler = std::function<void(bool)>;

    explicit Checkbox(std::string label = {}, bool checked = false);

    [[nodiscard]] const std::string& label() const noexcept;
    Checkbox& label(std::string label);
    void setLabel(std::string label);

    [[nodiscard]] bool isChecked() const noexcept;
    Checkbox& checked(bool value);
    void setChecked(bool value);
    Checkbox& bind(State<bool>& state);
    Checkbox& onChange(ChangeHandler handler);

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;
    bool onPointerEvent(const PointerEvent& event) override;
    bool onKeyEvent(const KeyEvent& event) override;

private:
    void toggle();
    std::string label_;
    bool checked_{false};
    std::optional<Binding<bool>> binding_;
    bool hasBinding_{false};
    ChangeHandler onChange_;
};

} // namespace wui
