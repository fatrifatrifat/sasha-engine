#pragma once
#include "geometry/Mesh.h"
#include "geometry/GeometryLibrary.h"

class Scene
{
public:
	void AddInstance(const std::string& meshName, const std::string& matName, const DirectX::XMFLOAT4X4& transform = d3dUtil::Identity4x4());
	void AddLight(const Light& light);
	void BuildRenderItems(GeometryLibrary& geoLib);

	std::vector<std::unique_ptr<RenderItem>>& GetRenderItems();
	std::vector<Light>& GetLights();

private:
	std::vector<Light> _lights;

	std::vector<ObjectInstance> _instances;
	std::vector<std::unique_ptr<RenderItem>> _renderItems;
};
