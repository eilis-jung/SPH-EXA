#include "window.h"

using namespace sphexa;

int main()
{
    std::string title = "ttt";
    Window window(800, 600, title);
    window.init();
    return 0;
}