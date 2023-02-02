// clang-format off
#include <Windows.h>
#include <CommCtrl.h>
// clang-format on

#include <array>
#include <cmath>
#include <format>
#include <memory>
#include <string_view>
#include <thread>

LRESULT CALLBACK MainProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
LRESULT CALLBACK ChildProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
LRESULT CALLBACK ButtonSubclass(HWND hwnd, UINT msg, WPARAM wparam,
                                LPARAM lparam, UINT_PTR, DWORD_PTR);
constexpr double pi = 3.141592654;
constexpr double halfpi = 1.5707963267;
uint8_t Curve(double step_size, size_t idx, double offset) {
  auto val = (std::sin((step_size * idx) + offset) + 1.0) / 2.0 * 0xff;
  return static_cast<uint8_t>(val);
};

auto CreateBrush(size_t num, size_t idx) {
  auto step_size = pi / num;
  auto clr = static_cast<COLORREF>(Curve(step_size, idx, 0) |
                                   Curve(step_size, idx, pi) << 8 |
                                   Curve(step_size, idx, halfpi) << 16);
  return CreateSolidBrush(clr);
}

constexpr std::wstring_view mainclass = L"main-window-class\0";
constexpr std::wstring_view childclass = L"child-window-class\0";
constexpr UINT child_timer_id = 1;
constexpr size_t brush_num = 5;
const auto brush_arr = [] {
  std::array<HBRUSH, brush_num> results{};
  for (size_t i = 0; i < brush_num; ++i) {
    results[i] = CreateBrush(brush_num, i);
  }
  return results;
}();
const auto brush_blue = CreateSolidBrush(0x00ff8010);
const auto brush_green = CreateSolidBrush(0x0080ff10);

struct MainWindowContext {
  bool maximized = false;
};
struct ChildWindowContext {
  HWND hwnd = nullptr;
  HDC hdc = nullptr;
  int color_idx = 0;
};

void RegMainWndClass(HINSTANCE hinst) {
  WNDCLASSEX wcex = {};
  wcex.cbSize = sizeof(wcex);
  wcex.style = CS_HREDRAW | CS_VREDRAW;
  wcex.lpfnWndProc = MainProc;
  wcex.lpszClassName = mainclass.data();
  wcex.hInstance = hinst;
  wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  if (!RegisterClassExW(&wcex)) {
    throw "failed to register main class.";
  }
}

void RegChildWndClass(HINSTANCE hinst) {
  WNDCLASSEX wcex = {};
  wcex.style = CS_HREDRAW | CS_VREDRAW;
  wcex.cbSize = sizeof(wcex);
  wcex.lpfnWndProc = ChildProc;
  wcex.lpszClassName = childclass.data();
  wcex.hInstance = hinst;
  wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  if (!RegisterClassExW(&wcex)) {
    throw "failed to register child class.";
  }
}

int APIENTRY wWinMain(_In_ HINSTANCE hinst, _In_opt_ HINSTANCE, _In_ LPWSTR,
                      _In_ int cmdshow) {
  RegMainWndClass(hinst);
  RegChildWndClass(hinst);
  auto main_hwnd = CreateWindowExW(
      0, mainclass.data(), L"main window", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
      0, CW_USEDEFAULT, 0, nullptr, nullptr, hinst, nullptr);
  RECT rc;
  GetClientRect(main_hwnd, &rc);
  auto child_func = [main_hwnd, hinst, main_tid = GetCurrentThreadId(), rc] {
    auto child_hwnd = CreateWindowExW(
        0, childclass.data(), L"child window", WS_BORDER | WS_CHILD, 100, 100,
        rc.right - rc.left - 200, rc.bottom - rc.top - 200, main_hwnd, nullptr,
        hinst, nullptr);
    auto hbutton = CreateWindowExW(
        0, L"BUTTON", L"This is a button", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        10, 10, rc.right - rc.left - 300, rc.bottom - rc.top - 300, child_hwnd,
        nullptr, hinst, nullptr);
    SetWindowSubclass(hbutton, ButtonSubclass, 0, 0);
    ShowWindow(child_hwnd, SW_NORMAL);
    BringWindowToTop(child_hwnd);
    BringWindowToTop(hbutton);
    if (main_tid != GetCurrentThreadId()) {
      MSG msg;
      while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
      }
    }
  };
  // child_func();
  std::jthread child{child_func};

  ShowWindow(main_hwnd, cmdshow);
  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  return 0;
}

