#pragma once

// Declarative builder authoring API (ADR-005).
//
// A thin, header-only, move-only builder layer that wraps the retained
// `wui::` node tree so page authors can write:
//
//     using namespace wui::ui;
//     auto page = Column()
//         .padding(16)
//         .gap(8)
//         .children(
//             Text("Settings"),
//             Button("Close").onClick([&] { nav.pop(); })
//         );
//     root.setContent(page);
//
// Ownership stays `std::unique_ptr` (single owner per node). Children are
// passed variadically (C++17 fold expression) so move-only nodes transfer
// cleanly without the `std::initializer_list` move restriction.

#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "wui/node.h"
#include "wui/accordion.h"
#include "wui/avatar.h"
#include "wui/basic_controls.h"
#include "wui/feedback.h"
#include "wui/form_feedback.h"
#include "wui/icons.h"
#include "wui/drawer.h"
#include "wui/badge.h"
#include "wui/date_time.h"
#include "wui/list_view.h"
#include "wui/navigation.h"
#include "wui/overlays.h"
#include "wui/popover.h"
#include "wui/persona.h"
#include "wui/rating.h"
#include "wui/selection.h"
#include "wui/virtual_list.h"
#include "wui/scheduler.h"
#include "wui/state.h"
#include "wui/structural.h"
#include "wui/table.h"
#include "wui/text_input.h"
#include "wui/tree.h"
#include "wui/types.h"
#include "wui/widgets.h"

namespace wui::ui {

// A raw unique_ptr<Node> passes through unchanged.
inline std::unique_ptr<Node> asNode(std::unique_ptr<Node> node)
{
    return node;
}

// Any builder (something with `.intoNode()`) is consumed into a node.
template <class Builder, class = decltype(std::declval<Builder&&>().intoNode())>
std::unique_ptr<Node> asNode(Builder&& builder)
{
    return std::forward<Builder>(builder).intoNode();
}

// CRTP base: owns the node, provides variadic children and ownership transfer.
template <class Self, class NodeT>
class BuilderBase {
public:
    template <class... Args>
    explicit BuilderBase(Args&&... args)
        : node_(std::make_unique<NodeT>(std::forward<Args>(args)...))
    {
    }

    // Append any number of child builders (or raw unique_ptr<Node>).
    template <class... Children>
    Self&& children(Children&&... items) &&
    {
        (node_->appendChild(asNode(std::forward<Children>(items))), ...);
        return std::move(self());
    }

    // Main-axis flex weight for Row/Column layout (0 = fixed size).
    Self&& flex(float weight) &&
    {
        node_->setFlex(weight);
        return std::move(self());
    }

    // Stable native-automation identity, especially important for keyed rows
    // whose visual Node instances may be reconstructed after state changes.
    Self&& accessibilityId(std::string id) &&
    {
        node_->setAccessibilityId(std::move(id));
        return std::move(self());
    }

    // Hand ownership to a parent or to setContent().
    std::unique_ptr<Node> intoNode() { return std::move(node_); }
    operator std::unique_ptr<Node>() { return std::move(node_); }

    NodeT* operator->() noexcept { return node_.get(); }
    NodeT& operator*() noexcept { return *node_; }
    [[nodiscard]] NodeT* get() const noexcept { return node_.get(); }

protected:
    Self&& self() noexcept { return std::move(static_cast<Self&>(*this)); }

    std::unique_ptr<NodeT> node_;
};

class Text : public BuilderBase<Text, wui::Text> {
public:
    explicit Text(std::string value = {})
        : BuilderBase(std::move(value))
    {
    }

    Text&& text(std::string value) &&
    {
        node_->setValue(std::move(value));
        return std::move(self());
    }

    Text&& size(float fontSize) &&
    {
        node_->setFontSize(fontSize);
        return std::move(self());
    }

    Text&& weight(int fontWeight) &&
    {
        node_->setFontWeight(fontWeight);
        return std::move(self());
    }

    Text&& lineHeight(float height) &&
    {
        node_->setLineHeight(height);
        return std::move(self());
    }

    Text&& style(const wui::TextStyleToken& value) &&
    {
        node_->setTextStyle(value);
        return std::move(self());
    }

    Text&& role(wui::TextRole value) && { node_->setRole(value); return std::move(self()); }
    Text&& align(wui::TextAlign value) && { node_->setAlignment(value); return std::move(self()); }
    Text&& underline(bool value = true) && { node_->setUnderline(value); return std::move(self()); }
    Text&& strikethrough(bool value = true) && { node_->setStrikethrough(value); return std::move(self()); }

    Text&& wrap(wui::TextWrap value = wui::TextWrap::Word) &&
    {
        node_->setWrap(value);
        return std::move(self());
    }

    Text&& maxLines(std::size_t value) &&
    {
        node_->setMaxLines(value);
        return std::move(self());
    }

    Text&& ellipsis(bool enabled = true) &&
    {
        node_->setOverflow(enabled ? wui::TextOverflow::Ellipsis : wui::TextOverflow::Clip);
        return std::move(self());
    }

    Text&& color(Color color) &&
    {
        node_->setColor(color);
        return std::move(self());
    }

    // Reactive: re-render the text whenever the observable source (State or
    // Computed) changes.
    template <class Observable, class Format>
    Text&& bind(Observable& source, Format format) &&
    {
        wui::Text* raw = node_.get();
        struct Subscription {
            std::size_t id{0};
            bool active{false};
        };
        auto subscription = std::make_shared<Subscription>();
        auto connect = [raw, source = &source, format, subscription] {
            raw->setValue(format(source->get()));
            if (subscription->active) {
                return;
            }
            subscription->id = source->subscribe([raw, format](const auto& value) {
                raw->setValue(format(value));
            });
            subscription->active = true;
        };
        auto disconnect = [source = &source, subscription] {
            if (!subscription->active) {
                return;
            }
            source->unsubscribe(subscription->id);
            subscription->active = false;
        };
        // Bind immediately for detached construction (useful for headless
        // composition), then pause updates whenever the node leaves a live
        // UI tree. Reattachment refreshes from the source before reconnecting.
        connect();
        raw->addAttachCallback(connect);
        raw->addDetachCallback(disconnect);
        raw->addTeardown(disconnect);
        return std::move(self());
    }

    // Convenience for State<std::string>.
    Text&& bind(wui::State<std::string>& state) &&
    {
        return std::move(*this).bind(state, [](const std::string& value) { return value; });
    }
};

class Icon : public BuilderBase<Icon, wui::Icon> {
public:
    explicit Icon(wui::IconName name = wui::IconName::Info)
        : BuilderBase(name) {}
    Icon&& name(wui::IconName value) && { node_->setName(value); return std::move(self()); }
    Icon&& size(wui::IconSize value) && { node_->setSize(value); return std::move(self()); }
    Icon&& style(wui::IconStyle value) && { node_->setStyle(value); return std::move(self()); }
    Icon&& color(Color value) && { node_->setColor(value); return std::move(self()); }
};

class Image : public BuilderBase<Image, wui::Image> {
public:
    Image() : BuilderBase() {}

    Image(std::vector<unsigned char> rgbaPixels, int pixelWidth, int pixelHeight)
        : BuilderBase(std::move(rgbaPixels), pixelWidth, pixelHeight)
    {
    }

    Image&& source(std::vector<unsigned char> rgbaPixels, int pixelWidth, int pixelHeight) &&
    {
        node_->setSource(std::move(rgbaPixels), pixelWidth, pixelHeight);
        return std::move(self());
    }

    Image&& fallback(std::vector<unsigned char> rgbaPixels, int pixelWidth, int pixelHeight) &&
    {
        node_->fallback(std::move(rgbaPixels), pixelWidth, pixelHeight);
        return std::move(self());
    }

    Image&& fit(wui::ImageFit fit) &&
    {
        node_->setFit(fit);
        return std::move(self());
    }

