#include <cstdlib>

bool declarativeBuilderBaseCompilesIndependently();
bool declarativeTextCompilesIndependently();
bool declarativeLayoutCompilesIndependently();
bool declarativeInputCompilesIndependently();
bool declarativeFeedbackCompilesIndependently();
bool declarativeNavigationCompilesIndependently();
bool declarativeCollectionsCompilesIndependently();
bool declarativeStructuralCompilesIndependently();

int main()
{
    const bool passed = declarativeBuilderBaseCompilesIndependently()
        && declarativeTextCompilesIndependently()
        && declarativeLayoutCompilesIndependently()
        && declarativeInputCompilesIndependently()
        && declarativeFeedbackCompilesIndependently()
        && declarativeNavigationCompilesIndependently()
        && declarativeCollectionsCompilesIndependently()
        && declarativeStructuralCompilesIndependently();
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
