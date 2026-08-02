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

#include "wui/accordion.h"
#include "wui/drawer.h"
#include "wui/navigation.h"
#include "wui/overlays.h"
#include "wui/popover.h"
#include "wui/runtime.h"
#include "wui/widgets.h"
#include "wui/declarative/builder_base.h"

namespace wui {

class AccordionItem : public SingleContentBuilderBase<AccordionItem, wui::AccordionItemNode> {
public:
    AccordionItem(std::string header = {}, std::string body = {}) : SingleContentBuilderBase(std::move(header), std::move(body)) {}
    AccordionItem& expanded(bool value = true) & { node_->setExpanded(value); return self(); }

    AccordionItem&& expanded(bool value = true) && { node_->setExpanded(value); return std::move(self()); }
};

class Accordion : public TypedChildrenBuilderBase<Accordion, wui::AccordionNode, wui::AccordionItemNode> {
public:
    Accordion() : TypedChildrenBuilderBase() {}
    Accordion& item(std::string header, std::string body = {}) & { node_->addItem(std::move(header), std::move(body)); return self(); }

    Accordion&& item(std::string header, std::string body = {}) && { node_->addItem(std::move(header), std::move(body)); return std::move(self()); }
    Accordion& expandMode(wui::AccordionExpandMode value) & { node_->setExpandMode(value); return self(); }

    Accordion&& expandMode(wui::AccordionExpandMode value) && { node_->setExpandMode(value); return std::move(self()); }
    Accordion& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    Accordion&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

class Drawer : public SingleContentBuilderBase<Drawer, wui::DrawerNode> {
public:
    Drawer(std::string title = {}, std::string subtitle = {}) : SingleContentBuilderBase(std::move(title), std::move(subtitle)) {}
    Drawer& type(wui::DrawerType value) & { node_->type(value); return self(); }

    Drawer&& type(wui::DrawerType value) && { node_->type(value); return std::move(self()); }
    Drawer& position(wui::DrawerPosition value) & { node_->position(value); return self(); }

    Drawer&& position(wui::DrawerPosition value) && { node_->position(value); return std::move(self()); }
    Drawer& size(wui::DrawerSize value) & { node_->size(value); return self(); }

    Drawer&& size(wui::DrawerSize value) && { node_->size(value); return std::move(self()); }
    Drawer& modal(bool value = true) & { node_->modal(value); return self(); }

    Drawer&& modal(bool value = true) && { node_->modal(value); return std::move(self()); }
    Drawer& dismissOnOutsidePress(bool value = true) & { node_->dismissOnOutsidePress(value); return self(); }

    Drawer&& dismissOnOutsidePress(bool value = true) && { node_->dismissOnOutsidePress(value); return std::move(self()); }
    Drawer& onDismiss(wui::DrawerNode::DismissHandler handler) & { node_->onDismiss(std::move(handler)); return self(); }

    Drawer&& onDismiss(wui::DrawerNode::DismissHandler handler) && { node_->onDismiss(std::move(handler)); return std::move(self()); }
};

class Popover : public BuilderBase<Popover, wui::PopoverNode> {
public:
    Popover(std::string title = {}, std::string body = {}) : BuilderBase(std::move(title), std::move(body)) {}
    Popover& appearance(wui::PopoverAppearance value) & { node_->appearance(value); return self(); }

    Popover&& appearance(wui::PopoverAppearance value) && { node_->appearance(value); return std::move(self()); }
    Popover& arrow(bool value = true) & { node_->showArrow(value); return self(); }

    Popover&& arrow(bool value = true) && { node_->showArrow(value); return std::move(self()); }
};

class PopoverButton : public BuilderBase<PopoverButton, wui::PopoverButtonNode> {
public:
    explicit PopoverButton(std::string label = {}) : BuilderBase(std::move(label)) {}
    PopoverButton& overlayHost(wui::OverlayHost& host) & { node_->bindOverlayHost(host); return self(); }

    PopoverButton&& overlayHost(wui::OverlayHost& host) && { node_->bindOverlayHost(host); return std::move(self()); }
    PopoverButton& popover(std::string title, std::string body = {}) &
    { node_->popover(std::move(title), std::move(body)); return self(); }

    PopoverButton&& popover(std::string title, std::string body = {}) &&
    { node_->popover(std::move(title), std::move(body)); return std::move(self()); }
};

class TeachingPopover : public BuilderBase<TeachingPopover, wui::TeachingPopoverNode> {
public:
    TeachingPopover(std::string title = {}, std::string body = {}) : BuilderBase(std::move(title), std::move(body)) {}
    TeachingPopover& primaryAction(std::string label, wui::TeachingPopoverNode::ActionHandler handler = {}) &
    { node_->primaryAction(std::move(label), std::move(handler)); return self(); }

    TeachingPopover&& primaryAction(std::string label, wui::TeachingPopoverNode::ActionHandler handler = {}) &&
    { node_->primaryAction(std::move(label), std::move(handler)); return std::move(self()); }
    TeachingPopover& secondaryAction(std::string label, wui::TeachingPopoverNode::ActionHandler handler = {}) &
    { node_->secondaryAction(std::move(label), std::move(handler)); return self(); }

    TeachingPopover&& secondaryAction(std::string label, wui::TeachingPopoverNode::ActionHandler handler = {}) &&
    { node_->secondaryAction(std::move(label), std::move(handler)); return std::move(self()); }
    TeachingPopover& step(std::string value) & { node_->stepText(std::move(value)); return self(); }

