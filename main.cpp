#include <Windows.h>
#include <iostream>
#include "TestApp.h"
#include "Application.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    bool testing = false;

    if (testing) {
        AllocConsole();
        FILE* f;
        freopen_s(&f, "CONOUT$", "w", stdout);
        freopen_s(&f, "CONOUT$", "w", stderr);

        std::cout << "=== DX12 Framework Test Started ===" << std::endl;

        TestApp app(hInstance);

        if (!app.Initialize()) {
            MessageBox(nullptr, L"Failed to initialize application", L"Error", MB_OK);
            return -1;
        }

        return app.Run();
    }
    else {
        Application app;
        return app.Run();
    }
}