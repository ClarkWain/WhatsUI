#include "wui/declarative/structural.h"

bool declarativeStructuralCompilesIndependently()
{
    wui::State<std::vector<int>> items{{1}};
    return wui::ForEach<int>(
               items,
               [](const int&) {
                   return std::make_unique<wui::BoxNode>();
               })
               .build()
        != nullptr;
}
