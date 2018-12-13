namespace Lib
{
namespace Priv
{

static LRESULT CALLBACK
ProcessWindowMessage(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    ImGuiIO& Io = ImGui::GetIO();

    switch (Message)
    {
    case WM_LBUTTONDOWN:
        Io.MouseDown[0] = true;
        return 0;
    case WM_LBUTTONUP:
        Io.MouseDown[0] = false;
        return 0;
    case WM_RBUTTONDOWN:
        Io.MouseDown[1] = true;
        return 0;
    case WM_RBUTTONUP:
        Io.MouseDown[1] = false;
        return 0;
    case WM_MBUTTONDOWN:
        Io.MouseDown[2] = true;
        return 0;
    case WM_MBUTTONUP:
        Io.MouseDown[2] = false;
        return 0;
    case WM_MOUSEWHEEL:
        Io.MouseWheel += GET_WHEEL_DELTA_WPARAM(WParam) > 0 ? 1.0f : -1.0f;
        return 0;
    case WM_MOUSEMOVE:
        Io.MousePos.x = (signed short)(LParam);
        Io.MousePos.y = (signed short)(LParam >> 16);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_KEYDOWN:
        {
            if (WParam < 256)
            {
                Io.KeysDown[WParam] = true;
                if (WParam == VK_ESCAPE)
                    PostQuitMessage(0);
                return 0;
            }
        }
        break;
    case WM_KEYUP:
        {
            if (WParam < 256)
            {
                Io.KeysDown[WParam] = false;
                return 0;
            }
        }
        break;
    case WM_CHAR:
        {
            if (WParam > 0 && WParam < 0x10000)
            {
                Io.AddInputCharacter((unsigned short)WParam);
                return 0;
            }
        }
        break;
    }
    return DefWindowProc(Window, Message, WParam, LParam);
}

} // namespace Priv

static std::vector<uint8_t>
LoadFile(const char* FileName)
{
    FILE* File = fopen(FileName, "rb");
    assert(File);
    fseek(File, 0, SEEK_END);
    long Size = ftell(File);
    assert(Size != -1);
    std::vector<uint8_t> Content(Size);
    fseek(File, 0, SEEK_SET);
    fread(&Content[0], 1, Content.size(), File);
    fclose(File);
    return Content;
}

static double
GetTime()
{
    static LARGE_INTEGER StartCounter;
    static LARGE_INTEGER Frequency;
    if (StartCounter.QuadPart == 0)
    {
        QueryPerformanceFrequency(&Frequency);
        QueryPerformanceCounter(&StartCounter);
    }
    LARGE_INTEGER Counter;
    QueryPerformanceCounter(&Counter);
    return (Counter.QuadPart - StartCounter.QuadPart) / (double)Frequency.QuadPart;
}

static void
UpdateFrameStats(HWND Window, const char* Name, double& OutTime, float& OutDeltaTime)
{
    static double PreviousTime = -1.0;
    static double HeaderRefreshTime = 0.0;
    static unsigned FrameCount = 0;

    if (PreviousTime < 0.0)
    {
        PreviousTime = GetTime();
        HeaderRefreshTime = PreviousTime;
    }

    OutTime = GetTime();
    OutDeltaTime = (float)(OutTime - PreviousTime);
    PreviousTime = OutTime;

    if ((OutTime - HeaderRefreshTime) >= 1.0)
    {
        const double FramesPerSecond = FrameCount / (OutTime - HeaderRefreshTime);
        const double MilliSeconds = (1.0 / FramesPerSecond) * 1000.0;
        char Header[256];
        snprintf(Header, sizeof(Header), "[%.1f fps  %.3f ms] %s", FramesPerSecond, MilliSeconds, Name);
        SetWindowText(Window, Header);
        HeaderRefreshTime = OutTime;
        FrameCount = 0;
    }
    FrameCount++;
}

static HWND
InitializeWindow(const char* Name, unsigned Width, unsigned Height)
{
    WNDCLASS WinClass = {};
    WinClass.lpfnWndProc = Priv::ProcessWindowMessage;
    WinClass.hInstance = GetModuleHandle(nullptr);
    WinClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    WinClass.lpszClassName = Name;
    if (!RegisterClass(&WinClass))
        assert(0);

    RECT Rect = { 0, 0, (LONG)Width, (LONG)Height };
    if (!AdjustWindowRect(&Rect, WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX, 0))
        assert(0);

    HWND Window = CreateWindowEx(
        0, Name, Name, WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        Rect.right - Rect.left, Rect.bottom - Rect.top,
        nullptr, nullptr, nullptr, 0);
    assert(Window);
    return Window;
}

} // namespace Lib
// vim: set ts=4 sw=4 expandtab:
