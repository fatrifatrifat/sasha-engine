#pragma once
#include "../../utility/d3dUtil.h"

struct MaterialConstant
{
	DirectX::XMFLOAT4 _diffuseAlbedo = { 1.f, 1.f, 1.f, 1.f };
	DirectX::XMFLOAT3 _fresnelR0 = { 0.1f, 0.1f, 0.1f };
	float _roughness = 0.25f;
	DirectX::XMFLOAT4X4 _transform = d3dUtil::Identity4x4();
};

struct Material
{
	std::string name = "";
	int _matCBIndex = -1;
	int _diffuseSrvHeapIndex = -1;
	int _numDirtyFlags = 3;
	MaterialConstant _matProperties;
};
