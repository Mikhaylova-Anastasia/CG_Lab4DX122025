// ObjLoader.h
#pragma once
#include "Common.h"
#include <string>

struct VertexPosNormal
{
    XMFLOAT3 Pos;
    XMFLOAT3 Normal;
};

struct ObjMeshData
{
    std::vector<VertexPosNormal> Vertices;
    std::vector<uint32_t> Indices;
};

class ObjLoader
{
public:
   
    static bool LoadObjPosNormal(const std::wstring& filename, ObjMeshData& out, bool convertToLH = true);
};

