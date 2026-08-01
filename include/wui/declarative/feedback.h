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

#include "wui/avatar.h"
#include "wui/badge.h"
#include "wui/basic_controls.h"
#include "wui/feedback.h"
#include "wui/form_feedback.h"
#include "wui/persona.h"
#include "wui/widgets.h"
#include "wui/declarative/builder_base.h"

namespace wui {

class Card : public ContainerBuilderBase<Card, wui::CardNode> {
public:
    Card() : ContainerBuilderBase() {}
    Card& appearance(wui::CardAppearance value) & { node_->setAppearance(value); return self(); }

    Card&& appearance(wui::CardAppearance value) && { node_->setAppearance(value); return std::move(self()); }
    Card& size(wui::CardSize value) & { node_->setSize(value); return self(); }

    Card&& size(wui::CardSize value) && { node_->setSize(value); return std::move(self()); }
    Card& orientation(wui::CardOrientation value) & { node_->setOrientation(value); return self(); }

    Card&& orientation(wui::CardOrientation value) && { node_->setOrientation(value); return std::move(self()); }
    Card& selected(bool value = true) & { node_->setSelected(value); return self(); }

    Card&& selected(bool value = true) && { node_->setSelected(value); return std::move(self()); }
    Card& selectable(bool value = true) & { node_->selectable(value); return self(); }

    Card&& selectable(bool value = true) && { node_->selectable(value); return std::move(self()); }
    Card& onSelectionChange(wui::CardNode::ChangeHandler value) & { node_->onSelectionChange(std::move(value)); return self(); }

    Card&& onSelectionChange(wui::CardNode::ChangeHandler value) && { node_->onSelectionChange(std::move(value)); return std::move(self()); }
};

class CardHeader : public BuilderBase<CardHeader, wui::CardHeaderNode> {
public:
    CardHeader(std::string title = {}, std::string description = {}) : BuilderBase(std::move(title), std::move(description)) {}
    template <class Child>
    CardHeader& media(Child&& value) & { node_->media(detail::materialize(std::forward<Child>(value))); return self(); }

    template <class Child>
    CardHeader&& media(Child&& value) && { node_->media(detail::materialize(std::forward<Child>(value))); return std::move(self()); }
    template <class Child>
    CardHeader& action(Child&& value) & { node_->action(detail::materialize(std::forward<Child>(value))); return self(); }

    template <class Child>
    CardHeader&& action(Child&& value) && { node_->action(detail::materialize(std::forward<Child>(value))); return std::move(self()); }
};

class CardPreview : public ContainerBuilderBase<CardPreview, wui::CardPreviewNode> {
public:
    CardPreview() : ContainerBuilderBase() {}
    CardPreview& height(float value) & { node_->setHeight(value); return self(); }

    CardPreview&& height(float value) && { node_->setHeight(value); return std::move(self()); }
};

class CardFooter : public ContainerBuilderBase<CardFooter, wui::CardFooterNode> {
public:
    CardFooter() : ContainerBuilderBase() {}
};

class Label : public BuilderBase<Label, wui::LabelNode> {
public:
    explicit Label(std::string text = {}) : BuilderBase(std::move(text)) {}
    Label& size(wui::LabelSize value) & { node_->setSize(value); return self(); }

    Label&& size(wui::LabelSize value) && { node_->setSize(value); return std::move(self()); }
    Label& required(bool value = true) & { node_->setRequired(value); return self(); }

    Label&& required(bool value = true) && { node_->setRequired(value); return std::move(self()); }
    Label& forControl(wui::TextFieldNode* control) & { node_->setForControl(control); return self(); }

    Label&& forControl(wui::TextFieldNode* control) && { node_->setForControl(control); return std::move(self()); }
};

class Field : public BuilderBase<Field, wui::FieldNode> {
public:
    explicit Field(std::string label = {}) : BuilderBase(std::move(label)) {}
    Field& label(std::string value) & { node_->setLabel(std::move(value)); return self(); }

    Field&& label(std::string value) && { node_->setLabel(std::move(value)); return std::move(self()); }
    Field& hint(std::string value) & { node_->setHint(std::move(value)); return self(); }

    Field&& hint(std::string value) && { node_->setHint(std::move(value)); return std::move(self()); }
    Field& validationMessage(std::string value) & { node_->setValidationMessage(std::move(value)); return self(); }

