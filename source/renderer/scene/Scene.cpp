#include "../../../include/sasha/renderer/scene/Scene.h"

void Scene::AddInstance(const std::string& meshName, const std::string& matName, const DirectX::XMFLOAT4X4& transform)
{
	_instances.push_back({ meshName, transform, matName });
}

void Scene::AddLight(const Light& light)
{
	_lights.push_back(light);
}

void Scene::BuildRenderItems(GeometryLibrary& geoLib)
{
	_renderItems.clear();

	int index = 0;
	for (const auto& inst : _instances)
	{
		auto ri = std::make_unique<RenderItem>();

		ri->_cbObjIndex = index++;
		
		const auto& submesh = geoLib.GetSubmeshID(inst.meshName);
		const auto& material = geoLib.GetMaterialID(inst.matName);
		ri->_submeshId = submesh;
		ri->_materialId = material;

		ri->_world = inst.transform;

		_renderItems.push_back(std::move(ri));
	}
}

std::vector<std::unique_ptr<RenderItem>>& Scene::GetRenderItems() 
{
	return _renderItems;
}

std::vector<Light>& Scene::GetLights()
{
	return _lights;
}
