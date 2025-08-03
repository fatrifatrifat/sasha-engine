#pragma once
#include "Mesh.h"
#include "GeometryGenerator.h"

class GeometryLibrary
{
public:
    void AddGeometry(const std::string& name, GeometryGenerator::MeshData& mesh, const DirectX::XMFLOAT4& color);
    void Upload(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);

    MeshGeometry* GetMesh();
    const SubmeshGeometry& GetSubmesh(const std::string& name) const;

private:
    std::vector<Vertex> _vertices;
    std::vector<std::uint16_t> _indices;

    std::unordered_map<std::string, SubmeshGeometry> _submeshes;

    std::unique_ptr<MeshGeometry> _mesh; // Combined
};
