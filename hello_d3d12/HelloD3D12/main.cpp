#include <stdio.h>

// DirectX 12 specific headers.
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

#define _DEBUG

#if !defined(NDEBUG) && !defined(_DEBUG)
#error "Define at least one."
#elif defined(NDEBUG) && defined(_DEBUG)
#error "Define at most one."
#endif

#if defined(_WIN64)
#if defined(_DEBUG)
#pragma comment (lib, "lib/64/SDL2-staticd")
#else
#pragma comment (lib, "lib/64/SDL2-static")
#endif
#else
#if defined(_DEBUG)
#pragma comment (lib, "lib/32/SDL2-staticd")
#else
#pragma comment (lib, "lib/32/SDL2-static")
#endif
#endif

#pragma comment (lib, "Imm32")
#pragma comment (lib, "Setupapi")
#pragma comment (lib, "Version")
#pragma comment (lib, "Winmm")

#pragma comment (lib, "d3d12")
#pragma comment (lib, "dxgi")
#pragma comment (lib, "dxguid")
#pragma comment (lib, "uuid")
#pragma comment (lib, "gdi32")

#define _CRT_SECURE_NO_WARNINGS 1

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define SDL_MAIN_HANDLED
#include "SDL2/SDL.h"
#include "SDL2/SDL_syswm.h"

#include <cstdio>
#include <algorithm>

//// Windows Runtime Library. Needed for Microsoft::WRL::ComPtr<> template class.
//#include <wrl.h>
//using namespace Microsoft::WRL;

/*
d3d12.lib
dxgi.lib
dxguid.lib
uuid.lib
kernel32.lib
user32.lib
gdi32.lib
winspool.lib
comdlg32.lib
advapi32.lib
shell32.lib
ole32.lib
oleaut32.lib
odbc32.lib
odbccp32.lib
runtimeobject.lib
*/

#define SUCCEEDED(hr)   (((HRESULT)(hr)) >= 0)
//#define SUCCEEDED_OPERATION(hr)   (((HRESULT)(hr)) == S_OK)
#define FAILED(hr)      (((HRESULT)(hr)) < 0)
//#define FAILED_OPERATION(hr)      (((HRESULT)(hr)) != S_OK)
#define CHECK_AND_FAIL(hr)                          \
    if (FAILED(hr)) {                               \
        ::printf("[ERROR] " #hr "() failed at line %d. \n", __LINE__);   \
        ::abort();                                  \
    }                                               \
    /**/

#if defined(_DEBUG)
#define ENABLE_DEBUG_LAYER 1
#else
#define ENABLE_DEBUG_LAYER 0
#endif

#if defined(_DEBUG)
UINT compiler_flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
UINT compiler_flags = 0;
#endif

int main () 
{
    // SDL_Init
    SDL_Init(SDL_INIT_VIDEO);

    // Enable Debug Layer
#if ENABLE_DEBUG_LAYER > 0
    ID3D12Debug * debug_interface_dx = nullptr;
    if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_interface_dx)))) {
        debug_interface_dx->EnableDebugLayer();
    }
