#include "../../include/sasha/renderer/D3DRenderer.h"
#include <filesystem>
#include <thread>
#include <numbers>
#include <ranges>
#include <cmath>
#include <DirectXTex.h>

D3DRenderer::D3DRenderer(HWND wh, int w, int h)
	: _wndHandle(wh)
	, _appWidth(w)
	, _appHeight(h)
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
	CreateDevice();
	CreateCmdObjs();
	CreateSwapChain();
	CreateDescriptorHeaps();
	OnResize();

	_cmdList->Reset();

	BuildRootSignature();
	BuildInputLayout();
	BuildGeometry();
	BuildMaterial();
	BuildLights();
	BuildTextures();
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

void D3DRenderer::CreateDevice()
{
	// Enabling the debugging layer so we can more easily debug if something goes wrong
	#if defined (DEBUG) || defined (_DEBUG)
	{
		ComPtr<ID3D12Debug> debugLayer;
		ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer)));
		debugLayer->EnableDebugLayer();
	}
	#endif

	// Creating the factory for DXGI objects like SwapChain
	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&_factory)));

	// Creating the device, basically the interface that let's you create of interfaces specific to d3d
	ThrowIfFailed(D3D12CreateDevice(
		nullptr,
		D3D_FEATURE_LEVEL_11_0,
		IID_PPV_ARGS(&_device)
	));

	// Checking the MSAA4x quality level
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS qualityLevels{};
	qualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	qualityLevels.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	qualityLevels.NumQualityLevels = 0;
	qualityLevels.SampleCount = 4;
	ThrowIfFailed(_device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		&qualityLevels,
		sizeof(qualityLevels)
	));
	assert(qualityLevels.NumQualityLevels > 0);
}

void D3DRenderer::CreateCmdObjs()
{
	// Creating the command queue which will contain the lists of command that was sent to the GPU
	// Creating a fence object so we can synchronize the CPU and GPU
	_cmdQueue = std::make_unique<CommandQueue>(_device.Get());

	// Creating the command list which will contain the list of commands that will the sent to the command queue
	// Creating the command allocator which will let you allocate memory of command lists
	_cmdList = std::make_unique<CommandList>(_device.Get());
	_cmdList->Get()->Close();

}

void D3DRenderer::CreateSwapChain()
{
	// Creating the SwapChain object to be able to access and swap between render targets that we will show as frames
	DXGI_SWAP_CHAIN_DESC scDesc;
	scDesc.BufferDesc.Height = _appHeight;
	scDesc.BufferDesc.Width = _appWidth;
	scDesc.BufferDesc.RefreshRate.Numerator = 60;
	scDesc.BufferDesc.RefreshRate.Denominator = 1;
	scDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	scDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	scDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

	scDesc.SampleDesc.Count = 1;
	scDesc.SampleDesc.Quality = 0;

	scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scDesc.BufferCount = bufferCount;
	scDesc.OutputWindow = _wndHandle;
	scDesc.Windowed = true;
	scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	scDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	ThrowIfFailed(_factory->CreateSwapChain(
		_cmdQueue->Get(),
		&scDesc,
		_swapChain.GetAddressOf()
	));
}

