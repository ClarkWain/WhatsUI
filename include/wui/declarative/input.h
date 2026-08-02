#pragma once

#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "wui/basic_controls.h"
#include "wui/overlays.h"
#include "wui/text_input.h"
#include "wui/widgets.h"
#include "wui/declarative/builder_base.h"

namespace wui {

class TextField : public BuilderBase<TextField, wui::TextFieldNode> {
public:
    explicit TextField(std::string placeholder = {})
        : BuilderBase(std::move(placeholder))
    {
    }

    TextField& placeholder(std::string value) &
    {
        node_->setPlaceholder(std::move(value));
        return self();
    }

    TextField&& placeholder(std::string value) &&
    {
        node_->setPlaceholder(std::move(value));
        return std::move(self());
    }

    TextField& onChange(wui::TextFieldNode::ChangeHandler handler) &
    {
        node_->onChange(std::move(handler));
        return self();
    }

    TextField&& onChange(wui::TextFieldNode::ChangeHandler handler) &&
    {
        node_->onChange(std::move(handler));
        return std::move(self());
    }

    TextField& onSubmit(wui::TextFieldNode::SubmitHandler handler) &
    {
        node_->onSubmit(std::move(handler));
        return self();
    }

    TextField&& onSubmit(wui::TextFieldNode::SubmitHandler handler) &&
    {
        node_->onSubmit(std::move(handler));
        return std::move(self());
    }

    TextField& onCancel(wui::TextFieldNode::CancelHandler handler) &
    {
        node_->onCancel(std::move(handler));
        return self();
    }

    TextField&& onCancel(wui::TextFieldNode::CancelHandler handler) &&
    {
        node_->onCancel(std::move(handler));
        return std::move(self());
    }

    TextField& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    TextField&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
    TextField& size(wui::InputSize value) & { node_->setSize(value); return self(); }

    TextField&& size(wui::InputSize value) && { node_->setSize(value); return std::move(self()); }
    TextField& appearance(wui::InputAppearance value) & { node_->setAppearance(value); return self(); }

    TextField&& appearance(wui::InputAppearance value) && { node_->setAppearance(value); return std::move(self()); }
    TextField& invalid(bool value = true) & { node_->setInvalid(value); return self(); }

    TextField&& invalid(bool value = true) && { node_->setInvalid(value); return std::move(self()); }
    TextField& motionEnabled(bool value = true) & { node_->setMotionEnabled(value); return self(); }

    TextField&& motionEnabled(bool value = true) && { node_->setMotionEnabled(value); return std::move(self()); }
};

class TextArea : public BuilderBase<TextArea, wui::TextAreaNode> {
public:
    explicit TextArea(std::string placeholder = {})
        : BuilderBase(std::move(placeholder))
    {
    }

    TextArea& placeholder(std::string value) & { node_->setPlaceholder(std::move(value)); return self(); }

    TextArea&& placeholder(std::string value) && { node_->setPlaceholder(std::move(value)); return std::move(self()); }
    TextArea& onChange(wui::TextFieldNode::ChangeHandler handler) & { node_->onChange(std::move(handler)); return self(); }

    TextArea&& onChange(wui::TextFieldNode::ChangeHandler handler) && { node_->onChange(std::move(handler)); return std::move(self()); }
    TextArea& onCancel(wui::TextFieldNode::CancelHandler handler) & { node_->onCancel(std::move(handler)); return self(); }

    TextArea&& onCancel(wui::TextFieldNode::CancelHandler handler) && { node_->onCancel(std::move(handler)); return std::move(self()); }
    TextArea& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    TextArea&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
    TextArea& size(wui::InputSize value) & { node_->setSize(value); return self(); }

    TextArea&& size(wui::InputSize value) && { node_->setSize(value); return std::move(self()); }
    TextArea& appearance(wui::InputAppearance value) & { node_->setAppearance(value); return self(); }

    TextArea&& appearance(wui::InputAppearance value) && { node_->setAppearance(value); return std::move(self()); }
    TextArea& invalid(bool value = true) & { node_->setInvalid(value); return self(); }

    TextArea&& invalid(bool value = true) && { node_->setInvalid(value); return std::move(self()); }
    TextArea& motionEnabled(bool value = true) & { node_->setMotionEnabled(value); return self(); }

    TextArea&& motionEnabled(bool value = true) && { node_->setMotionEnabled(value); return std::move(self()); }
    TextArea& rows(std::size_t value) & { node_->setRows(value); return self(); }

    TextArea&& rows(std::size_t value) && { node_->setRows(value); return std::move(self()); }
};

class Button : public BuilderBase<Button, wui::ButtonNode> {
public:
    explicit Button(std::string label = {})
        : BuilderBase(std::move(label))
    {
    }

    Button& label(std::string label) &
    {
        node_->setLabel(std::move(label));
        return self();
    }

    Button&& label(std::string label) &&
    {
        node_->setLabel(std::move(label));
        return std::move(self());
    }

    Button& onClick(std::function<void()> handler) &
    {
        node_->onClick(std::move(handler));
        return self();
    }

    Button&& onClick(std::function<void()> handler) &&
    {
        node_->onClick(std::move(handler));
        return std::move(self());
    }

    Button& appearance(wui::ButtonAppearance value) &
    {
        node_->setAppearance(value);
        return self();
    }

    Button&& appearance(wui::ButtonAppearance value) &&
    {
        node_->setAppearance(value);
        return std::move(self());
    }

    Button& size(wui::ButtonSize value) &
    {
        node_->setSize(value);
        return self();
    }

    Button&& size(wui::ButtonSize value) &&
    {
        node_->setSize(value);
        return std::move(self());
    }

    Button& shape(wui::ButtonShape value) &
    {
        node_->setShape(value);
        return self();
    }

    Button&& shape(wui::ButtonShape value) &&
    {
        node_->setShape(value);
        return std::move(self());
    }

    Button& icon(wui::IconName value) &
    {
        node_->setIcon(value);
        return self();
    }

    Button&& icon(wui::IconName value) &&
    {
        node_->setIcon(value);
        return std::move(self());
    }

    Button& iconStyle(wui::IconStyle value) &
    {
        node_->setIconStyle(value);
        return self();
    }

    Button&& iconStyle(wui::IconStyle value) &&
    {
        node_->setIconStyle(value);
        return std::move(self());
    }

    Button& iconPosition(wui::ButtonIconPosition value) &
    {
        node_->setIconPosition(value);
        return self();
    }

    Button&& iconPosition(wui::ButtonIconPosition value) &&
    {
        node_->setIconPosition(value);
        return std::move(self());
    }

    Button& iconOnly(bool value = true) &
    {
        node_->setIconOnly(value);
        return self();
    }

    Button&& iconOnly(bool value = true) &&
    {
        node_->setIconOnly(value);
        return std::move(self());
    }
};

class Checkbox : public BuilderBase<Checkbox, wui::CheckboxNode> {
public:
    explicit Checkbox(std::string label = {}, bool checked = false) : BuilderBase(std::move(label), checked) {}
    Checkbox& label(std::string value) & { node_->setLabel(std::move(value)); return self(); }

    Checkbox&& label(std::string value) && { node_->setLabel(std::move(value)); return std::move(self()); }
    Checkbox& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    Checkbox&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
    Checkbox& checked(bool value) & { node_->setChecked(value); return self(); }

    Checkbox&& checked(bool value) && { node_->setChecked(value); return std::move(self()); }
    Checkbox& mixed(bool value = true) & { node_->setMixed(value); return self(); }

    Checkbox&& mixed(bool value = true) && { node_->setMixed(value); return std::move(self()); }
    Checkbox& checkState(wui::CheckboxState value) & { node_->setCheckState(value); return self(); }

    Checkbox&& checkState(wui::CheckboxState value) && { node_->setCheckState(value); return std::move(self()); }
    Checkbox& bind(wui::State<bool>& state) & { node_->bind(state); return self(); }

    Checkbox&& bind(wui::State<bool>& state) && { node_->bind(state); return std::move(self()); }
    Checkbox& onChange(std::function<void(bool)> handler) & { node_->onChange(std::move(handler)); return self(); }

    Checkbox&& onChange(std::function<void(bool)> handler) && { node_->onChange(std::move(handler)); return std::move(self()); }
    Checkbox& onStateChange(std::function<void(wui::CheckboxState)> handler) & { node_->onStateChange(std::move(handler)); return self(); }

    Checkbox&& onStateChange(std::function<void(wui::CheckboxState)> handler) && { node_->onStateChange(std::move(handler)); return std::move(self()); }
    Checkbox& size(wui::CheckboxSize value) & { node_->setSize(value); return self(); }

    Checkbox&& size(wui::CheckboxSize value) && { node_->setSize(value); return std::move(self()); }
    Checkbox& shape(wui::CheckboxShape value) & { node_->setShape(value); return self(); }

    Checkbox&& shape(wui::CheckboxShape value) && { node_->setShape(value); return std::move(self()); }
    Checkbox& labelPosition(wui::CheckboxLabelPosition value) & { node_->setLabelPosition(value); return self(); }

    Checkbox&& labelPosition(wui::CheckboxLabelPosition value) && { node_->setLabelPosition(value); return std::move(self()); }
    Checkbox& required(bool value = true) & { node_->setRequired(value); return self(); }

    Checkbox&& required(bool value = true) && { node_->setRequired(value); return std::move(self()); }
    Checkbox& enabled(bool value) & { node_->setEnabled(value); return self(); }

    Checkbox&& enabled(bool value) && { node_->setEnabled(value); return std::move(self()); }
};

class ToggleButton : public BuilderBase<ToggleButton, wui::ToggleButtonNode> {
public:
    explicit ToggleButton(std::string label = {}, bool checked = false) : BuilderBase(std::move(label), checked) {}
    ToggleButton& label(std::string value) & { node_->setLabel(std::move(value)); return self(); }

    ToggleButton&& label(std::string value) && { node_->setLabel(std::move(value)); return std::move(self()); }
    ToggleButton& checked(bool value) & { node_->setChecked(value); return self(); }

    ToggleButton&& checked(bool value) && { node_->setChecked(value); return std::move(self()); }
    ToggleButton& bind(wui::State<bool>& state) & { node_->bind(state); return self(); }

    ToggleButton&& bind(wui::State<bool>& state) && { node_->bind(state); return std::move(self()); }
    ToggleButton& onChange(std::function<void(bool)> handler) & { node_->onChange(std::move(handler)); return self(); }

    ToggleButton&& onChange(std::function<void(bool)> handler) && { node_->onChange(std::move(handler)); return std::move(self()); }
    ToggleButton& size(wui::ButtonSize value) & { node_->setSize(value); return self(); }

    ToggleButton&& size(wui::ButtonSize value) && { node_->setSize(value); return std::move(self()); }
    ToggleButton& shape(wui::ButtonShape value) & { node_->setShape(value); return self(); }

    ToggleButton&& shape(wui::ButtonShape value) && { node_->setShape(value); return std::move(self()); }
    ToggleButton& appearance(wui::ButtonAppearance value) & { node_->setAppearance(value); return self(); }

    ToggleButton&& appearance(wui::ButtonAppearance value) && { node_->setAppearance(value); return std::move(self()); }
    ToggleButton& icon(wui::IconName value) & { node_->setIcon(value); return self(); }

    ToggleButton&& icon(wui::IconName value) && { node_->setIcon(value); return std::move(self()); }
    ToggleButton& iconStyle(wui::IconStyle value) & { node_->setIconStyle(value); return self(); }

    ToggleButton&& iconStyle(wui::IconStyle value) && { node_->setIconStyle(value); return std::move(self()); }
    ToggleButton& iconPosition(wui::ButtonIconPosition value) & { node_->setIconPosition(value); return self(); }

    ToggleButton&& iconPosition(wui::ButtonIconPosition value) && { node_->setIconPosition(value); return std::move(self()); }
    ToggleButton& iconOnly(bool value = true) & { node_->setIconOnly(value); return self(); }

    ToggleButton&& iconOnly(bool value = true) && { node_->setIconOnly(value); return std::move(self()); }
};

class CompoundButton : public BuilderBase<CompoundButton, wui::CompoundButtonNode> {
public:
    CompoundButton(std::string label = {}, std::string secondaryContent = {})
        : BuilderBase(std::move(label), std::move(secondaryContent)) {}
    CompoundButton& label(std::string value) & { node_->setLabel(std::move(value)); return self(); }

    CompoundButton&& label(std::string value) && { node_->setLabel(std::move(value)); return std::move(self()); }
    CompoundButton& secondaryContent(std::string value) & { node_->setSecondaryContent(std::move(value)); return self(); }

    CompoundButton&& secondaryContent(std::string value) && { node_->setSecondaryContent(std::move(value)); return std::move(self()); }
    CompoundButton& onClick(std::function<void()> handler) & { node_->onClick(std::move(handler)); return self(); }

    CompoundButton&& onClick(std::function<void()> handler) && { node_->onClick(std::move(handler)); return std::move(self()); }
    CompoundButton& appearance(wui::ButtonAppearance value) & { node_->setAppearance(value); return self(); }

    CompoundButton&& appearance(wui::ButtonAppearance value) && { node_->setAppearance(value); return std::move(self()); }
    CompoundButton& size(wui::ButtonSize value) & { node_->setSize(value); return self(); }

    CompoundButton&& size(wui::ButtonSize value) && { node_->setSize(value); return std::move(self()); }
    CompoundButton& shape(wui::ButtonShape value) & { node_->setShape(value); return self(); }

    CompoundButton&& shape(wui::ButtonShape value) && { node_->setShape(value); return std::move(self()); }
};

class Radio : public BuilderBase<Radio, wui::RadioNode> {
public:
    explicit Radio(std::string label = {}, bool selected = false) : BuilderBase(std::move(label), selected) {}
    Radio& selected(bool value) & { node_->setSelected(value); return self(); }

    Radio&& selected(bool value) && { node_->setSelected(value); return std::move(self()); }
    Radio& bind(wui::State<bool>& state) & { node_->bind(state); return self(); }

    Radio&& bind(wui::State<bool>& state) && { node_->bind(state); return std::move(self()); }
    Radio& onChange(std::function<void(bool)> handler) & { node_->onChange(std::move(handler)); return self(); }

    Radio&& onChange(std::function<void(bool)> handler) && { node_->onChange(std::move(handler)); return std::move(self()); }
};

class RadioGroup : public TypedChildrenBuilderBase<RadioGroup, wui::RadioGroupNode, wui::RadioNode> {
public:
    RadioGroup() : TypedChildrenBuilderBase() {}
    RadioGroup& option(std::string value, std::string label, bool enabled = true) &
    {
        node_->addOption(std::move(value), std::move(label), enabled);
        return self();
    }

    RadioGroup&& option(std::string value, std::string label, bool enabled = true) &&
    {
        node_->addOption(std::move(value), std::move(label), enabled);
        return std::move(self());
    }
    RadioGroup& name(std::string value) & { node_->setName(std::move(value)); return self(); }

    RadioGroup&& name(std::string value) && { node_->setName(std::move(value)); return std::move(self()); }
    RadioGroup& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    RadioGroup&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
    RadioGroup& value(std::string value) & { node_->setValue(std::move(value)); return self(); }

    RadioGroup&& value(std::string value) && { node_->setValue(std::move(value)); return std::move(self()); }
    RadioGroup& bind(wui::State<std::string>& state) & { node_->bind(state); return self(); }