    Field&& validationMessage(std::string value) && { node_->setValidationMessage(std::move(value)); return std::move(self()); }
    Field& validationState(wui::FieldValidationState value) & { node_->setValidationState(value); return self(); }

    Field&& validationState(wui::FieldValidationState value) && { node_->setValidationState(value); return std::move(self()); }
    Field& required(bool value = true) & { node_->setRequired(value); return self(); }

    Field&& required(bool value = true) && { node_->setRequired(value); return std::move(self()); }
    Field& orientation(wui::FieldOrientation value) & { node_->setOrientation(value); return self(); }

    Field&& orientation(wui::FieldOrientation value) && { node_->setOrientation(value); return std::move(self()); }
    Field& enabled(bool value) & { node_->setEnabled(value); return self(); }

    Field&& enabled(bool value) && { node_->setEnabled(value); return std::move(self()); }
    template <class Child>
    Field& control(Child&& value) & { node_->setControl(detail::materialize(std::forward<Child>(value))); return self(); }

    template <class Child>
    Field&& control(Child&& value) && { node_->setControl(detail::materialize(std::forward<Child>(value))); return std::move(self()); }
};

class MessageBar : public BuilderBase<MessageBar, wui::MessageBarNode> {
public:
    explicit MessageBar(std::string body = {}) : BuilderBase(std::move(body)) {}
    MessageBar& title(std::string value) & { node_->setTitle(std::move(value)); return self(); }

    MessageBar&& title(std::string value) && { node_->setTitle(std::move(value)); return std::move(self()); }
    MessageBar& body(std::string value) & { node_->setBody(std::move(value)); return self(); }

    MessageBar&& body(std::string value) && { node_->setBody(std::move(value)); return std::move(self()); }
    MessageBar& intent(wui::MessageBarIntent value) & { node_->setIntent(value); return self(); }

    MessageBar&& intent(wui::MessageBarIntent value) && { node_->setIntent(value); return std::move(self()); }
    MessageBar& multiline(bool value = true) & { node_->setMultiline(value); return self(); }

    MessageBar&& multiline(bool value = true) && { node_->setMultiline(value); return std::move(self()); }
    MessageBar& action(wui::MessageBarAction value) & { node_->addAction(std::move(value)); return self(); }

    MessageBar&& action(wui::MessageBarAction value) && { node_->addAction(std::move(value)); return std::move(self()); }
    MessageBar& dismissible(bool value = true) & { node_->setDismissible(value); return self(); }

    MessageBar&& dismissible(bool value = true) && { node_->setDismissible(value); return std::move(self()); }
    MessageBar& onDismiss(wui::MessageBarNode::DismissHandler value) & { node_->onDismiss(std::move(value)); return self(); }

    MessageBar&& onDismiss(wui::MessageBarNode::DismissHandler value) && { node_->onDismiss(std::move(value)); return std::move(self()); }
};

class ProgressBar : public BuilderBase<ProgressBar, wui::ProgressBarNode> {
public:
    ProgressBar(float minimum = 0.0f, float maximum = 1.0f,
                std::optional<float> value = std::nullopt)
        : BuilderBase(minimum, maximum, value) {}
    ProgressBar& value(float value) & { node_->setValue(value); return self(); }

    ProgressBar&& value(float value) && { node_->setValue(value); return std::move(self()); }
    ProgressBar& bind(wui::State<float>& state) & { node_->bind(state); return self(); }

    ProgressBar&& bind(wui::State<float>& state) && { node_->bind(state); return std::move(self()); }
    ProgressBar& indeterminate(bool value = true) & { node_->setIndeterminate(value); return self(); }

    ProgressBar&& indeterminate(bool value = true) && { node_->setIndeterminate(value); return std::move(self()); }
    ProgressBar& color(wui::ProgressBarColor value) & { node_->setColor(value); return self(); }

    ProgressBar&& color(wui::ProgressBarColor value) && { node_->setColor(value); return std::move(self()); }
    ProgressBar& shape(wui::ProgressBarShape value) & { node_->setShape(value); return self(); }

    ProgressBar&& shape(wui::ProgressBarShape value) && { node_->setShape(value); return std::move(self()); }
    ProgressBar& thickness(wui::ProgressBarThickness value) & { node_->setThickness(value); return self(); }

    ProgressBar&& thickness(wui::ProgressBarThickness value) && { node_->setThickness(value); return std::move(self()); }
    ProgressBar& motionEnabled(bool value) & { node_->setMotionEnabled(value); return self(); }

