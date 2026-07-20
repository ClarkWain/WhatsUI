#include <array>
#include <stdexcept>

#include "wui/icons.h"
#include "wui/overlays.h"

namespace {

void expect(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

void testPinnedMappings()
{
    using wui::IconName;
    using wui::IconSize;
    using wui::IconStyle;

    expect(wui::iconCodepoint(IconName::Delete, IconSize::Size20,
                              IconStyle::Regular) == 0xF34C,
           "Delete 20 regular must stay pinned to Fluent v1.1.328");
    expect(wui::iconCodepoint(IconName::Star, IconSize::Size20,
                              IconStyle::Filled) == 0xF718,
           "Star 20 filled must stay pinned to Fluent v1.1.328");
    expect(wui::iconCodepoint(IconName::Calendar, IconSize::Size20,
                              IconStyle::Regular) == 0xF0284,
           "Calendar must preserve its supplementary private-use codepoint");
    expect(wui::iconCodepoint(IconName::TaskList, IconSize::Size16,
                              IconStyle::Regular) == 0xEFAD,
           "TaskList must map to the square LTR Fluent glyph at 16 DIP");
    expect(wui::iconCodepoint(IconName::Checkmark, IconSize::Size12,
                              IconStyle::Filled) == 0xF293,
           "Checkbox must use the dedicated Fluent Checkmark 12 glyph");

    constexpr std::array names{
        IconName::Add, IconName::Delete, IconName::Dismiss, IconName::Edit,
        IconName::Star, IconName::StarOff, IconName::Checkmark,
        IconName::CheckmarkCircle, IconName::Square, IconName::Circle,
        IconName::Info, IconName::Warning,
        IconName::ErrorCircle, IconName::ChevronDown, IconName::ChevronUp,
        IconName::ChevronLeft, IconName::ChevronRight, IconName::Search,
        IconName::MoreHorizontal, IconName::MoreVertical, IconName::ArrowUndo,
        IconName::Calendar, IconName::Clock, IconName::Important,
        IconName::TaskList,
    };
    for (const auto name : names) {
        for (const auto style : {IconStyle::Regular, IconStyle::Filled}) {
            for (const auto size :
                 {IconSize::Size12, IconSize::Size16,
                  IconSize::Size20, IconSize::Size24}) {
                expect(wui::iconCodepoint(name, size, style) != 0,
                       "Every public semantic icon must resolve at every size");
                expect(!wui::iconUtf8(name, size, style).empty(),
                       "Every public semantic icon must encode as UTF-8");
            }
        }
    }
    expect(wui::iconUtf8(IconName::Calendar).size() == 4,
           "Supplementary Fluent codepoints must use four-byte UTF-8");
}

void testPublicWidgets()
{
    wui::Icon icon(wui::IconName::Delete);
    icon.size(wui::IconSize::Size24).style(wui::IconStyle::Filled);
    const auto measured = icon.measure({0.0f, 100.0f, 0.0f, 100.0f});
    expect(measured.width == 24.0f && measured.height == 24.0f,
           "Icon must measure to its semantic DIP size");

    wui::IconButton button(wui::IconName::Delete, "Delete task");
    expect(button.fluentIcon() == wui::IconName::Delete &&
               button.icon().empty() &&
               button.accessibleLabel() == "Delete task",
           "IconButton must keep its semantic icon separate from its label");
    button.setIconStyle(wui::IconStyle::Filled);
    expect(button.iconStyle() == wui::IconStyle::Filled,
           "IconButton must expose regular and filled Fluent states");
}

} // namespace

int main()
{
    try {
        testPinnedMappings();
        testPublicWidgets();
        return 0;
    } catch (...) {
        return 1;
    }
}
