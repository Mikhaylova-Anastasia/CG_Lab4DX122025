#include <Windows.h>
#include "Application.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    Application app;
    return app.Run();
}