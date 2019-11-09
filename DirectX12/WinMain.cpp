#include<windows.h>
#include <wrl.h>
#include<memory>
#include<array>
#include <d3d12.h>
#pragma comment(lib, "d3d12.lib")
#include <dxgi1_4.h>
#pragma comment(lib, "dxgi.lib")
#include <D3Dcompiler.h>
//#pragma comment(lib, "d3dcompiler.lib")
#include <DirectXMath.h>

using Microsoft::WRL::ComPtr;

void Draw();
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

uint32_t g_frameIndex = 0;
uint32_t g_rtvDescriptorSize = 0;
uint32_t g_dsvDescriptorSize = 0;

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

std::shared_ptr<ID3D12Device> g_pDevice = nullptr;

// デバイス作成
[[nodiscard]] std::shared_ptr<ID3D12Device> CreateDevice()
{
	ID3D12Device* pDevice = nullptr;
	if (auto hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&pDevice)); FAILED(hr)) {
		return nullptr;
	}
	return std::shared_ptr<ID3D12Device>(pDevice, [](ID3D12Device* c) {
		c->Release();
		});
}

// command queue
[[nodiscard]] std::shared_ptr<ID3D12CommandQueue> CreateCommandQueue(const std::shared_ptr<ID3D12Device>& pDevice)
{
	D3D12_COMMAND_QUEUE_DESC queueDesc{
		.Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
		.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE
	};
	ID3D12CommandQueue* pCommandQueue = nullptr;
	if (auto hr = pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&pCommandQueue)); FAILED(hr)) {
		return nullptr;
	}
	return std::shared_ptr<ID3D12CommandQueue>(pCommandQueue, [](ID3D12CommandQueue* c) {
		c->Release();
		});
}

// command allocater
[[nodiscard]] std::shared_ptr<ID3D12CommandAllocator> CreateCommandAllocator(const std::shared_ptr<ID3D12Device>& pDevice)
{
	ID3D12CommandAllocator* pCmdAllocator = nullptr;
	if (auto hr = pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&pCmdAllocator)); FAILED(hr)) {
		return nullptr;
	}
	return std::shared_ptr<ID3D12CommandAllocator>(pCmdAllocator, [](ID3D12CommandAllocator* c) {
		c->Release();
	});
}

// commandlist
[[nodiscard]] std::shared_ptr<ID3D12CommandList> CreateCommandList(
	const std::shared_ptr<ID3D12Device>& pDevice, 
	const std::shared_ptr<ID3D12CommandAllocator>& pCmdAllocator
){
	ID3D12CommandList* pCmdList = nullptr;
	if (auto hr = pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,pCmdAllocator.get(),nullptr, IID_PPV_ARGS(&pCmdList)); FAILED(hr)) {
		return nullptr;
	}
	return std::shared_ptr<ID3D12CommandList>(pCmdList, [](ID3D12CommandList* c) {
		c->Release();
	});
}

// dxgi factory
[[nodiscard]] std::shared_ptr<IDXGIFactory4> CreateDXGIFactory()
{
	IDXGIFactory4* pFactory = nullptr;

	if (auto hr = CreateDXGIFactory1(IID_PPV_ARGS(&pFactory)); FAILED(hr)) {
		return nullptr;
	}
	return std::shared_ptr<IDXGIFactory4>(pFactory, [](IDXGIFactory4* c) {
		return c->Release();
		});
}

constexpr int32_t BufferCount = 2;