    Image&& align(float x, float y) &&
    {
        node_->setAlignment(x, y);
        return std::move(self());
    }

    Image&& shape(wui::ImageShape value) && { node_->setShape(value); return std::move(self()); }
    Image&& bordered(bool value = true) && { node_->setBordered(value); return std::move(self()); }
    Image&& shadow(bool value = true) && { node_->setShadow(value); return std::move(self()); }
    Image&& block(bool value = true) && { node_->setBlock(value); return std::move(self()); }
    Image&& alt(std::string value) && { node_->setAlt(std::move(value)); return std::move(self()); }
    Image&& decorative(bool value = true) && { node_->setDecorative(value); return std::move(self()); }
};

class Box : public BuilderBase<Box, wui::Container> {
public:
    Box() : BuilderBase() {}

    Box&& background(Color color) &&
    {
        node_->setBackground(color);
        return std::move(self());
    }

    Box&& radius(float radius) &&
    {
        node_->setRadius(radius);
        return std::move(self());
    }

    Box&& padding(InsetsF padding) &&
    {
        node_->setPadding(padding);
        return std::move(self());
    }

    Box&& padding(float all) &&
    {
        node_->setPadding(InsetsF{all, all, all, all});
        return std::move(self());
    }

    Box&& contentAlign(Alignment horizontal, Alignment vertical) &&
    {
        node_->setContentAlignment(horizontal, vertical);
        return std::move(self());
    }

    Box&& width(float width) &&
    {
        node_->setWidth(width);
        return std::move(self());
    }

    Box&& height(float height) &&
    {
        node_->setHeight(height);
        return std::move(self());
    }
};

class Spacer : public BuilderBase<Spacer, wui::Spacer> {
public:
    explicit Spacer(float width = 0.0f, float height = 0.0f)
        : BuilderBase(wui::SizeF{width, height})
    {
    }
};

class TextField : public BuilderBase<TextField, wui::TextInput> {
public:
    explicit TextField(std::string placeholder = {})
        : BuilderBase(std::move(placeholder))
    {
    }

    TextField&& placeholder(std::string value) &&
    {
        node_->setPlaceholder(std::move(value));
        return std::move(self());
    }

    TextField&& onChange(wui::TextInput::ChangeHandler handler) &&
    {
        node_->onChange(std::move(handler));
        return std::move(self());
    }

    TextField&& onSubmit(wui::TextInput::SubmitHandler handler) &&
    {
        node_->onSubmit(std::move(handler));
        return std::move(self());
    }

    TextField&& onCancel(wui::TextInput::CancelHandler handler) &&
    {
        node_->onCancel(std::move(handler));
        return std::move(self());
    }

    TextField&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
    TextField&& size(wui::InputSize value) && { node_->setSize(value); return std::move(self()); }
    TextField&& appearance(wui::InputAppearance value) && { node_->setAppearance(value); return std::move(self()); }
    TextField&& invalid(bool value = true) && { node_->setInvalid(value); return std::move(self()); }
    TextField&& motionEnabled(bool value = true) && { node_->setMotionEnabled(value); return std::move(self()); }
};

class TextArea : public BuilderBase<TextArea, wui::TextArea> {
public:
    explicit TextArea(std::string placeholder = {})
        : BuilderBase(std::move(placeholder))
    {
    }

    TextArea&& placeholder(std::string value) && { node_->setPlaceholder(std::move(value)); return std::move(self()); }
    TextArea&& onChange(wui::TextInput::ChangeHandler handler) && { node_->onChange(std::move(handler)); return std::move(self()); }
    TextArea&& onCancel(wui::TextInput::CancelHandler handler) && { node_->onCancel(std::move(handler)); return std::move(self()); }
    TextArea&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
    TextArea&& size(wui::InputSize value) && { node_->setSize(value); return std::move(self()); }
    TextArea&& appearance(wui::InputAppearance value) && { node_->setAppearance(value); return std::move(self()); }
    TextArea&& invalid(bool value = true) && { node_->setInvalid(value); return std::move(self()); }
    TextArea&& motionEnabled(bool value = true) && { node_->setMotionEnabled(value); return std::move(self()); }
    TextArea&& rows(std::size_t value) && { node_->setRows(value); return std::move(self()); }
};

class Card : public BuilderBase<Card, wui::Card> {
public:
    Card() : BuilderBase() {}
    Card&& appearance(wui::CardAppearance value) && { node_->setAppearance(value); return std::move(self()); }
    Card&& size(wui::CardSize value) && { node_->setSize(value); return std::move(self()); }
    Card&& orientation(wui::CardOrientation value) && { node_->setOrientation(value); return std::move(self()); }
    Card&& selected(bool value = true) && { node_->setSelected(value); return std::move(self()); }
    Card&& selectable(bool value = true) && { node_->selectable(value); return std::move(self()); }
    Card&& onSelectionChange(wui::Card::ChangeHandler value) && { node_->onSelectionChange(std::move(value)); return std::move(self()); }
};

class CardHeader : public BuilderBase<CardHeader, wui::CardHeader> {
public:
    CardHeader(std::string title = {}, std::string description = {}) : BuilderBase(std::move(title), std::move(description)) {}
    CardHeader&& media(std::unique_ptr<wui::Node> value) && { node_->media(std::move(value)); return std::move(self()); }
    CardHeader&& action(std::unique_ptr<wui::Node> value) && { node_->action(std::move(value)); return std::move(self()); }
};

class CardPreview : public BuilderBase<CardPreview, wui::CardPreview> {
public:
    CardPreview() : BuilderBase() {}
    CardPreview&& height(float value) && { node_->setHeight(value); return std::move(self()); }
};

class CardFooter : public BuilderBase<CardFooter, wui::CardFooter> {
public:
    CardFooter() : BuilderBase() {}
};

class Label : public BuilderBase<Label, wui::Label> {
public:
    explicit Label(std::string text = {}) : BuilderBase(std::move(text)) {}
    Label&& size(wui::LabelSize value) && { node_->setSize(value); return std::move(self()); }
    Label&& required(bool value = true) && { node_->setRequired(value); return std::move(self()); }
    Label&& forControl(wui::TextInput* control) && { node_->setForControl(control); return std::move(self()); }
};

class Field : public BuilderBase<Field, wui::Field> {
public:
    explicit Field(std::string label = {}) : BuilderBase(std::move(label)) {}
    Field&& label(std::string value) && { node_->setLabel(std::move(value)); return std::move(self()); }
    Field&& hint(std::string value) && { node_->setHint(std::move(value)); return std::move(self()); }
    Field&& validationMessage(std::string value) && { node_->setValidationMessage(std::move(value)); return std::move(self()); }
    Field&& validationState(wui::FieldValidationState value) && { node_->setValidationState(value); return std::move(self()); }
    Field&& required(bool value = true) && { node_->setRequired(value); return std::move(self()); }
    Field&& orientation(wui::FieldOrientation value) && { node_->setOrientation(value); return std::move(self()); }
    Field&& enabled(bool value) && { node_->setEnabled(value); return std::move(self()); }
    template <class Child>
    Field&& control(Child&& value) && { node_->setControl(asNode(std::forward<Child>(value))); return std::move(self()); }
};

class MessageBar : public BuilderBase<MessageBar, wui::MessageBar> {
public:
    explicit MessageBar(std::string body = {}) : BuilderBase(std::move(body)) {}
    MessageBar&& title(std::string value) && { node_->setTitle(std::move(value)); return std::move(self()); }
    MessageBar&& body(std::string value) && { node_->setBody(std::move(value)); return std::move(self()); }
    MessageBar&& intent(wui::MessageBarIntent value) && { node_->setIntent(value); return std::move(self()); }
    MessageBar&& multiline(bool value = true) && { node_->setMultiline(value); return std::move(self()); }
    MessageBar&& action(wui::MessageBarAction value) && { node_->addAction(std::move(value)); return std::move(self()); }
    MessageBar&& dismissible(bool value = true) && { node_->setDismissible(value); return std::move(self()); }
    MessageBar&& onDismiss(wui::MessageBar::DismissHandler value) && { node_->onDismiss(std::move(value)); return std::move(self()); }
};

class Button : public BuilderBase<Button, wui::Button> {
public:
    explicit Button(std::string label = {})
        : BuilderBase(std::move(label))
    {
    }

