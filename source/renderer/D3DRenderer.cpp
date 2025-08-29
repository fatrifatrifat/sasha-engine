#include "../../include/sasha/renderer/D3DRenderer.h"
#include <thread>
#include <numbers>
#include <ranges>
#include <cmath>

D3DRenderer::D3DRenderer(HWND wh, int w, int h)
	: _wndHandle(wh)
	, _appWidth(w)
	, _appHeight(h)
	, _camera(AspectRatio())
{
	_eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
}

D3DRenderer::~D3DRenderer()
{
	if (_device)
		_cmdQueue->Flush();
	CloseHandle(_eventHandle);
}

void D3DRenderer::d3dInit()
{
	_device = std::make_unique<Device>();

	_psoCache = std::make_unique<PSOCache>(_device->Get());

	// Creating the command queue which will contain the lists of command that was sent to the GPU
	// Creating a fence object so we can synchronize the CPU and GPU
	_cmdQueue = std::make_unique<CommandQueue>(_device->Get());

	// Creating the command list which will contain the list of commands that will the sent to the command queue
	// Creating the command allocator which will let you allocate memory of command lists
	_cmdList = std::make_unique<CommandList>(_device->Get());
	_cmdList->Get()->Close();

	_swapChain = std::make_unique<SwapChain>(_wndHandle, _device.get(), _cmdQueue.get(), _appHeight, _appWidth);

	// Creating descriptor heaps for the rtv and dsv which will contain rtvs and dsv descriptor that will be bind to the GPU pipeline
	// Getting the size of a render target view descriptor (rtv), depth stencil view (dsv) and, constant buffer view, shader resource view and UAV
	_rtvHeap = std::make_unique<DescriptorHeap>(_device->Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2u);

	_dsvHeap = std::make_unique<DescriptorHeap>(_device->Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	OnResize();

	_cmdList->Reset();

	BuildRootSignature();
	BuildInputLayout();

	BuildGeometry();
	BuildTextures();
	BuildMaterial();
	BuildLights();
	BuildScene();

	BuildFrameResources();
	if (_usingDescriptorTables)
	{
		BuildCbvDescriptorHeap();
		BuildConstantBuffers();
	}
	BuildPSO();

	_cmdQueue->ExecuteCmdList(_cmdList->Get());
	_cmdQueue->Flush();
}

void D3DRenderer::SetInputs(Keyboard* kb, Mouse* m) noexcept
{
	_kbd = kb;
	_mouse = m;
}

void D3DRenderer::RenderFrame(Timer& t)
{
	BeginFrame();
	DrawFrame();
	EndFrame();
}

void D3DRenderer::Update(Timer& t)
{
	UpdateCamera(t);

	_frameResourceIndex = (_frameResourceIndex + 1) % _frameResourceCount;
	_currFrameResource = _frameResources[_frameResourceIndex].get();

	if (_currFrameResource->_fence != 0 && _cmdQueue->GetFence()->GetCompletedValue() < _currFrameResource->_fence)
	{
		_cmdQueue->GetFence()->SetEventOnCompletion(_currFrameResource->_fence, _eventHandle);
		WaitForSingleObject(_eventHandle, INFINITE);
	}

	UpdateModels(t);
	
	UpdateObjCB(t);
	UpdatePassCB(t);
	UpdateMatCB(t);
}

float D3DRenderer::AspectRatio() const noexcept
{
	return static_cast<float>(_appWidth) / _appHeight;
}

void D3DRenderer::SetAppSize(int w, int h) noexcept
{
	_appHeight = h;
	_appWidth = w;	
}

void D3DRenderer::BuildInputLayout()
{
	// Getting and compiling the shaders
	_vertexShader = std::make_unique<Shader>("defaultVS.cso");
	_pixelShader = std::make_unique<Shader>("defaultPS.cso");
	_alphaTestShader = std::make_unique<Shader>("alphaTestedPS.cso");

	// Creating the input layout
	_inputLayoutDesc =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
}

void D3DRenderer::BuildGeometry()
{
	// Concatenating every vertices in the same array as well as for the indices for more efficient draw calls with a technique called instancing
	GeometryGenerator g;
	auto geoSphere = g.CreateGeosphere(1.f, 3);
	auto box = g.CreateBox(5.f, 5.f, 5.f, 0);
	auto cylinder = g.CreateCylinder(0.5f, 0.3f, 3.f, 10, 10);
	auto grid = g.CreateGrid(160.f, 160.f, 100, 100);
	auto skull = g.ReadFile("skull.txt");

	std::vector<Vertex> mirror =
	{
		// Mirror
		Vertex{{-9.f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}}, // 16
		Vertex{{-9.f, 18.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},
		Vertex{{+9.f, 18.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}},
		Vertex{{+9.f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}}
	};

	std::vector<uint32_t> indices =
	{
		0, 1, 2,
		0, 2, 3
	};

	GeometryGenerator::MeshData m;
	m.Vertices.resize(mirror.size());
	for (int i = 0; i < mirror.size(); i++)
	{
		m.Vertices[i].Position = mirror[i].Pos;
		m.Vertices[i].Normal = mirror[i].Normal;
		m.Vertices[i].TexC = mirror[i].Tex;
	}
	m.Indices32 = indices;

	_geoLib.AddGeometry("box", box);
	_geoLib.AddGeometry("sphere", geoSphere);
	_geoLib.AddGeometry("cylinder", cylinder);
	_geoLib.AddGeometry("grid", grid);
	_geoLib.AddGeometry("skull", skull);
	_geoLib.AddGeometry("mirror", m);

	// Once all are added:
	_geoLib.Upload(_device->Get(), _cmdList->Get());
}

void D3DRenderer::BuildMaterial()
{
	auto skullMat = std::make_unique<Material>();
	skullMat->name = "skullMat";
	skullMat->_matProperties._diffuseAlbedo = { 0.5f, 0.3f, 0.3f, 1.f };
	skullMat->_matProperties._fresnelR0 = { 0.6f, 0.6f, 0.6f };
	skullMat->_matProperties._roughness = 0.45f;

	auto boxMat = std::make_unique<Material>();
	boxMat->name = "boxMat";
	boxMat->_matProperties._diffuseAlbedo = { 1.f, 1.f, 1.f, 1.0f };
	boxMat->_matProperties._fresnelR0 = { 0.5f, 0.5f, 0.5f };
	boxMat->_matProperties._roughness = 0.25f;
	boxMat->_texture = &_geoLib.GetTexture("box");

	auto sphereMat = std::make_unique<Material>();
	sphereMat->name = "sphereMat";
	sphereMat->_matProperties._diffuseAlbedo = { 0.2f, 0.5f, 0.8f, 0.45f };
	sphereMat->_matProperties._fresnelR0 = { 0.6f, 0.6f, 0.9f };
	sphereMat->_matProperties._roughness = 0.2f;
	sphereMat->_texture = &_geoLib.GetTexture("sphere");

	auto cylinderMat = std::make_unique<Material>();
	cylinderMat->name = "cylinderMat";
	cylinderMat->_matProperties._diffuseAlbedo = { 0.5f, 0.5f, 0.5f, 1.0f };
	cylinderMat->_matProperties._fresnelR0 = { 0.8f, 0.8f, 0.8f };
	cylinderMat->_matProperties._roughness = 0.3f;
	cylinderMat->_texture = &_geoLib.GetTexture("cylinder");

	auto hillMat = std::make_unique<Material>();
	hillMat->name = "hillMat";
	hillMat->_matProperties._diffuseAlbedo = { 0.45f, 0.33f, 0.18f, 1.0f };
	hillMat->_matProperties._fresnelR0 = { 0.800f, 0.600f, 0.400f };
	hillMat->_matProperties._roughness = 0.55f;
	hillMat->_texture = &_geoLib.GetTexture("grid");

	auto lightSphereMat = std::make_unique<Material>();
	lightSphereMat->name = "lightSphereMat";
	lightSphereMat->_matProperties._diffuseAlbedo = { 1.0f, 1.0f, 1.0f, 0.8f };
	lightSphereMat->_matProperties._fresnelR0 = { 0.1f, 0.1f, 0.1f };
	lightSphereMat->_matProperties._roughness = 0.5f;
	lightSphereMat->_texture = &_geoLib.GetTexture("lightSphere");

	_geoLib.AddMaterial(boxMat->name, std::move(boxMat));
	_geoLib.AddMaterial(hillMat->name, std::move(hillMat));
	_geoLib.AddMaterial(cylinderMat->name, std::move(cylinderMat));
	_geoLib.AddMaterial(sphereMat->name, std::move(sphereMat));
	_geoLib.AddMaterial(lightSphereMat->name, std::move(lightSphereMat));
	_geoLib.AddMaterial(skullMat->name, std::move(skullMat));
}

void D3DRenderer::BuildLights()
{
	Light light;

	light.Strength = { 1.0f, 0.85f, 0.6f };
	light.FalloffStart = 1.0f;
	light.FalloffEnd = 12.f;
	light.Position = { 12.f * cosf(5.f*XM_PI/4.f), 5.f, 12.f * sinf(5.f * XM_PI / 4.f) };

	_scene.AddLight(light);

	light.Strength = { 1.0f, 0.85f, 0.6f };
	light.FalloffStart = 1.0f;
	light.FalloffEnd = 12.f;
	light.Position = { 12.f * cosf(3.f * XM_PI / 2.f), 5.f, 12.f * sinf(3.f * XM_PI / 2.f) };

	_scene.AddLight(light);

	light.Strength = { 1.0f, 0.85f, 0.6f };
	light.FalloffStart = 1.0f;
	light.FalloffEnd = 12.f;
	light.Position = { 12.f * cosf(7.f * XM_PI / 4.f), 5.f, 12.f * sinf(7.f * XM_PI / 4.f) };

	_scene.AddLight(light);
}

void D3DRenderer::BuildTextures()
{
	auto box = std::make_unique<Texture>(_device->Get(), *_cmdList, "box", "WireFence.dds");
	auto grid = std::make_unique<Texture>(_device->Get(), *_cmdList, "grid", "tile.dds");
	auto cylinder = std::make_unique<Texture>(_device->Get(), *_cmdList, "cylinder", "stone.dds");
	auto sphere = std::make_unique<Texture>(_device->Get(), *_cmdList, "sphere", "water1.dds");
	auto lightSphere = std::make_unique<Texture>(_device->Get(), *_cmdList, "lightSphere", "ice.dds");

	_srvHeap = std::make_unique<DescriptorHeap>(_device->Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 5u, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = box->_resource->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = box->_resource->GetDesc().MipLevels;
	_device->Get()->CreateShaderResourceView(box->_resource.Get(), &srvDesc, _srvHeap->GetCPUStart());

	srvDesc.Format = grid->_resource->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = grid->_resource->GetDesc().MipLevels;
	_device->Get()->CreateShaderResourceView(grid->_resource.Get(), &srvDesc, _srvHeap->GetCPUStart(1u));

	srvDesc.Format = cylinder->_resource->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = cylinder->_resource->GetDesc().MipLevels;
	_device->Get()->CreateShaderResourceView(cylinder->_resource.Get(), &srvDesc, _srvHeap->GetCPUStart(2u));

	srvDesc.Format = sphere->_resource->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = sphere->_resource->GetDesc().MipLevels;
	_device->Get()->CreateShaderResourceView(sphere->_resource.Get(), &srvDesc, _srvHeap->GetCPUStart(3u));

	srvDesc.Format = lightSphere->_resource->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = lightSphere->_resource->GetDesc().MipLevels;
	_device->Get()->CreateShaderResourceView(lightSphere->_resource.Get(), &srvDesc, _srvHeap->GetCPUStart(4u));

	_geoLib.AddTexture(box->_name, std::move(box));
	_geoLib.AddTexture(grid->_name, std::move(grid));
	_geoLib.AddTexture(cylinder->_name, std::move(cylinder));
	_geoLib.AddTexture(sphere->_name, std::move(sphere));
	_geoLib.AddTexture(lightSphere->_name, std::move(lightSphere));
}

void D3DRenderer::BuildScene()
{
	_scene.AddInstance("grid", "hillMat");
	_scene.AddInstance("mirror", "lightSphereMat", d3dUtil::GetTranslation(0.f, 0.f, 12.f), { ItemType::Mirror, ItemType::Transparent });
	for (float theta = 0; theta < 2.f * d3dUtil::PI; theta += (d3dUtil::PI / 18.f))
	{
		//_scene.AddInstance("cylinder", "cylinderMat", d3dUtil::GetTranslation(12.f * cosf(theta), 1.5f, 12.f * sinf(theta)));
		//_scene.AddInstance("sphere", "sphereMat", d3dUtil::GetTranslation(12.f * cosf(theta), 3.5f, 12.f * sinf(theta)), ItemType::Transparent);
	}
	//_scene.AddInstance("box", "boxMat", d3dUtil::GetTranslation(0.f, 2.6f, 0.f), ItemType::AlphaTested);
	//_scene.AddInstance("sphere", "lightSphereMat", d3dUtil::MatToFloat4x4(XMMatrixMultiply(XMMatrixScaling(2.f, 2.f, 2.f), XMMatrixTranslation(0.f, 2.1f, 0.f))), ItemType::Transparent);
	_scene.AddInstance("skull", "skullMat", d3dUtil::MatToFloat4x4(XMMatrixRotationY(XM_PIDIV2)));
	_scene.AddInstance("skull", "skullMat", d3dUtil::Identity4x4(), { ItemType::Reflected });

	_scene.BuildRenderItems(_geoLib);
}

void D3DRenderer::BuildFrameResources()
{
	// Build the Frame Resources
	for (int i = 0; i < _frameResourceCount; i++)
		_frameResources.push_back(std::make_unique<FrameResource>(
			_device->Get(),
			2u, // Pass buffer
			static_cast<UINT>(_scene.GetAllRenderItems().size()), // Obj buffer
			static_cast<UINT>(_geoLib.GetMaterialCount()) // Mat buffer
		));
}

void D3DRenderer::BuildCbvDescriptorHeap()
{
	// Creating the constant buffer descriptor heap
	// Making a heap capable of holding 3n + 3 descriptors
	// 3n so each object can have their own frame resource on each frame resource
	// + 3 so each frame resource has access to it's global constant buffer that's non unique to each object
	UINT descriptorHeapCount = static_cast<UINT>(((_scene.GetAllRenderItems().size() + 1 + _geoLib.GetMaterialCount()) * _frameResourceCount));
	// Getting the index at which the global constant buffers are, right after all the unique constant buffers
	_matCbvOffset = static_cast<UINT>(_frameResourceCount * _scene.GetAllRenderItems().size());
	_passCbvOffset = static_cast<UINT>(_matCbvOffset + _frameResourceCount * _geoLib.GetMaterialCount());

	_cbvHeap = std::make_unique<DescriptorHeap>(_device->Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, descriptorHeapCount, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
}

void D3DRenderer::BuildConstantBuffers()
{
	// Building the constant buffers at the place of each descriptor inside the heap created earlier
	// Getting the size of both cbs padded ceiled to a multiple of 256 for padding reasons on the shader and GPU side
	UINT cbSize = d3dUtil::CalcConstantBufferSize(sizeof(ConstantBuffer));
	UINT passSize = d3dUtil::CalcConstantBufferSize(sizeof(PassBuffer));
	UINT matSize = d3dUtil::CalcConstantBufferSize(sizeof(MaterialConstant));

	for (UINT i = 0; i < _frameResourceCount; i++)
	{
		// Get the constant buffer from the current frame resource
		auto objCB = _frameResources[i]->_cb->GetResource();
		for (UINT j = 0; j < _scene.GetAllRenderItems().size(); j++)
		{
			// For each object, get the virtual address of the constant buffer and increment to access the jth object's cb
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = objCB->GetGPUVirtualAddress();
			cbAddress += j * cbSize;
			
			// Get the index on the heap that contains the constant buffer for every object and get a handle to the right position of the constant buffer
			int heapIndex = (int)(i * _scene.GetAllRenderItems().size() + j);

			// Create constant buffer view
			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = cbSize;

			_device->Get()->CreateConstantBufferView(&cbvDesc, _cbvHeap->GetCPUStart(heapIndex));
		}

		auto matCB = _frameResources[i]->_mat->GetResource();
		for (UINT j = 0; j < _geoLib.GetMaterialCount(); j++)
		{
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = matCB->GetGPUVirtualAddress();
			cbAddress += j * matSize;

			int heapIndex = (int)(_matCbvOffset + i * _geoLib.GetMaterialCount() + j);

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = matSize;

			_device->Get()->CreateConstantBufferView(&cbvDesc, _cbvHeap->GetCPUStart(heapIndex));
		}
		// Do something similar for the global constant buffers that are shared between objects in the same frame resource
		auto passCB = _frameResources[i]->_pass->GetResource();
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress = passCB->GetGPUVirtualAddress();

		int heapIndex = _passCbvOffset + i;

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
		cbvDesc.BufferLocation = cbAddress;
		cbvDesc.SizeInBytes = passSize;

		_device->Get()->CreateConstantBufferView(&cbvDesc, _cbvHeap->GetCPUStart(heapIndex));
	}
}

void D3DRenderer::BuildRootSignature()
{
	// This describes a slot for the constant buffers for the shaders
	RootSignature rootBuilder;
	if (_usingDescriptorTables)
	{
		rootBuilder.AddDescriptorTable(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1u, 0u);
		rootBuilder.AddDescriptorTable(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1u, 1u);
		rootBuilder.AddDescriptorTable(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1u, 2u);
	}
	else
	{
		rootBuilder.AddDescriptorTable(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1u, 0u);
		rootBuilder.AddCBV(0);
		rootBuilder.AddCBV(1);
		rootBuilder.AddCBV(2);
	}

	_rootSignature = rootBuilder.Build(_device->Get(), Texture::GetStaticSampler());
}

void D3DRenderer::BuildPSO()
{
	// Building the Pipeline State Object to prepare the GPU to get parts of the pipeline in certain ways
	auto vs = _vertexShader->GetByteCode();
	auto ps = _pixelShader->GetByteCode();
	auto alphaTest = _alphaTestShader->GetByteCode();

	// opaque
	GraphicsPipelineRecipe recipe = GraphicsPipelineRecipe::MakeDefault(_inputLayoutDesc, vs, ps);
	_opaque = recipe;
	_psoCache->GetOrCreate(_rootSignature.Get(), recipe, _rtDesc);

	// Alphatested
	recipe._rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	recipe._ps = alphaTest;
	_alphaTested = recipe;
	_psoCache->GetOrCreate(_rootSignature.Get(), recipe, _rtDesc);

	// Wireframe
	recipe._rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	recipe._rasterizerDesc.FillMode = D3D12_FILL_MODE_WIREFRAME;
	recipe._ps = ps;
	_wireframe = recipe;
	_psoCache->GetOrCreate(_rootSignature.Get(), recipe, _rtDesc);

	// Transparency
	recipe._rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	D3D12_RENDER_TARGET_BLEND_DESC rtBlendDesc{};
	rtBlendDesc.BlendEnable = true;
	rtBlendDesc.LogicOpEnable = false;
	rtBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	rtBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	rtBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	rtBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	rtBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	rtBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	rtBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	rtBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	recipe._blendDesc.RenderTarget[0] = rtBlendDesc;
	recipe._depthStentilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	_transparent = recipe;
	_psoCache->GetOrCreate(_rootSignature.Get(), recipe, _rtDesc);

	// Mirror
	recipe = GraphicsPipelineRecipe::MakeDefault(_inputLayoutDesc, vs, ps);
	recipe._blendDesc.RenderTarget[0].RenderTargetWriteMask = 0;
	
	recipe._depthStentilDesc.DepthEnable = true;
	recipe._depthStentilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	recipe._depthStentilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	
	recipe._depthStentilDesc.StencilEnable = true;
	recipe._depthStentilDesc.StencilReadMask = 0xff;
	recipe._depthStentilDesc.StencilWriteMask = 0xff;
	
	recipe._depthStentilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	recipe._depthStentilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	recipe._depthStentilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
	recipe._depthStentilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	recipe._depthStentilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	recipe._depthStentilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	recipe._depthStentilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
	recipe._depthStentilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	_mirror = recipe;
	_psoCache->GetOrCreate(_rootSignature.Get(), recipe, _rtDesc);

	// Reflected
	recipe = GraphicsPipelineRecipe::MakeDefault(_inputLayoutDesc, vs, ps);
	recipe._depthStentilDesc.DepthEnable = true;
	recipe._depthStentilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	recipe._depthStentilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	
	recipe._depthStentilDesc.StencilEnable = true;
	recipe._depthStentilDesc.StencilReadMask = 0xff;
	recipe._depthStentilDesc.StencilWriteMask = 0xff;

	recipe._depthStentilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	recipe._depthStentilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	recipe._depthStentilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	recipe._depthStentilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

	recipe._depthStentilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	recipe._depthStentilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	recipe._depthStentilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	recipe._depthStentilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

	recipe._rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	recipe._rasterizerDesc.FrontCounterClockwise = true;
	_reflected = recipe;
	_psoCache->GetOrCreate(_rootSignature.Get(), recipe, _rtDesc);
}

void D3DRenderer::OnResize()
{
	// Called whenever the size of the app is changed
	// Also creates the render target view as well as the depth stencil view
	_cmdQueue->Flush();
	_cmdList->Reset();

	_swapChain->OnResize(_device.get(), _cmdList.get(), *_rtvHeap.get(), *_dsvHeap.get());

	_rtDesc._numRenderTargets = 1u;
	for (UINT i = 0; i < _rtDesc._numRenderTargets; i++)
		_rtDesc._rtvFormats[i] = _swapChain->GetRtFormat();
	_rtDesc._dsvFormat = _swapChain->GetDsvFormat();
	_rtDesc._sampleDesc = { 1,0 };

	_camera.OnResize(_appWidth, _appHeight);

	_cmdQueue->ExecuteCmdList(_cmdList->Get());
	_cmdQueue->Flush();
}

void D3DRenderer::BeginFrame()
{
	auto& _currCmdAlloc = _currFrameResource->_cmdAlloc;

	ThrowIfFailed(_currFrameResource->_cmdAlloc->Reset());

	GraphicsPipelineRecipe recipe = _isWireFrame ? _wireframe : _opaque;
	auto* pso = _psoCache->GetOrCreate(_rootSignature.Get(), recipe, _rtDesc);

	_cmdList->Reset(_currCmdAlloc.Get(), pso);
	
	_cmdList->ChangeResourceState(_swapChain->GetCurrBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
}

void D3DRenderer::DrawFrame()
{
	_cmdList->Get()->RSSetViewports(1, _swapChain->GetViewport());
	_cmdList->Get()->RSSetScissorRects(1, _swapChain->GetRect());

	_cmdList->Get()->ClearRenderTargetView(_swapChain->GetCurrBackBufferView(*_rtvHeap.get()), Colors::SteelBlue, 0, nullptr);
	_cmdList->Get()->ClearDepthStencilView(_swapChain->GetDSView(*_dsvHeap.get()), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.f, 0, 0, nullptr);

	auto currBackBufferView = _swapChain->GetCurrBackBufferView(*_rtvHeap.get());
	auto dsv = _swapChain->GetDSView(*_dsvHeap.get());
	_cmdList->Get()->OMSetRenderTargets(1, &currBackBufferView, true, &dsv);

	if (_usingDescriptorTables)
	{
		ID3D12DescriptorHeap* descriptorHeaps[] = { _cbvHeap->Get() };
		_cmdList->Get()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	}

	_cmdList->Get()->SetGraphicsRootSignature(_rootSignature.Get());

	if (_usingDescriptorTables)
	{
		int passCbvIndex = _passCbvOffset + _frameResourceIndex;
		_cmdList->Get()->SetGraphicsRootDescriptorTable(2, _cbvHeap->GetGPUStart(passCbvIndex));
	}
	else
	{
		ID3D12DescriptorHeap* descriptorsHeap[] = { _srvHeap->Get() };
		_cmdList->Get()->SetDescriptorHeaps(_countof(descriptorsHeap), descriptorsHeap);

		auto passAddress = _currFrameResource->_pass->GetResource()->GetGPUVirtualAddress();
		_cmdList->Get()->SetGraphicsRootConstantBufferView(3, passAddress);
	}
	
	DrawRenderItems(_scene.GetItems(ItemType::Opaque), ItemType::Opaque);
	DrawRenderItems(_scene.GetItems(ItemType::AlphaTested), ItemType::AlphaTested);
	DrawRenderItems(_scene.GetItems(ItemType::Transparent), ItemType::Transparent);
}

void D3DRenderer::DrawRenderItems(const std::vector<RenderItem*>& items, ItemType type)
{
	if (!_isWireFrame)
	{
		switch (type)
		{
		case ItemType::Opaque:
			_cmdList->Get()->SetPipelineState(_psoCache->GetOrCreate(_rootSignature.Get(), _opaque, _rtDesc));
			break;
		case ItemType::Transparent:
			_cmdList->Get()->SetPipelineState(_psoCache->GetOrCreate(_rootSignature.Get(), _transparent, _rtDesc));
			break;
		case ItemType::AlphaTested:
			_cmdList->Get()->SetPipelineState(_psoCache->GetOrCreate(_rootSignature.Get(), _alphaTested, _rtDesc));
			break;
		}
	}
	else
	{
		_cmdList->Get()->SetPipelineState(_psoCache->GetOrCreate(_rootSignature.Get(), _wireframe, _rtDesc));
	}

	auto objCBSize = d3dUtil::CalcConstantBufferSize(sizeof(ConstantBuffer));
	auto matCBSize = d3dUtil::CalcConstantBufferSize(sizeof(MaterialConstant));

	for (const auto& ri : items)
	{
		const auto& vbv = _geoLib.GetMesh().VertexBufferView();
		const auto& ibv = _geoLib.GetMesh().IndexBufferView();

		const auto& mat = _geoLib.GetMaterial(ri->_materialId);
		const auto& submesh = _geoLib.GetSubmesh(ri->_submeshId);

		if (mat.name == "boxMat" && !_isWireFrame)
		{
			auto* pso = _psoCache->GetOrCreate(_rootSignature.Get(), _alphaTested, _rtDesc);
			_cmdList->Get()->SetPipelineState(pso);
		}

		_cmdList->Get()->IASetVertexBuffers(0, 1, &vbv);
		_cmdList->Get()->IASetIndexBuffer(&ibv);
		_cmdList->Get()->IASetPrimitiveTopology(ri->_primitiveType);

		if (_usingDescriptorTables)
		{
			UINT cbvIndex = _frameResourceIndex * static_cast<UINT>(_scene.GetAllRenderItems().size()) + ri->_cbObjIndex;
			UINT matIndex = _frameResourceIndex * static_cast<UINT>(_geoLib.GetMaterialCount()) + _matCbvOffset + mat._matCBIndex;

			_cmdList->Get()->SetGraphicsRootDescriptorTable(0, _cbvHeap->GetGPUStart(cbvIndex));
			_cmdList->Get()->SetGraphicsRootDescriptorTable(1, _cbvHeap->GetGPUStart(matIndex));
		}
		else
		{
			auto cbvAddress = _currFrameResource->_cb->GetResource()->GetGPUVirtualAddress() + ri->_cbObjIndex * objCBSize;
			auto matAddress = _currFrameResource->_mat->GetResource()->GetGPUVirtualAddress() + mat._matCBIndex * matCBSize;
			auto texAddress = _srvHeap->GetGPUStart(mat._diffuseSrvHeapIndex);

			if(mat._texture)
				_cmdList->Get()->SetGraphicsRootDescriptorTable(0, texAddress);
			_cmdList->Get()->SetGraphicsRootConstantBufferView(1, cbvAddress);
			_cmdList->Get()->SetGraphicsRootConstantBufferView(2, matAddress);
		}

		_cmdList->Get()->DrawIndexedInstanced(submesh._indexCount, 1u, submesh._startIndexLocation, submesh._baseVertexLocation, 0u);
	}
}

void D3DRenderer::EndFrame()
{
	_cmdList->ChangeResourceState(_swapChain->GetCurrBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

	_cmdQueue->ExecuteCmdList(_cmdList->Get());
	_swapChain->Present();

	_currFrameResource->_fence = ++_cmdQueue->GetCurrFence();

	_cmdQueue->Signal();
}

void D3DRenderer::UpdateCamera(const Timer& t)
{
	const float dt = t.DeltaTime();

	// Camera Movement
	if (_kbd->IsKeyPressed('W'))
		_camera.Walk(dt);
	else if (_kbd->IsKeyPressed('S'))
		_camera.Walk(-dt);
	if (_kbd->IsKeyPressed('D'))
		_camera.Strafe(dt);
	else if (_kbd->IsKeyPressed('A'))
		_camera.Strafe(-dt);
	if (_kbd->IsKeyPressed(VK_SPACE))
		_camera.Rise(dt);
	else if (_kbd->IsKeyPressed(VK_CONTROL))
		_camera.Rise(-dt);

	if (_mouse->OnRButtonDown())
		_camera.AddYawPitch(static_cast<float>(_mouse->GetDeltaX()), static_cast<float>(_mouse->GetDeltaY()));

	// WireFrame Control
	if (_kbd->IsKeyPressed(VK_F2) && _kbd->WasKeyPressedThisFrame(VK_F2))
		_isWireFrame = !_isWireFrame;

	// Light Controll
	if (_kbd->IsKeyPressed(VK_UP))
		_lightPhi -= _sunSpeed * dt;
	else if (_kbd->IsKeyPressed(VK_DOWN))
		_lightPhi += _sunSpeed * dt;
	if (_kbd->IsKeyPressed(VK_LEFT))
		_lightTheta -= _sunSpeed * dt;
	else if (_kbd->IsKeyPressed(VK_RIGHT))
		_lightTheta += _sunSpeed * dt;

	_lightPhi = std::clamp(_lightPhi, 0.1f, XM_PIDIV2);
}

void D3DRenderer::UpdateModels(const Timer& t)
{
}

void D3DRenderer::UpdateObjCB(const Timer& t)
{
	auto currObjCB = _currFrameResource->_cb.get();
	for (auto& e : _scene.GetAllRenderItems())
	{
		XMMATRIX world = XMLoadFloat4x4(&e->_world);
		ConstantBuffer cb;
		XMStoreFloat4x4(&cb.world, XMMatrixTranspose(world));
		auto name = _geoLib.GetMaterial(e->_materialId).name;
		//if(name == "sphereMat" || name == "lightSphereMat")
		//	XMStoreFloat4x4(&cb.texTrans, XMMatrixTranspose(XMMatrixMultiply(XMMatrixIdentity(), XMMatrixRotationZ(t.TotalTime()))));
		if(name == "hillMat")
			XMStoreFloat4x4(&cb.texTrans, XMMatrixTranspose(XMMatrixScaling(25, 25, 25)));
		currObjCB->CopyData(e->_cbObjIndex, cb);
	}
}

void D3DRenderer::UpdatePassCB(const Timer& t)
{
	auto pos = _camera.GetPositionF();
	auto view = _camera.GetView();
	auto proj = _camera.GetProj();
	auto viewProj = _camera.GetViewProj();

	auto viewDet = XMMatrixDeterminant(view);
	auto projDet = XMMatrixDeterminant(proj);
	auto viewprojDet = XMMatrixDeterminant(viewProj);
	XMMATRIX invView = XMMatrixInverse(&viewDet, view);
	XMMATRIX invProj = XMMatrixInverse(&projDet, proj);
	XMMATRIX invViewProj = XMMatrixInverse(&viewprojDet, viewProj);

	XMStoreFloat4x4(&_mainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&_mainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&_mainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&_mainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&_mainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&_mainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	_mainPassCB.EyePosW = pos;
	_mainPassCB.RenderTargetSize = XMFLOAT2((float)_appWidth, (float)_appHeight);
	_mainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / _appWidth, 1.0f / _appHeight);
	_mainPassCB.NearZ = _camera.GetNearZ();
	_mainPassCB.FarZ = _camera.GetFarZ();
	_mainPassCB.TotalTime = t.TotalTime();
	_mainPassCB.DeltaTime = t.DeltaTime();
	_mainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	_mainPassCB.FogColor = { 0.2f, 0.2f, 0.2f, 1.f };
	_mainPassCB.FogStart = 0.f;
	_mainPassCB.FogRange = 50.f;

	// Direction Light
	/*XMVECTOR lightDir = -DirectX::XMVectorSet(
		1.f * sinf(_lightPhi) * cosf(_lightTheta),
		1.f * cosf(_lightPhi),
		1.f * sinf(_lightPhi) * sinf(_lightTheta),
		1.0f);
	XMStoreFloat3(&_mainPassCB.Lights[0].Direction, lightDir);
	_mainPassCB.Lights[0].Strength = { 1.0f, 0.9f, 0.7f};*/

	// Point Light
	/*_mainPassCB.Lights[0].Strength = { 1.0f, 0.85f, 0.6f };
	_mainPassCB.Lights[0].FalloffStart = 1.0f;
	_mainPassCB.Lights[0].FalloffEnd = 1000.0f;
	_mainPassCB.Lights[0].Position = { 0.0f, 10.0f, 0.0f };*/

	// Spot Light
	/*int i = 0;
	for (float theta = 0; theta < 2.f * d3dUtil::PI; theta += (d3dUtil::PI / 5.f), ++i)
	{
		_mainPassCB.Lights[i].Strength = { 1.0f, 0.95f, 0.8f };
		_mainPassCB.Lights[i].FalloffStart = 2.0f;
		_mainPassCB.Lights[i].FalloffEnd = 10000.0f;
		_mainPassCB.Lights[i].Position = { 12.f * cosf(theta), 5.f, 12.f * sinf(theta) };
		_mainPassCB.Lights[i].Direction = { 0.0f, -1.f, 0.f };
		_mainPassCB.Lights[i].SpotPower = 32.0f;
	}*/

	int i = 0;
	for (; i < _scene.GetLights().size(); i++)
		_mainPassCB.Lights[i] = _scene.GetLights().at(i);

	XMVECTOR lightDir = -DirectX::XMVectorSet(
		1.f * sinf(_lightPhi) * cosf(_lightTheta),
		1.f * cosf(_lightPhi),
		1.f * sinf(_lightPhi) * sinf(_lightTheta),
		1.0f);
	XMStoreFloat3(&_mainPassCB.Lights[i].Direction, lightDir);
	_mainPassCB.Lights[i].Strength = { 1.f, 1.f, 1.f };
	_mainPassCB.Lights[i].FalloffStart = 2.0f;
	_mainPassCB.Lights[i].FalloffEnd = 1000.0f;
	_mainPassCB.Lights[i].Position = { 0.f, 15.f, 0.f };
	_mainPassCB.Lights[i].SpotPower = 8.0f;

	_currFrameResource->_pass->CopyData(0, _mainPassCB);

	_reflectedPassCB = _mainPassCB;
	XMVECTOR mirrorPlane = XMVectorSet(0.f, 0.f, 1.f, 0.f);
	XMMATRIX R = XMMatrixReflect(mirrorPlane);

	for (i = 0; i < _scene.GetLights(ItemType::Reflected).size(); i++)
	{
		XMVECTOR lightDir = XMLoadFloat3(&_mainPassCB.Lights[i].Direction);
		XMVECTOR reflectedLightDir = XMVector3TransformNormal(lightDir, R);
		XMStoreFloat3(&_reflectedPassCB.Lights[i].Direction, reflectedLightDir);
	}
	
	_currFrameResource->_pass->CopyData(1, _reflectedPassCB);
}

void D3DRenderer::UpdateMatCB(const Timer& t)
{
	auto currMatCB = _currFrameResource->_mat.get();
	for (auto& e : _scene.GetAllRenderItems())
	{
		auto& mat = _geoLib.GetMaterial(e->_materialId);
		XMMATRIX transform = XMLoadFloat4x4(&mat._matProperties._transform);
		MaterialConstant cb;
		cb._diffuseAlbedo = mat._matProperties._diffuseAlbedo;
		cb._fresnelR0 = mat._matProperties._fresnelR0;
		cb._roughness = mat._matProperties._roughness;
		XMStoreFloat4x4(&cb._transform, XMMatrixTranspose(transform));

		currMatCB->CopyData(mat._matCBIndex, cb);
	}
}
