#include "wui/declarative/collections.h"

bool declarativeCollectionsCompilesIndependently()
{
    return wui::ListBox().build() != nullptr;
}