    Button&& label(std::string label) &&
    {
        node_->setLabel(std::move(label));
        return std::move(self());
    }

    Button&& onClick(std::function<void()> handler) &&
    {
        node_->onClick(std::move(handler));
        return std::move(self());
    }

    Button&& variant(wui::ButtonVariant variant) &&
    {
        node_->setVariant(variant);
        return std::move(self());
    }

    Button&& appearance(wui::ButtonAppearance value) &&
    {
        node_->setAppearance(value);
        return std::move(self());
    }

    Button&& size(wui::ButtonSize value) &&
    {
        node_->setSize(value);
        return std::move(self());
    }

    Button&& shape(wui::ButtonShape value) &&
    {
        node_->setShape(value);
        return std::move(self());
    }

    Button&& icon(wui::IconName value) &&
    {
        node_->setIcon(value);
        return std::move(self());
    }

    Button&& iconStyle(wui::IconStyle value) &&
    {
        node_->setIconStyle(value);
        return std::move(self());
    }

    Button&& iconPosition(wui::ButtonIconPosition value) &&
    {
        node_->setIconPosition(value);
        return std::move(self());
    }

    Button&& iconOnly(bool value = true) &&
    {
        node_->setIconOnly(value);
        return std::move(self());
    }
};

class Checkbox : public BuilderBase<Checkbox, wui::Checkbox> {
public:
    explicit Checkbox(std::string label = {}, bool checked = false) : BuilderBase(std::move(label), checked) {}
    Checkbox&& label(std::string value) && { node_->setLabel(std::move(value)); return std::move(self()); }
    Checkbox&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
    Checkbox&& checked(bool value) && { node_->setChecked(value); return std::move(self()); }
    Checkbox&& mixed(bool value = true) && { node_->setMixed(value); return std::move(self()); }
    Checkbox&& checkState(wui::CheckboxState value) && { node_->setCheckState(value); return std::move(self()); }
    Checkbox&& bind(wui::State<bool>& state) && { node_->bind(state); return std::move(self()); }
    Checkbox&& onChange(std::function<void(bool)> handler) && { node_->onChange(std::move(handler)); return std::move(self()); }
    Checkbox&& onStateChange(std::function<void(wui::CheckboxState)> handler) && { node_->onStateChange(std::move(handler)); return std::move(self()); }
    Checkbox&& size(wui::CheckboxSize value) && { node_->setSize(value); return std::move(self()); }
    Checkbox&& shape(wui::CheckboxShape value) && { node_->setShape(value); return std::move(self()); }
    Checkbox&& labelPosition(wui::CheckboxLabelPosition value) && { node_->setLabelPosition(value); return std::move(self()); }
    Checkbox&& required(bool value = true) && { node_->setRequired(value); return std::move(self()); }
    Checkbox&& enabled(bool value) && { node_->setEnabled(value); return std::move(self()); }
};

class ToggleButton : public BuilderBase<ToggleButton, wui::ToggleButton> {
public:
    explicit ToggleButton(std::string label = {}, bool checked = false) : BuilderBase(std::move(label), checked) {}
    ToggleButton&& label(std::string value) && { node_->setLabel(std::move(value)); return std::move(self()); }
    ToggleButton&& checked(bool value) && { node_->setChecked(value); return std::move(self()); }
    ToggleButton&& bind(wui::State<bool>& state) && { node_->bind(state); return std::move(self()); }
    ToggleButton&& onChange(std::function<void(bool)> handler) && { node_->onChange(std::move(handler)); return std::move(self()); }
    ToggleButton&& size(wui::ButtonSize value) && { node_->setSize(value); return std::move(self()); }
    ToggleButton&& shape(wui::ButtonShape value) && { node_->setShape(value); return std::move(self()); }
    ToggleButton&& appearance(wui::ButtonAppearance value) && { node_->setAppearance(value); return std::move(self()); }
    ToggleButton&& icon(wui::IconName value) && { node_->setIcon(value); return std::move(self()); }
    ToggleButton&& iconStyle(wui::IconStyle value) && { node_->setIconStyle(value); return std::move(self()); }
    ToggleButton&& iconPosition(wui::ButtonIconPosition value) && { node_->setIconPosition(value); return std::move(self()); }
    ToggleButton&& iconOnly(bool value = true) && { node_->setIconOnly(value); return std::move(self()); }
};

class CompoundButton : public BuilderBase<CompoundButton, wui::CompoundButton> {
public:
    CompoundButton(std::string label = {}, std::string secondaryContent = {})
        : BuilderBase(std::move(label), std::move(secondaryContent)) {}
    CompoundButton&& label(std::string value) && { node_->setLabel(std::move(value)); return std::move(self()); }
    CompoundButton&& secondaryContent(std::string value) && { node_->setSecondaryContent(std::move(value)); return std::move(self()); }
    CompoundButton&& onClick(std::function<void()> handler) && { node_->onClick(std::move(handler)); return std::move(self()); }
    CompoundButton&& appearance(wui::ButtonAppearance value) && { node_->setAppearance(value); return std::move(self()); }
    CompoundButton&& size(wui::ButtonSize value) && { node_->setSize(value); return std::move(self()); }
    CompoundButton&& shape(wui::ButtonShape value) && { node_->setShape(value); return std::move(self()); }
};

class Radio : public BuilderBase<Radio, wui::Radio> {
public:
    explicit Radio(std::string label = {}, bool selected = false) : BuilderBase(std::move(label), selected) {}
    Radio&& selected(bool value) && { node_->setSelected(value); return std::move(self()); }
    Radio&& bind(wui::State<bool>& state) && { node_->bind(state); return std::move(self()); }
    Radio&& onChange(std::function<void(bool)> handler) && { node_->onChange(std::move(handler)); return std::move(self()); }
};

class RadioGroup : public BuilderBase<RadioGroup, wui::RadioGroup> {
public:
    RadioGroup() : BuilderBase() {}
    RadioGroup&& option(std::string value, std::string label, bool enabled = true) &&
    {
        node_->addOption(std::move(value), std::move(label), enabled);
        return std::move(self());
    }
    RadioGroup&& name(std::string value) && { node_->setName(std::move(value)); return std::move(self()); }
    RadioGroup&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
    RadioGroup&& value(std::string value) && { node_->setValue(std::move(value)); return std::move(self()); }
    RadioGroup&& bind(wui::State<std::string>& state) && { node_->bind(state); return std::move(self()); }
    RadioGroup&& onChange(std::function<void(const std::string&)> handler) && { node_->onChange(std::move(handler)); return std::move(self()); }
    RadioGroup&& layout(wui::RadioGroupLayout value) && { node_->setGroupLayout(value); return std::move(self()); }
    RadioGroup&& required(bool value = true) && { node_->setRequired(value); return std::move(self()); }
    RadioGroup&& enabled(bool value) && { node_->setEnabled(value); return std::move(self()); }
};

class Switch : public BuilderBase<Switch, wui::Switch> {
public:
    explicit Switch(std::string label = {}, bool on = false) : BuilderBase(std::move(label), on) {}
    Switch&& label(std::string value) && { node_->setLabel(std::move(value)); return std::move(self()); }
    Switch&& on(bool value) && { node_->setOn(value); return std::move(self()); }
    Switch&& bind(wui::State<bool>& state) && { node_->bind(state); return std::move(self()); }
    Switch&& onChange(std::function<void(bool)> handler) && { node_->onChange(std::move(handler)); return std::move(self()); }
    Switch&& size(wui::SwitchSize value) && { node_->setSize(value); return std::move(self()); }
    Switch&& labelPosition(wui::SwitchLabelPosition value) && { node_->setLabelPosition(value); return std::move(self()); }
    Switch&& required(bool value = true) && { node_->setRequired(value); return std::move(self()); }
    Switch&& enabled(bool value) && { node_->setEnabled(value); return std::move(self()); }
};

class Slider : public BuilderBase<Slider, wui::Slider> {
public:
    Slider(float minimum = 0.0f, float maximum = 100.0f, float value = 0.0f) : BuilderBase(minimum, maximum, value) {}
    Slider&& value(float value) && { node_->setValue(value); return std::move(self()); }
    Slider&& step(float value) && { node_->setStep(value); return std::move(self()); }
    Slider&& bind(wui::State<float>& state) && { node_->bind(state); return std::move(self()); }
    Slider&& onChange(std::function<void(float)> handler) && { node_->onChange(std::move(handler)); return std::move(self()); }
    Slider&& size(wui::SliderSize value) && { node_->setSize(value); return std::move(self()); }
    Slider&& orientation(wui::SliderOrientation value) && { node_->setOrientation(value); return std::move(self()); }
    Slider&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
    Slider&& enabled(bool value) && { node_->setEnabled(value); return std::move(self()); }
};

class ProgressBar : public BuilderBase<ProgressBar, wui::ProgressBar> {
public:
    ProgressBar(float minimum = 0.0f, float maximum = 1.0f,
                std::optional<float> value = std::nullopt)
        : BuilderBase(minimum, maximum, value) {}
    ProgressBar&& value(float value) && { node_->setValue(value); return std::move(self()); }
    ProgressBar&& bind(wui::State<float>& state) && { node_->bind(state); return std::move(self()); }
    ProgressBar&& indeterminate(bool value = true) && { node_->setIndeterminate(value); return std::move(self()); }
    ProgressBar&& color(wui::ProgressBarColor value) && { node_->setColor(value); return std::move(self()); }
    ProgressBar&& shape(wui::ProgressBarShape value) && { node_->setShape(value); return std::move(self()); }
    ProgressBar&& thickness(wui::ProgressBarThickness value) && { node_->setThickness(value); return std::move(self()); }
    ProgressBar&& motionEnabled(bool value) && { node_->setMotionEnabled(value); return std::move(self()); }
    ProgressBar&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

class Toast : public BuilderBase<Toast, wui::Toast> {
public:
    Toast(std::string title = {}, std::string body = {}) : BuilderBase(std::move(title), std::move(body)) {}
    Toast&& title(std::string value) && { node_->setTitle(std::move(value)); return std::move(self()); }
    Toast&& body(std::string value) && { node_->setBody(std::move(value)); return std::move(self()); }
    Toast&& intent(wui::ToastIntent value) && { node_->setIntent(value); return std::move(self()); }
    Toast&& position(wui::ToastPosition value) && { node_->setPosition(value); return std::move(self()); }
    Toast&& action(std::string label, std::function<void()> handler) && { node_->setAction(std::move(label), std::move(handler)); return std::move(self()); }
    Toast&& timeout(std::chrono::milliseconds value) && { node_->setTimeout(value); return std::move(self()); }
};

class Spinner : public BuilderBase<Spinner, wui::Spinner> {
public:
    explicit Spinner(std::string label = {}) : BuilderBase(std::move(label)) {}
    Spinner&& label(std::string value) && { node_->setLabel(std::move(value)); return std::move(self()); }
    Spinner&& size(wui::SpinnerSize value) && { node_->setSize(value); return std::move(self()); }
    Spinner&& labelPosition(wui::SpinnerLabelPosition value) && { node_->setLabelPosition(value); return std::move(self()); }
    Spinner&& motionEnabled(bool value) && { node_->setMotionEnabled(value); return std::move(self()); }
};

class Divider : public BuilderBase<Divider, wui::Divider> {
public:
    explicit Divider(wui::DividerOrientation orientation = wui::DividerOrientation::Horizontal) : BuilderBase(orientation) {}
    Divider&& thickness(float value) && { node_->setThickness(value); return std::move(self()); }
    Divider&& content(std::string value) && { node_->setContent(std::move(value)); return std::move(self()); }
    Divider&& appearance(wui::DividerAppearance value) && { node_->setAppearance(value); return std::move(self()); }
    Divider&& contentAlignment(wui::DividerContentAlignment value) && { node_->setContentAlignment(value); return std::move(self()); }
    Divider&& inset(bool value = true) && { node_->setInset(value); return std::move(self()); }
};

class Badge : public BuilderBase<Badge, wui::Badge> {
public:
    explicit Badge(std::string text = {}) : BuilderBase(std::move(text)) {}
    Badge&& text(std::string value) && { node_->setText(std::move(value)); return std::move(self()); }
    Badge&& appearance(wui::BadgeAppearance value) && { node_->setAppearance(value); return std::move(self()); }
    Badge&& color(wui::BadgeColor value) && { node_->setColor(value); return std::move(self()); }
    Badge&& size(wui::BadgeSize value) && { node_->setSize(value); return std::move(self()); }
    Badge&& shape(wui::BadgeShape value) && { node_->setShape(value); return std::move(self()); }
    Badge&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

class CounterBadge : public BuilderBase<CounterBadge, wui::CounterBadge> {
public:
    explicit CounterBadge(std::uint64_t count = 0) : BuilderBase(count) {}
    CounterBadge&& count(std::uint64_t value) && { node_->setCount(value); return std::move(self()); }
    CounterBadge&& max(std::uint64_t value) && { node_->setMax(value); return std::move(self()); }
    CounterBadge&& showZero(bool value = true) && { node_->setShowZero(value); return std::move(self()); }
    CounterBadge&& size(wui::BadgeSize value) && { node_->setSize(value); return std::move(self()); }
    CounterBadge&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

class PresenceBadge : public BuilderBase<PresenceBadge, wui::PresenceBadge> {
public:
    explicit PresenceBadge(wui::PresenceStatus status = wui::PresenceStatus::Available) : BuilderBase(status) {}
    PresenceBadge&& status(wui::PresenceStatus value) && { node_->setStatus(value); return std::move(self()); }
    PresenceBadge&& position(wui::PresenceBadgePosition value) && { node_->setPosition(value); return std::move(self()); }
    PresenceBadge&& avatarSize(float value) && { node_->setAvatarSize(value); return std::move(self()); }
    PresenceBadge&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

class Avatar : public BuilderBase<Avatar, wui::Avatar> {
public:
    explicit Avatar(std::string name = {}, wui::AvatarSize size = wui::AvatarSize::Size32)
        : BuilderBase(std::move(name), size) {}
    Avatar&& initials(std::string value) && { node_->setInitials(std::move(value)); return std::move(self()); }
    Avatar&& image(wui::ImageSource source) && { node_->setImage(std::move(source)); return std::move(self()); }
    Avatar&& size(wui::AvatarSize value) && { node_->setSize(value); return std::move(self()); }
    Avatar&& shape(wui::AvatarShape value) && { node_->setShape(value); return std::move(self()); }
    Avatar&& color(wui::AvatarColor value) && { node_->setColor(value); return std::move(self()); }
    Avatar&& active(bool value = true) && { node_->setActive(value); return std::move(self()); }
    Avatar&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

class AvatarGroup : public BuilderBase<AvatarGroup, wui::AvatarGroup> {
public:
    AvatarGroup() : BuilderBase() {}
    AvatarGroup&& avatar(std::string name, wui::AvatarSize size = wui::AvatarSize::Size32) &&
    { node_->addAvatar(std::move(name), size); return std::move(self()); }
    AvatarGroup&& maxVisible(std::size_t value) && { node_->setMaxVisible(value); return std::move(self()); }
    AvatarGroup&& layout(wui::AvatarGroupLayout value) && { node_->setGroupLayout(value); return std::move(self()); }
    AvatarGroup&& size(wui::AvatarSize value) && { node_->setSize(value); return std::move(self()); }
    AvatarGroup&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

class Persona : public BuilderBase<Persona, wui::Persona> {
public:
    explicit Persona(std::string name = {}, wui::PersonaSize size = wui::PersonaSize::Medium)
        : BuilderBase(std::move(name), size) {}
    Persona&& primaryText(std::string value) && { node_->setPrimaryText(std::move(value)); return std::move(self()); }
    Persona&& secondaryText(std::string value) && { node_->setSecondaryText(std::move(value)); return std::move(self()); }
    Persona&& tertiaryText(std::string value) && { node_->setTertiaryText(std::move(value)); return std::move(self()); }
    Persona&& quaternaryText(std::string value) && { node_->setQuaternaryText(std::move(value)); return std::move(self()); }
    Persona&& size(wui::PersonaSize value) && { node_->setSize(value); return std::move(self()); }
    Persona&& avatarColor(wui::AvatarColor value) && { node_->setAvatarColor(value); return std::move(self()); }
    Persona&& avatarShape(wui::AvatarShape value) && { node_->setAvatarShape(value); return std::move(self()); }
    Persona&& avatarImage(wui::ImageSource value) && { node_->setAvatarImage(std::move(value)); return std::move(self()); }
    Persona&& presence(wui::PresenceStatus value) && { node_->setPresence(value); return std::move(self()); }
    Persona&& presenceOnly(bool value = true) && { node_->setPresenceOnly(value); return std::move(self()); }
    Persona&& textPosition(wui::PersonaTextPosition value) && { node_->setTextPosition(value); return std::move(self()); }
    Persona&& textAlignment(wui::PersonaTextAlignment value) && { node_->setTextAlignment(value); return std::move(self()); }
    Persona&& onClick(wui::Persona::ClickHandler handler) && { node_->onClick(std::move(handler)); return std::move(self()); }
    Persona&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

class Calendar : public BuilderBase<Calendar, wui::Calendar> {
public:
    Calendar() : BuilderBase() {}
    Calendar&& displayedMonth(wui::CivilDate value) && { node_->setDisplayedMonth(value); return std::move(self()); }
    Calendar&& selectedDate(std::optional<wui::CivilDate> value) && { node_->setSelectedDate(value); return std::move(self()); }
    Calendar&& selectionMode(wui::CalendarSelectionMode value) && { node_->setSelectionMode(value); return std::move(self()); }
    Calendar&& minimumDate(std::optional<wui::CivilDate> value) && { node_->minimumDate(value); return std::move(self()); }
    Calendar&& maximumDate(std::optional<wui::CivilDate> value) && { node_->maximumDate(value); return std::move(self()); }
};

class DatePicker : public BuilderBase<DatePicker, wui::DatePicker> {
public:
    explicit DatePicker(std::string placeholder = "Select a date") : BuilderBase(std::move(placeholder)) {}
    DatePicker&& value(std::optional<wui::CivilDate> value) && { node_->setValue(value); return std::move(self()); }
    DatePicker&& text(std::string value) && { node_->text(std::move(value)); return std::move(self()); }
    DatePicker&& overlayHost(wui::OverlayHost& host) && { node_->bindOverlayHost(host); return std::move(self()); }
};

class TimePicker : public BuilderBase<TimePicker, wui::TimePicker> {
public:
    explicit TimePicker(std::string placeholder = "Select a time") : BuilderBase(std::move(placeholder)) {}
    TimePicker&& value(std::optional<wui::CivilTime> value) && { node_->setValue(value); return std::move(self()); }
    TimePicker&& text(std::string value) && { node_->text(std::move(value)); return std::move(self()); }
    TimePicker&& minuteStep(int value) && { node_->minuteStep(value); return std::move(self()); }
};

class Table : public BuilderBase<Table, wui::Table> {
public:
    explicit Table(std::vector<wui::TableColumn> columns = {}) : BuilderBase(std::move(columns)) {}
    Table&& rows(std::vector<wui::TableRow> value) && { node_->setRows(std::move(value)); return std::move(self()); }
    Table&& maxVisibleRows(std::size_t value) && { node_->maxVisibleRows(value); return std::move(self()); }
    Table&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

class DataGrid : public BuilderBase<DataGrid, wui::DataGrid> {
public:
    DataGrid() : BuilderBase() {}
    DataGrid&& columns(std::vector<wui::TableColumn> value) && { node_->setColumns(std::move(value)); return std::move(self()); }
    DataGrid&& rows(std::vector<wui::TableRow> value) && { node_->setRows(std::move(value)); return std::move(self()); }
    DataGrid&& selectionMode(wui::DataGridSelectionMode value) && { node_->selectionMode(value); return std::move(self()); }
    DataGrid&& selectedRows(std::vector<std::size_t> value) && { node_->selectedRows(std::move(value)); return std::move(self()); }
    DataGrid&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

class Tree : public BuilderBase<Tree, wui::Tree> {
public:
    Tree() : BuilderBase() {}
    Tree&& item(std::string id, std::string label) && { node_->addItem(std::move(id), std::move(label)); return std::move(self()); }
    Tree&& maxVisibleItems(std::size_t value) && { node_->setMaxVisibleItems(value); return std::move(self()); }
    Tree&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

class AccordionItem : public BuilderBase<AccordionItem, wui::AccordionItem> {
public:
    AccordionItem(std::string header = {}, std::string body = {}) : BuilderBase(std::move(header), std::move(body)) {}
    AccordionItem&& expanded(bool value = true) && { node_->setExpanded(value); return std::move(self()); }
    template <class Content>
    AccordionItem&& content(Content&& value) && { node_->setContent(asNode(std::forward<Content>(value))); return std::move(self()); }
};

class Accordion : public BuilderBase<Accordion, wui::Accordion> {
public:
    Accordion() : BuilderBase() {}
    Accordion&& item(std::string header, std::string body = {}) && { node_->addItem(std::move(header), std::move(body)); return std::move(self()); }
    Accordion&& expandMode(wui::AccordionExpandMode value) && { node_->setExpandMode(value); return std::move(self()); }
    Accordion&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

class Drawer : public BuilderBase<Drawer, wui::Drawer> {
public:
    Drawer(std::string title = {}, std::string subtitle = {}) : BuilderBase(std::move(title), std::move(subtitle)) {}
    Drawer&& type(wui::DrawerType value) && { node_->type(value); return std::move(self()); }
    Drawer&& position(wui::DrawerPosition value) && { node_->position(value); return std::move(self()); }
    Drawer&& size(wui::DrawerSize value) && { node_->size(value); return std::move(self()); }
    Drawer&& modal(bool value = true) && { node_->modal(value); return std::move(self()); }
    Drawer&& dismissOnOutsidePress(bool value = true) && { node_->dismissOnOutsidePress(value); return std::move(self()); }
    Drawer&& onDismiss(wui::Drawer::DismissHandler handler) && { node_->onDismiss(std::move(handler)); return std::move(self()); }
    template <class Content>
    Drawer&& content(Content&& value) && { node_->content(asNode(std::forward<Content>(value))); return std::move(self()); }
};

class Popover : public BuilderBase<Popover, wui::Popover> {
public:
    Popover(std::string title = {}, std::string body = {}) : BuilderBase(std::move(title), std::move(body)) {}
    Popover&& appearance(wui::PopoverAppearance value) && { node_->appearance(value); return std::move(self()); }
    Popover&& arrow(bool value = true) && { node_->showArrow(value); return std::move(self()); }
};

class PopoverButton : public BuilderBase<PopoverButton, wui::PopoverButton> {
public:
    explicit PopoverButton(std::string label = {}) : BuilderBase(std::move(label)) {}
    PopoverButton&& overlayHost(wui::OverlayHost& host) && { node_->bindOverlayHost(host); return std::move(self()); }
    PopoverButton&& popover(std::string title, std::string body = {}) &&
    { node_->popover(std::move(title), std::move(body)); return std::move(self()); }
};

class TeachingPopover : public BuilderBase<TeachingPopover, wui::TeachingPopover> {
public:
    TeachingPopover(std::string title = {}, std::string body = {}) : BuilderBase(std::move(title), std::move(body)) {}
    TeachingPopover&& primaryAction(std::string label, wui::TeachingPopover::ActionHandler handler = {}) &&
    { node_->primaryAction(std::move(label), std::move(handler)); return std::move(self()); }
    TeachingPopover&& secondaryAction(std::string label, wui::TeachingPopover::ActionHandler handler = {}) &&
    { node_->secondaryAction(std::move(label), std::move(handler)); return std::move(self()); }
    TeachingPopover&& step(std::string value) && { node_->stepText(std::move(value)); return std::move(self()); }
};

class Toolbar : public BuilderBase<Toolbar, wui::Toolbar> {
public:
    Toolbar() : BuilderBase() {}
    Toolbar&& item(std::string label, wui::ToolbarItemAppearance appearance = wui::ToolbarItemAppearance::Subtle) &&
    { node_->addItem(std::move(label), appearance); return std::move(self()); }
    Toolbar&& orientation(wui::ToolbarOrientation value) && { node_->setOrientation(value); return std::move(self()); }
    Toolbar&& onOverflow(wui::Toolbar::OverflowHandler handler) && { node_->onOverflow(std::move(handler)); return std::move(self()); }
    Toolbar&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

class TabList : public BuilderBase<TabList, wui::TabList> {
public:
    TabList() : BuilderBase() {}
    TabList&& tab(std::string value, std::string label, bool enabled = true) &&
    { node_->addTab(std::move(value), std::move(label), enabled); return std::move(self()); }
    TabList&& value(std::string value) && { node_->setValue(std::move(value)); return std::move(self()); }
    TabList&& onChange(std::function<void(const std::string&)> handler) &&
    { node_->onChange(std::move(handler)); return std::move(self()); }
    TabList&& activationMode(wui::TabList::ActivationMode value) &&
    { node_->setActivationMode(value); return std::move(self()); }
};

class TabPanel : public BuilderBase<TabPanel, wui::TabPanel> {
public:
    explicit TabPanel(std::string value = {}) : BuilderBase(std::move(value)) {}
    TabPanel&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
    TabPanel&& tabList(wui::TabList& value) && { node_->setTabList(&value); return std::move(self()); }
    TabPanel&& active(bool value = true) && { node_->setActive(value); return std::move(self()); }
};

class Link : public BuilderBase<Link, wui::Link> {
public:
    explicit Link(std::string label = {}) : BuilderBase(std::move(label)) {}
    Link&& href(std::string value) && { node_->setHref(std::move(value)); return std::move(self()); }
    Link&& onClick(std::function<void()> handler) && { node_->onInvoke(std::move(handler)); return std::move(self()); }
};

class Breadcrumb : public BuilderBase<Breadcrumb, wui::Breadcrumb> {
public:
    Breadcrumb() : BuilderBase() {}
    Breadcrumb&& item(std::string label, bool current = false) &&
    { node_->addItem(std::move(label), current); return std::move(self()); }
    Breadcrumb&& maxVisible(std::size_t value) && { node_->setMaxVisible(value); return std::move(self()); }
};

class ListBox : public BuilderBase<ListBox, wui::ListBox> {
public:
    explicit ListBox(std::vector<wui::Option> options = {}) : BuilderBase(std::move(options)) {}
    ListBox&& option(wui::Option value) && { node_->addOption(std::move(value)); return std::move(self()); }
    ListBox&& selectedIndex(int value) && { node_->setSelectedIndex(value); return std::move(self()); }
    ListBox&& multiple(bool value = true) &&
    { node_->setSelectionMode(value ? wui::ListBoxSelectionMode::Multiple : wui::ListBoxSelectionMode::Single); return std::move(self()); }
    ListBox&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

class Combobox : public BuilderBase<Combobox, wui::Combobox> {
public:
    explicit Combobox(std::string placeholder = {}) : BuilderBase(std::move(placeholder)) {}
    Combobox&& option(wui::Option value) && { node_->addOption(std::move(value)); return std::move(self()); }
    Combobox&& selectedIndex(int value) && { node_->setSelectedIndex(value); return std::move(self()); }
    Combobox&& selectedIndices(std::vector<int> value) && { node_->setSelectedIndices(std::move(value)); return std::move(self()); }
    Combobox&& multiselect(bool value = true) && { node_->setMultiselect(value); return std::move(self()); }
    Combobox&& overlayHost(wui::OverlayHost& host) && { node_->bindOverlayHost(host); return std::move(self()); }
    Combobox&& onSelectionChanged(wui::Combobox::SelectionHandler handler) &&
    { node_->onSelectionChanged(std::move(handler)); return std::move(self()); }
    Combobox&& onChange(wui::TextInput::ChangeHandler handler) && { node_->onChange(std::move(handler)); return std::move(self()); }
};

class Dropdown : public BuilderBase<Dropdown, wui::Dropdown> {
public:
    explicit Dropdown(std::string placeholder = "Select an option") : BuilderBase(std::move(placeholder)) {}
    Dropdown&& option(wui::Option value) && { node_->addOption(std::move(value)); return std::move(self()); }
    Dropdown&& selectedIndex(int value) && { node_->setSelectedIndex(value); return std::move(self()); }
    Dropdown&& selectedIndices(std::vector<int> value) && { node_->setSelectedIndices(std::move(value)); return std::move(self()); }
    Dropdown&& multiselect(bool value = true) && { node_->setMultiselect(value); return std::move(self()); }
    Dropdown&& overlayHost(wui::OverlayHost& host) && { node_->bindOverlayHost(host); return std::move(self()); }
    Dropdown&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
    Dropdown&& onSelectionChanged(wui::Dropdown::SelectionHandler handler) &&
    { node_->onSelectionChanged(std::move(handler)); return std::move(self()); }
};

class Rating : public BuilderBase<Rating, wui::Rating> {
public:
    explicit Rating(float value = 0.0f, int maximum = 5) : BuilderBase(value, maximum) {}
    Rating&& value(float value) && { node_->setValue(value); return std::move(self()); }
    Rating&& maximum(int value) && { node_->setMaximum(value); return std::move(self()); }
    Rating&& step(float value) && { node_->setStep(value); return std::move(self()); }
    Rating&& color(wui::RatingColor value) && { node_->setColor(value); return std::move(self()); }
    Rating&& size(wui::RatingSize value) && { node_->setSize(value); return std::move(self()); }
    Rating&& shape(wui::RatingShape value) && { node_->setShape(value); return std::move(self()); }
    Rating&& readOnly(bool value = true) && { node_->setReadOnly(value); return std::move(self()); }
    Rating&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
    Rating&& itemLabel(wui::Rating::ItemLabelHandler handler) && { node_->setItemLabel(std::move(handler)); return std::move(self()); }
    Rating&& bind(wui::State<float>& state) && { node_->bind(state); return std::move(self()); }
    Rating&& onChange(std::function<void(float)> handler) && { node_->onChange(std::move(handler)); return std::move(self()); }
    Rating&& enabled(bool value) && { node_->setEnabled(value); return std::move(self()); }
};

class RatingDisplay : public BuilderBase<RatingDisplay, wui::RatingDisplay> {
public:
    explicit RatingDisplay(std::optional<float> value = std::optional<float>{0.0f}, int maximum = 5)
        : BuilderBase(value, maximum) {}
    RatingDisplay&& value(float value) && { node_->setValue(value); return std::move(self()); }
    RatingDisplay&& maximum(int value) && { node_->setMaximum(value); return std::move(self()); }
    RatingDisplay&& count(std::uint64_t value) && { node_->setCount(value); return std::move(self()); }
    RatingDisplay&& compact(bool value = true) && { node_->setCompact(value); return std::move(self()); }
    RatingDisplay&& color(wui::RatingColor value) && { node_->setColor(value); return std::move(self()); }
    RatingDisplay&& size(wui::RatingSize value) && { node_->setSize(value); return std::move(self()); }
    RatingDisplay&& shape(wui::RatingShape value) && { node_->setShape(value); return std::move(self()); }
    RatingDisplay&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

class ListView : public BuilderBase<ListView, wui::ListView> {
public:
    explicit ListView(std::vector<wui::ListView::Item> items = {}, int selectedIndex = -1)
        : BuilderBase(std::move(items), selectedIndex) {}
    ListView&& selectedIndex(int value) && { node_->setSelectedIndex(value); return std::move(self()); }
    ListView&& bind(wui::State<int>& state) && { node_->bind(state); return std::move(self()); }
    ListView&& onSelectionChanged(std::function<void(int)> handler) && { node_->onSelectionChanged(std::move(handler)); return std::move(self()); }
};

class IconButton : public BuilderBase<IconButton, wui::IconButton> {
public:
    explicit IconButton(std::string icon = {}, std::string accessibleLabel = {}) : BuilderBase(std::move(icon), std::move(accessibleLabel)) {}
    explicit IconButton(wui::IconName icon, std::string accessibleLabel = {}) : BuilderBase(icon, std::move(accessibleLabel)) {}
    IconButton&& icon(wui::IconName value) && { node_->setIcon(value); return std::move(self()); }
    IconButton&& iconStyle(wui::IconStyle value) && { node_->setIconStyle(value); return std::move(self()); }
    IconButton&& checked(bool value) && { node_->setChecked(value); return std::move(self()); }
    IconButton&& onClick(std::function<void()> handler) && { node_->onClick(std::move(handler)); return std::move(self()); }
};

class MenuButton : public BuilderBase<MenuButton, wui::MenuButton> {
public:
    explicit MenuButton(std::string label = {}) : BuilderBase(std::move(label)) {}
    MenuButton&& item(wui::MenuItem value) && { node_->addItem(std::move(value)); return std::move(self()); }
    MenuButton&& overlayHost(wui::OverlayHost& value) && { node_->bindOverlayHost(value); return std::move(self()); }
};

class SplitButton : public BuilderBase<SplitButton, wui::SplitButton> {
public:
    explicit SplitButton(std::string label = {}) : BuilderBase(std::move(label)) {}
    SplitButton&& onClick(std::function<void()> handler) && { node_->onClick(std::move(handler)); return std::move(self()); }
    SplitButton&& item(wui::MenuItem value) && { node_->addItem(std::move(value)); return std::move(self()); }
    SplitButton&& overlayHost(wui::OverlayHost& value) && { node_->bindOverlayHost(value); return std::move(self()); }
};

class SearchField : public BuilderBase<SearchField, wui::SearchField> {
public:
    explicit SearchField(std::string placeholder = "Search") : BuilderBase(std::move(placeholder)) {}
    SearchField&& query(std::string value) && { node_->query(std::move(value)); return std::move(self()); }
    SearchField&& onChange(wui::TextInput::ChangeHandler handler) && { node_->onQueryChange(std::move(handler)); return std::move(self()); }
};

class Row : public BuilderBase<Row, wui::Row> {
public:
    Row() : BuilderBase() {}

