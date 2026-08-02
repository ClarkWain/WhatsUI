#include "wui/declarative/navigation.h"

bool declarativeNavigationCompilesIndependently()
{
    wui::View view = wui::Toolbar().item("Copy");
    return !view.empty();
}