void D3DRenderer::CreateDescriptorHeaps()
{
	// Creating descriptor heaps for the rtv and dsv which will contain rtvs and dsv descriptor that will be bind to the GPU pipeline
	// Getting the size of a render target view descriptor (rtv), depth stencil view (dsv) and, constant buffer view, shader resource view and UAV
	_rtvHeap = std::make_unique<DescriptorHeap>(_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2u);

	_dsvHeap = std::make_unique<DescriptorHeap>(_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
}

void D3DRenderer::CreateRTV()
{
	// Creating a rtv with every buffer held by the SwapChain
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvBegHandle = _rtvHeap->GetCPUStart();
	for (int i = 0; i < bufferCount; i++)
	{
		ThrowIfFailed(_swapChain->GetBuffer(i, IID_PPV_ARGS(&_swapChainBuffer[i])));
		_device->CreateRenderTargetView(_swapChainBuffer[i].Get(), nullptr, rtvBegHandle);
		rtvBegHandle.Offset(_rtvHeap->GetSize());
	}
}

void D3DRenderer::CreateDSV()
{
	// Creating the depth stencil view for the depth stencil buffer
	const CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
	const CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC
		::Tex2D(
			DXGI_FORMAT_D32_FLOAT,
			_appWidth, _appHeight,
			1, 0, 1, 0,
			D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
		);

	D3D12_CLEAR_VALUE clearVal{};
	clearVal.DepthStencil = { 1.f, 0 };
	clearVal.Format = DXGI_FORMAT_D32_FLOAT;

	ThrowIfFailed(_device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&clearVal,
		IID_PPV_ARGS(_depthStencilBuffer.GetAddressOf())
	));

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.Texture2D.MipSlice = 0;
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	_device->CreateDepthStencilView(_depthStencilBuffer.Get(), &dsvDesc, GetDSView());

	_cmdList->ChangeResourceState(_depthStencilBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);

	// Initialize the viewport and scissors rectangle
	_vp.MaxDepth = 1.f;
	_vp.MinDepth = 0.f;
	_vp.Height = static_cast<float>(_appHeight);
	_vp.Width = static_cast<float>(_appWidth);
	_vp.TopLeftX = 0.f;
	_vp.TopLeftY = 0.f;

	_scissor = { 0, 0, _appWidth, _appHeight };

	_cmdQueue->ExecuteCmdList(_cmdList->Get());
	_cmdQueue->Flush();
}

void D3DRenderer::BuildInputLayout()
{
	// Getting and compiling the shaders
	std::filesystem::path shaderPath1 = std::filesystem::current_path() / ".." / "shaders" / "defaultVS.cso";
	std::filesystem::path shaderPath2 = std::filesystem::current_path() / ".." / "shaders" / "defaultPS.cso";
	ThrowIfFailed(D3DReadFileToBlob(shaderPath1.c_str(), &_vertexShader));
	ThrowIfFailed(D3DReadFileToBlob(shaderPath2.c_str(), &_pixelShader));

	// Creating the input layout
	_inputLayoutDesc =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
}

void D3DRenderer::BuildGeometry()
{
	// Concatenating every vertices in the same array as well as for the indices for more efficient draw calls with a technique called instancing
	GeometryGenerator g;
	auto geoSphere = g.CreateGeosphere(1.f, 3);
	auto box = g.CreateBox(5.f, 2.f, 5.f, 0);
	auto cylinder = g.CreateCylinder(0.5f, 0.3f, 3.f, 10, 10);
	auto grid = g.CreateGrid(160.f, 160.f, 100, 100);
	std::filesystem::path skullPath = std::filesystem::current_path() / ".." / "assets" / "models" / "skull.txt";
	auto skull = g.ReadFile(skullPath.string());

	_geoLib.AddGeometry("box", box);
	_geoLib.AddGeometry("sphere", geoSphere);
	_geoLib.AddGeometry("cylinder", cylinder);
	_geoLib.AddGeometry("grid", grid);
	_geoLib.AddGeometry("skull", skull);

	// Once all are added:
	_geoLib.Upload(_device.Get(), _cmdList->Get());
}

