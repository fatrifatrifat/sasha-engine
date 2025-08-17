#pragma once
#include "Mesh.h"
#include "Material.h"
#include "GeometryGenerator.h"

class GeometryLibrary
{
    friend class Scene;
public:
    void AddGeometry(const std::string& name, GeometryGenerator::MeshData& mesh);
    void AddMaterial(const std::string& name, std::unique_ptr<Material>&& mat);

    void Upload(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);

    MeshGeometry* GetMesh();

    const SubmeshGeometry& GetSubmesh(const std::string& name) const;
    const SubmeshGeometry& GetSubmesh(SubMeshID id) const;
    SubMeshID GetSubmeshID(const std::string& name) const;

    Material* GetMaterial(const std::string& name) const;
    Material* GetMaterial(MaterialID id) const;
    MaterialID GetMaterialID(const std::string& name) const;
    size_t GetMaterialCount() const noexcept;

private:
    std::vector<Vertex> _vertices;
    std::vector<std::uint16_t> _indices;

    std::unordered_map<std::string, SubMeshID> _nameToSubmesh;
    std::unordered_map<std::string, MaterialID> _nameToMaterial;

    std::vector<SubmeshGeometry> _submeshes;
    std::vector<std::unique_ptr<Material>> _materials;

    std::unique_ptr<MeshGeometry> _mesh;
};