    RadioGroup&& bind(wui::State<std::string>& state) && { node_->bind(state); return std::move(self()); }
    RadioGroup& onChange(std::function<void(const std::string&)> handler) & { node_->onChange(std::move(handler)); return self(); }

    RadioGroup&& onChange(std::function<void(const std::string&)> handler) && { node_->onChange(std::move(handler)); return std::move(self()); }
    RadioGroup& layout(wui::RadioGroupLayout value) & { node_->setGroupLayout(value); return self(); }

    RadioGroup&& layout(wui::RadioGroupLayout value) && { node_->setGroupLayout(value); return std::move(self()); }
    RadioGroup& required(bool value = true) & { node_->setRequired(value); return self(); }

    RadioGroup&& required(bool value = true) && { node_->setRequired(value); return std::move(self()); }
    RadioGroup& enabled(bool value) & { node_->setEnabled(value); return self(); }

    RadioGroup&& enabled(bool value) && { node_->setEnabled(value); return std::move(self()); }
};

class Switch : public BuilderBase<Switch, wui::SwitchNode> {
public:
    explicit Switch(std::string label = {}, bool on = false) : BuilderBase(std::move(label), on) {}
    Switch& label(std::string value) & { node_->setLabel(std::move(value)); return self(); }

    Switch&& label(std::string value) && { node_->setLabel(std::move(value)); return std::move(self()); }
    Switch& on(bool value) & { node_->setOn(value); return self(); }

    Switch&& on(bool value) && { node_->setOn(value); return std::move(self()); }
    Switch& bind(wui::State<bool>& state) & { node_->bind(state); return self(); }

    Switch&& bind(wui::State<bool>& state) && { node_->bind(state); return std::move(self()); }
    Switch& onChange(std::function<void(bool)> handler) & { node_->onChange(std::move(handler)); return self(); }

    Switch&& onChange(std::function<void(bool)> handler) && { node_->onChange(std::move(handler)); return std::move(self()); }
    Switch& size(wui::SwitchSize value) & { node_->setSize(value); return self(); }

    Switch&& size(wui::SwitchSize value) && { node_->setSize(value); return std::move(self()); }
    Switch& labelPosition(wui::SwitchLabelPosition value) & { node_->setLabelPosition(value); return self(); }

    Switch&& labelPosition(wui::SwitchLabelPosition value) && { node_->setLabelPosition(value); return std::move(self()); }
    Switch& required(bool value = true) & { node_->setRequired(value); return self(); }

    Switch&& required(bool value = true) && { node_->setRequired(value); return std::move(self()); }
    Switch& enabled(bool value) & { node_->setEnabled(value); return self(); }

    Switch&& enabled(bool value) && { node_->setEnabled(value); return std::move(self()); }
};

class Slider : public BuilderBase<Slider, wui::SliderNode> {
public:
    Slider(float minimum = 0.0f, float maximum = 100.0f, float value = 0.0f) : BuilderBase(minimum, maximum, value) {}
    Slider& value(float value) & { node_->setValue(value); return self(); }

    Slider&& value(float value) && { node_->setValue(value); return std::move(self()); }
    Slider& step(float value) & { node_->setStep(value); return self(); }

    Slider&& step(float value) && { node_->setStep(value); return std::move(self()); }
    Slider& bind(wui::State<float>& state) & { node_->bind(state); return self(); }

    Slider&& bind(wui::State<float>& state) && { node_->bind(state); return std::move(self()); }
    Slider& onChange(std::function<void(float)> handler) & { node_->onChange(std::move(handler)); return self(); }

    Slider&& onChange(std::function<void(float)> handler) && { node_->onChange(std::move(handler)); return std::move(self()); }
    Slider& size(wui::SliderSize value) & { node_->setSize(value); return self(); }

    Slider&& size(wui::SliderSize value) && { node_->setSize(value); return std::move(self()); }
    Slider& orientation(wui::SliderOrientation value) & { node_->setOrientation(value); return self(); }