    ProgressBar&& motionEnabled(bool value) && { node_->setMotionEnabled(value); return std::move(self()); }
    ProgressBar& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    ProgressBar&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

class Toast : public BuilderBase<Toast, wui::ToastNode> {
public:
    Toast(std::string title = {}, std::string body = {}) : BuilderBase(std::move(title), std::move(body)) {}
    Toast& title(std::string value) & { node_->setTitle(std::move(value)); return self(); }

    Toast&& title(std::string value) && { node_->setTitle(std::move(value)); return std::move(self()); }
    Toast& body(std::string value) & { node_->setBody(std::move(value)); return self(); }

    Toast&& body(std::string value) && { node_->setBody(std::move(value)); return std::move(self()); }
    Toast& intent(wui::ToastIntent value) & { node_->setIntent(value); return self(); }

    Toast&& intent(wui::ToastIntent value) && { node_->setIntent(value); return std::move(self()); }
    Toast& position(wui::ToastPosition value) & { node_->setPosition(value); return self(); }

    Toast&& position(wui::ToastPosition value) && { node_->setPosition(value); return std::move(self()); }
    Toast& action(std::string label, std::function<void()> handler) & { node_->setAction(std::move(label), std::move(handler)); return self(); }

    Toast&& action(std::string label, std::function<void()> handler) && { node_->setAction(std::move(label), std::move(handler)); return std::move(self()); }
    Toast& timeout(std::chrono::milliseconds value) & { node_->setTimeout(value); return self(); }

    Toast&& timeout(std::chrono::milliseconds value) && { node_->setTimeout(value); return std::move(self()); }
};

class Spinner : public BuilderBase<Spinner, wui::SpinnerNode> {
public:
    explicit Spinner(std::string label = {}) : BuilderBase(std::move(label)) {}
    Spinner& label(std::string value) & { node_->setLabel(std::move(value)); return self(); }

    Spinner&& label(std::string value) && { node_->setLabel(std::move(value)); return std::move(self()); }
    Spinner& size(wui::SpinnerSize value) & { node_->setSize(value); return self(); }

    Spinner&& size(wui::SpinnerSize value) && { node_->setSize(value); return std::move(self()); }
    Spinner& labelPosition(wui::SpinnerLabelPosition value) & { node_->setLabelPosition(value); return self(); }

    Spinner&& labelPosition(wui::SpinnerLabelPosition value) && { node_->setLabelPosition(value); return std::move(self()); }
    Spinner& motionEnabled(bool value) & { node_->setMotionEnabled(value); return self(); }

    Spinner&& motionEnabled(bool value) && { node_->setMotionEnabled(value); return std::move(self()); }
};

class Divider : public BuilderBase<Divider, wui::DividerNode> {
public:
    explicit Divider(wui::DividerOrientation orientation = wui::DividerOrientation::Horizontal) : BuilderBase(orientation) {}
    Divider& thickness(float value) & { node_->setThickness(value); return self(); }

    Divider&& thickness(float value) && { node_->setThickness(value); return std::move(self()); }
    Divider& content(std::string value) & { node_->setContent(std::move(value)); return self(); }

    Divider&& content(std::string value) && { node_->setContent(std::move(value)); return std::move(self()); }
    Divider& appearance(wui::DividerAppearance value) & { node_->setAppearance(value); return self(); }

    Divider&& appearance(wui::DividerAppearance value) && { node_->setAppearance(value); return std::move(self()); }
    Divider& contentAlignment(wui::DividerContentAlignment value) & { node_->setContentAlignment(value); return self(); }

    Divider&& contentAlignment(wui::DividerContentAlignment value) && { node_->setContentAlignment(value); return std::move(self()); }
    Divider& inset(bool value = true) & { node_->setInset(value); return self(); }

    Divider&& inset(bool value = true) && { node_->setInset(value); return std::move(self()); }
};

class Badge : public BuilderBase<Badge, wui::BadgeNode> {
public:
    explicit Badge(std::string text = {}) : BuilderBase(std::move(text)) {}
    Badge& text(std::string value) & { node_->setText(std::move(value)); return self(); }

    Badge&& text(std::string value) && { node_->setText(std::move(value)); return std::move(self()); }
    Badge& appearance(wui::BadgeAppearance value) & { node_->setAppearance(value); return self(); }