void ChildTick(ChildWindowContext* ctx) {
  RECT rc;
  GetClientRect(ctx->hwnd, &rc);
  FillRect(ctx->hdc, &rc, brush_arr[ctx->color_idx]);
  ctx->color_idx = (ctx->color_idx + 1) % brush_num;
}

LRESULT CALLBACK MainProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  switch (msg) {
    case WM_CREATE: {
      auto ctx = new MainWindowContext();
      SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(ctx));
      break;
    }
    case WM_KEYDOWN:
      if (wparam == VK_SPACE) {
        auto ctx = reinterpret_cast<MainWindowContext*>(
            GetWindowLongPtr(hwnd, GWLP_USERDATA));
        ShowWindow(hwnd, ctx->maximized ? SW_RESTORE : SW_SHOWMAXIMIZED);
        ctx->maximized = !ctx->maximized;
        break;
      } else if (wparam == VK_ESCAPE) {
        PostMessage(hwnd, WM_CLOSE, 0, 0);
      } else {
        return DefWindowProcW(hwnd, msg, wparam, lparam);
      }
    case WM_SIZE: {
      RECT rc;
      GetClientRect(hwnd, &rc);
      MoveWindow(GetWindow(hwnd, GW_CHILD), 100, 100, rc.right - rc.left - 200,
                 rc.bottom - rc.top - 200, false);
      MoveWindow(GetWindow(GetWindow(hwnd, GW_CHILD), GW_CHILD), 50, 50,
                 rc.right - rc.left - 300, rc.bottom - rc.top - 300, false);
      // InvalidateRect(hwnd, &rc, false);
      break;
    }
    case WM_CLOSE: {
      PostMessage(GetWindow(hwnd, GW_CHILD), WM_CLOSE, 0, 0);
      DestroyWindow(hwnd);
      break;
    }
    case WM_ERASEBKGND: {
      OutputDebugStringA("MainWnd EraseBkg\n");
      auto hdc = reinterpret_cast<HDC>(wparam);
      RECT rc;
      GetClientRect(hwnd, &rc);
      FillRect(hdc, &rc, brush_blue);
      return true;
    }
    case WM_PAINT: {
      OutputDebugStringA("MainWnd Repaints\n");
      PAINTSTRUCT ps;
      auto hdc = BeginPaint(hwnd, &ps);
      FillRect(hdc, &ps.rcPaint, brush_green);
      EndPaint(hwnd, &ps);
      return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
    case WM_DESTROY: {
      PostQuitMessage(0);
      break;
    }
    default:
      return DefWindowProcW(hwnd, msg, wparam, lparam);
  }
  return 0;
}
LRESULT CALLBACK ChildProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  switch (msg) {
    case WM_CREATE: {
      auto ctx = new ChildWindowContext();
      ctx->hwnd = hwnd;
      ctx->hdc = GetDC(hwnd);
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(ctx));
      // SetTimer(hwnd, child_timer_id, 30, nullptr);
      break;
    }
    case WM_TIMER:
      if (wparam == child_timer_id) {
        ChildTick(reinterpret_cast<ChildWindowContext*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA)));
      }
      break;
    case WM_PAINT: {
      if (InSendMessage()) {
        int c = 0;
      }
      PAINTSTRUCT ps;
      auto hdc = BeginPaint(hwnd, &ps);
      OutputDebugStringA(std::format(" ChildWnd Repaints, hdc = {}\n",
                                     reinterpret_cast<size_t>(hdc))
                             .c_str());
      auto ctx = reinterpret_cast<ChildWindowContext*>(
          GetWindowLongPtr(hwnd, GWLP_USERDATA));
      RECT rc;
      GetClientRect(hwnd, &rc);
      FillRect(hdc, &rc, brush_arr[ctx->color_idx]);
      ctx->color_idx = (ctx->color_idx + 1) % brush_num;
      EndPaint(hwnd, &ps);
      return 0;
    }
    case WM_DESTROY: {
      KillTimer(hwnd, child_timer_id);
      PostQuitMessage(0);
      break;
    }
    default:
      return DefWindowProcW(hwnd, msg, wparam, lparam);
  }
  return 0;
}

LRESULT CALLBACK ButtonSubclass(HWND hwnd, UINT msg, WPARAM wparam,
                                LPARAM lparam, UINT_PTR, DWORD_PTR) {
  switch (msg) {
    case WM_PAINT: {
      OutputDebugStringA(" Button Repaints\n");
      break;
    }
  }
  return DefSubclassProc(hwnd, msg, wparam, lparam);
}
