#pragma once

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "wui/node.h"
#include "wui/ui.h"

namespace whatsui::gallery::view::components {

// A presentational choice subtree which keeps a dense Fluent toggle row on
// desktop, while exposing the exact same choices as an accessible vertical
// RadioGroup when its assigned bounds become narrow.  Pages provide the
// selected-value reader, so a rebuilt subtree always reflects the view model.
struct ResponsiveChoiceOption {
    std::string value;
    std::string label;
};

class ResponsiveChoiceGroup final : public wui::ContainerNode {
public:
    using SelectedValue = std::function<std::string()>;
    using SelectionHandler = std::function<void(const std::string&)>;

    ResponsiveChoiceGroup(std::vector<ResponsiveChoiceOption> options,
                          SelectedValue selectedValue,
                          SelectionHandler onSelect,
                          std::string accessibleLabel,
                          std::size_t desktopRowLength = 0,
                          float compactBreakpoint = 520.0f)
        : options_(std::move(options))
        , selectedValue_(std::move(selectedValue))
        , onSelect_(std::move(onSelect))
        , accessibleLabel_(std::move(accessibleLabel))
        , desktopRowLength_(desktopRowLength)
        , compactBreakpoint_(compactBreakpoint)
    {
    }

    [[nodiscard]] wui::SizeF measure(const wui::Constraints& constraints) const override
    {
        const bool compact = constraints.maxWidth < compactBreakpoint_;
        const std::size_t rows = compact
            ? options_.size()
            : std::max<std::size_t>(1, (options_.size() + normalizedRowLength() - 1) /
                                         normalizedRowLength());
        constexpr float kChoiceHeight = 32.0f;
        constexpr float kGap = 6.0f;
        const float height = rows == 0 ? 0.0f : rows * kChoiceHeight + (rows - 1) * kGap;
        return constraints.clamp({constraints.maxWidth, height});
    }

    void layout(const wui::RectF& bounds) override
    {
        wui::Node::layout(bounds);
        const bool compact = bounds.width < compactBreakpoint_;
        if (content_ == nullptr || compact != compact_) {
            rebuild(compact);
        }
        const auto size = content_->measureWithConstraints(
            {0.0f, bounds.width, 0.0f, 1000000.0f});
        content_->layout({bounds.x, bounds.y, bounds.width, size.height});
        setBounds({bounds.x, bounds.y, bounds.width, size.height});
        clearLayoutDirtyRecursively();
    }

    void paint(wui::PaintContext& context) override
    {
        if (content_ != nullptr) content_->paint(context);
        clearDirty(wui::DirtyFlag::Paint);
    }

    [[nodiscard]] wui::Node* hitTest(wui::PointF point) override
    {
        if (!bounds().contains(point)) return nullptr;
        if (content_ != nullptr) {
            if (auto* hit = content_->hitTest(point)) return hit;
        }
        return this;
    }

private:
    [[nodiscard]] std::size_t normalizedRowLength() const noexcept
    {
        return std::max<std::size_t>(1, desktopRowLength_ == 0 ? options_.size() : desktopRowLength_);
    }

    [[nodiscard]] std::unique_ptr<wui::Node> buildDesktopContent()
    {
        using namespace wui::ui;
        auto rows = std::make_unique<wui::Column>();
        rows->setGap(6.0f);
        rows->setAlign(wui::Alignment::Start);
        const auto selected = selectedValue_();
        const auto rowLength = normalizedRowLength();
        auto buttons = std::make_shared<std::vector<std::pair<std::string, wui::ToggleButton*>>>();
        for (std::size_t begin = 0; begin < options_.size(); begin += rowLength) {
            auto row = std::make_unique<wui::Row>();
            row->setGap(6.0f);
            row->setAlign(wui::Alignment::Center);
            const auto end = std::min(options_.size(), begin + rowLength);
            for (std::size_t index = begin; index < end; ++index) {
                const auto& option = options_[index];
                auto button = std::make_unique<wui::ToggleButton>(option.label, option.value == selected);
                button->setAppearance(wui::ButtonAppearance::Subtle);
                auto* raw = button.get();
                buttons->push_back({option.value, raw});
                button->onChange([this, value = option.value, buttons, raw](bool checked) {
                    if (!checked) {
                        raw->setChecked(true);
                        return;
                    }
                    onSelect_(value);
                    const auto active = selectedValue_();
                    for (const auto& [candidate, item] : *buttons) {
                        item->setChecked(candidate == active);
                    }
                });
                row->appendChild(std::move(button));
            }
            rows->appendChild(std::move(row));
        }
        return rows;
    }

    [[nodiscard]] std::unique_ptr<wui::Node> buildCompactContent()
    {
        auto group = std::make_unique<wui::RadioGroup>();
        group->setAccessibleLabel(accessibleLabel_);
        group->setValue(selectedValue_());
        group->setGroupLayout(wui::RadioGroupLayout::Vertical);
        group->onChange([this](const std::string& value) { onSelect_(value); });
        for (const auto& option : options_) {
            group->addOption(option.value, option.label);
        }
        return group;
    }

    void rebuild(bool compact)
    {
        clearChildren();
        compact_ = compact;
        auto content = compact_ ? buildCompactContent() : buildDesktopContent();
        content_ = content.get();
        appendChild(std::move(content));
    }

    std::vector<ResponsiveChoiceOption> options_;
    SelectedValue selectedValue_;
    SelectionHandler onSelect_;
    std::string accessibleLabel_;
    std::size_t desktopRowLength_{0};
    float compactBreakpoint_{520.0f};
    wui::Node* content_{nullptr};
    bool compact_{false};
};

} // namespace whatsui::gallery::view::components
