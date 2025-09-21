#include "../../../include/sasha/renderer/scene/Scene.h"

void Scene::AddInstance(const std::string& meshName, const std::string& matName, const DirectX::XMFLOAT4X4& transform, std::vector<ItemType> type)
{
	_instances.push_back({ meshName, transform, matName, type });
}

void Scene::AddLight(const Light& light, ItemType type)
{
	type == ItemType::Opaque ? _lights.push_back(light) : _reflectedLights.push_back(light);
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
		ri->_types = inst.type;
		
		for (auto& type : inst.type)
		{
			if (type == ItemType::Opaque)
				_opaques.push_back(ri.get());
			if (type == ItemType::Transparent)
				_transparents.push_back(ri.get());
			if (type == ItemType::AlphaTested)
				_alphaTesteds.push_back(ri.get());
			if (type == ItemType::Mirror)
				_mirrors.push_back(ri.get());
			if (type == ItemType::Reflected)
				_reflected.push_back(ri.get());
			if (type == ItemType::Shadow)
				_shadows.push_back(ri.get());
			if (type == ItemType::ReflectedShadow)
				_reflectedShadows.push_back(ri.get());
			if (type == ItemType::Pusheen)
				_pusheens.push_back(ri.get());
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
	case ItemType::Mirror:
		return _mirrors;
	case ItemType::Reflected:
		return _reflected;
	case ItemType::Shadow:
		return _shadows;
	case ItemType::ReflectedShadow:
		return _reflectedShadows;
	case ItemType::Pusheen:
		return _pusheens;
	}
}

std::vector<Light>& Scene::GetLights(ItemType type)
{
	return type == ItemType::Opaque ? _lights : _reflectedLights;
}
