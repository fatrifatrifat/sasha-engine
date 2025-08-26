#pragma once
#include "RenderItem.h"
#include "../geometry/GeometryLibrary.h"

class Scene
{
public:
	void AddInstance(const std::string& meshName, const std::string& matName, const DirectX::XMFLOAT4X4& transform = d3dUtil::Identity4x4(), ItemType type = ItemType::Opaque);
	void AddLight(const Light& light);
	void BuildRenderItems(GeometryLibrary& geoLib);

	std::vector<std::unique_ptr<RenderItem>>& GetAllRenderItems();
	std::vector<RenderItem*>& GetItems(ItemType type);
	std::vector<Light>& GetLights();

private:
	std::vector<Light> _lights;

	std::vector<ObjectInstance> _instances;
	std::vector<RenderItem*> _opaques;
	std::vector<RenderItem*> _transparents;
	std::vector<RenderItem*> _alphaTesteds;
	std::vector<std::unique_ptr<RenderItem>> _renderItems;
};
