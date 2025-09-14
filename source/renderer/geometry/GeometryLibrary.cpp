#include "../../../include/sasha/renderer/geometry/GeometryLibrary.h"

void GeometryLibrary::AddGeometry(const std::string& name, GeometryGenerator::MeshData& mesh)
{
    SubmeshGeometry sub;

    sub._baseVertexLocation = static_cast<UINT>(_vertices.size());
    sub._startIndexLocation = static_cast<UINT>(_indices.size());
    sub._indexCount = static_cast<UINT>(mesh.Indices32.size());

    for (const auto& v : mesh.Vertices)
        _vertices.push_back({ v.Position, v.Normal, v.TexC });

    //auto& indices16 = mesh.GetIndices16();
    auto& indices16 = mesh.Indices32;
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

void GeometryLibrary::AddTexture(std::string& name, std::unique_ptr<Texture>&& tex)
{
    static int repetitiveCount = 1;
    if (_nameToTexture.find(name) != _nameToTexture.end())
    {
        name = name.substr(0, name.size() - 1) + std::to_string(repetitiveCount);
        repetitiveCount++;
    }
    _nameToTexture[name] = static_cast<UINT>(_textures.size());
    _textures.push_back(std::move(tex));
}

void GeometryLibrary::AddModelTexturePath(const std::filesystem::path& filePath)
{
    _modelTexturePaths.push_back(filePath);
}

void GeometryLibrary::Upload(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
{
    _mesh = std::make_unique<MeshGeometry>(device, cmdList, _vertices, _indices);
}

std::unique_ptr<DescriptorHeap> GeometryLibrary::BuildSrvHeap(ID3D12Device* device) const
{
    auto srvHeap = std::make_unique<DescriptorHeap>(
        device,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        _textures.size(),
        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
    );

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    for (UINT i = 0; i < _textures.size(); i++)
    {
        srvDesc.Format = _textures[i]->_resource->GetDesc().Format;
        srvDesc.Texture2D.MipLevels = _textures[i]->_resource->GetDesc().MipLevels;
        device->CreateShaderResourceView(_textures[i]->_resource.Get(), &srvDesc, srvHeap->GetCPUStart(i));
    }

    return srvHeap;
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

const std::vector<std::filesystem::path>& GeometryLibrary::GetModelTexturePaths() const
{
    return _modelTexturePaths;
}

std::vector<std::filesystem::path>& GeometryLibrary::GetModelTexturePaths()
{
    return _modelTexturePaths;
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
