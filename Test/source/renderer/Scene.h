#pragma once
#include "geometry/Mesh.h"
#include "geometry/GeometryLibrary.h"

class Scene
{
public:
	void AddInstance(const std::string& meshName, const DirectX::XMFLOAT4& color, const DirectX::XMFLOAT4X4& transform);
	void BuildRenderItems(GeometryLibrary& geoLib);

	const std::vector<std::unique_ptr<RenderItem>>& GetRenderItems() const;

private:
	std::vector<ObjectInstance> _instances;
	std::vector<std::unique_ptr<RenderItem>> _renderItems;
};
