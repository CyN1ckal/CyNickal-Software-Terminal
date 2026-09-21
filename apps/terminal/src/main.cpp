// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "terminal/Application.h"

#include <exception>
#include <iostream>

int main()
{
    try
    {
        terminal::Application app;
        return app.run();
    }
    catch (const std::exception& exception)
    {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
