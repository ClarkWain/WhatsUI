#include "wui/declarative/navigation.h"

bool declarativeNavigationCompilesIndependently()
{
    return wui::Toolbar().item("Copy").build() != nullptr;
}
