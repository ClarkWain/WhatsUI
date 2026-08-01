#include "wui/declarative/text.h"

bool declarativeTextCompilesIndependently()
{
    wui::View view = wui::Text("Text");
    return !view.empty();
}