    TeachingPopover&& step(std::string value) && { node_->stepText(std::move(value)); return std::move(self()); }
};

class Toolbar : public BuilderBase<Toolbar, wui::ToolbarNode> {
public:
    Toolbar() : BuilderBase() {}
    Toolbar& item(std::string label, wui::ToolbarItemAppearance appearance = wui::ToolbarItemAppearance::Subtle) &
    { node_->addItem(std::move(label), appearance); return self(); }

    Toolbar&& item(std::string label, wui::ToolbarItemAppearance appearance = wui::ToolbarItemAppearance::Subtle) &&
    { node_->addItem(std::move(label), appearance); return std::move(self()); }
    Toolbar& orientation(wui::ToolbarOrientation value) & { node_->setOrientation(value); return self(); }

    Toolbar&& orientation(wui::ToolbarOrientation value) && { node_->setOrientation(value); return std::move(self()); }
    Toolbar& onOverflow(wui::ToolbarNode::OverflowHandler handler) & { node_->onOverflow(std::move(handler)); return self(); }

    Toolbar&& onOverflow(wui::ToolbarNode::OverflowHandler handler) && { node_->onOverflow(std::move(handler)); return std::move(self()); }
    Toolbar& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    Toolbar&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

class TabList : public BuilderBase<TabList, wui::TabListNode> {
public:
    TabList() : BuilderBase() {}
    TabList& tab(std::string value, std::string label, bool enabled = true) &
    { node_->addTab(std::move(value), std::move(label), enabled); return self(); }

    TabList&& tab(std::string value, std::string label, bool enabled = true) &&
    { node_->addTab(std::move(value), std::move(label), enabled); return std::move(self()); }
    TabList& value(std::string value) & { node_->setValue(std::move(value)); return self(); }

    TabList&& value(std::string value) && { node_->setValue(std::move(value)); return std::move(self()); }
    TabList& onChange(std::function<void(const std::string&)> handler) &
    { node_->onChange(std::move(handler)); return self(); }

    TabList&& onChange(std::function<void(const std::string&)> handler) &&
    { node_->onChange(std::move(handler)); return std::move(self()); }
    TabList& activationMode(wui::TabListNode::ActivationMode value) &
    { node_->setActivationMode(value); return self(); }

    TabList&& activationMode(wui::TabListNode::ActivationMode value) &&
    { node_->setActivationMode(value); return std::move(self()); }
};

class TabPanel : public ContainerBuilderBase<TabPanel, wui::TabPanelNode> {
public:
    explicit TabPanel(std::string value = {}) : ContainerBuilderBase(std::move(value)) {}
    TabPanel& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    TabPanel&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
    TabPanel& tabList(wui::TabListNode& value) & { node_->setTabList(&value); return self(); }

    TabPanel&& tabList(wui::TabListNode& value) && { node_->setTabList(&value); return std::move(self()); }
    TabPanel& active(bool value = true) & { node_->setActive(value); return self(); }

    TabPanel&& active(bool value = true) && { node_->setActive(value); return std::move(self()); }
};

class Link : public BuilderBase<Link, wui::LinkNode> {
public:
    explicit Link(std::string label = {}) : BuilderBase(std::move(label)) {}
    Link& href(std::string value) & { node_->setHref(std::move(value)); return self(); }

    Link&& href(std::string value) && { node_->setHref(std::move(value)); return std::move(self()); }
    Link& onClick(std::function<void()> handler) & { node_->onInvoke(std::move(handler)); return self(); }

    Link&& onClick(std::function<void()> handler) && { node_->onInvoke(std::move(handler)); return std::move(self()); }
};

class Breadcrumb : public BuilderBase<Breadcrumb, wui::BreadcrumbNode> {
public:
    Breadcrumb() : BuilderBase() {}
    Breadcrumb& item(std::string label, bool current = false) &
    { node_->addItem(std::move(label), current); return self(); }

    Breadcrumb&& item(std::string label, bool current = false) &&
    { node_->addItem(std::move(label), current); return std::move(self()); }
    Breadcrumb& maxVisible(std::size_t value) & { node_->setMaxVisible(value); return self(); }

    Breadcrumb&& maxVisible(std::size_t value) && { node_->setMaxVisible(value); return std::move(self()); }
};

class Dialog : public SingleContentBuilderBase<Dialog, wui::DialogNode> {
public:
    Dialog() : SingleContentBuilderBase() {}

    Dialog& maxWidth(float width) &
    {
        node_->setMaxWidth(width);
        return self();
    }

    Dialog&& maxWidth(float width) &&
    {
        node_->setMaxWidth(width);
        return std::move(self());
    }

    Dialog& dismissOnBackdrop(bool enabled = true) &
    {
        node_->setBackdropDismissEnabled(enabled);
        return self();
    }

    Dialog&& dismissOnBackdrop(bool enabled = true) &&
    {
        node_->setBackdropDismissEnabled(enabled);
        return std::move(self());
    }

    Dialog& onDismiss(std::function<void()> handler) &
    {
        node_->onDismiss(std::move(handler));
        return self();
    }

    Dialog&& onDismiss(std::function<void()> handler) &&
    {
        node_->onDismiss(std::move(handler));
        return std::move(self());
    }

    // Dialogs are shown through UiWindow::showDialog(), which intentionally
    // accepts the concrete modal type so it can manage focus restoration.
};

} // namespace wui
