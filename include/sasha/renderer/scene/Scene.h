#pragma once
#include "RenderItem.h"
#include "../geometry/GeometryLibrary.h"

class Scene
{
public:
	void AddInstance(const std::string& meshName, const std::string& matName, const DirectX::XMFLOAT4X4& transform = d3dUtil::Identity4x4(), std::vector<ItemType> type = { ItemType::Opaque });
	void AddLight(const Light& light, ItemType type = ItemType::Opaque);
	void BuildRenderItems(GeometryLibrary& geoLib);

	std::vector<std::unique_ptr<RenderItem>>& GetAllRenderItems();
	std::vector<RenderItem*>& GetItems(ItemType type);
	std::vector<Light>& GetLights(ItemType type = ItemType::Opaque);

private:
	std::vector<Light> _lights;
	std::vector<Light> _reflectedLights;

	std::vector<ObjectInstance> _instances;
	std::vector<RenderItem*> _opaques;
	std::vector<RenderItem*> _transparents;
	std::vector<RenderItem*> _alphaTesteds;
	std::vector<RenderItem*> _mirrors;
	std::vector<RenderItem*> _reflected;
	std::vector<RenderItem*> _shadows;
	std::vector<RenderItem*> _reflectedShadows;
	std::vector<std::unique_ptr<RenderItem>> _renderItems;
};
