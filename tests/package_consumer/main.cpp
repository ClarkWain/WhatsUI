#include <wui/wui.h>

int main()
{
    wui::SizeF size{640.0f, 480.0f};
    return size.width == 640.0f && size.height == 480.0f ? 0 : 1;
}
