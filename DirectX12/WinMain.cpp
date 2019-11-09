#include<windows.h>
#include <wrl.h>
#include<memory>
#include<array>
#include <d3d12.h>
#pragma comment(lib, "d3d12.lib")
#include <dxgi1_4.h>
#pragma comment(lib, "dxgi.lib")
#include <D3Dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")
#include <DirectXMath.h>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

// vertex layout 
struct VertexLayout
{
	XMFLOAT3 position;
	XMFLOAT2 tex;
	XMFLOAT4 color;
};

bool Draw();
// ウィンドウプロシージャ
LRESULT CALLBACK WindowProc(HWND hWnd, UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	switch (nMsg) {
	case WM_PAINT:
		Draw();
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, nMsg, wParam, lParam);
	}

	return 0;
}

constexpr auto WindowTitle = "DirectX12_learn";
constexpr int32_t WindowWidth = 800;
constexpr int32_t WindowHeight = 600;
constexpr float	AspectRatio = (float)WindowWidth / (float)WindowHeight;

ComPtr<ID3D12Resource>		g_vertexBuffer;
D3D12_VERTEX_BUFFER_VIEW	g_vertexBufferView;

uint32_t g_frameIndex = 0;
HANDLE g_fenceEvent;
ComPtr<ID3D12Fence>	g_fence;
UINT64 g_fenceValue;

uint32_t g_rtvDescriptorSize = 0;
uint32_t g_dsvDescriptorSize = 0;

// FIXME
D3D12_VIEWPORT g_viewport = { 0.0f, 0.0f, (float)WindowWidth, (float)WindowHeight, 0.0f, 1.0f };
D3D12_RECT	g_scissorRect = { 0, 0, WindowWidth, WindowHeight };
ComPtr<ID3D12Device> g_pDevice = nullptr;
ComPtr<ID3D12CommandQueue> g_commandQueue = nullptr;
ComPtr<ID3D12CommandAllocator> g_commandAllocator = nullptr;
ComPtr<ID3D12GraphicsCommandList> g_commandList = nullptr;
ComPtr<ID3D12PipelineState> g_pipelineState = nullptr;
ComPtr<ID3D12DescriptorHeap> g_rtvHeap = nullptr;

ComPtr<ID3D12RootSignature> g_rootSignature = nullptr;
ComPtr<IDXGISwapChain3> g_swapChain = nullptr;

// ウィンドウを作成
HWND InitWindow(HINSTANCE hInstance)
{
	WNDCLASSEX	wndclass{
		.cbSize = sizeof(WNDCLASSEX),
		.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS,
		.lpfnWndProc = WindowProc,
		.hInstance = hInstance,
		.hCursor = LoadCursor(NULL, IDC_ARROW),
		.lpszClassName = WindowTitle
	};
	RegisterClassEx(&wndclass);

	RECT	windowRect = { 0, 0, WindowWidth, WindowHeight };

	AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

	// ウィンドウを作成
	return CreateWindow(
		WindowTitle,
		WindowTitle,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		windowRect.right - windowRect.left,
		windowRect.bottom - windowRect.top,
		NULL,
		NULL,
		hInstance,
		NULL
	);
}


// デバイス作成
[[nodiscard]] ComPtr<ID3D12Device> CreateDevice()
{
	ComPtr<ID3D12Device> pDevice = nullptr;
	if (auto hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&pDevice)); FAILED(hr)) {
		return nullptr;
	}
	return pDevice;
}

// command queue
[[nodiscard]] ComPtr<ID3D12CommandQueue> CreateCommandQueue(const ComPtr<ID3D12Device>& pDevice)
{
	D3D12_COMMAND_QUEUE_DESC queueDesc{
		.Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
		.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE
	};
	ComPtr<ID3D12CommandQueue> pCommandQueue = nullptr;
	if (auto hr = pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&pCommandQueue)); FAILED(hr)) {
		return nullptr;
	}
	return pCommandQueue;
}

// command allocater
[[nodiscard]] ComPtr<ID3D12CommandAllocator> CreateCommandAllocator(const ComPtr<ID3D12Device>& pDevice)
{
	ComPtr<ID3D12CommandAllocator> pCmdAllocator = nullptr;
	if (auto hr = pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&pCmdAllocator)); FAILED(hr)) {
		return nullptr;
	}
	return pCmdAllocator;
}

