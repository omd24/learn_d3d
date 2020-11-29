#include <stdio.h>

// DirectX 12 specific headers.
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

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

#include <dxgidebug.h>
//#include <directxmath.h>

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
    UINT dxgiFactoryFlags = 0;
#if ENABLE_DEBUG_LAYER > 0
    ID3D12Debug * debug_interface_dx = nullptr;
    //ID3D12Debug1 * debug_controller = nullptr;
    if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_interface_dx)))) {
        debug_interface_dx->EnableDebugLayer();
        //debug_interface_dx->QueryInterface(IID_PPV_ARGS(&debug_controller));
        //debug_controller->SetEnableGPUBasedValidation(true); // -- not needed for now
        dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
#endif
    // Create a Window
    SDL_Window * wnd = SDL_CreateWindow("LearningD3D12", 0, 0, 1280, 720, 0);
    if(nullptr == wnd) {
        ::abort();
    }

    // Query Adapter (PhysicalDevice)
    IDXGIFactory * dxgi_factory = nullptr;
    CHECK_AND_FAIL(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgi_factory)));

    constexpr uint32_t MaxAdapters = 8;
    IDXGIAdapter * adapters[MaxAdapters] = {};
    IDXGIAdapter * pAdapter;
    for (UINT i = 0; dxgi_factory->EnumAdapters(i, &pAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
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

    // Release adaptors
    for (unsigned i = 0; i < MaxAdapters; ++i) {
        if (adapters[i] != nullptr) {
            adapters[i]->Release();
        }
    }

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

    // -- to get current backbuffer index
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
    ID3DBlob * signature_error_blob = nullptr;
    res = D3D12SerializeRootSignature(&root_desc, D3D_ROOT_SIGNATURE_VERSION::D3D_ROOT_SIGNATURE_VERSION_1, &signature, &signature_error_blob);
    CHECK_AND_FAIL(res);

    ID3D12RootSignature * root_signature = nullptr;
    res = d3d_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_signature));
    CHECK_AND_FAIL(res);

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

    // Create vertex-input-layout Elements
    D3D12_INPUT_ELEMENT_DESC input_desc [2];
    input_desc[0] = {};
    input_desc[0].SemanticName = "POSITION";
    input_desc[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    input_desc[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    
    input_desc[1] = {};
    input_desc[1].SemanticName = "COLOR";
    input_desc[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    input_desc[1].AlignedByteOffset = 12; //?
    input_desc[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

    // Create pipeline state object

    D3D12_BLEND_DESC blend_desc = {};
    blend_desc.AlphaToCoverageEnable                    = FALSE;
    blend_desc.IndependentBlendEnable                   = FALSE;
    blend_desc.RenderTarget[0].BlendEnable              = FALSE;
    blend_desc.RenderTarget[0].LogicOpEnable            = FALSE;
    blend_desc.RenderTarget[0].SrcBlend                 = D3D12_BLEND_ONE;
    blend_desc.RenderTarget[0].DestBlend                = D3D12_BLEND_ZERO;
    blend_desc.RenderTarget[0].BlendOp                  = D3D12_BLEND_OP_ADD;
    blend_desc.RenderTarget[0].SrcBlendAlpha            = D3D12_BLEND_ONE;
    blend_desc.RenderTarget[0].DestBlendAlpha           = D3D12_BLEND_ZERO;
    blend_desc.RenderTarget[0].BlendOpAlpha             = D3D12_BLEND_OP_ADD;
    blend_desc.RenderTarget[0].LogicOp                  = D3D12_LOGIC_OP_NOOP;
    blend_desc.RenderTarget[0].RenderTargetWriteMask    = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_RASTERIZER_DESC rasterizer_desc = {};
    rasterizer_desc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizer_desc.CullMode = D3D12_CULL_MODE_BACK;
    rasterizer_desc.FrontCounterClockwise = false;
    rasterizer_desc.DepthClipEnable = TRUE;
    rasterizer_desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
    pso_desc.pRootSignature = root_signature;
    pso_desc.VS.pShaderBytecode = vertex_shader->GetBufferPointer();
    pso_desc.VS.BytecodeLength = vertex_shader->GetBufferSize();
    pso_desc.PS.pShaderBytecode = pixel_shader->GetBufferPointer();
    pso_desc.PS.BytecodeLength = pixel_shader->GetBufferSize();
    pso_desc.BlendState = blend_desc;
    pso_desc.SampleMask = UINT_MAX;
    pso_desc.RasterizerState = rasterizer_desc;
    pso_desc.DepthStencilState.StencilEnable = FALSE;
    pso_desc.DepthStencilState.DepthEnable = FALSE;
    pso_desc.InputLayout.pInputElementDescs = input_desc;
    pso_desc.InputLayout.NumElements = _countof(input_desc);
    pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE::D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso_desc.NumRenderTargets = 1;
    pso_desc.RTVFormats[0] = DXGI_FORMAT::DXGI_FORMAT_B8G8R8A8_UNORM;
    pso_desc.SampleDesc.Count = 1;
    pso_desc.SampleDesc.Quality = 0;

    ID3D12PipelineState * d3d_pso = nullptr;
    res = d3d_device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&d3d_pso));
    CHECK_AND_FAIL(res);

    // Create command list
    ID3D12GraphicsCommandList * direct_cmd_list = nullptr;
    res = d3d_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, d3d_cmd_allocator, d3d_pso, IID_PPV_ARGS(&direct_cmd_list));
    CHECK_AND_FAIL(res);

    // -- close command list for now (nothing to record yet)
    CHECK_AND_FAIL(direct_cmd_list->Close());

    // Create vertex buffer (VB)

    // vertex data
    struct Vertex {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT4 color;
    };
    float aspect_ratio = (float)width / (float)height;

    Vertex v_red = {};
    v_red.position.x = 0.0f;
    v_red.position.y = 0.25f * aspect_ratio;
    v_red.position.z = 0.0f;
    v_red.color.x = 1.0f;
    v_red.color.y = 0.0f;
    v_red.color.z = 0.0f;
    v_red.color.w = 1.0f;

    Vertex v_green = {};
    v_green.position.x = 0.25f;
    v_green.position.y = -0.25f * aspect_ratio;
    v_green.position.z = 0.0f;
    v_green.color.x = 0.0f;
    v_green.color.y = 1.0f;
    v_green.color.z = 0.0f;
    v_green.color.w = 1.0f;

    Vertex v_blue = {};
    v_blue.position.x = -0.25f;
    v_blue.position.y = -0.25f * aspect_ratio;
    v_blue.position.z = 0.0f;
    v_blue.color.x = 0.0f;
    v_blue.color.y = 0.0f;
    v_blue.color.z = 1.0f;
    v_blue.color.w = 1.0f;

    Vertex vertices[] = {v_red, v_green, v_blue};
    size_t vb_size = sizeof(vertices);

    // NOTE(omid): An upload heap is used here for code simplicity 
    //      and because there are very few verts to actually transfer.
    // NOTE(omid): using upload heaps to transfer static data such as vb(s) is not recommended.
    //      Every time the GPU needs it, the upload heap will be marshalled over. 
    //      Read up on Default Heap usage.

    D3D12_HEAP_PROPERTIES heap_props = {};
    heap_props.Type = D3D12_HEAP_TYPE_UPLOAD;
    heap_props.CreationNodeMask = 1U;
    heap_props.VisibleNodeMask = 1U;

    D3D12_RESOURCE_DESC rsc_desc = {};
    rsc_desc.Dimension = D3D12_RESOURCE_DIMENSION::D3D12_RESOURCE_DIMENSION_BUFFER;
    rsc_desc.Width = vb_size;
    rsc_desc.Height = 1;
    rsc_desc.DepthOrArraySize = 1;
    rsc_desc.MipLevels = 1;
    rsc_desc.SampleDesc.Count = 1;
    rsc_desc.SampleDesc.Quality = 0;
    rsc_desc.Layout = D3D12_TEXTURE_LAYOUT::D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ID3D12Resource * vertex_buffer = nullptr;
    res = d3d_device->CreateCommittedResource(
        &heap_props, D3D12_HEAP_FLAG_NONE, &rsc_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(&vertex_buffer)
    );
    CHECK_AND_FAIL(res);

    // Copy vertex data to vertex buffer
    uint8_t * vertex_data = nullptr;
    D3D12_RANGE mem_range = {};
    mem_range.Begin = mem_range.End = 0; // We do not intend to read from this resource on the CPU.
    vertex_buffer->Map(0, &mem_range, reinterpret_cast<void **>(&vertex_data));
    memcpy(vertex_data, vertices, vb_size);
    vertex_buffer->Unmap(0, nullptr /*aka full-range*/);

    // Initialize the vertex buffer view (vbv)
    D3D12_VERTEX_BUFFER_VIEW vbv = {};
    vbv.BufferLocation = vertex_buffer->GetGPUVirtualAddress();
    vbv.SizeInBytes = vb_size;
    vbv.StrideInBytes = sizeof(Vertex);

    // Create fence
    
    // create synchronization objects and wait until assets have been uploaded to the GPU.

    ID3D12Fence * d3d_fence = nullptr;
    res = d3d_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&d3d_fence));
    CHECK_AND_FAIL(res);

    UINT64 fence_value = 1;

    // Create an event handle to use for frame synchronization.
    HANDLE fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if(nullptr == fence_event) {
        // map the error code to an HRESULT value.
        res = HRESULT_FROM_WIN32(GetLastError());
        CHECK_AND_FAIL(res);
    }


    // NOTE(omid):  We wait for the command list to execute; we are reusing the same command 
    //              list in our main loop but for now, we just want to wait for setup to 
    //              complete before continuing.

    // -- Caveat emptor:
    // NOTE(omid):  WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
    //              This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
    //              sample illustrates how to use fences for efficient resource usage and to
    //              maximize GPU utilization.

    // -- 1. signal and increment the fence value:
    UINT64 fence = fence_value;
    res = d3d_cmd_q->Signal(d3d_fence, fence);
    CHECK_AND_FAIL(res);
    ++fence_value;

    // -- 2. wait until the previous frame is finished
    if (d3d_fence->GetCompletedValue() < fence) {
        res = d3d_fence->SetEventOnCompletion(fence, fence_event);
        CHECK_AND_FAIL(res);
        WaitForSingleObject(fence_event, INFINITE /*return only when the object is signaled*/ );
    }

    // -- 3. update frame index
    frame_index = d3d_swapchain->GetCurrentBackBufferIndex();


    // OnUpdate()
    // -- nothing is updated


    // OnRender
    // NOTE(omid):  Rendering involves
    //              populating the command list, 
    //              then the command list can be executed and 
    //              then next buffer in the swap chain is presented (present the frame),

    // OnDestroy
    //              wait for the gpu to finish (to be done with all resources)
    //              close handle (fence_event)


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



    // -- Cleanup

    d3d_fence->Release();

    vertex_buffer->Release();

    direct_cmd_list->Release();
    d3d_pso->Release();

    pixel_shader->Release();
    vertex_shader->Release();

    root_signature->Release();
    if (signature_error_blob)
        signature_error_blob->Release();
    signature->Release();

    d3d_cmd_allocator->Release();

    for (unsigned i = 0; i < frame_count; ++i) {
        render_targets[i]->Release();
    }

    d3d_heap->Release();

    d3d_swapchain->Release();
    swapchain->Release();
    d3d_cmd_q->Release();
    d3d_device->Release();
    dxgi_factory->Release();

    debug_interface_dx->Release();

    // -- advanced debugging and reporting live objects [from https://walbourn.github.io/dxgi-debug-device/]

    typedef HRESULT (WINAPI * LPDXGIGETDEBUGINTERFACE)(REFIID, void ** );

    //HMODULE dxgidebug_dll = LoadLibraryEx( L"dxgidebug_dll.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32 );
    HMODULE dxgidebug_dll = LoadLibrary(L"DXGIDebug.dll");
    if (dxgidebug_dll) {
        auto dxgiGetDebugInterface = reinterpret_cast<LPDXGIGETDEBUGINTERFACE>(
            reinterpret_cast<void*>(GetProcAddress(dxgidebug_dll, "DXGIGetDebugInterface")));

        // -- working with dxgi_info_queue
        /*
        IDXGIInfoQueue * dxgiInfoQueue = nullptr;
        DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiInfoQueue)); 
        if (dxgiInfoQueue) {
            dxgiInfoQueue->SetBreakOnSeverity( DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true );
            dxgiInfoQueue->SetBreakOnSeverity( DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true );
        }
        */

        IDXGIDebug1 * dxgi_debugger = nullptr;
        DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgi_debugger));
        dxgi_debugger->ReportLiveObjects(
            DXGI_DEBUG_ALL,
            DXGI_DEBUG_RLO_DETAIL
            /* DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL) */
        );
        dxgi_debugger->Release();
        FreeLibrary(dxgidebug_dll);
    }

    SDL_DestroyWindow(wnd);
    SDL_Quit();
    
    return 0;
}