    Row&& gap(float gap) &&
    {
        node_->setGap(gap);
        return std::move(self());
    }

    Row&& padding(float all) &&
    {
        node_->setPadding(InsetsF{all, all, all, all});
        return std::move(self());
    }

    Row&& padding(InsetsF insets) &&
    {
        node_->setPadding(insets);
        return std::move(self());
    }

    Row&& align(Alignment align) &&
    {
        node_->setAlign(align);
        return std::move(self());
    }
};

class Column : public BuilderBase<Column, wui::Column> {
public:
    Column() : BuilderBase() {}

    Column&& gap(float gap) &&
    {
        node_->setGap(gap);
        return std::move(self());
    }

    Column&& padding(float all) &&
    {
        node_->setPadding(InsetsF{all, all, all, all});
        return std::move(self());
    }

    Column&& padding(InsetsF insets) &&
    {
        node_->setPadding(insets);
        return std::move(self());
    }

    Column&& align(Alignment align) &&
    {
        node_->setAlign(align);
        return std::move(self());
    }
};

class ScrollView : public BuilderBase<ScrollView, wui::ScrollView> {
public:
    ScrollView() : BuilderBase() {}

    ScrollView&& axis(wui::ScrollAxis axis) &&
    {
        node_->setAxis(axis);
        return std::move(self());
    }

