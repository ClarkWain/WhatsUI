#include "wui/declarative/input.h"

bool declarativeInputCompilesIndependently()
{
    return wui::Button("Save").build() != nullptr;
}
