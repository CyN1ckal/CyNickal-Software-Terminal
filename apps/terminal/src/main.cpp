#include "terminal/Application.h"

#include <exception>
#include <iostream>

int main()
{
    try
    {
        myapp::Application app;
        return app.run();
    }
    catch (const std::exception& exception)
    {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