    ScrollView&& offset(float value) &&
    {
        node_->setScrollOffset(value);
        return std::move(self());
    }

    ScrollView&& offset(wui::PointF value) &&
    {
        node_->setScrollOffset(value);
        return std::move(self());
    }
};

class Dialog : public BuilderBase<Dialog, wui::Dialog> {
public:
    Dialog() : BuilderBase() {}

    Dialog&& maxWidth(float width) &&
    {
        node_->setMaxWidth(width);
        return std::move(self());
    }

    Dialog&& dismissOnBackdrop(bool enabled = true) &&
    {
        node_->setBackdropDismissEnabled(enabled);
        return std::move(self());
    }

    Dialog&& onDismiss(std::function<void()> handler) &&
    {
        node_->onDismiss(std::move(handler));
        return std::move(self());
    }

    template <class Content>
    Dialog&& content(Content&& value) &&
    {
        node_->content(asNode(std::forward<Content>(value)));
        return std::move(self());
    }

    // Dialogs are shown through UiWindow::showDialog(), which intentionally
    // accepts the concrete modal type so it can manage focus restoration.
    std::unique_ptr<wui::Dialog> intoDialog() { return std::move(node_); }
};

// Structural control: mount `then(...)` only while `state` is true.
class If : public BuilderBase<If, wui::IfNode> {
public:
    explicit If(wui::State<bool>& state)
        : BuilderBase()
        , state_(&state)
    {
    }

