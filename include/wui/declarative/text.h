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

#include "wui/icons.h"
#include "wui/state.h"
#include "wui/widgets.h"
#include "wui/declarative/builder_base.h"

namespace wui {

class Text : public BuilderBase<Text, wui::TextNode> {
public:
    explicit Text(std::string value = {})
        : BuilderBase(std::move(value))
    {
    }

    Text& text(std::string value) &
    {
        node_->setValue(std::move(value));
        return self();
    }

    Text&& text(std::string value) &&
    {
        node_->setValue(std::move(value));
        return std::move(self());
    }

    Text& size(float fontSize) &
    {
        node_->setFontSize(fontSize);
        return self();
    }

    Text&& size(float fontSize) &&
    {
        node_->setFontSize(fontSize);
        return std::move(self());
    }

    Text& weight(int fontWeight) &
    {
        node_->setFontWeight(fontWeight);
        return self();
    }

    Text&& weight(int fontWeight) &&
    {
        node_->setFontWeight(fontWeight);
        return std::move(self());
    }

    Text& lineHeight(float height) &
    {
        node_->setLineHeight(height);
        return self();
    }

    Text&& lineHeight(float height) &&
    {
        node_->setLineHeight(height);
        return std::move(self());
    }

    Text& fillWidth(bool fill = true) &
    {
        node_->setFillAvailableWidth(fill);
        return self();
    }

    Text&& fillWidth(bool fill = true) &&
    {
        node_->setFillAvailableWidth(fill);
        return std::move(self());
    }

    Text& style(const wui::TextStyleToken& value) &
    {
        node_->setTextStyle(value);
        return self();
    }

    Text&& style(const wui::TextStyleToken& value) &&
    {
        node_->setTextStyle(value);
        return std::move(self());
    }

    Text& role(wui::TextRole value) & { node_->setRole(value); return self(); }

    Text&& role(wui::TextRole value) && { node_->setRole(value); return std::move(self()); }
    Text& align(wui::TextAlign value) & { node_->setAlignment(value); return self(); }

    Text&& align(wui::TextAlign value) && { node_->setAlignment(value); return std::move(self()); }
    Text& underline(bool value = true) & { node_->setUnderline(value); return self(); }

    Text&& underline(bool value = true) && { node_->setUnderline(value); return std::move(self()); }
    Text& strikethrough(bool value = true) & { node_->setStrikethrough(value); return self(); }

    Text&& strikethrough(bool value = true) && { node_->setStrikethrough(value); return std::move(self()); }

    Text& wrap(wui::TextWrap value = wui::TextWrap::Word) &
    {
        node_->setWrap(value);
        return self();
    }

    Text&& wrap(wui::TextWrap value = wui::TextWrap::Word) &&
    {
        node_->setWrap(value);
        return std::move(self());
    }

    Text& maxLines(std::size_t value) &
    {
        node_->setMaxLines(value);
        return self();
    }

    Text&& maxLines(std::size_t value) &&
    {
        node_->setMaxLines(value);
        return std::move(self());
    }

    Text& ellipsis(bool enabled = true) &
    {
        node_->setOverflow(enabled ? wui::TextOverflow::Ellipsis : wui::TextOverflow::Clip);
        return self();
    }

    Text&& ellipsis(bool enabled = true) &&
    {
        node_->setOverflow(enabled ? wui::TextOverflow::Ellipsis : wui::TextOverflow::Clip);
        return std::move(self());
    }

    Text& color(Color color) &
    {
        node_->setColor(color);
        return self();
    }

    Text&& color(Color color) &&
    {
        node_->setColor(color);
        return std::move(self());
    }

    // Reactive: re-render the text whenever the observable source changes.
    template <class T, class Format>
    Text& bind(wui::State<T>& source, Format format) &
    {
        applyStateBinding(source, std::move(format));
        return self();
    }

    template <class T, class Format>
    Text&& bind(wui::State<T>& source, Format format) &&
    {
        applyStateBinding(source, std::move(format));
        return std::move(self());
    }

    template <class Observable, class Format>
    Text& bind(Observable& source, Format format) &
    {
        applyObservableBinding(source, std::move(format));
        return self();
    }

    template <class Observable, class Format>
    Text&& bind(Observable& source, Format format) &&
    {
        applyObservableBinding(source, std::move(format));
        return std::move(self());
    }

    Text& bind(wui::State<std::string>& state) &
    {
        return this->bind(
            state, [](const std::string& value) { return value; });
    }