void D3DRenderer::BuildMaterial()
{
	auto skullMat = std::make_unique<Material>();
	skullMat->name = "skullMat";
	skullMat->_matCBIndex = 0;
	skullMat->_diffuseSrvHeapIndex = 0;
	skullMat->_matProperties._diffuseAlbedo = { 0.12f, 0.10f, 0.05f, 1.0f };
	skullMat->_matProperties._fresnelR0 = { 1.000f, 0.766f, 0.336f };
	skullMat->_matProperties._roughness = 0.15f;

	auto boxMat = std::make_unique<Material>();
	boxMat->name = "boxMat";
	boxMat->_matCBIndex = 0;
	boxMat->_diffuseSrvHeapIndex = 0;
	boxMat->_matProperties._diffuseAlbedo = { 0.8f, 0.2f, 0.2f, 1.0f };
	boxMat->_matProperties._fresnelR0 = { 0.9f, 0.7f, 0.5f };
	boxMat->_matProperties._roughness = 0.25f;

	auto sphereMat = std::make_unique<Material>();
	sphereMat->name = "sphereMat";
	sphereMat->_matCBIndex = 2;
	sphereMat->_diffuseSrvHeapIndex = 2;
	sphereMat->_matProperties._diffuseAlbedo = { 0.2f, 0.5f, 0.8f, 1.0f };
	sphereMat->_matProperties._fresnelR0 = { 0.6f, 0.6f, 0.9f };
	sphereMat->_matProperties._roughness = 0.2f;

	auto cylinderMat = std::make_unique<Material>();
	cylinderMat->name = "cylinderMat";
	cylinderMat->_matCBIndex = 3;
	cylinderMat->_diffuseSrvHeapIndex = 3;
	cylinderMat->_matProperties._diffuseAlbedo = { 0.5f, 0.5f, 0.5f, 1.0f };
	cylinderMat->_matProperties._fresnelR0 = { 0.8f, 0.8f, 0.8f };
	cylinderMat->_matProperties._roughness = 0.3f;

	auto gridMat = std::make_unique<Material>();
	gridMat->name = "gridMat";
	gridMat->_matCBIndex = 4;
	gridMat->_diffuseSrvHeapIndex = 4;
	gridMat->_matProperties._diffuseAlbedo = { 0.1f, 0.1f, 0.1f, 1.0f };
	gridMat->_matProperties._fresnelR0 = { 0.5f, 0.5f, 0.5f };
	gridMat->_matProperties._roughness = 0.7f;

	auto hillMat = std::make_unique<Material>();
	hillMat->name = "hillMat";
	hillMat->_matCBIndex = 5;
	hillMat->_diffuseSrvHeapIndex = 5;
	hillMat->_matProperties._diffuseAlbedo = { 0.45f, 0.33f, 0.18f, 1.0f };
	hillMat->_matProperties._fresnelR0 = { 0.800f, 0.600f, 0.400f };
	hillMat->_matProperties._roughness = 0.55f;

	//_geoLib.AddMaterial(skullMat->name, std::move(skullMat));
	_geoLib.AddMaterial(boxMat->name, std::move(boxMat));
	//_geoLib.AddMaterial(sphereMat->name, std::move(sphereMat));
	//_geoLib.AddMaterial(cylinderMat->name, std::move(cylinderMat));
	//_geoLib.AddMaterial(gridMat->name, std::move(gridMat));
	//_geoLib.AddMaterial(hillMat->name, std::move(hillMat));
}

void D3DRenderer::BuildLights()
{
	for (float theta = 0; theta < 2.f * d3dUtil::PI; theta += (d3dUtil::PI / 5.f))
	{
		Light light;

		light.Strength = { 1.0f, 0.95f, 0.8f };
		light.FalloffStart = 2.0f;
		light.FalloffEnd = 10000.0f;
		light.Position = { 12.f * cosf(theta), 5.f, 12.f * sinf(theta) };
		light.Direction = { 0.0f, -1.f, 0.f };
		light.SpotPower = 32.0f;
		_scene.AddLight(light);
	}
}