#endif
    // Create a Window
    SDL_Window * wnd = SDL_CreateWindow("LearningD3D12", 0, 0, 1280, 720, 0);
    if(nullptr == wnd) {
        ::abort();
    }

    // Query Adapter (PhysicalDevice)
    IDXGIFactory * dxgi_factory = nullptr;
    CHECK_AND_FAIL(CreateDXGIFactory(IID_PPV_ARGS(&dxgi_factory)));

    constexpr uint32_t MaxAdapters = 8;
    IDXGIAdapter * adapters[MaxAdapters] = {};
    IDXGIAdapter * pAdapter;
    for (UINT i = 0; dxgi_factory->EnumAdapters(i, &pAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
    {
        adapters[i] = pAdapter;
        DXGI_ADAPTER_DESC adapter_desc = {};
        ::printf("GPU Info [%d] :\n", i);
        if(SUCCEEDED(pAdapter->GetDesc(&adapter_desc))) {
            ::printf("\tDescription: %ls\n", adapter_desc.Description);
            ::printf("\tDedicatedVideoMemory: %zu\n", adapter_desc.DedicatedVideoMemory);
        }
    } // WARP -> Windows Advanced Rasterization ...

    // Create Logical Device
    ID3D12Device * d3d_device = nullptr;
    auto res = D3D12CreateDevice(adapters[0], D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&d3d_device));
    CHECK_AND_FAIL(res);

    // Create Command Queues
    D3D12_COMMAND_QUEUE_DESC cmd_q_desc = {};
    cmd_q_desc.Type = D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT;
    cmd_q_desc.Flags = D3D12_COMMAND_QUEUE_FLAGS::D3D12_COMMAND_QUEUE_FLAG_NONE;
    ID3D12CommandQueue * d3d_cmd_q = nullptr;
    res = d3d_device->CreateCommandQueue(&cmd_q_desc, IID_PPV_ARGS(&d3d_cmd_q));
    CHECK_AND_FAIL(res);

    // -- data
    UINT width = 1280;
    UINT height = 720;
    UINT const frame_count = 2;   // Use double-buffering
    UINT rtv_descriptor_size = 0;
    ID3D12Resource * render_targets [frame_count];

    DXGI_MODE_DESC backbuffer_desc = {};
    backbuffer_desc.Width = 1280;
    backbuffer_desc.Height = 720;
    backbuffer_desc.Format = DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM;

    DXGI_SAMPLE_DESC sampler_desc = {};
    sampler_desc.Count = 1;
    sampler_desc.Quality = 0;

    SDL_SysWMinfo wnd_info = {};
    SDL_GetWindowWMInfo(wnd, &wnd_info);
    HWND hwnd = wnd_info.info.win.window;

    // Create Swapchain
    DXGI_SWAP_CHAIN_DESC swapchain_desc = {};
    swapchain_desc.BufferDesc = backbuffer_desc;
    swapchain_desc.SampleDesc = sampler_desc;
    swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchain_desc.BufferCount = 2; // Use double-buffering;
    swapchain_desc.OutputWindow = hwnd;
    swapchain_desc.Windowed = TRUE;
    swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT::DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapchain_desc.Flags = DXGI_SWAP_CHAIN_FLAG::DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    IDXGISwapChain * swapchain = nullptr;
    res = dxgi_factory->CreateSwapChain(d3d_cmd_q, &swapchain_desc, &swapchain);
    CHECK_AND_FAIL(res);

    IDXGISwapChain3 * d3d_swapchain = nullptr;
    res = swapchain->QueryInterface(__uuidof(IDXGISwapChain3), (void**)&d3d_swapchain);
    CHECK_AND_FAIL(res);
    UINT frame_index = d3d_swapchain->GetCurrentBackBufferIndex();
    ::printf("The current frame index is %d\n", frame_index);

    // Create Render Target View Descriptor Heap
    D3D12_DESCRIPTOR_HEAP_DESC descriptor_heap_desc = {};
    descriptor_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    descriptor_heap_desc.NumDescriptors = frame_count;
    descriptor_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAGS::D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    ID3D12DescriptorHeap * d3d_heap = nullptr;
    res = d3d_device->CreateDescriptorHeap(&descriptor_heap_desc, IID_PPV_ARGS(&d3d_heap));
    CHECK_AND_FAIL(res);

    rtv_descriptor_size = d3d_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    ::printf("size of rtv descriptor heap (to increment handle): %d\n", rtv_descriptor_size);

    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle_start = d3d_heap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < frame_count; ++i) {
        res = d3d_swapchain->GetBuffer(i, IID_PPV_ARGS(&render_targets[i]));
        CHECK_AND_FAIL(res);
        D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = {};
        cpu_handle.ptr = rtv_handle_start.ptr + ((UINT64)i * rtv_descriptor_size);
        d3d_device->CreateRenderTargetView(render_targets[i], nullptr, cpu_handle);
        ::printf("render target %d width = %d, height = %d\n", i, (UINT)render_targets[i]->GetDesc().Width, (UINT)render_targets[i]->GetDesc().Height);
    }

    // Create command allocator
    ID3D12CommandAllocator * d3d_cmd_allocator = nullptr;
    res = d3d_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&d3d_cmd_allocator));
    CHECK_AND_FAIL(res);

    // Create empty root signature
    D3D12_ROOT_SIGNATURE_DESC root_desc = {};
    root_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ID3DBlob * signature = nullptr;
    ID3DBlob * error_blob = nullptr;
    res = D3D12SerializeRootSignature(&root_desc, D3D_ROOT_SIGNATURE_VERSION::D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error_blob);
    CHECK_AND_FAIL(res);

    ID3D12RootSignature * root_signature = nullptr;
    res = d3d_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_signature));
    if (S_OK != res) {
        ::printf("Could not create empty root signature!\n");
        ::abort();
    }

    // Load and compile shaders
    wchar_t const * shaders_path = L"./shaders/basic.hlsl";
    ID3DBlob * vertex_shader = nullptr;
    ID3DBlob * vs_err = nullptr;
    ID3DBlob * pixel_shader = nullptr;
    ID3DBlob * ps_err = nullptr;
    res = D3DCompileFromFile(shaders_path, nullptr, nullptr, "VSMain", "vs_5_0", compiler_flags, 0, &vertex_shader, &vs_err);
    if (FAILED(res)) {
        if (vs_err) {
            OutputDebugStringA((char *)vs_err->GetBufferPointer());
            vs_err->Release();
        } else {
            ::printf("could not load/compile shader\n");
        }
    }
    res = D3DCompileFromFile(shaders_path, nullptr, nullptr, "PSMain", "ps_5_0", compiler_flags, 0, &pixel_shader, &ps_err);
    if (FAILED(res)) {
        if (ps_err) {
            OutputDebugStringA((char *)ps_err->GetBufferPointer());
            ps_err->Release();
        }
    }

    // cleanup
    root_signature->Release();
    if (error_blob)
        error_blob->Release();
    signature->Release();
    d3d_swapchain->Release();
    swapchain->Release();
    d3d_cmd_q->Release();
    d3d_device->Release();
    dxgi_factory->Release();
    debug_interface_dx->Release();


    // Loop 
    /*
    * 
        // Wait for fences
        CHECK(YRB::WaitForFences(&fences_framedone[frame_index], 1));

        - SwapChain -> Give me the next image to render to.

        - Render to Image

        - Present SwapChain Image
    */


    // Other stuff -> Shaders, DescriptorManagement (DescriptorHeap, RootSignature), PSO, Sync Objects, Buffers, Textures, ...
    
    return 0;
}