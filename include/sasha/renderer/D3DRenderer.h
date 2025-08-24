#include "../../../include/sasha/input/Keyboard.h"
#include "../../../include/sasha/input/Mouse.h"
#include "../sasha.h"
#include "FrameResource.h"

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
	void BuildInputLayout();

	void BuildGeometry();
	void BuildMaterial();
	void BuildLights();
	void BuildTextures();
	
	void BuildScene();

	void BuildFrameResources();
	void BuildCbvDescriptorHeap();
	void BuildConstantBuffers();
	void BuildRootSignature();
	void BuildPSO();
	
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

	Camera _camera;
	Keyboard* _kbd = nullptr;
	Mouse* _mouse = nullptr;

	std::unique_ptr<Device> _device;
	std::unique_ptr<SwapChain> _swapChain;

	std::unique_ptr<CommandQueue> _cmdQueue;
	std::unique_ptr<CommandList> _cmdList;

	std::unique_ptr<DescriptorHeap> _rtvHeap;
	std::unique_ptr<DescriptorHeap> _dsvHeap;

	ComPtr<ID3DBlob> _vertexShader;
	ComPtr<ID3DBlob> _pixelShader;
	std::vector<D3D12_INPUT_ELEMENT_DESC> _inputLayoutDesc{};

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

	std::unique_ptr<DescriptorHeap> _srvHeap;

	static constexpr float _sunSpeed = 2.5f;
	float _lightTheta = 1.25f * XM_PI;
	float _lightPhi = 0.1f;

	std::array<ComPtr<ID3D12PipelineState>, 2> _pso;
	ComPtr<ID3D12RootSignature> _rootSignature;
	bool _usingDescriptorTables = false;
};