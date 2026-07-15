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
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "wui/node.h"
#include "wui/basic_controls.h"
#include "wui/list_view.h"
#include "wui/overlays.h"
#include "wui/virtual_list.h"
#include "wui/scheduler.h"
#include "wui/state.h"
#include "wui/structural.h"
#include "wui/text_input.h"
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
};

class Checkbox : public BuilderBase<Checkbox, wui::Checkbox> {
public:
    explicit Checkbox(std::string label = {}, bool checked = false) : BuilderBase(std::move(label), checked) {}
    Checkbox&& label(std::string value) && { node_->setLabel(std::move(value)); return std::move(self()); }
    Checkbox&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
    Checkbox&& checked(bool value) && { node_->setChecked(value); return std::move(self()); }
    Checkbox&& bind(wui::State<bool>& state) && { node_->bind(state); return std::move(self()); }
    Checkbox&& onChange(std::function<void(bool)> handler) && { node_->onChange(std::move(handler)); return std::move(self()); }
    Checkbox&& enabled(bool value) && { node_->setEnabled(value); return std::move(self()); }
};

class Radio : public BuilderBase<Radio, wui::Radio> {
public:
    explicit Radio(std::string label = {}, bool selected = false) : BuilderBase(std::move(label), selected) {}
    Radio&& selected(bool value) && { node_->setSelected(value); return std::move(self()); }
    Radio&& bind(wui::State<bool>& state) && { node_->bind(state); return std::move(self()); }
    Radio&& onChange(std::function<void(bool)> handler) && { node_->onChange(std::move(handler)); return std::move(self()); }
};

class Switch : public BuilderBase<Switch, wui::Switch> {
public:
    explicit Switch(std::string label = {}, bool on = false) : BuilderBase(std::move(label), on) {}
    Switch&& on(bool value) && { node_->setOn(value); return std::move(self()); }
    Switch&& bind(wui::State<bool>& state) && { node_->bind(state); return std::move(self()); }
    Switch&& onChange(std::function<void(bool)> handler) && { node_->onChange(std::move(handler)); return std::move(self()); }
};

class Slider : public BuilderBase<Slider, wui::Slider> {
public:
    Slider(float minimum = 0.0f, float maximum = 100.0f, float value = 0.0f) : BuilderBase(minimum, maximum, value) {}
    Slider&& value(float value) && { node_->setValue(value); return std::move(self()); }
    Slider&& step(float value) && { node_->setStep(value); return std::move(self()); }
    Slider&& bind(wui::State<float>& state) && { node_->bind(state); return std::move(self()); }
    Slider&& onChange(std::function<void(float)> handler) && { node_->onChange(std::move(handler)); return std::move(self()); }
};

class ProgressBar : public BuilderBase<ProgressBar, wui::ProgressBar> {
public:
    ProgressBar(float minimum = 0.0f, float maximum = 100.0f, float value = 0.0f) : BuilderBase(minimum, maximum, value) {}
    ProgressBar&& value(float value) && { node_->setValue(value); return std::move(self()); }
    ProgressBar&& bind(wui::State<float>& state) && { node_->bind(state); return std::move(self()); }
};

class Divider : public BuilderBase<Divider, wui::Divider> {
public:
    explicit Divider(wui::DividerOrientation orientation = wui::DividerOrientation::Horizontal) : BuilderBase(orientation) {}
    Divider&& thickness(float value) && { node_->setThickness(value); return std::move(self()); }
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
    IconButton&& checked(bool value) && { node_->setChecked(value); return std::move(self()); }
    IconButton&& onClick(std::function<void()> handler) && { node_->onClick(std::move(handler)); return std::move(self()); }
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
