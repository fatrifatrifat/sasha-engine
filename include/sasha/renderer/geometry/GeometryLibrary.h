#pragma once
#include "Mesh.h"
#include "Material.h"
#include "Texture.h"
#include "GeometryGenerator.h"

class GeometryLibrary
{
public:
    void AddGeometry(const std::string& name, GeometryGenerator::MeshData& mesh);
    void AddMaterial(const std::string& name, std::unique_ptr<Material>&& mat);
    void AddTexture(const std::string& name, std::unique_ptr<Texture>&& tex);

    void Upload(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);

    [[nodiscard]] const MeshGeometry& GetMesh() const noexcept;
    [[nodiscard]] MeshGeometry& GetMesh() noexcept;

    const SubmeshGeometry& GetSubmesh(const std::string& name) const;
    const SubmeshGeometry& GetSubmesh(SubMeshID id) const;

    const Material& GetMaterial(const std::string& name) const;
    Material& GetMaterial(const std::string& name);
    const Material& GetMaterial(MaterialID id) const;
    Material& GetMaterial(MaterialID id);

    const Texture& GetTexture(const std::string& name) const;
    Texture& GetTexture(const std::string& name);
    const Texture& GetTexture(TextureID id) const;
    Texture& GetTexture(TextureID id);
    
    SubMeshID GetSubmeshID(const std::string& name) const;
    MaterialID GetMaterialID(const std::string& name) const;
    
    size_t GetMaterialCount() const noexcept;
    size_t GetTextureCount() const noexcept;

private:
    std::vector<Vertex> _vertices;
    std::vector<std::uint16_t> _indices;

    std::unordered_map<std::string, SubMeshID> _nameToSubmesh;
    std::unordered_map<std::string, MaterialID> _nameToMaterial;
    std::unordered_map<std::string, TextureID> _nameToTexture;

    std::vector<SubmeshGeometry> _submeshes;
    std::vector<std::unique_ptr<Material>> _materials;
    std::vector <std::unique_ptr<Texture>> _textures;

    std::unique_ptr<MeshGeometry> _mesh;
};