// commandlist
[[nodiscard]] ComPtr<ID3D12GraphicsCommandList> CreateCommandList(
	const ComPtr<ID3D12Device>& pDevice,
	const ComPtr<ID3D12CommandAllocator>& pCmdAllocator,
	const ComPtr<ID3D12PipelineState>& pipelineState
) {
	ComPtr<ID3D12GraphicsCommandList> pCmdList = nullptr;
	if (auto hr = pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, pCmdAllocator.Get(), pipelineState.Get(), IID_PPV_ARGS(&pCmdList)); FAILED(hr)) {
		return nullptr;
	}
	return pCmdList;
}

// dxgi factory
[[nodiscard]] ComPtr<IDXGIFactory4> CreateDXGIFactory()
{
	ComPtr<IDXGIFactory4> pFactory = nullptr;

	if (auto hr = CreateDXGIFactory1(IID_PPV_ARGS(&pFactory)); FAILED(hr)) {
		return nullptr;
	}
	return pFactory;
}

constexpr int32_t BufferCount = 2;

// swapchain
[[nodiscard]] ComPtr<IDXGISwapChain> CreateSwapChain(
	HWND hWnd,
	const ComPtr<ID3D12CommandQueue>& pCommandQueue,
	const ComPtr<IDXGIFactory4>& pFactory
) {
	DXGI_SWAP_CHAIN_DESC desc = {
		.BufferDesc {
			.Width = WindowWidth,
			.Height = WindowHeight,
			.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
		},
		.SampleDesc {
		  .Count = 1
		},
		.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
		.BufferCount = BufferCount,
		.OutputWindow = hWnd,
		.Windowed = true,
		.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
	};

	ComPtr<IDXGISwapChain> pSwapChain = nullptr;
	if (auto hr = pFactory->CreateSwapChain(pCommandQueue.Get(), &desc, &pSwapChain); FAILED(hr)) {
		return nullptr;
	}

	return pSwapChain;
}
// swapchain
[[nodiscard]] ComPtr<IDXGISwapChain3> CreateSwapChain3(
	HWND hWnd,
	const ComPtr<ID3D12CommandQueue>& pCommandQueue,
	const ComPtr<IDXGIFactory4>& pFactory
) {

	auto pSwapChain = CreateSwapChain(hWnd, pCommandQueue, pFactory);
	ComPtr<IDXGISwapChain3> pSwapChain3 = nullptr;
	if (auto hr = pSwapChain->QueryInterface(IID_PPV_ARGS(&pSwapChain3)); FAILED(hr)) {
		return nullptr;
	}
	return pSwapChain3;
};

