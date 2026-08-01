#include "wui/declarative/layout.h"

bool declarativeLayoutCompilesIndependently()
{
    wui::View view = wui::Column().children(wui::Spacer());
    return !view.empty();
}
