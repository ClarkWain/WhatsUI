#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "wui/node.h"

namespace wsc {
class Canvas;
}

namespace wui {

// Semantic names keep application code independent from the private-use
// codepoints assigned by an individual icon font release.
enum class IconName {
    Add,
    Delete,
    Dismiss,
    Edit,
    Star,
    StarOff,
    Checkmark,
    CheckmarkCircle,
    Square,
    Circle,
    Info,
    Warning,
    ErrorCircle,
    ChevronDown,
    ChevronUp,
    ChevronLeft,
    ChevronRight,
    Search,
    MoreHorizontal,
    MoreVertical,
    ArrowUndo,
    Calendar,
    Clock,
    Important,
    TaskList,
};

enum class IconStyle { Regular, Filled };
enum class IconSize : std::uint8_t {
    Size12 = 12,
    Size16 = 16,
    Size20 = 20,
    Size24 = 24,
};

[[nodiscard]] const char* iconFontFamily(IconStyle style) noexcept;
[[nodiscard]] std::uint32_t iconCodepoint(
    IconName name, IconSize size = IconSize::Size20,
    IconStyle style = IconStyle::Regular) noexcept;
[[nodiscard]] std::string iconUtf8(
    IconName name, IconSize size = IconSize::Size20,
    IconStyle style = IconStyle::Regular);
void drawIcon(PaintContext& context, IconName name, const RectF& bounds,
              Color color, IconSize size = IconSize::Size20,
              IconStyle style = IconStyle::Regular);

struct IconFontStatus {
    bool regular{false};
    bool filled{false};
    [[nodiscard]] bool complete() const noexcept { return regular && filled; }
};

// Registers the bundled Fluent System Icons faces with a WhatsCanvas. The
// resolver checks the source tree during development and standard installed
// asset locations beside the executable. Applications can call this again
// safely; WhatsCanvas treats equivalent face registration as idempotent.
[[nodiscard]] IconFontStatus registerDefaultIconFonts(wsc::Canvas& canvas);

class Icon : public Node {
public:
    explicit Icon(IconName name = IconName::Info);

    Icon& name(IconName value) noexcept;
    Icon& size(IconSize value) noexcept;
    Icon& style(IconStyle value) noexcept;
    Icon& color(Color value) noexcept;
    Icon& useThemeColor() noexcept;

    void setName(IconName value) noexcept;
    void setSize(IconSize value) noexcept;
    void setStyle(IconStyle value) noexcept;
    void setColor(std::optional<Color> value) noexcept;

    [[nodiscard]] IconName name() const noexcept;
    [[nodiscard]] IconSize size() const noexcept;
    [[nodiscard]] IconStyle style() const noexcept;
    [[nodiscard]] std::optional<Color> color() const noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;

private:
    IconName name_{IconName::Info};
    IconSize size_{IconSize::Size20};
    IconStyle style_{IconStyle::Regular};
    std::optional<Color> color_;
};

} // namespace wui