    Text&& bind(wui::State<std::string>& state) &&
    {
        return std::move(*this).bind(
            state, [](const std::string& value) { return value; });
    }

private:
    template <class T, class Format>
    void applyStateBinding(wui::State<T>& source, Format format)
    {
        wui::TextNode* raw = node_.get();
        wui::State<T> retained = source;
        struct Subscription {
            std::size_t id{0};
            bool active{false};
        };
        auto subscription = std::make_shared<Subscription>();
        auto connect = [raw, retained, format, subscription] {
            raw->setValue(format(retained.get()));
            if (subscription->active) {
                return;
            }
            subscription->id = retained.subscribe(
                [raw, format](const T& value) {
                    raw->setValue(format(value));
                });
            subscription->active = true;
        };
        auto disconnect = [retained, subscription] {
            if (!subscription->active) {
                return;
            }
            retained.unsubscribe(subscription->id);
            subscription->active = false;
        };
        connect();
        raw->addAttachCallback(connect);
        raw->addDetachCallback(disconnect);
        raw->addTeardown(disconnect);
    }

    template <class Observable, class Format>
    void applyObservableBinding(Observable& source, Format format)
    {
        wui::TextNode* raw = node_.get();
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
            subscription->id = source->subscribe(
                [raw, format](const auto& value) {
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
        connect();
        raw->addAttachCallback(connect);
        raw->addDetachCallback(disconnect);
        raw->addTeardown(disconnect);
    }
};

class Icon : public BuilderBase<Icon, wui::IconNode> {
public:
    explicit Icon(wui::IconName name = wui::IconName::Info)
        : BuilderBase(name) {}
    Icon& name(wui::IconName value) & { node_->setName(value); return self(); }

    Icon&& name(wui::IconName value) && { node_->setName(value); return std::move(self()); }
    Icon& size(wui::IconSize value) & { node_->setSize(value); return self(); }

    Icon&& size(wui::IconSize value) && { node_->setSize(value); return std::move(self()); }
    Icon& style(wui::IconStyle value) & { node_->setStyle(value); return self(); }

    Icon&& style(wui::IconStyle value) && { node_->setStyle(value); return std::move(self()); }
    Icon& color(Color value) & { node_->setColor(value); return self(); }

    Icon&& color(Color value) && { node_->setColor(value); return std::move(self()); }
};

class Image : public BuilderBase<Image, wui::ImageNode> {
public:
    Image() : BuilderBase() {}

    Image(std::vector<unsigned char> rgbaPixels, int pixelWidth, int pixelHeight)
        : BuilderBase(std::move(rgbaPixels), pixelWidth, pixelHeight)
    {
    }

    Image& source(std::vector<unsigned char> rgbaPixels, int pixelWidth, int pixelHeight) &
    {
        node_->setSource(std::move(rgbaPixels), pixelWidth, pixelHeight);
        return self();
    }

    Image&& source(std::vector<unsigned char> rgbaPixels, int pixelWidth, int pixelHeight) &&
    {
        node_->setSource(std::move(rgbaPixels), pixelWidth, pixelHeight);
        return std::move(self());
    }

    Image& fallback(std::vector<unsigned char> rgbaPixels, int pixelWidth, int pixelHeight) &
    {
        node_->fallback(std::move(rgbaPixels), pixelWidth, pixelHeight);
        return self();
    }

    Image&& fallback(std::vector<unsigned char> rgbaPixels, int pixelWidth, int pixelHeight) &&
    {
        node_->fallback(std::move(rgbaPixels), pixelWidth, pixelHeight);
        return std::move(self());
    }

    Image& fit(wui::ImageFit fit) &
    {
        node_->setFit(fit);
        return self();
    }

    Image&& fit(wui::ImageFit fit) &&
    {
        node_->setFit(fit);
        return std::move(self());
    }

    Image& align(float x, float y) &
    {
        node_->setAlignment(x, y);
        return self();
    }

    Image&& align(float x, float y) &&
    {
        node_->setAlignment(x, y);
        return std::move(self());
    }

    Image& shape(wui::ImageShape value) & { node_->setShape(value); return self(); }

    Image&& shape(wui::ImageShape value) && { node_->setShape(value); return std::move(self()); }
    Image& bordered(bool value = true) & { node_->setBordered(value); return self(); }

    Image&& bordered(bool value = true) && { node_->setBordered(value); return std::move(self()); }
    Image& shadow(bool value = true) & { node_->setShadow(value); return self(); }

    Image&& shadow(bool value = true) && { node_->setShadow(value); return std::move(self()); }
    Image& block(bool value = true) & { node_->setBlock(value); return self(); }

    Image&& block(bool value = true) && { node_->setBlock(value); return std::move(self()); }
    Image& alt(std::string value) & { node_->setAlt(std::move(value)); return self(); }

    Image&& alt(std::string value) && { node_->setAlt(std::move(value)); return std::move(self()); }
    Image& decorative(bool value = true) & { node_->setDecorative(value); return self(); }

    Image&& decorative(bool value = true) && { node_->setDecorative(value); return std::move(self()); }
};

} // namespace wui