    Badge&& appearance(wui::BadgeAppearance value) && { node_->setAppearance(value); return std::move(self()); }
    Badge& color(wui::BadgeColor value) & { node_->setColor(value); return self(); }

    Badge&& color(wui::BadgeColor value) && { node_->setColor(value); return std::move(self()); }
    Badge& size(wui::BadgeSize value) & { node_->setSize(value); return self(); }

    Badge&& size(wui::BadgeSize value) && { node_->setSize(value); return std::move(self()); }
    Badge& shape(wui::BadgeShape value) & { node_->setShape(value); return self(); }

    Badge&& shape(wui::BadgeShape value) && { node_->setShape(value); return std::move(self()); }
    Badge& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    Badge&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

class CounterBadge : public BuilderBase<CounterBadge, wui::CounterBadgeNode> {
public:
    explicit CounterBadge(std::uint64_t count = 0) : BuilderBase(count) {}
    CounterBadge& count(std::uint64_t value) & { node_->setCount(value); return self(); }

    CounterBadge&& count(std::uint64_t value) && { node_->setCount(value); return std::move(self()); }
    CounterBadge& max(std::uint64_t value) & { node_->setMax(value); return self(); }

    CounterBadge&& max(std::uint64_t value) && { node_->setMax(value); return std::move(self()); }
    CounterBadge& showZero(bool value = true) & { node_->setShowZero(value); return self(); }

    CounterBadge&& showZero(bool value = true) && { node_->setShowZero(value); return std::move(self()); }
    CounterBadge& size(wui::BadgeSize value) & { node_->setSize(value); return self(); }

    CounterBadge&& size(wui::BadgeSize value) && { node_->setSize(value); return std::move(self()); }
    CounterBadge& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    CounterBadge&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

class PresenceBadge : public BuilderBase<PresenceBadge, wui::PresenceBadgeNode> {
public:
    explicit PresenceBadge(wui::PresenceStatus status = wui::PresenceStatus::Available) : BuilderBase(status) {}
    PresenceBadge& status(wui::PresenceStatus value) & { node_->setStatus(value); return self(); }

    PresenceBadge&& status(wui::PresenceStatus value) && { node_->setStatus(value); return std::move(self()); }
    PresenceBadge& position(wui::PresenceBadgePosition value) & { node_->setPosition(value); return self(); }

    PresenceBadge&& position(wui::PresenceBadgePosition value) && { node_->setPosition(value); return std::move(self()); }
    PresenceBadge& avatarSize(float value) & { node_->setAvatarSize(value); return self(); }

    PresenceBadge&& avatarSize(float value) && { node_->setAvatarSize(value); return std::move(self()); }
    PresenceBadge& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    PresenceBadge&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

class Avatar : public BuilderBase<Avatar, wui::AvatarNode> {
public:
    explicit Avatar(std::string name = {}, wui::AvatarSize size = wui::AvatarSize::Size32)
        : BuilderBase(std::move(name), size) {}
    Avatar& initials(std::string value) & { node_->setInitials(std::move(value)); return self(); }

    Avatar&& initials(std::string value) && { node_->setInitials(std::move(value)); return std::move(self()); }
    Avatar& image(wui::ImageSource source) & { node_->setImage(std::move(source)); return self(); }

    Avatar&& image(wui::ImageSource source) && { node_->setImage(std::move(source)); return std::move(self()); }
    Avatar& size(wui::AvatarSize value) & { node_->setSize(value); return self(); }

    Avatar&& size(wui::AvatarSize value) && { node_->setSize(value); return std::move(self()); }
    Avatar& shape(wui::AvatarShape value) & { node_->setShape(value); return self(); }

    Avatar&& shape(wui::AvatarShape value) && { node_->setShape(value); return std::move(self()); }
    Avatar& color(wui::AvatarColor value) & { node_->setColor(value); return self(); }

    Avatar&& color(wui::AvatarColor value) && { node_->setColor(value); return std::move(self()); }
    Avatar& active(bool value = true) & { node_->setActive(value); return self(); }

    Avatar&& active(bool value = true) && { node_->setActive(value); return std::move(self()); }
    Avatar& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    Avatar&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

class AvatarGroup : public TypedChildrenBuilderBase<AvatarGroup, wui::AvatarGroupNode, wui::AvatarNode> {
public:
    AvatarGroup() : TypedChildrenBuilderBase() {}
    AvatarGroup& avatar(std::string name, wui::AvatarSize size = wui::AvatarSize::Size32) &
    { node_->addAvatar(std::move(name), size); return self(); }

