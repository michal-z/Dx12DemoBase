namespace Dx
{

typedef ID3D12Device3 TDevice;
typedef ID3D12GraphicsCommandList2 TGraphicsCommandList;

static TDevice* GDevice;
static ID3D12CommandQueue* GCmdQueue;
static ID3D12CommandAllocator* GCmdAlloc[2];
static TGraphicsCommandList* GCmdList;
static ID3D12Resource* GDepthBuffer;
static D3D12_CPU_DESCRIPTOR_HANDLE GDepthBufferHandle;
static HWND GWindow;
static unsigned GResolution[2];
static unsigned GDescriptorSize;
static unsigned GDescriptorSizeRtv;
static unsigned GFrameIndex;
static std::vector<ID3D12Resource*> GIntermediateResources;

static void AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE Type,
                                unsigned Count,
                                D3D12_CPU_DESCRIPTOR_HANDLE& OutFirst);
static void AllocateGpuDescriptors(unsigned Count,
                                   D3D12_CPU_DESCRIPTOR_HANDLE& OutFirstCpu,
                                   D3D12_GPU_DESCRIPTOR_HANDLE& OutFirstGpu);
static void* AllocateGpuUploadMemory(unsigned Size,
                                     D3D12_GPU_VIRTUAL_ADDRESS& OutGpuAddress);

static D3D12_GPU_DESCRIPTOR_HANDLE CopyDescriptorsToGpu(unsigned Count,
                                                        D3D12_CPU_DESCRIPTOR_HANDLE Source);

static inline void GetBackBuffer(ID3D12Resource*& OutResource,
                                 D3D12_CPU_DESCRIPTOR_HANDLE& OutHandle);

static inline void SetDescriptorHeap();

static void Initialize(HWND Window);
static void Shutdown();
static void PresentFrame();
static void WaitForGpu();

} // namespace Dx

namespace Gui
{

static void Initialize();
static void Shutdown();
static void Update(float DeltaTime);
static void Render();

} // namespace Gui

namespace Lib
{

static std::vector<uint8_t> LoadFile(const char* FileName);

static double GetTime();

static void UpdateFrameStats(HWND Window,
                             const char* Name,
                             double& OutTime,
                             float& OutDeltaTime);

static HWND InitializeWindow(const char* Name,
                             unsigned Width,
                             unsigned Height);

} // namespace Lib
// vim: set ts=4 sw=4 expandtab:
