#include "../../include/sasha/renderer/Scene.h"

void Scene::AddInstance(const std::string& meshName, const DirectX::XMFLOAT4X4& transform)
{
	_instances.push_back({ meshName, transform });
}

void Scene::BuildRenderItems(GeometryLibrary& geoLib)
{
	_renderItems.clear();

	int index = 0;
	for (const auto& inst : _instances)
	{
		auto ri = std::make_unique<RenderItem>();

		ri->_cbObjIndex = index++;
		ri->_mesh = geoLib.GetMesh();
		
		const auto& submesh = geoLib.GetSubmesh(inst.meshName);
		ri->_indexCount = submesh._indexCount;
		ri->_startIndex = submesh._startIndexLocation;
		ri->_baseVertex = submesh._baseVertexLocation;
		ri->_world = inst.transform;

		_renderItems.push_back(std::move(ri));
	}
}

const std::vector<std::unique_ptr<RenderItem>>& Scene::GetRenderItems() const
{
	return _renderItems;
}
