// CubeApp.h
#pragma once
#include "D3DApp.h"
#include "CubeRenderer.h"

class CubeApp : public D3DApp
{
public:
    CubeApp(HINSTANCE hInstance);
    virtual ~CubeApp();

    virtual bool Initialize() override;

    virtual void OnResize() override;
    virtual void Update(const GameTimer& gt) override;
    virtual void Draw(const GameTimer& gt) override;

private:
    std::unique_ptr<CubeRenderer> mCube;
};

