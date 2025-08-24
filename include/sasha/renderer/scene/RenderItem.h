#pragma once
#include "../geometry/Mesh.h"

class RenderItem
{
	friend class Scene;
	friend class D3DRenderer;

public:
	RenderItem() = default;
	~RenderItem() = default;

private:
	DirectX::XMFLOAT4X4 _world = d3dUtil::Identity4x4();
	DirectX::XMFLOAT4X4 _texTrans = d3dUtil::Identity4x4();

	UINT _cbObjIndex = -1;

	SubMeshID _submeshId = -1;
	MaterialID _materialId = -1;

	D3D12_PRIMITIVE_TOPOLOGY _primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
};
