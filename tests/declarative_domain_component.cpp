#include "wui/component.h"
#include "wui/declarative/layout.h"

namespace {

class ExternalComponent {
public:
    auto body()
    {
        return wui::Box().children(wui::Spacer(8.0f, 8.0f));
    }
};

static_assert(wui::isViewLikeV<ExternalComponent>);

} // namespace

bool declarativeComponentCompilesIndependently()
{
    wui::View view = ExternalComponent{};
    return !view.empty();
}
