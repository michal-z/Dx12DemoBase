namespace Dx
{
namespace Priv
{

struct TDescriptorHeap
{
    ID3D12DescriptorHeap* Heap;
    D3D12_CPU_DESCRIPTOR_HANDLE CpuStart;
    D3D12_GPU_DESCRIPTOR_HANDLE GpuStart;
    unsigned Size;
    unsigned Capacity;
};

struct TGpuMemoryHeap
{
    ID3D12Resource* Heap;
    uint8_t* CpuStart;
    D3D12_GPU_VIRTUAL_ADDRESS GpuStart;
    unsigned Size;
    unsigned Capacity;
};

static TDescriptorHeap GRenderTargetHeap;
static TDescriptorHeap GDepthStencilHeap;

// shader visible descriptor heaps
static TDescriptorHeap GShaderVisibleHeaps[2];

// non-shader visible descriptor heap
static TDescriptorHeap GNonShaderVisibleHeap;

static TGpuMemoryHeap GUploadMemoryHeaps[2];

static IDXGISwapChain3* GSwapChain;
static ID3D12Resource* GSwapBuffers[4];
static ID3D12Fence* GFrameFence;
static HANDLE GFrameFenceEvent;

static uint64_t GFrameCount;
static unsigned GBackBufferIndex;


static TDescriptorHeap&
GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE Type, D3D12_DESCRIPTOR_HEAP_FLAGS Flags, unsigned& OutDescriptorSize)
{
    if (Type == D3D12_DESCRIPTOR_HEAP_TYPE_RTV)
    {
        OutDescriptorSize = GDescriptorSizeRtv;
        return GRenderTargetHeap;
    }
    else if (Type == D3D12_DESCRIPTOR_HEAP_TYPE_DSV)
    {
        OutDescriptorSize = GDescriptorSizeRtv;
        return GDepthStencilHeap;
    }
    else if (Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
    {
        OutDescriptorSize = GDescriptorSize;
        if (Flags == D3D12_DESCRIPTOR_HEAP_FLAG_NONE)
            return GNonShaderVisibleHeap;
        else if (Flags == D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
            return GShaderVisibleHeaps[GFrameIndex];
    }
    assert(0);
    OutDescriptorSize = 0;
    return GNonShaderVisibleHeap;
}

} // namespace Priv

static void
AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE Type, unsigned Count, D3D12_CPU_DESCRIPTOR_HANDLE& OutFirst)
{
    unsigned DescriptorSize;
    Priv::TDescriptorHeap& DescriptorHeap = Priv::GetDescriptorHeap(Type, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, DescriptorSize);

    assert((DescriptorHeap.Size + Count) < DescriptorHeap.Capacity);

    OutFirst.ptr = DescriptorHeap.CpuStart.ptr + DescriptorHeap.Size * DescriptorSize;

    DescriptorHeap.Size += Count;
}

static void
AllocateGpuDescriptors(unsigned Count, D3D12_CPU_DESCRIPTOR_HANDLE& OutFirstCpu, D3D12_GPU_DESCRIPTOR_HANDLE& OutFirstGpu)
{
    unsigned DescriptorSize;
    Priv::TDescriptorHeap& DescriptorHeap = Priv::GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                                                                    D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
                                                                    DescriptorSize);

    assert((DescriptorHeap.Size + Count) < DescriptorHeap.Capacity);

    OutFirstCpu.ptr = DescriptorHeap.CpuStart.ptr + DescriptorHeap.Size * DescriptorSize;
    OutFirstGpu.ptr = DescriptorHeap.GpuStart.ptr + DescriptorHeap.Size * DescriptorSize;

    DescriptorHeap.Size += Count;
}

static D3D12_GPU_DESCRIPTOR_HANDLE
CopyDescriptorsToGpu(unsigned Count, D3D12_CPU_DESCRIPTOR_HANDLE Source)
{
    D3D12_CPU_DESCRIPTOR_HANDLE DestinationCpu;
    D3D12_GPU_DESCRIPTOR_HANDLE DestinationGpu;
    AllocateGpuDescriptors(Count, DestinationCpu, DestinationGpu);

    GDevice->CopyDescriptorsSimple(Count, DestinationCpu, Source, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    return DestinationGpu;
}

static inline void
GetBackBuffer(ID3D12Resource*& OutResource, D3D12_CPU_DESCRIPTOR_HANDLE& OutHandle)
{
    OutResource = Priv::GSwapBuffers[Priv::GBackBufferIndex];

    OutHandle = Priv::GRenderTargetHeap.CpuStart;
    OutHandle.ptr += Priv::GBackBufferIndex * GDescriptorSizeRtv;
}

static inline void
SetDescriptorHeap()
{
    GCmdList->SetDescriptorHeaps(1, &Priv::GShaderVisibleHeaps[GFrameIndex].Heap);
}

static void*
AllocateGpuUploadMemory(unsigned Size, D3D12_GPU_VIRTUAL_ADDRESS& OutGpuAddress)
{
    assert(Size > 0);

    if (Size & 0xff) // always align to 256 bytes
        Size = (Size + 255) & ~0xff;

    Priv::TGpuMemoryHeap& UploadHeap = Priv::GUploadMemoryHeaps[GFrameIndex];
    assert((UploadHeap.Size + Size) < UploadHeap.Capacity);

    void* CpuAddr = UploadHeap.CpuStart + UploadHeap.Size;
    OutGpuAddress = UploadHeap.GpuStart + UploadHeap.Size;

    UploadHeap.Size += Size;
    return CpuAddr;
}

static void
Initialize(HWND Window)
{
    IDXGIFactory4* Factory;
#ifdef _DEBUG
    VHR(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&Factory)));
#else
    VHR(CreateDXGIFactory2(0, IID_PPV_ARGS(&Factory)));
#endif

#ifdef _DEBUG
    {
        ID3D12Debug* Dbg;
        D3D12GetDebugInterface(IID_PPV_ARGS(&Dbg));
        if (Dbg)
        {
            Dbg->EnableDebugLayer();
            ID3D12Debug1* Dbg1;
            Dbg->QueryInterface(IID_PPV_ARGS(&Dbg1));
            if (Dbg1)
                Dbg1->SetEnableGPUBasedValidation(TRUE);
            SAFE_RELEASE(Dbg);
            SAFE_RELEASE(Dbg1);
        }
    }
#endif
    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_1, IID_PPV_ARGS(&GDevice))))
    {
        MessageBox(Window, "This application requires Windows 10 1709 (RS3) or newer.",
                   "D3D12CreateDevice failed", MB_OK | MB_ICONERROR);
        return;
    }

    D3D12_COMMAND_QUEUE_DESC CmdQueueDesc = {};
    CmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    CmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    CmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    VHR(GDevice->CreateCommandQueue(&CmdQueueDesc, IID_PPV_ARGS(&GCmdQueue)));

    DXGI_SWAP_CHAIN_DESC SwapChainDesc = {};
    SwapChainDesc.BufferCount = 4;
    SwapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    SwapChainDesc.OutputWindow = GWindow = Window;
    SwapChainDesc.SampleDesc.Count = 1;
    SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    SwapChainDesc.Windowed = TRUE;

    IDXGISwapChain* TempSwapChain;
    VHR(Factory->CreateSwapChain(GCmdQueue, &SwapChainDesc, &TempSwapChain));
    VHR(TempSwapChain->QueryInterface(IID_PPV_ARGS(&Priv::GSwapChain)));
    SAFE_RELEASE(TempSwapChain);
    SAFE_RELEASE(Factory);

    RECT Rect;
    GetClientRect(Window, &Rect);
    GResolution[0] = (unsigned)Rect.right;
    GResolution[1] = (unsigned)Rect.bottom;

    for (unsigned Index = 0; Index < 2; ++Index)
        VHR(GDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&GCmdAlloc[Index])));

    GDescriptorSize = GDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    GDescriptorSizeRtv = GDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // Render Target Descriptor Heap
    {
        Priv::GRenderTargetHeap.Size = 0;
        Priv::GRenderTargetHeap.Capacity = 16;
        Priv::GRenderTargetHeap.CpuStart.ptr = 0;
        Priv::GRenderTargetHeap.GpuStart.ptr = 0;

        D3D12_DESCRIPTOR_HEAP_DESC HeapDesc = {};
        HeapDesc.NumDescriptors = Priv::GRenderTargetHeap.Capacity;
        HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        VHR(GDevice->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&Priv::GRenderTargetHeap.Heap)));
        Priv::GRenderTargetHeap.CpuStart = Priv::GRenderTargetHeap.Heap->GetCPUDescriptorHandleForHeapStart();
    }

    // Depth Stencil Descriptor Heap
    {
        Priv::GDepthStencilHeap.Size = 0;
        Priv::GDepthStencilHeap.Capacity = 8;
        Priv::GDepthStencilHeap.CpuStart.ptr = 0;
        Priv::GDepthStencilHeap.GpuStart.ptr = 0;

        D3D12_DESCRIPTOR_HEAP_DESC HeapDesc = {};
        HeapDesc.NumDescriptors = Priv::GDepthStencilHeap.Capacity;
        HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        VHR(GDevice->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&Priv::GDepthStencilHeap.Heap)));
        Priv::GDepthStencilHeap.CpuStart = Priv::GDepthStencilHeap.Heap->GetCPUDescriptorHandleForHeapStart();
    }

    // Shader Visible Descriptor Heaps
    {
        for (unsigned Index = 0; Index < 2; ++Index)
        {
            Priv::TDescriptorHeap& Heap = Priv::GShaderVisibleHeaps[Index];

            Heap.Size = 0;
            Heap.Capacity = 10000;
            Heap.CpuStart.ptr = 0;
            Heap.GpuStart.ptr = 0;

            D3D12_DESCRIPTOR_HEAP_DESC HeapDesc = {};
            HeapDesc.NumDescriptors = Heap.Capacity;
            HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            VHR(GDevice->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&Heap.Heap)));

            Heap.CpuStart = Heap.Heap->GetCPUDescriptorHandleForHeapStart();
            Heap.GpuStart = Heap.Heap->GetGPUDescriptorHandleForHeapStart();
        }
    }

    // Non-Shader Visible Descriptor Heap
    {
        Priv::GNonShaderVisibleHeap.Size = 0;
        Priv::GNonShaderVisibleHeap.Capacity = 10000;
        Priv::GNonShaderVisibleHeap.CpuStart.ptr = 0;
        Priv::GNonShaderVisibleHeap.GpuStart.ptr = 0;

        D3D12_DESCRIPTOR_HEAP_DESC HeapDesc = {};
        HeapDesc.NumDescriptors = Priv::GNonShaderVisibleHeap.Capacity;
        HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        VHR(GDevice->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&Priv::GNonShaderVisibleHeap.Heap)));
        Priv::GNonShaderVisibleHeap.CpuStart = Priv::GNonShaderVisibleHeap.Heap->GetCPUDescriptorHandleForHeapStart();
    }

    // Upload Memory Heaps
    {
        for (unsigned Index = 0; Index < 2; ++Index)
        {
            Priv::TGpuMemoryHeap& UploadHeap = Priv::GUploadMemoryHeaps[Index];
            UploadHeap.Size = 0;
            UploadHeap.Capacity = 8*1024*1024;
            UploadHeap.CpuStart = 0;
            UploadHeap.GpuStart = 0;

            VHR(GDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
                                                 &CD3DX12_RESOURCE_DESC::Buffer(UploadHeap.Capacity),
                                                 D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                 IID_PPV_ARGS(&UploadHeap.Heap)));

            VHR(UploadHeap.Heap->Map(0, &CD3DX12_RANGE(0, 0), (void**)&UploadHeap.CpuStart));

            UploadHeap.GpuStart = UploadHeap.Heap->GetGPUVirtualAddress();
        }
    }

    // Swap buffer render targets
    {
        D3D12_CPU_DESCRIPTOR_HANDLE Handle;
        AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 4, Handle);

        for (unsigned Index = 0; Index < 4; ++Index)
        {
            VHR(Priv::GSwapChain->GetBuffer(Index, IID_PPV_ARGS(&Priv::GSwapBuffers[Index])));

            GDevice->CreateRenderTargetView(Priv::GSwapBuffers[Index], nullptr, Handle);
            Handle.ptr += GDescriptorSizeRtv;
        }
    }

    // Depth-stencil target
    {
        auto ImageDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, GResolution[0], GResolution[1]);
        ImageDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
        VHR(GDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
                                             &ImageDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE,
                                             &CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, 1.0f, 0),
                                             IID_PPV_ARGS(&GDepthBuffer)));

        AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, GDepthBufferHandle);

        D3D12_DEPTH_STENCIL_VIEW_DESC ViewDesc = {};
        ViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
        ViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        ViewDesc.Flags = D3D12_DSV_FLAG_NONE;
        GDevice->CreateDepthStencilView(GDepthBuffer, &ViewDesc, GDepthBufferHandle);
    }

    VHR(GDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, GCmdAlloc[0], nullptr, IID_PPV_ARGS(&GCmdList)));

    VHR(GDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Priv::GFrameFence)));
    Priv::GFrameFenceEvent = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
}

