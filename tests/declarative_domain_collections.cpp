#include "wui/declarative/collections.h"

bool declarativeCollectionsCompilesIndependently()
{
    wui::View view = wui::ListBox();
    return !view.empty();
}