    Slider&& orientation(wui::SliderOrientation value) && { node_->setOrientation(value); return std::move(self()); }
    Slider& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    Slider&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
    Slider& enabled(bool value) & { node_->setEnabled(value); return self(); }

    Slider&& enabled(bool value) && { node_->setEnabled(value); return std::move(self()); }
};

class IconButton
    : public BuilderBase<IconButton, wui::IconButtonNode>,
      public AccessibleBuilderMixin<IconButton, wui::IconButtonNode> {
public:
    explicit IconButton(std::string icon = {}, std::string accessibleLabel = {}) : BuilderBase(std::move(icon), std::move(accessibleLabel)) {}
    explicit IconButton(wui::IconName icon, std::string accessibleLabel = {}) : BuilderBase(icon, std::move(accessibleLabel)) {}
    IconButton& icon(wui::IconName value) & { node_->setIcon(value); return self(); }

    IconButton&& icon(wui::IconName value) && { node_->setIcon(value); return std::move(self()); }
    IconButton& iconStyle(wui::IconStyle value) & { node_->setIconStyle(value); return self(); }

    IconButton&& iconStyle(wui::IconStyle value) && { node_->setIconStyle(value); return std::move(self()); }
    IconButton& checked(bool value) & { node_->setChecked(value); return self(); }

    IconButton&& checked(bool value) && { node_->setChecked(value); return std::move(self()); }
    IconButton& onClick(std::function<void()> handler) & { node_->onClick(std::move(handler)); return self(); }

    IconButton&& onClick(std::function<void()> handler) && { node_->onClick(std::move(handler)); return std::move(self()); }
};

class MenuButton : public BuilderBase<MenuButton, wui::MenuButtonNode> {
public:
    explicit MenuButton(std::string label = {}) : BuilderBase(std::move(label)) {}
    MenuButton& item(wui::MenuItem value) & { node_->addItem(std::move(value)); return self(); }

    MenuButton&& item(wui::MenuItem value) && { node_->addItem(std::move(value)); return std::move(self()); }
    MenuButton& overlayHost(wui::OverlayHost& value) & { node_->bindOverlayHost(value); return self(); }

    MenuButton&& overlayHost(wui::OverlayHost& value) && { node_->bindOverlayHost(value); return std::move(self()); }
};

class SplitButton : public BuilderBase<SplitButton, wui::SplitButtonNode> {
public:
    explicit SplitButton(std::string label = {}) : BuilderBase(std::move(label)) {}
    SplitButton& onClick(std::function<void()> handler) & { node_->onClick(std::move(handler)); return self(); }

    SplitButton&& onClick(std::function<void()> handler) && { node_->onClick(std::move(handler)); return std::move(self()); }
    SplitButton& item(wui::MenuItem value) & { node_->addItem(std::move(value)); return self(); }

    SplitButton&& item(wui::MenuItem value) && { node_->addItem(std::move(value)); return std::move(self()); }
    SplitButton& overlayHost(wui::OverlayHost& value) & { node_->bindOverlayHost(value); return self(); }

    SplitButton&& overlayHost(wui::OverlayHost& value) && { node_->bindOverlayHost(value); return std::move(self()); }
};

class SearchField : public BuilderBase<SearchField, wui::SearchFieldNode> {
public:
    explicit SearchField(std::string placeholder = "Search") : BuilderBase(std::move(placeholder)) {}
    SearchField& query(std::string value) & { node_->query(std::move(value)); return self(); }

    SearchField&& query(std::string value) && { node_->query(std::move(value)); return std::move(self()); }
    SearchField& onChange(wui::TextFieldNode::ChangeHandler handler) & { node_->onQueryChange(std::move(handler)); return self(); }

    SearchField&& onChange(wui::TextFieldNode::ChangeHandler handler) && { node_->onQueryChange(std::move(handler)); return std::move(self()); }
};

} // namespace wui
