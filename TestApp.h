#pragma once
#include "Framework.h"

class TestApp : public DX12Framework {
public:
    TestApp(HINSTANCE hInstance);
    bool Initialize() override;
    void Update(const GameTimer& timer) override;
    void Draw(const GameTimer& timer) override;
    void OnResize() override;
    void CreateRtvAndDsvDescriptorHeaps() override;
    void OnMouseDown(WPARAM btnState, int x, int y) override;
    void OnMouseUp(WPARAM btnState, int x, int y) override;
    void OnMouseMove(WPARAM btnState, int x, int y) override;

private:
    void TestTimerFunctions();
    void TestInputHandling();
    void TestWindowFunctions();
    void UpdateTestInfo();
    float clearColor[4];
    int testPhase;
    float phaseTimer;
    bool testCompleted;

    int mouseX, mouseY;
    bool leftMouseDown;
    bool rightMouseDown;

    float lastDeltaTime;
    int frameCount;
    float fpsUpdateTimer;
};