static void
Shutdown()
{
    // @Incomplete: Release all resources.
    SAFE_RELEASE(GCmdList);
    SAFE_RELEASE(GCmdAlloc[0]);
    SAFE_RELEASE(GCmdAlloc[1]);
    SAFE_RELEASE(Priv::GRenderTargetHeap.Heap);
    SAFE_RELEASE(Priv::GDepthStencilHeap.Heap);
    for (unsigned Index = 0; Index < 4; ++Index)
        SAFE_RELEASE(Priv::GSwapBuffers[Index]);
    CloseHandle(Priv::GFrameFenceEvent);
    SAFE_RELEASE(Priv::GFrameFence);
    SAFE_RELEASE(Priv::GSwapChain);
    SAFE_RELEASE(GCmdQueue);
    SAFE_RELEASE(GDevice);
}

static void
PresentFrame()
{
    Priv::GSwapChain->Present(0, 0);
    GCmdQueue->Signal(Priv::GFrameFence, ++Priv::GFrameCount);

    const uint64_t GpuFrameCount = Priv::GFrameFence->GetCompletedValue();

    if ((Priv::GFrameCount - GpuFrameCount) >= 2)
    {
        Priv::GFrameFence->SetEventOnCompletion(GpuFrameCount + 1, Priv::GFrameFenceEvent);
        WaitForSingleObject(Priv::GFrameFenceEvent, INFINITE);
    }

    GFrameIndex = !GFrameIndex;
    Priv::GBackBufferIndex = Priv::GSwapChain->GetCurrentBackBufferIndex();

    Priv::GShaderVisibleHeaps[GFrameIndex].Size = 0;
    Priv::GUploadMemoryHeaps[GFrameIndex].Size = 0;
}

static void
WaitForGpu()
{
    GCmdQueue->Signal(Priv::GFrameFence, ++Priv::GFrameCount);
    Priv::GFrameFence->SetEventOnCompletion(Priv::GFrameCount, Priv::GFrameFenceEvent);
    WaitForSingleObject(Priv::GFrameFenceEvent, INFINITE);
}

} // namespace Dx
// vim: set ts=4 sw=4 expandtab:
