#include "wui/declarative/builder_base.h"

namespace {

class CustomNode final : public wui::Node {
public:
    [[nodiscard]] wui::SizeF measure(
        const wui::Constraints& constraints) const override
    {
        return constraints.clamp({1.0f, 1.0f});
    }

    void paint(wui::PaintContext&) override {}
};

class Custom : public wui::BuilderBase<Custom, CustomNode> {
public:
    Custom() : BuilderBase() {}
};

} // namespace

bool declarativeBuilderBaseCompilesIndependently()
{
    auto node = Custom().debugName("External.Custom").build();
    return node != nullptr;
}