    // `factory` returns a builder (or unique_ptr<Node>) for the mounted subtree.
    template <class Factory>
    If&& then(Factory factory) &&
    {
        wui::IfNode* raw = node_.get();
        raw->setFactory([factory = std::move(factory)]() mutable -> std::unique_ptr<wui::Node> {
            return asNode(factory());
        });
        raw->setVisible(state_->get());
        wui::State<bool>* state = state_;
        struct Subscription { std::size_t id{0}; bool active{false}; };
        auto alive = std::make_shared<bool>(true);
        auto subscription = std::make_shared<Subscription>();
        auto connect = [raw, state, alive, subscription] {
            raw->setVisible(state->get());
            if (subscription->active) {
                return;
            }
            subscription->id = state->subscribe([raw, state, weakAlive = std::weak_ptr<bool>(alive)](const bool&) {
                wui::scheduleStructuralUpdate(raw, [raw, state, weakAlive] {
                    const auto guard = weakAlive.lock();
                    if (guard && *guard) {
                        raw->setVisible(state->get());
                    }
                });
            });
            subscription->active = true;
        };
        auto disconnect = [state, subscription] {
            if (subscription->active) {
                state->unsubscribe(subscription->id);
                subscription->active = false;
            }
        };
        connect();
        raw->addAttachCallback(connect);
        raw->addDetachCallback(disconnect);
        raw->addTeardown([disconnect, alive] { *alive = false; disconnect(); });
        return std::move(self());
    }

private:
    wui::State<bool>* state_{nullptr};
};

// Structural control: (re)generate a list of children from `items`.
// Layout direction is configurable (vertical by default, horizontal via
// `.direction(ForEachDirection::Horizontal)`).
template <class T>
class ForEach : public BuilderBase<ForEach<T>, wui::ForEachNode> {
public:
    template <class ItemBuilder>
    ForEach(wui::State<std::vector<T>>& items, ItemBuilder itemBuilder)
        : BuilderBase<ForEach<T>, wui::ForEachNode>()
    {
        wui::ForEachNode* raw = this->node_.get();
        auto rebuild = [raw, &items, itemBuilder]() {
            raw->clearChildren();
            for (const auto& item : items.get()) {
                raw->appendChild(asNode(itemBuilder(item)));
            }
        };
        rebuild();
        wui::State<std::vector<T>>* state = &items;
        struct Subscription { std::size_t id{0}; bool active{false}; };
        auto alive = std::make_shared<bool>(true);
        auto subscription = std::make_shared<Subscription>();
        auto connect = [raw, state, rebuild, alive, subscription] {
            rebuild();
            if (subscription->active) {
                return;
            }
            subscription->id = state->subscribe([raw, rebuild, weakAlive = std::weak_ptr<bool>(alive)](const std::vector<T>&) {
                wui::scheduleStructuralUpdate(raw, [rebuild, weakAlive] {
                    const auto guard = weakAlive.lock();
                    if (guard && *guard) {
                        rebuild();
                    }
                });
            });
            subscription->active = true;
        };
        auto disconnect = [state, subscription] {
            if (subscription->active) {
                state->unsubscribe(subscription->id);
                subscription->active = false;
            }
        };
        connect();
        raw->addAttachCallback(connect);
        raw->addDetachCallback(disconnect);
        raw->addTeardown([disconnect, alive] { *alive = false; disconnect(); });
    }