void D3DRenderer::BuildTextures()
{
	auto woodCrateTex = std::make_unique<Texture>();
	std::filesystem::path texPath = std::filesystem::current_path() / ".." / "assets" / "textures" / "WoodCrate01.dds";
	woodCrateTex->_name = "woodCrateTex";
	woodCrateTex->_filename = texPath.wstring();
	ThrowIfFailed(CreateDDSTextureFromFile12(_device.Get(), _cmdList->Get(), woodCrateTex->_filename.c_str(), woodCrateTex->_resource, woodCrateTex->_uploadBuffer));

	_srvHeap = std::make_unique<DescriptorHeap>(_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1u, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = woodCrateTex->_resource->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = woodCrateTex->_resource->GetDesc().MipLevels;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.f;

	_device->CreateShaderResourceView(woodCrateTex->_resource.Get(), &srvDesc, _srvHeap->GetCPUStart());
}

void D3DRenderer::BuildScene()
{
	/*_scene.AddInstance("grid", "hillMat");
	for (float theta = 0, i = 0; theta < 2.f * d3dUtil::PI; theta += (d3dUtil::PI / 25.f), i++)
	{
		_scene.AddInstance("cylinder", "cylinderMat", d3dUtil::GetTranslation(12.f * cosf(theta), 1.5f, 12.f * sinf(theta)));
		_scene.AddInstance("sphere", "sphereMat", d3dUtil::GetTranslation(12.f * cosf(theta), 3.5f, 12.f * sinf(theta)));
	}*/
	_scene.AddInstance("box", "boxMat");
	//_scene.AddInstance("skull", "skullMat", d3dUtil::GetTranslation(0.f, 2.f, 0.f));

	_scene.BuildRenderItems(_geoLib);
}

void D3DRenderer::BuildFrameResources()
{
	// Build the Frame Resources
	for (int i = 0; i < _frameResourceCount; i++)
		_frameResources.push_back(std::make_unique<FrameResource>(_device.Get(), 1u, static_cast<UINT>(_scene.GetRenderItems().size()), static_cast<UINT>(_geoLib.GetMaterialCount())));
}

void D3DRenderer::BuildCbvDescriptorHeap()
{
	// Creating the constant buffer descriptor heap
	// Making a heap capable of holding 3n + 3 descriptors
	// 3n so each object can have their own frame resource on each frame resource
	// + 3 so each frame resource has access to it's global constant buffer that's non unique to each object
	UINT descriptorHeapCount = static_cast<UINT>(((_scene.GetRenderItems().size() + 1 + _geoLib.GetMaterialCount()) * _frameResourceCount));
	// Getting the index at which the global constant buffers are, right after all the unique constant buffers
	_matCbvOffset = static_cast<UINT>(_frameResourceCount * _scene.GetRenderItems().size());
	_passCbvOffset = static_cast<UINT>(_matCbvOffset + _frameResourceCount * _geoLib.GetMaterialCount());

	_cbvHeap = std::make_unique<DescriptorHeap>(_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, descriptorHeapCount, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
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
		for (UINT j = 0; j < _scene.GetRenderItems().size(); j++)
		{
			// For each object, get the virtual address of the constant buffer and increment to access the jth object's cb
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = objCB->GetGPUVirtualAddress();
			cbAddress += j * cbSize;
			
			// Get the index on the heap that contains the constant buffer for every object and get a handle to the right position of the constant buffer
			int heapIndex = (int)(i * _scene.GetRenderItems().size() + j);

			// Create constant buffer view
			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = cbSize;

			_device->CreateConstantBufferView(&cbvDesc, _cbvHeap->GetCPUStart(heapIndex));
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

			_device->CreateConstantBufferView(&cbvDesc, _cbvHeap->GetCPUStart(heapIndex));
		}
		// Do something similar for the global constant buffers that are shared between objects in the same frame resource
		auto passCB = _frameResources[i]->_pass->GetResource();
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress = passCB->GetGPUVirtualAddress();

		int heapIndex = _passCbvOffset + i;

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
		cbvDesc.BufferLocation = cbAddress;
		cbvDesc.SizeInBytes = passSize;

		_device->CreateConstantBufferView(&cbvDesc, _cbvHeap->GetCPUStart(heapIndex));
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

	_rootSignature = rootBuilder.Build(_device.Get(), GetStaticSampler());
}

void D3DRenderer::BuildPSO()
{
	// Building the Pipeline State Object to prepare the GPU to get parts of the pipeline in certain ways
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	psoDesc.InputLayout = { 
		_inputLayoutDesc.data(),
		static_cast<UINT>(_inputLayoutDesc.size())
	};
	psoDesc.pRootSignature = _rootSignature.Get();
	psoDesc.VS = { 
		reinterpret_cast<BYTE*>(_vertexShader->GetBufferPointer()),
		_vertexShader->GetBufferSize()
	};
	psoDesc.PS = {
		reinterpret_cast<BYTE*>(_pixelShader->GetBufferPointer()),
		_pixelShader->GetBufferSize()
	};
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	ThrowIfFailed(_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&_pso[0])));

	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;

	ThrowIfFailed(_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&_pso[1])));
}

void D3DRenderer::OnResize()
{
	// Called whenever the size of the app is changed
	// Also creates the render target view as well as the depth stencil view
	_cmdQueue->Flush();
	_cmdList->Reset();

	for (int i = 0; i < bufferCount; i++)
		_swapChainBuffer[i].Reset();
	_depthStencilBuffer.Reset();

	ThrowIfFailed(_swapChain->ResizeBuffers(bufferCount,
		_appWidth, _appHeight,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
	));

	_currBackBuffer = 0u;

	CreateRTV();
	CreateDSV();

	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * d3dUtil::PI, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&_proj, P);
}

ID3D12Resource* D3DRenderer::GetCurrBackBuffer()
{
	return _swapChainBuffer[_currBackBuffer].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DRenderer::GetCurrBackBufferView()
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		_rtvHeap->GetCPUStart(),
		_currBackBuffer,
		_rtvHeap->GetSize()
	);
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DRenderer::GetDSView()
{
	return _dsvHeap->GetCPUStart();
}