[[nodiscard]] ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(const ComPtr<ID3D12Device>& pDevice, D3D12_DESCRIPTOR_HEAP_TYPE type)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc{
		.Type = type,
		.NumDescriptors = BufferCount,
		.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE
	};
	ComPtr<ID3D12DescriptorHeap> pHeap = nullptr;
	if (auto hr = pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&pHeap)); FAILED(hr)) {
		return nullptr;
	}
	return pHeap;
}
[[nodiscard]] ComPtr<ID3D12DescriptorHeap> CreateRenderTargetViewHeap(const ComPtr<ID3D12Device>& pDevice)
{
	return ::CreateDescriptorHeap(pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
}

// FIXME
std::array<ComPtr<ID3D12Resource>, BufferCount> g_renderTargetViews;

bool CreateRenderTargertViews(
	const ComPtr<ID3D12Device>& pDevice,
	const ComPtr<IDXGISwapChain3>& swapChain,
	const ComPtr<ID3D12DescriptorHeap>& pRtvHeap
) {
	auto handle = pRtvHeap->GetCPUDescriptorHandleForHeapStart();

	D3D12_RENDER_TARGET_VIEW_DESC desc{
		.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
		.Texture2D{
			 .MipSlice = 0,
			 .PlaneSlice = 0
		}
	};

	for (uint32_t count = 0; count < BufferCount; ++count) {
		if (auto hr = swapChain->GetBuffer(count, IID_PPV_ARGS(&g_renderTargetViews[count])); FAILED(hr)) {
			return false;
		}
		pDevice->CreateRenderTargetView(g_renderTargetViews[count].Get(), &desc, handle);
		handle.ptr += g_rtvDescriptorSize;
	}
	return true;
}

[[nodiscard]] ComPtr<ID3D12DescriptorHeap> CreateDepthStencilViewHeap(const ComPtr<ID3D12Device>& pDevice)
{
	return ::CreateDescriptorHeap(pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
}

[[nodiscard]] ComPtr<ID3D12Resource> CreateDepthStencilView(
	const ComPtr<ID3D12Device>& pDevice,
	const ComPtr<ID3D12DescriptorHeap>& pDsvHeap
) {
	D3D12_HEAP_PROPERTIES prop{
		.Type = D3D12_HEAP_TYPE_DEFAULT,
		.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
		.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
		.CreationNodeMask = 1,
		.VisibleNodeMask = 1,
	};

	D3D12_RESOURCE_DESC desc{
		.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
		.Alignment = 0,
		.Width = WindowWidth,
		.Height = WindowHeight,
		.DepthOrArraySize = 1,
		.MipLevels = 0,
		.Format = DXGI_FORMAT_D32_FLOAT,
		.SampleDesc{
			.Count = 1,
			.Quality = 0
		},
		.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
		.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
	};
	D3D12_CLEAR_VALUE clearValue{
		.Format = DXGI_FORMAT_D32_FLOAT,
		.DepthStencil{
			.Depth = 1.0,
			.Stencil = 0
		}
	};
	ComPtr<ID3D12Resource> pDepthStencil = nullptr;

	if (auto hr = pDevice->CreateCommittedResource(
		&prop,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&clearValue,
		IID_PPV_ARGS(&pDepthStencil)
	); FAILED(hr)) {
		return nullptr;
	}
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{
		.Format = DXGI_FORMAT_D32_FLOAT,
		.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
		.Flags = D3D12_DSV_FLAG_NONE
	};
	pDevice->CreateDepthStencilView(pDepthStencil.Get(), &dsvDesc, pDsvHeap->GetCPUDescriptorHandleForHeapStart());

	return pDepthStencil;
}

[[nodiscard]] ComPtr<ID3D12RootSignature> CreateRootSignature(const ComPtr<ID3D12Device>& pDevice)
{
	D3D12_ROOT_SIGNATURE_DESC desc{
		.NumParameters = 0,
		.pParameters = nullptr,
		.NumStaticSamplers = 0,
		.pStaticSamplers = nullptr,
		.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	};
	ComPtr<ID3DBlob> signature = nullptr;
	ComPtr<ID3DBlob> error = nullptr;
	if (auto hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error); FAILED(hr)) {
		return nullptr;
	}
	ComPtr<ID3D12RootSignature> pRootSignature = nullptr;
	if (auto hr = pDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&pRootSignature)); FAILED(hr)) {
		return nullptr;
	}
	return pRootSignature;
}

[[nodiscard]] ComPtr<ID3D12PipelineState> CreatePipelineState(
	const ComPtr<ID3D12Device>& pDevice,
	const ComPtr<ID3D12RootSignature>& pRootSignature
)
{
	ComPtr<ID3DBlob> vs;
	ComPtr<ID3DBlob> ps;
	uint8_t compileFlags = 0;

	if (auto hr = D3DCompileFromFile(L"shader.hlsl", nullptr, nullptr, "VS", "vs_5_0", compileFlags, 0, &vs, nullptr); FAILED(hr)) {
		return nullptr;
	}

	if (auto hr = D3DCompileFromFile(L"shader.hlsl", nullptr, nullptr, "PS", "ps_5_0", compileFlags, 0, &ps, nullptr); FAILED(hr)) {
		return nullptr;
	}

	// setting input layout.
	D3D12_INPUT_ELEMENT_DESC inputElements[] = {
		{ "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR",     0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC	psoDesc = {};
	psoDesc.InputLayout = { inputElements, _countof(inputElements) };
	psoDesc.pRootSignature = pRootSignature.Get();
	{
		psoDesc.VS = D3D12_SHADER_BYTECODE{
			.pShaderBytecode = vs->GetBufferPointer(),
			.BytecodeLength = vs->GetBufferSize()
		};
	}
	{
		psoDesc.PS = D3D12_SHADER_BYTECODE{
			.pShaderBytecode = ps->GetBufferPointer(),
			.BytecodeLength = ps->GetBufferSize()
		};
	}
	{
		D3D12_RASTERIZER_DESC rasterizerDesc = {};
		rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
		rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
		rasterizerDesc.FrontCounterClockwise = false;
		rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
		rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		rasterizerDesc.DepthClipEnable = true;
		rasterizerDesc.MultisampleEnable = false;
		rasterizerDesc.AntialiasedLineEnable = false;
		rasterizerDesc.ForcedSampleCount = 0;
		rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
		psoDesc.RasterizerState = rasterizerDesc;
	}
	{
		D3D12_BLEND_DESC blendDesc = {};
		blendDesc.AlphaToCoverageEnable = false;
		blendDesc.IndependentBlendEnable = false;

		D3D12_RENDER_TARGET_BLEND_DESC descRTBS{
			.BlendEnable = false,
			.LogicOpEnable = false,
			.SrcBlend = D3D12_BLEND_ONE,
			.DestBlend = D3D12_BLEND_ZERO,
			.BlendOp = D3D12_BLEND_OP_ADD,
			.SrcBlendAlpha = D3D12_BLEND_ONE,
			.DestBlendAlpha = D3D12_BLEND_ZERO,
			.BlendOpAlpha = D3D12_BLEND_OP_ADD,
			.LogicOp = D3D12_LOGIC_OP_NOOP,
			.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL,
		};
		for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i) {
			blendDesc.RenderTarget[i] = descRTBS;
		}
		psoDesc.BlendState = blendDesc;
	}
	psoDesc.DepthStencilState.DepthEnable = false;
	psoDesc.DepthStencilState.StencilEnable = false;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.SampleDesc.Count = 1;

	ComPtr<ID3D12PipelineState> pPipelineState = nullptr;
	if (auto hr = pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pPipelineState)); FAILED(hr)) {
		return nullptr;
	}
	return pPipelineState;
}

