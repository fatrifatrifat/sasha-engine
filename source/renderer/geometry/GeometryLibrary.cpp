#include "../../../include/sasha/renderer/geometry/GeometryLibrary.h"

void GeometryLibrary::AddGeometry(const std::string& name, GeometryGenerator::MeshData& mesh)
{
    SubmeshGeometry sub;

    sub._baseVertexLocation = static_cast<UINT>(_vertices.size());
    sub._startIndexLocation = static_cast<UINT>(_indices.size());
    sub._indexCount = static_cast<UINT>(mesh.Indices32.size());

    for (const auto& v : mesh.Vertices)
        _vertices.push_back({ v.Position, v.Normal, v.TexC });

    auto& indices16 = mesh.GetIndices16();
    _indices.insert(_indices.end(), indices16.begin(), indices16.end());

    _nameToSubmesh[name] = static_cast<SubMeshID>(_submeshes.size());
    _submeshes.push_back(sub);
}

void GeometryLibrary::AddMaterial(const std::string& name, std::unique_ptr<Material>&& mat)
{
    _nameToMaterial[name] = static_cast<MaterialID>(_materials.size());
    mat->_matCBIndex = static_cast<UINT>(_materials.size());
    mat->_diffuseSrvHeapIndex = static_cast<UINT>(_materials.size());
    _materials.push_back(std::move(mat));
}

void GeometryLibrary::AddTexture(const std::string& name, std::unique_ptr<Texture>&& tex)
{
    _nameToTexture[name] = static_cast<UINT>(_textures.size());
    _textures.push_back(std::move(tex));
}

void GeometryLibrary::Upload(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
{
    _mesh = std::make_unique<MeshGeometry>(device, cmdList, _vertices, _indices);
}

const MeshGeometry& GeometryLibrary::GetMesh() const noexcept
{
    return *_mesh;
}

MeshGeometry& GeometryLibrary::GetMesh() noexcept
{
    return *_mesh;
}

const SubmeshGeometry& GeometryLibrary::GetSubmesh(const std::string& name) const
{
    return _submeshes.at(static_cast<UINT>(GetSubmeshID(name)));
}

const SubmeshGeometry& GeometryLibrary::GetSubmesh(SubMeshID id) const
{
    return _submeshes.at(id);
}

SubMeshID GeometryLibrary::GetSubmeshID(const std::string& name) const
{
    return _nameToSubmesh.at(name);
}

Material& GeometryLibrary::GetMaterial(const std::string& name)
{
    return *_materials.at(static_cast<UINT>(GetMaterialID(name)));
}

const Material& GeometryLibrary::GetMaterial(const std::string& name) const
{
    return *_materials.at(static_cast<UINT>(GetMaterialID(name)));
}

const Material& GeometryLibrary::GetMaterial(MaterialID id) const
{
    assert(id < _materials.size() && _materials[id]);
    return *_materials.at(id);
}

Material& GeometryLibrary::GetMaterial(MaterialID id)
{
    assert(id < _materials.size() && _materials[id]);
    return *_materials.at(id);
}

const Texture& GeometryLibrary::GetTexture(const std::string& name) const
{
    return *_textures.at(static_cast<UINT>(_nameToTexture.at(name)));
}

Texture& GeometryLibrary::GetTexture(const std::string& name)
{
    return *_textures.at(static_cast<UINT>(_nameToTexture.at(name)));
}

const Texture& GeometryLibrary::GetTexture(TextureID id) const
{
    assert(id < _textures.size() && _textures[id]);
    return *_textures.at(id);
}

Texture& GeometryLibrary::GetTexture(TextureID id)
{
    assert(id < _textures.size() && _textures[id]);
    return *_textures.at(id);

}

MaterialID GeometryLibrary::GetMaterialID(const std::string& name) const
{
    return _nameToMaterial.at(name);
}

size_t GeometryLibrary::GetMaterialCount() const noexcept
{
    return _materials.size();
}

size_t GeometryLibrary::GetTextureCount() const noexcept
{
    return _textures.size();
}
