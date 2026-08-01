#include "wui/declarative/feedback.h"

bool declarativeFeedbackCompilesIndependently()
{
    return wui::Card().build() != nullptr;
}