    AvatarGroup&& avatar(std::string name, wui::AvatarSize size = wui::AvatarSize::Size32) &&
    { node_->addAvatar(std::move(name), size); return std::move(self()); }
    AvatarGroup& maxVisible(std::size_t value) & { node_->setMaxVisible(value); return self(); }

    AvatarGroup&& maxVisible(std::size_t value) && { node_->setMaxVisible(value); return std::move(self()); }
    AvatarGroup& layout(wui::AvatarGroupLayout value) & { node_->setGroupLayout(value); return self(); }

    AvatarGroup&& layout(wui::AvatarGroupLayout value) && { node_->setGroupLayout(value); return std::move(self()); }
    AvatarGroup& size(wui::AvatarSize value) & { node_->setSize(value); return self(); }

    AvatarGroup&& size(wui::AvatarSize value) && { node_->setSize(value); return std::move(self()); }
    AvatarGroup& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    AvatarGroup&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

class Persona : public BuilderBase<Persona, wui::PersonaNode> {
public:
    explicit Persona(std::string name = {}, wui::PersonaSize size = wui::PersonaSize::Medium)
        : BuilderBase(std::move(name), size) {}
    Persona& primaryText(std::string value) & { node_->setPrimaryText(std::move(value)); return self(); }

    Persona&& primaryText(std::string value) && { node_->setPrimaryText(std::move(value)); return std::move(self()); }
    Persona& secondaryText(std::string value) & { node_->setSecondaryText(std::move(value)); return self(); }

    Persona&& secondaryText(std::string value) && { node_->setSecondaryText(std::move(value)); return std::move(self()); }
    Persona& tertiaryText(std::string value) & { node_->setTertiaryText(std::move(value)); return self(); }

    Persona&& tertiaryText(std::string value) && { node_->setTertiaryText(std::move(value)); return std::move(self()); }
    Persona& quaternaryText(std::string value) & { node_->setQuaternaryText(std::move(value)); return self(); }

    Persona&& quaternaryText(std::string value) && { node_->setQuaternaryText(std::move(value)); return std::move(self()); }
    Persona& size(wui::PersonaSize value) & { node_->setSize(value); return self(); }

    Persona&& size(wui::PersonaSize value) && { node_->setSize(value); return std::move(self()); }
    Persona& avatarColor(wui::AvatarColor value) & { node_->setAvatarColor(value); return self(); }

    Persona&& avatarColor(wui::AvatarColor value) && { node_->setAvatarColor(value); return std::move(self()); }
    Persona& avatarShape(wui::AvatarShape value) & { node_->setAvatarShape(value); return self(); }

    Persona&& avatarShape(wui::AvatarShape value) && { node_->setAvatarShape(value); return std::move(self()); }
    Persona& avatarImage(wui::ImageSource value) & { node_->setAvatarImage(std::move(value)); return self(); }

    Persona&& avatarImage(wui::ImageSource value) && { node_->setAvatarImage(std::move(value)); return std::move(self()); }
    Persona& presence(wui::PresenceStatus value) & { node_->setPresence(value); return self(); }

    Persona&& presence(wui::PresenceStatus value) && { node_->setPresence(value); return std::move(self()); }
    Persona& presenceOnly(bool value = true) & { node_->setPresenceOnly(value); return self(); }

    Persona&& presenceOnly(bool value = true) && { node_->setPresenceOnly(value); return std::move(self()); }
    Persona& textPosition(wui::PersonaTextPosition value) & { node_->setTextPosition(value); return self(); }

    Persona&& textPosition(wui::PersonaTextPosition value) && { node_->setTextPosition(value); return std::move(self()); }
    Persona& textAlignment(wui::PersonaTextAlignment value) & { node_->setTextAlignment(value); return self(); }

    Persona&& textAlignment(wui::PersonaTextAlignment value) && { node_->setTextAlignment(value); return std::move(self()); }
    Persona& onClick(wui::PersonaNode::ClickHandler handler) & { node_->onClick(std::move(handler)); return self(); }

    Persona&& onClick(wui::PersonaNode::ClickHandler handler) && { node_->onClick(std::move(handler)); return std::move(self()); }
    Persona& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    Persona&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

} // namespace wui
