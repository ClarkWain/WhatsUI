#include "wui/declarative/layout.h"

bool declarativeLayoutCompilesIndependently()
{
    return wui::Column().children(wui::Spacer()).build() != nullptr;
}