bool WaitForGPU()
{
	const uint64_t fence = g_fenceValue;
	if (auto hr = g_commandQueue->Signal(g_fence.Get(), fence); FAILED(hr)) {
		return false;
	}
	g_fenceValue++;

	// 前のフレームが終了するまで待機
	if (g_fence->GetCompletedValue() < fence) {
		if (auto hr = g_fence->SetEventOnCompletion(fence, g_fenceEvent); FAILED(hr)) {
			return false;
		}
		WaitForSingleObject(g_fenceEvent, INFINITE);
	}

	g_frameIndex = g_swapChain->GetCurrentBackBufferIndex();

	return true;
}
bool InitDirectX12(HWND hWnd)
{
	// device
	auto device = ::CreateDevice();
	if (!device) {
		return false;
	}
	g_pDevice = device; // FIXME

	// command queue
	auto commandQueue = ::CreateCommandQueue(device);
	if (!commandQueue) {
		return false;
	}
	g_commandQueue = commandQueue;

	// dxgi factory
	auto dxgiFactory = ::CreateDXGIFactory();
	if (!dxgiFactory) {
		return false;
	}

	// swap chain
	auto swapChain = ::CreateSwapChain3(hWnd, commandQueue, dxgiFactory);
	if (!swapChain) {
		return false;
	}
	g_swapChain = swapChain;
	g_frameIndex = swapChain->GetCurrentBackBufferIndex();

	auto rtvHeap = ::CreateRenderTargetViewHeap(device);
	if (!rtvHeap) {
		return false;
	}
	g_rtvHeap = rtvHeap;
	g_rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// render target view
	if (!::CreateRenderTargertViews(device, swapChain, rtvHeap)) {
		return false;
	}
	auto dsvHeap = ::CreateDepthStencilViewHeap(device);
	if (!dsvHeap) {
		return false;
	}
	g_dsvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	// depth stencil view
	auto depthStecilView = ::CreateDepthStencilView(device, dsvHeap);
	if (!depthStecilView) {
		return false;
	}

	auto cmdAllocator = ::CreateCommandAllocator(device);
	if (!cmdAllocator) {
		return false;
	}
	g_commandAllocator = cmdAllocator;

	auto rootSignature = ::CreateRootSignature(device);
	if (!rootSignature) {
		return false;
	}
	g_rootSignature = rootSignature;

	auto pipelineState = ::CreatePipelineState(device, rootSignature);
	if (!pipelineState) {
		return false;
	}
	g_pipelineState = pipelineState;

	auto commandList = ::CreateCommandList(device, cmdAllocator, pipelineState);
	if (!commandList) {
		return false;
	}
	if (auto hr = commandList->Close(); FAILED(hr)) {
		return false;
	}
	g_commandList = commandList;

	{
		constexpr float s = 0.5f;
		// 三角形のジオメトリを定義
		VertexLayout triangleVertices[] = {
			{{ -s,  s * AspectRatio, 0.0f}, {0.0,0.0}, {1.0f, 0.0f, 0.0f, 1.0f}},
			{{ s, s * AspectRatio, 0.0f}, {1.0,0.0}, {0.0f, 1.0f, 0.0f, 1.0f}},
			{{ s, -s * AspectRatio, 0.0f}, {1.0,1.0}, {0.0f, 0.0f, 1.0f, 1.0f}},
			{{ -s,  s * AspectRatio, 0.0f}, {0.0,0.0}, {1.0f, 0.0f, 0.0f, 1.0f}},
			{{ s, -s * AspectRatio, 0.0f}, {1.0,1.0}, {0.0f, 0.0f, 1.0f, 1.0f}},
			{{ -s, -s * AspectRatio, 0.0f}, {0.0,1.0}, {0.0f, 0.0f, 1.0f, 1.0f}},
		};

		const UINT	vertexBufferSize = sizeof(triangleVertices);

		{
			D3D12_HEAP_PROPERTIES	heapProperties = {};
			heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
			heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			heapProperties.CreationNodeMask = 1;
			heapProperties.VisibleNodeMask = 1;

			D3D12_RESOURCE_DESC	resourceDesc = {};
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resourceDesc.Alignment = 0;
			resourceDesc.Width = vertexBufferSize;
			resourceDesc.Height = 1;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
			resourceDesc.SampleDesc.Count = 1;
			resourceDesc.SampleDesc.Quality = 0;
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

			if (FAILED(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&g_vertexBuffer)))) return FALSE;
		}

		// 頂点バッファに頂点データをコピー
		UINT8* pVertexDataBegin;
		D3D12_RANGE	readRange = { 0, 0 };		// CPUからバッファを読み込まない設定
		if (FAILED(g_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)))) return false;
		memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices));
		g_vertexBuffer->Unmap(0, nullptr);

		// 頂点バッファビューを初期化
		g_vertexBufferView.BufferLocation = g_vertexBuffer->GetGPUVirtualAddress();
		g_vertexBufferView.StrideInBytes = sizeof(VertexLayout);
		g_vertexBufferView.SizeInBytes = vertexBufferSize;
	}

	// 同期オブジェクトを作成してリソースがGPUにアップロードされるまで待機
	{
		if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence)))) return false;
		g_fenceValue = 1;

		// フレーム同期に使用するイベントハンドラを作成
		g_fenceEvent = CreateEvent(nullptr, false, false, nullptr);
		if (g_fenceEvent == nullptr) {
			if (FAILED(HRESULT_FROM_WIN32(GetLastError()))) return false;
		}

		if (!WaitForGPU()) return false;
	}
	return true;
}

