#include "inspector_page.h"

#include "inspector_components.h"

#include "wui/theme.h"

namespace whatsui::debug_inspector {

wui::Box InspectorPage::body()
{
    using namespace wui;

    return Box()
        .background(theme().colors.background)
        .children(
            Row()
                .align(Alignment::Stretch)
                .children(
                    Spacer().flex(1.0f),
                    InspectorRail(diagnostics_),
                    Spacer().flex(1.0f)
                )
        );
}

} // namespace whatsui::debug_inspector
