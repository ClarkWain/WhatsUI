#include "wui/declarative/text.h"

bool declarativeTextCompilesIndependently()
{
    return wui::Text("Text").build() != nullptr;
}
