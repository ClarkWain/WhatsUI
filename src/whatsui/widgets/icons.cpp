#include "wui/icons.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <system_error>
#include <vector>

#include "wui/paint_context.h"
#include "wui/theme.h"

#ifdef WHATSUI_HAS_WHATSCANVAS
#include "wsc/Canvas.h"
#include "wsc/Font.h"
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
#endif

namespace wui {
namespace {

constexpr std::uint32_t bySize(IconSize size, std::uint32_t size16,
                               std::uint32_t size20,
                               std::uint32_t size24) noexcept
{
    switch (size) {
    case IconSize::Size16: return size16;
    case IconSize::Size24: return size24;
    case IconSize::Size20:
    default: return size20;
    }
}

std::string encodeUtf8(std::uint32_t codepoint)
{
    std::string result;
    if (codepoint <= 0x7F) {
        result.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FF) {
        result.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
        result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0xFFFF) {
        result.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
        result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0x10FFFF) {
        result.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
        result.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
    return result;
}

#ifdef WHATSUI_HAS_WHATSCANVAS
std::filesystem::path executableDirectory()
{
#if defined(_WIN32)
    std::array<wchar_t, 32768> buffer{};
    const DWORD length = GetModuleFileNameW(
        nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length > 0 && length < buffer.size())
        return std::filesystem::path(
            std::wstring(buffer.data(), static_cast<std::size_t>(length)))
            .parent_path();
#endif
    std::error_code error;
    return std::filesystem::current_path(error);
}

std::vector<std::filesystem::path> iconAssetDirectories()
{
    std::vector<std::filesystem::path> candidates;
#if defined(WHATSUI_FLUENT_ICONS_SOURCE_DIR)
    candidates.emplace_back(WHATSUI_FLUENT_ICONS_SOURCE_DIR);
#endif
    const auto executable = executableDirectory();
    candidates.push_back(executable / "assets" / "fonts" /
                         "fluent-system-icons");
    candidates.push_back(executable.parent_path() / "share" / "WhatsUI" /
                         "fonts" / "fluent-system-icons");
    candidates.push_back(executable.parent_path() / "share" / "whatsui" /
                         "fonts" / "fluent-system-icons");
    std::error_code error;
    const auto current = std::filesystem::current_path(error);
    if (!error)
        candidates.push_back(current / "assets" / "fonts" /
                             "fluent-system-icons");
    return candidates;
}

std::filesystem::path findIconFont(const char* fileName)
{
    std::error_code error;
    for (const auto& directory : iconAssetDirectories()) {
        const auto candidate = directory / fileName;
        if (std::filesystem::is_regular_file(candidate, error)) return candidate;
        error.clear();
    }
    return {};
}

bool registerFace(wsc::Canvas& canvas, const char* family, const char* fileName)
{
    const auto path = findIconFont(fileName);
    if (path.empty()) return false;
    auto face = wsc::FontFace::fromFile(
        wsc::FontDescriptor(family, 400), path.u8string());
    // Current releases allocate both BMP and supplementary private-use
    // codepoints. Declaring both ranges keeps fallback selection deterministic.
    face.addCodepointRange(0xE000, 0xF8FF);
    face.addCodepointRange(0xF0000, 0xFFFFD);
    return canvas.registerFontFace(face);
}
#endif

} // namespace

const char* iconFontFamily(IconStyle style) noexcept
{
    return style == IconStyle::Filled ? "FluentSystemIcons-Filled"
                                      : "FluentSystemIcons-Regular";
}

std::uint32_t iconCodepoint(IconName name, IconSize size,
                            IconStyle style) noexcept
{
    const bool filled = style == IconStyle::Filled;
    switch (name) {
    case IconName::Add:
        return bySize(size, 0xF108, 0xF109, 0xF10A);
    case IconName::Delete:
        return filled ? bySize(size, 0xE487, 0xF34C, 0xF34D)
                      : bySize(size, 0xE47B, 0xF34C, 0xF34D);
    case IconName::Dismiss:
        return bySize(size, 0xF368, 0xF369, 0xF36A);
    case IconName::Edit:
        return filled ? bySize(size, 0xF3DB, 0xF3DC, 0xF3DD)
                      : bySize(size, 0xF3DC, 0xF3DD, 0xF3DE);
    case IconName::Star:
        return filled ? bySize(size, 0xF717, 0xF718, 0xF719)
                      : bySize(size, 0xF70E, 0xF70F, 0xF710);
    case IconName::StarOff:
        return filled ? bySize(size, 0xF727, 0xF728, 0xF729)
                      : bySize(size, 0xF719, 0xF71A, 0xF71B);
    case IconName::Checkmark:
        return filled ? bySize(size, 0xE319, 0xF294, 0xF295)
                      : bySize(size, 0xE305, 0xF294, 0xF295);
    case IconName::CheckmarkCircle:
        return bySize(size, 0xF297, 0xF298, 0xF299);
    case IconName::Info:
        return filled ? bySize(size, 0xF4A9, 0xF4AA, 0xF4AB)
                      : bySize(size, 0xF4A2, 0xF4A3, 0xF4A4);
    case IconName::Warning:
        return filled ? bySize(size, 0xF880, 0xF881, 0xF882)
                      : bySize(size, 0xF868, 0xF869, 0xF86A);
    case IconName::ErrorCircle:
        return filled ? bySize(size, 0xF3EF, 0xF3F0, 0xF3F1)
                      : bySize(size, 0xF3F0, 0xF3F1, 0xF3F2);
    case IconName::ChevronDown:
        return bySize(size, 0xF2A2, 0xF2A3, 0xF2A4);
    case IconName::ChevronUp:
        return bySize(size, 0xF2B5, 0xF2B6, 0xF2B7);
    case IconName::ChevronLeft:
        return bySize(size, 0xF2A9, 0xF2AA, 0xF2AB);
    case IconName::ChevronRight:
        return bySize(size, 0xF2AF, 0xF2B0, 0xF2B1);
    case IconName::Search:
        return filled ? bySize(size, 0xEA84, 0xF699, 0xF69A)
                      : bySize(size, 0xEA7C, 0xF68F, 0xF690);
    case IconName::MoreHorizontal:
        return filled ? bySize(size, 0xE831, 0xE832, 0xE833)
                      : bySize(size, 0xE823, 0xE824, 0xE825);
    case IconName::MoreVertical:
        return filled ? bySize(size, 0xE837, 0xF560, 0xF561)
                      : bySize(size, 0xE829, 0xF556, 0xF557);
    case IconName::ArrowUndo:
        return bySize(size, 0xE126, 0xF199, 0xF19A);
    case IconName::Calendar:
        return filled ? bySize(size, 0xF0296, 0xF0297, 0xF0298)
                      : bySize(size, 0xF0283, 0xF0284, 0xF0285);
    case IconName::Clock:
        return bySize(size, 0xF2DC, 0xF2DD, 0xF2DE);
    case IconName::Important:
        return filled ? bySize(size, 0xF4A5, 0xF4A6, 0xF4A7)
                      : bySize(size, 0xF49E, 0xF49F, 0xF4A0);
    case IconName::TaskList:
        return filled ? bySize(size, 0xEFAD, 0xEC97, 0xEC98)
                      : bySize(size, 0xEFAD, 0xEC99, 0xEC9A);
    }
    return 0;
}

std::string iconUtf8(IconName name, IconSize size, IconStyle style)
{
    return encodeUtf8(iconCodepoint(name, size, style));
}

void drawIcon(PaintContext& context, IconName name, const RectF& bounds,
              Color color, IconSize size, IconStyle style)
{
    const float extent = static_cast<float>(size);
    const auto glyph = iconUtf8(name, size, style);
    const auto family = iconFontFamily(style);
    float glyphWidth = extent;
#ifdef WHATSUI_HAS_WHATSCANVAS
    if (const auto* canvas = context.canvas()) {
        wsc::Paint paint;
        paint.setTextSize(extent * context.canvasCoordinateScale());
        paint.setFontFamily(family);
        paint.setFontWeight(400);
        glyphWidth = canvas->measureTextMetrics(glyph, paint).width /
                     context.canvasCoordinateScale();
    }
#endif
    const float x =
        bounds.x + std::max(0.0f, (bounds.width - glyphWidth) * 0.5f);
    context.drawText(
        glyph, x,
        context.centeredTextBottom(glyph, bounds, extent, 400, family),
        extent, color, 400, family);
}

IconFontStatus registerDefaultIconFonts(wsc::Canvas& canvas)
{
#ifdef WHATSUI_HAS_WHATSCANVAS
    return {
        registerFace(canvas, iconFontFamily(IconStyle::Regular),
                     "FluentSystemIcons-Regular.ttf"),
        registerFace(canvas, iconFontFamily(IconStyle::Filled),
                     "FluentSystemIcons-Filled.ttf"),
    };
#else
    (void)canvas;
    return {};
#endif
}

Icon::Icon(IconName name) : name_(name) {}
Icon& Icon::name(IconName value) noexcept { setName(value); return *this; }
Icon& Icon::size(IconSize value) noexcept { setSize(value); return *this; }
Icon& Icon::style(IconStyle value) noexcept { setStyle(value); return *this; }
Icon& Icon::color(Color value) noexcept { setColor(value); return *this; }
Icon& Icon::useThemeColor() noexcept { setColor(std::nullopt); return *this; }
void Icon::setName(IconName value) noexcept { if (name_ != value) { name_ = value; markDirty(DirtyFlag::Paint); } }
void Icon::setSize(IconSize value) noexcept { if (size_ != value) { size_ = value; markDirty(DirtyFlag::Layout); } }
void Icon::setStyle(IconStyle value) noexcept { if (style_ != value) { style_ = value; markDirty(DirtyFlag::Paint); } }
void Icon::setColor(std::optional<Color> value) noexcept
{
    const bool same =
        color_.has_value() == value.has_value() &&
        (!color_ ||
         (color_->r == value->r && color_->g == value->g &&
          color_->b == value->b && color_->a == value->a));
    if (!same) {
        color_ = value;
        markDirty(DirtyFlag::Paint);
    }
}
IconName Icon::name() const noexcept { return name_; }
IconSize Icon::size() const noexcept { return size_; }
IconStyle Icon::style() const noexcept { return style_; }
std::optional<Color> Icon::color() const noexcept { return color_; }

SizeF Icon::measure(const Constraints& constraints) const
{
    const float extent = static_cast<float>(size_);
    return constraints.clamp({extent, extent});
}

void Icon::paint(PaintContext& context)
{
    drawIcon(context, name_, bounds(),
             color_.value_or(theme().colors.neutralForeground1), size_,
             style_);
    clearDirty(DirtyFlag::Paint);
}

} // namespace wui