// swapchain
[[nodiscard]] std::shared_ptr<IDXGISwapChain> CreateSwapChain(
	HWND hWnd,
	const std::shared_ptr<ID3D12CommandQueue>& pCommandQueue,
	const std::shared_ptr<IDXGIFactory4>& pFactory
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

	IDXGISwapChain* pSwapChain = nullptr;
	if (auto hr = pFactory->CreateSwapChain(pCommandQueue.get(), &desc, &pSwapChain); FAILED(hr)) {
		return nullptr;
	}

	return std::shared_ptr<IDXGISwapChain>(pSwapChain, [](IDXGISwapChain* c) {
		return c->Release();
		});
}
// swapchain
[[nodiscard]] std::shared_ptr<IDXGISwapChain3> CreateSwapChain3(
	HWND hWnd,
	const std::shared_ptr<ID3D12CommandQueue>& pCommandQueue,
	const std::shared_ptr<IDXGIFactory4>& pFactory
) {

	auto pSwapChain = CreateSwapChain(hWnd, pCommandQueue, pFactory);
	IDXGISwapChain3* pSwapChain3 = nullptr;
	if (auto hr = pSwapChain->QueryInterface(IID_PPV_ARGS(&pSwapChain3)); FAILED(hr)) {
		return nullptr;
	}
	return std::shared_ptr<IDXGISwapChain3>(pSwapChain3, [](IDXGISwapChain3* c) {
		return c->Release();
		});
};

[[nodiscard]] std::shared_ptr<ID3D12DescriptorHeap> CreateDescriptorHeap(const std::shared_ptr<ID3D12Device>& pDevice, D3D12_DESCRIPTOR_HEAP_TYPE type)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc{
		.Type = type,
		.NumDescriptors = BufferCount,
		.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE
	};
	ID3D12DescriptorHeap* pHeap = nullptr;
	if (auto hr = pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&pHeap)); FAILED(hr)) {
		return nullptr;
	}
	return std::shared_ptr<ID3D12DescriptorHeap>(pHeap, [](ID3D12DescriptorHeap* c) {
		c->Release();
		});
}
[[nodiscard]] std::shared_ptr<ID3D12DescriptorHeap> CreateRenderTargetViewHeap(const std::shared_ptr<ID3D12Device>& pDevice)
{
	return ::CreateDescriptorHeap(pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
}

// FIXME
std::array<std::shared_ptr<ID3D12Resource>, BufferCount> g_renderTargetViews;

bool CreateRenderTargertViews(
	const std::shared_ptr<ID3D12Device>& pDevice,
	const std::shared_ptr<IDXGISwapChain3>& swapChain,
	const std::shared_ptr<ID3D12DescriptorHeap>& pRtvHeap
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
		ID3D12Resource* renderTarget = nullptr;
		if (auto hr = swapChain->GetBuffer(count, IID_PPV_ARGS(&renderTarget)); FAILED(hr)) {
			return false;
		}
		pDevice->CreateRenderTargetView(renderTarget, &desc, handle);
		handle.ptr += g_rtvDescriptorSize;

		g_renderTargetViews[count] = std::shared_ptr<ID3D12Resource>(renderTarget, [](ID3D12Resource* c) {
			c->Release();
			});
	}
	return true;
}

[[nodiscard]] std::shared_ptr<ID3D12DescriptorHeap> CreateDepthStencilViewHeap(const std::shared_ptr<ID3D12Device>& pDevice)
{
	return ::CreateDescriptorHeap(pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
}

[[nodiscard]] std::shared_ptr<ID3D12Resource> CreateDepthStencilView(
	const std::shared_ptr<ID3D12Device>& pDevice,
	const std::shared_ptr<ID3D12DescriptorHeap>& pDsvHeap
){
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
	ID3D12Resource* pDepthStencil = nullptr;

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
	pDevice->CreateDepthStencilView(pDepthStencil,&dsvDesc,pDsvHeap->GetCPUDescriptorHandleForHeapStart());

	return std::shared_ptr<ID3D12Resource>(pDepthStencil, [](ID3D12Resource* c) {
		c->Release();
	});
}

[[nodiscard]] std::shared_ptr<ID3D12RootSignature> CreateRootSignature(const std::shared_ptr<ID3D12Device>& pDevice)
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
	ID3D12RootSignature* pRootSignature = nullptr;
	if (auto hr = pDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&pRootSignature)); FAILED(hr)) {
		return nullptr;
	}
	return std::shared_ptr<ID3D12RootSignature>(pRootSignature, [](ID3D12RootSignature* c) {
		c->Release();
	});
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
	g_frameIndex = swapChain->GetCurrentBackBufferIndex();

	auto rtvHeap = ::CreateRenderTargetViewHeap(device);
	if (!rtvHeap) {
		return false;
	}
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
	return true;
}

void Draw()
{

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