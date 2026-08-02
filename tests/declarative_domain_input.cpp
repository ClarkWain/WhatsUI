#include "wui/declarative/input.h"

bool declarativeInputCompilesIndependently()
{
    wui::View view = wui::Button("Save");
    return !view.empty();
}
