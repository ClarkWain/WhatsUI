#include "wui/declarative/feedback.h"

bool declarativeFeedbackCompilesIndependently()
{
    wui::View view = wui::Card();
    return !view.empty();
}