bool Draw()
{
	if (FAILED(g_commandAllocator->Reset())) return FALSE;
	if (FAILED(g_commandList->Reset(g_commandAllocator.Get(), g_pipelineState.Get()))) return FALSE;

	g_commandList->SetGraphicsRootSignature(g_rootSignature.Get());
	g_commandList->RSSetViewports(1, &g_viewport);
	g_commandList->RSSetScissorRects(1, &g_scissorRect);

	// バックバッファをレンダリングターゲットとして使用
	{
		D3D12_RESOURCE_BARRIER	resourceBarrier = {};
		resourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		resourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		resourceBarrier.Transition.pResource = g_renderTargetViews[g_frameIndex].Get();
		resourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		resourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		resourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		g_commandList->ResourceBarrier(1, &resourceBarrier);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE	rtvHandle = {};
	rtvHandle.ptr = g_rtvHeap->GetCPUDescriptorHandleForHeapStart().ptr + g_frameIndex * g_rtvDescriptorSize;
	g_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

	// バックバッファに描画
	const float	clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	g_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	g_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	g_commandList->IASetVertexBuffers(0, 1, &g_vertexBufferView);
	g_commandList->DrawInstanced(6, 2, 0, 0);

	// バックバッファを表示
	{
		D3D12_RESOURCE_BARRIER	resourceBarrier = {};
		resourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		resourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		resourceBarrier.Transition.pResource = g_renderTargetViews[g_frameIndex].Get();
		resourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		resourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		resourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		g_commandList->ResourceBarrier(1, &resourceBarrier);
	}

	if (FAILED(g_commandList->Close())) return FALSE;


	// コマンドリストを実行
	ID3D12CommandList* ppCommandLists[] = { g_commandList.Get() };
	g_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// フレームを最終出力
	if (FAILED(g_swapChain->Present(1, 0))) return FALSE;

	return WaitForGPU();
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
	auto hWnd = ::InitWindow(hInstance);

	if (!InitDirectX12(hWnd)) {
		return 0;
	}
	ShowWindow(hWnd, SW_SHOW);
	UpdateWindow(hWnd);

	// メッセージループ
	MSG	msg;

	while (1) {
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) break;

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return 0;
}