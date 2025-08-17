#include "../../../include/sasha/input/Keyboard.h"
#include "../../../include/sasha/input/Mouse.h"
#include "../sasha.h"
#include "Scene.h"
#include "FrameResource.h"
#include <functional>

using namespace Microsoft::WRL;
using namespace DirectX;

class D3DRenderer
{
public:
	D3DRenderer(HWND wh, int w, int h);
	~D3DRenderer();
	
	D3DRenderer(const D3DRenderer&) = delete;
	D3DRenderer& operator=(const D3DRenderer&) = delete;

	void d3dInit();
	void SetInputs(Keyboard* kb, Mouse* m) noexcept;
	void RenderFrame(Timer& t);
	void Update(Timer& t);
	
	void OnResize();
	float AspectRatio() const noexcept;
	void SetAppSize(int w, int h) noexcept;

private:
	void CreateDevice();
	void CreateCmdObjs();
	void CreateSwapChain();
	void CreateDescriptorHeaps();
	void CreateRTV();
	void CreateDSV();

	void BuildInputLayout();
	void BuildGeometry();
	void BuildMaterial();
	void BuildLights();
	void BuildScene();
	void BuildFrameResources();
	void BuildCbvDescriptorHeap();
	void BuildConstantBuffers();
	void BuildRootSignature();
	void BuildPSO();
	
	ID3D12Resource* GetCurrBackBuffer();
	D3D12_CPU_DESCRIPTOR_HANDLE GetCurrBackBufferView();
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSView();

	void BeginFrame();
	void DrawFrame();
	void EndFrame();

	void UpdateCamera(const Timer& t);
	void UpdateModels(const Timer& t);
	void UpdateObjCB(const Timer& t);
	void UpdatePassCB(const Timer& t);
	void UpdateMatCB(const Timer& t);

private:
	int _appWidth;
	int _appHeight;
	HWND _wndHandle;
	bool _isWireFrame = false;

	HANDLE _eventHandle;

	Keyboard* _kbd = nullptr;
	Mouse* _mouse = nullptr;

	XMFLOAT4 _cameraPos = { 0.f, 5.f, -5.f, 1.f };
	const float _cameraSpeed = 25.f;

	ComPtr<ID3D12Device> _device;
	ComPtr<IDXGIFactory4> _factory;
	ComPtr<IDXGISwapChain> _swapChain;

	std::unique_ptr<CommandQueue> _cmdQueue;
	std::unique_ptr<CommandList> _cmdList;

	std::unique_ptr<DescriptorHeap> _rtvHeap;
	std::unique_ptr<DescriptorHeap> _dsvHeap;

	static constexpr UINT bufferCount = 2u;
	UINT _currBackBuffer = 0u;
	ComPtr<ID3D12Resource> _swapChainBuffer[bufferCount];
	ComPtr<ID3D12Resource> _depthStencilBuffer;

	D3D12_VIEWPORT _vp{};
	D3D12_RECT _scissor{};

	ComPtr<ID3DBlob> _vertexShader;
	ComPtr<ID3DBlob> _pixelShader;
	std::vector<D3D12_INPUT_ELEMENT_DESC> _inputLayoutDesc{};

	XMFLOAT4X4 _view = d3dUtil::Identity4x4();
	XMFLOAT4X4 _proj = d3dUtil::Identity4x4();

	GeometryLibrary _geoLib;
	Scene _scene;
	
	static constexpr int _frameResourceCount = 3;
	std::vector<std::unique_ptr<FrameResource>> _frameResources;
	FrameResource* _currFrameResource = nullptr;
	int _frameResourceIndex = 0u;

	PassBuffer _mainPassCB;
	UINT _passCbvOffset = 0u;
	UINT _matCbvOffset = 0u;
	std::unique_ptr<DescriptorHeap> _cbvHeap;

	static constexpr float _sunSpeed = 2.5f;
	float _lightTheta = 1.25f * XM_PI;
	float _lightPhi = 0.1f;

	std::array<ComPtr<ID3D12PipelineState>, 2> _pso;
	ComPtr<ID3D12RootSignature> _rootSignature;
};