#include "wui/text_metrics.h"

namespace wui {

namespace {
TextMeasurer* g_textMeasurer = nullptr;
}

void setTextMeasurer(TextMeasurer* measurer) noexcept
{
    g_textMeasurer = measurer;
}

TextMeasurer* textMeasurer() noexcept
{
    return g_textMeasurer;
}

} // namespace wui