std::vector<CD3DX12_STATIC_SAMPLER_DESC> D3DRenderer::GetStaticSampler()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return { 
		pointWrap, pointClamp,
		linearWrap, linearClamp, 
		anisotropicWrap, anisotropicClamp };
}

void D3DRenderer::BeginFrame()
{
	auto& _currCmdAlloc = _currFrameResource->_cmdAlloc;

	ThrowIfFailed(_currFrameResource->_cmdAlloc->Reset());
	if (_isWireFrame)
	{
		_cmdList->Reset(_currCmdAlloc.Get(), _pso[0].Get());
	}
	else
		_cmdList->Reset(_currCmdAlloc.Get(), _pso[1].Get());

	_cmdList->ChangeResourceState(GetCurrBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
}

void D3DRenderer::DrawFrame()
{
	_cmdList->Get()->RSSetViewports(1, &_vp);
	_cmdList->Get()->RSSetScissorRects(1, &_scissor);

	_cmdList->Get()->ClearRenderTargetView(GetCurrBackBufferView(), Colors::SteelBlue, 0, nullptr);
	_cmdList->Get()->ClearDepthStencilView(GetDSView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.f, 0, 0, nullptr);

	auto currBackBufferView = GetCurrBackBufferView();
	auto dsv = GetDSView();
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
		auto passAddress = _currFrameResource->_pass->GetResource()->GetGPUVirtualAddress();
		_cmdList->Get()->SetGraphicsRootConstantBufferView(3, passAddress);
	}

	for (const auto& ri : _scene.GetRenderItems())
	{
		const auto& vbv = _geoLib.GetMesh().VertexBufferView();
		const auto& ibv = _geoLib.GetMesh().IndexBufferView();

		const auto& mat = _geoLib.GetMaterial(ri->_materialId);
		const auto& submesh = _geoLib.GetSubmesh(ri->_submeshId);

		_cmdList->Get()->IASetVertexBuffers(0, 1, &vbv);
		_cmdList->Get()->IASetIndexBuffer(&ibv);
		_cmdList->Get()->IASetPrimitiveTopology(ri->_primitiveType);

		if (_usingDescriptorTables)
		{
			UINT cbvIndex = _frameResourceIndex * static_cast<UINT>(_scene.GetRenderItems().size()) + ri->_cbObjIndex;
			UINT matIndex = _frameResourceIndex * static_cast<UINT>(_geoLib.GetMaterialCount()) + _matCbvOffset + mat._matCBIndex;

			_cmdList->Get()->SetGraphicsRootDescriptorTable(0, _cbvHeap->GetGPUStart(cbvIndex));
			_cmdList->Get()->SetGraphicsRootDescriptorTable(1, _cbvHeap->GetGPUStart(matIndex));
		}
		else
		{
			_cmdList->Get()->SetGraphicsRootDescriptorTable(0, _srvHeap->GetGPUStart(mat._diffuseSrvHeapIndex));

			auto cbvAddress = _currFrameResource->_cb->GetResource()->GetGPUVirtualAddress() + ri->_cbObjIndex * d3dUtil::CalcConstantBufferSize(sizeof(ConstantBuffer));
			auto matAddress = _currFrameResource->_mat->GetResource()->GetGPUVirtualAddress() + mat._matCBIndex * d3dUtil::CalcConstantBufferSize(sizeof(MaterialConstant));

			_cmdList->Get()->SetGraphicsRootConstantBufferView(1, cbvAddress);
			_cmdList->Get()->SetGraphicsRootConstantBufferView(2, matAddress);
		}

		_cmdList->Get()->DrawIndexedInstanced(submesh._indexCount, 1u, submesh._startIndexLocation, submesh._baseVertexLocation, 0u);
	}
}

