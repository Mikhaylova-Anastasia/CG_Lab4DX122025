// ObjLoader.cpp
#include "ObjLoader.h"
#include <fstream>
#include <sstream>
#include <unordered_map>

struct IdxTriplet
{
    int v = 0;
    int vn = 0;
    bool operator==(const IdxTriplet& o) const { return v == o.v && vn == o.vn; }
};

struct IdxHash
{
    size_t operator()(const IdxTriplet& t) const noexcept
    {
        return (size_t)t.v * 73856093u ^ (size_t)t.vn * 19349663u;
    }
};

static bool ParseFaceToken(const std::string& tok, int& v, int& vn)
{
    
    v = 0; vn = 0;

    int a = 0, b = 0, c = 0;
    char slash = 0;

    
    int slashCount = 0;
    for (char ch : tok) if (ch == '/') slashCount++;

    if (slashCount == 0)
    {
        a = std::stoi(tok);
        v = a;
        return true;
    }
    if (slashCount == 1)
    {
        // v/vt
        std::stringstream ss(tok);
        std::string s1, s2;
        std::getline(ss, s1, '/');
        std::getline(ss, s2, '/');
        v = std::stoi(s1);
        return true;
    }

    
    {
        std::stringstream ss(tok);
        std::string s1, s2, s3;
        std::getline(ss, s1, '/');
        std::getline(ss, s2, '/');
        std::getline(ss, s3, '/');
        v = s1.empty() ? 0 : std::stoi(s1);
        
        vn = s3.empty() ? 0 : std::stoi(s3);
        return true;
    }
}

bool ObjLoader::LoadObjPosNormal(const std::wstring& filename, ObjMeshData& out, bool convertToLH)
{
    out.Vertices.clear();
    out.Indices.clear();

    std::ifstream fin(filename);
    if (!fin.is_open())
        return false;

    std::vector<XMFLOAT3> positions(1); 
    std::vector<XMFLOAT3> normals(1);

    std::unordered_map<IdxTriplet, uint32_t, IdxHash> uniqueMap;

    std::string line;
    while (std::getline(fin, line))
    {
        if (line.empty() || line[0] == '#') continue;

        std::stringstream ss(line);
        std::string tag;
        ss >> tag;

        if (tag == "v")
        {
            float x, y, z;
            ss >> x >> y >> z;
            if (convertToLH) z = -z;
            positions.push_back(XMFLOAT3(x, y, z));
        }
        else if (tag == "vn")
        {
            float x, y, z;
            ss >> x >> y >> z;
            if (convertToLH) z = -z;
            normals.push_back(XMFLOAT3(x, y, z));
        }
        else if (tag == "f")
        {
            
            std::vector<std::string> toks;
            std::string t;
            while (ss >> t) toks.push_back(t);

            if (toks.size() < 3) continue;

            
            auto getIndex = [&](const std::string& tok) -> uint32_t
                {
                    int v = 0, vn = 0;
                    ParseFaceToken(tok, v, vn);

                    
                    if (v < 0) v = (int)positions.size() + v;
                    if (vn < 0) vn = (int)normals.size() + vn;

                    IdxTriplet key{ v, vn };
                    auto it = uniqueMap.find(key);
                    if (it != uniqueMap.end())
                        return it->second;

                    VertexPosNormal vert{};
                    vert.Pos = positions[(size_t)v];

                    if (vn > 0 && (size_t)vn < normals.size())
                        vert.Normal = normals[(size_t)vn];
                    else
                        vert.Normal = XMFLOAT3(0, 1, 0); 

                    uint32_t newIndex = (uint32_t)out.Vertices.size();
                    out.Vertices.push_back(vert);
                    uniqueMap[key] = newIndex;
                    return newIndex;
                };

            uint32_t i0 = getIndex(toks[0]);
            for (size_t i = 1; i + 1 < toks.size(); ++i)
            {
                uint32_t i1 = getIndex(toks[i]);
                uint32_t i2 = getIndex(toks[i + 1]);

                
                if (convertToLH)
                {
                    out.Indices.push_back(i0);
                    out.Indices.push_back(i2);
                    out.Indices.push_back(i1);
                }
                else
                {
                    out.Indices.push_back(i0);
                    out.Indices.push_back(i1);
                    out.Indices.push_back(i2);
                }
            }
        }
    }

    return !out.Vertices.empty() && !out.Indices.empty();
}
