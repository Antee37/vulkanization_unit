#include <iostream>
#include "vkGfx.h"

int main()
{
    std::cout << "Hello World!\n";

    vk::Gfx vkGfx;
    if (vkGfx.Init()) {
        while (!vkGfx.IsQuitRequest()) {
            if (vkGfx.PollEventsAndRender()) {

                

            }
        }


    }
}