void D3DRenderer::EndFrame()
{
	_cmdList->ChangeResourceState(GetCurrBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

	_cmdQueue->ExecuteCmdList(_cmdList->Get());
	_swapChain->Present(0, 0);
	_currBackBuffer = (_currBackBuffer + 1) % bufferCount;

	_currFrameResource->_fence = ++_cmdQueue->GetCurrFence();

	_cmdQueue->Signal();
}

void D3DRenderer::UpdateCamera(const Timer& t)
{

	// Camera Control
	if (_kbd->IsKeyPressed('W'))
		_cameraPos.z += _cameraSpeed * t.DeltaTime();
	else if (_kbd->IsKeyPressed('S'))
		_cameraPos.z -= _cameraSpeed * t.DeltaTime();
	else if (_kbd->IsKeyPressed('D'))
		_cameraPos.x += _cameraSpeed * t.DeltaTime();
	else if (_kbd->IsKeyPressed('A'))
		_cameraPos.x -= _cameraSpeed * t.DeltaTime();
	else if (_kbd->IsKeyPressed(VK_SPACE))
		_cameraPos.y += _cameraSpeed * t.DeltaTime();
	else if (_kbd->IsKeyPressed(VK_CONTROL))
		_cameraPos.y -= _cameraSpeed * t.DeltaTime();

	// WireFrame Control
	else if (_kbd->IsKeyPressed(VK_F2) && _kbd->WasKeyPressedThisFrame(VK_F2))
		_isWireFrame = !_isWireFrame;

	// Light Controll
	else if (_kbd->IsKeyPressed(VK_UP))
		_lightPhi -= _sunSpeed * t.DeltaTime();
	else if (_kbd->IsKeyPressed(VK_DOWN))
		_lightPhi += _sunSpeed * t.DeltaTime();
	else if (_kbd->IsKeyPressed(VK_LEFT))
		_lightTheta -= _sunSpeed * t.DeltaTime();
	else if (_kbd->IsKeyPressed(VK_RIGHT))
		_lightTheta += _sunSpeed * t.DeltaTime();

	_lightPhi = std::clamp(_lightPhi, 0.1f, XM_PIDIV2);
}

void D3DRenderer::UpdateModels(const Timer& t)
{
}

void D3DRenderer::UpdateObjCB(const Timer& t)
{
	auto currObjCB = _currFrameResource->_cb.get();
	for (auto& e : _scene.GetRenderItems())
	{
		XMMATRIX world = XMLoadFloat4x4(&e->_world);
		ConstantBuffer cb;
		XMStoreFloat4x4(&cb.world, XMMatrixTranspose(world));
		currObjCB->CopyData(e->_cbObjIndex, cb);

		e->_numDirtyFlags--;
	}
}

void D3DRenderer::UpdatePassCB(const Timer& t)
{
	XMVECTOR pos = XMLoadFloat4(&_cameraPos);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.f, 1.f, 0.f, 0.f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&_view, view);

	XMMATRIX proj = XMLoadFloat4x4(&_proj);

	XMMATRIX viewProj = view * proj;
	auto a = XMMatrixDeterminant(view);
	auto b = XMMatrixDeterminant(proj);
	auto c = XMMatrixDeterminant(viewProj);
	XMMATRIX invView = XMMatrixInverse(&a, view);
	XMMATRIX invProj = XMMatrixInverse(&b, proj);
	XMMATRIX invViewProj = XMMatrixInverse(&c, viewProj);

	XMStoreFloat4x4(&_mainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&_mainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&_mainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&_mainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&_mainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&_mainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	_mainPassCB.EyePosW = { _cameraPos.x, _cameraPos.y, _cameraPos.z };
	_mainPassCB.RenderTargetSize = XMFLOAT2((float)_appWidth, (float)_appHeight);
	_mainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / _appWidth, 1.0f / _appHeight);
	_mainPassCB.NearZ = 1.0f;
	_mainPassCB.FarZ = 1000.0f;
	_mainPassCB.TotalTime = t.TotalTime();
	_mainPassCB.DeltaTime = t.DeltaTime();
	_mainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };

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
	_mainPassCB.Lights[i].Strength = { 1.0f, 0.4f, 0.3f };
	_mainPassCB.Lights[i].FalloffStart = 2.0f;
	_mainPassCB.Lights[i].FalloffEnd = 1000.0f;
	_mainPassCB.Lights[i].Position = { 0.f, 10.f, 0.f };
	_mainPassCB.Lights[i].SpotPower = 8.0f;

	_currFrameResource->_pass->CopyData(0, _mainPassCB);
}

void D3DRenderer::UpdateMatCB(const Timer& t)
{
	auto currMatCB = _currFrameResource->_mat.get();
	for (auto& e : _scene.GetRenderItems())
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