    ForEach<T>&& direction(ForEachDirection dir) &&
    {
        this->node_->setDirection(dir);
        return std::move(this->self());
    }

    ForEach<T>&& gap(float gap) &&
    {
        this->node_->setGap(gap);
        return std::move(this->self());
    }

    ForEach<T>&& padding(float all) &&
    {
        this->node_->setPadding(InsetsF{all, all, all, all});
        return std::move(this->self());
    }

    ForEach<T>&& padding(InsetsF insets) &&
    {
        this->node_->setPadding(insets);
        return std::move(this->self());
    }

    ForEach<T>&& align(Alignment align) &&
    {
        this->node_->setAlign(align);
        return std::move(this->self());
    }
};

// A keyed alternative to ForEach for interactive collections. Unchanged keys
// retain their Nodes (and therefore focus, callbacks, and transient control
// state); only inserted, removed, or value-changed entries are materialised.
// Keys must be stable and unique within the supplied State.
template <class T>
class KeyedForEach : public BuilderBase<KeyedForEach<T>, wui::ForEachNode> {
public:
    template <class KeyProvider, class ItemBuilder>
    KeyedForEach(wui::State<std::vector<T>>& items, KeyProvider keyProvider, ItemBuilder itemBuilder)
        : BuilderBase<KeyedForEach<T>, wui::ForEachNode>()
    {
        using NodeFactory = std::function<std::unique_ptr<Node>(const T&)>;
        using KeyFactory = std::function<std::string(const T&)>;

        struct Entry {
            std::string key;
            T value;
        };
        struct Reconciler {
            wui::ForEachNode* raw{nullptr};
            wui::State<std::vector<T>>* state{nullptr};
            KeyFactory keyFor;
            NodeFactory build;
            std::vector<Entry> entries;
            bool reconciling{false};
            bool pending{false};

            void reconcile()
            {
                if (reconciling) {
                    pending = true;
                    return;
                }
                reconciling = true;
                do {
                    pending = false;
                    reconcileOnce();
                } while (pending);
                reconciling = false;
            }

            void reconcileOnce()
            {
                std::vector<Entry> desired;
                desired.reserve(state->get().size());
                for (const T& item : state->get()) {
                    std::string key = keyFor(item);
                    if (key.empty()) key = std::to_string(desired.size());
                    const auto duplicate = std::find_if(desired.begin(), desired.end(), [&key](const Entry& entry) {
                        return entry.key == key;
                    });
                    if (duplicate != desired.end()) key += "#" + std::to_string(desired.size());
                    desired.push_back({std::move(key), item});
                }

                // Destroy only removed or changed rows. A changed value keeps
                // its key but receives a fresh row so static Text/Button
                // properties stay truthful after an edit.
                for (std::size_t index = entries.size(); index > 0; --index) {
                    const Entry& previous = entries[index - 1];
                    const auto next = std::find_if(desired.begin(), desired.end(), [&previous](const Entry& entry) {
                        return entry.key == previous.key;
                    });
                    if (next == desired.end() || next->value != previous.value) {
                        (void)raw->removeChild(index - 1);
                        entries.erase(entries.begin() + static_cast<std::ptrdiff_t>(index - 1));
                    }
                }

                // Reorder retained rows without detach/reattach, then build
                // only new rows. This makes appending, deleting and filtering
                // proportional to the changed items instead of the list size.
                for (std::size_t index = 0; index < desired.size(); ++index) {
                    const Entry& next = desired[index];
                    const auto existing = std::find_if(entries.begin(), entries.end(), [&next](const Entry& entry) {
                        return entry.key == next.key;
                    });
                    if (existing == entries.end()) {
                        raw->insertChild(index, build(next.value));
                        entries.insert(entries.begin() + static_cast<std::ptrdiff_t>(index), next);
                        continue;
                    }
                    const std::size_t current = static_cast<std::size_t>(existing - entries.begin());
                    if (current != index) {
                        raw->moveChild(current, index);
                        Entry retained = std::move(entries[current]);
                        entries.erase(entries.begin() + static_cast<std::ptrdiff_t>(current));
                        entries.insert(entries.begin() + static_cast<std::ptrdiff_t>(index), std::move(retained));
                    }
                }
            }
        };

        auto reconciler = std::make_shared<Reconciler>();
        reconciler->raw = this->node_.get();
        reconciler->state = &items;
        reconciler->keyFor = KeyFactory(std::move(keyProvider));
        reconciler->build = [itemBuilder = std::move(itemBuilder)](const T& item) mutable {
            return asNode(itemBuilder(item));
        };
        reconciler->reconcile();

        struct Subscription { std::size_t id{0}; bool active{false}; };
        auto alive = std::make_shared<bool>(true);
        auto subscription = std::make_shared<Subscription>();
        auto connect = [reconciler, alive, subscription] {
            reconciler->reconcile();
            if (subscription->active) return;
            subscription->id = reconciler->state->subscribe([reconciler, weakAlive = std::weak_ptr<bool>(alive)](const std::vector<T>&) {
                wui::scheduleStructuralUpdate(reconciler->raw, [reconciler, weakAlive] {
                    const auto guard = weakAlive.lock();
                    if (guard && *guard) reconciler->reconcile();
                });
            });
            subscription->active = true;
        };
        auto disconnect = [reconciler, subscription] {
            if (!subscription->active) return;
            reconciler->state->unsubscribe(subscription->id);
            subscription->active = false;
        };
        connect();
        this->node_->addAttachCallback(connect);
        this->node_->addDetachCallback(disconnect);
        this->node_->addTeardown([disconnect, alive] { *alive = false; disconnect(); });
    }

    KeyedForEach<T>&& direction(ForEachDirection dir) && { this->node_->setDirection(dir); return std::move(this->self()); }
    KeyedForEach<T>&& gap(float gap) && { this->node_->setGap(gap); return std::move(this->self()); }
    KeyedForEach<T>&& padding(float all) && { this->node_->setPadding(InsetsF{all, all, all, all}); return std::move(this->self()); }
    KeyedForEach<T>&& padding(InsetsF insets) && { this->node_->setPadding(insets); return std::move(this->self()); }
    KeyedForEach<T>&& align(Alignment align) && { this->node_->setAlign(align); return std::move(this->self()); }
};

} // namespace wui::ui
