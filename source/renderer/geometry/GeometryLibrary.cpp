#include "../../../include/sasha/renderer/geometry/GeometryLibrary.h"

void GeometryLibrary::AddGeometry(const std::string& name, GeometryGenerator::MeshData& mesh, const DirectX::XMFLOAT4& color)
{
    SubmeshGeometry sub;

    sub._baseVertexLocation = static_cast<UINT>(_vertices.size());
    sub._startIndexLocation = static_cast<UINT>(_indices.size());
    sub._indexCount = static_cast<UINT>(mesh.Indices32.size());

    for (const auto& v : mesh.Vertices)
        _vertices.push_back({ v.Position, color, v.Normal });

    auto indices16 = mesh.GetIndices16();
    _indices.insert(_indices.end(), indices16.begin(), indices16.end());

    _submeshes[name] = sub;
}

void GeometryLibrary::AddGeometry(const std::string& name, GeometryGenerator::MeshData& mesh)
{
    SubmeshGeometry sub;

    sub._baseVertexLocation = static_cast<UINT>(_vertices.size());
    sub._startIndexLocation = static_cast<UINT>(_indices.size());
    sub._indexCount = static_cast<UINT>(mesh.Indices32.size());

    for (const auto& v : mesh.Vertices)
        _vertices.push_back({ v.Position, DirectX::XMFLOAT4(v.Color.x, v.Color.y, v.Color.z, 1.f), v.Normal });

    auto indices16 = mesh.GetIndices16();
    _indices.insert(_indices.end(), indices16.begin(), indices16.end());

    _submeshes[name] = sub;
}

void GeometryLibrary::AddMaterial(const std::string& name, std::unique_ptr<Material>&& mat)
{
    _materials[name] = std::move(mat);
}

void GeometryLibrary::Upload(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
{
    _mesh = std::make_unique<MeshGeometry>(device, cmdList, _vertices, _indices);
    _mesh->_subGeometry = _submeshes;
}

MeshGeometry* GeometryLibrary::GetMesh()
{
    return _mesh.get();
}

const SubmeshGeometry& GeometryLibrary::GetSubmesh(const std::string& name) const
{
    return _submeshes.at(name);
}

Material* GeometryLibrary::GetMaterial(const std::string& name) const
{
    return _materials.at(name).get();
}
