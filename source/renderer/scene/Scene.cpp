#include "../../../include/sasha/renderer/scene/Scene.h"

void Scene::AddInstance(const std::string& meshName, const std::string& matName, const DirectX::XMFLOAT4X4& transform, ItemType type)
{
	_instances.push_back({ meshName, transform, matName, type });
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

		switch (inst.type)
		{
		case ItemType::Opaque:
			_opaques.push_back(ri.get());
			break;
		case ItemType::AlphaTested:
			_alphaTesteds.push_back(ri.get());
			break;
		case ItemType::Transparent:
			_transparents.push_back(ri.get());
			break;
		}
		_renderItems.push_back(std::move(ri));
	}
}

std::vector<std::unique_ptr<RenderItem>>& Scene::GetAllRenderItems() 
{
	return _renderItems;
}

std::vector<RenderItem*>& Scene::GetItems(ItemType type)
{
	switch (type)
	{
	case ItemType::Opaque:
		return _opaques;
	case ItemType::AlphaTested:
		return _alphaTesteds;
	case ItemType::Transparent:
		return _transparents;
	}
}

std::vector<Light>& Scene::GetLights()
{
	return _lights;
}
