#include <Windows.h>

#include <array>
#include <cmath>
#include <format>
#include <memory>
#include <string_view>
#include <thread>

LRESULT CALLBACK MainProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
LRESULT CALLBACK ChildProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

constexpr double pi = 3.141592654;
constexpr double halfpi = 1.5707963267;
uint8_t curve(double step_size, size_t idx, double offset) {
  auto val = (std::sin((step_size * idx) + offset) + 1.0) / 2.0 * 0xff;
  return static_cast<uint8_t>(val);
};

auto CreateBrush(size_t num, size_t idx) {
  auto step_size = pi / num;
  auto clr = static_cast<COLORREF>(curve(step_size, idx, 0) |
                                   curve(step_size, idx, pi) << 8 |
                                   curve(step_size, idx, halfpi) << 16);
  return CreateSolidBrush(clr);
}

constexpr std::wstring_view mainclass = L"main-window-class\0";
constexpr std::wstring_view childclass = L"child-window-class\0";
constexpr UINT child_timer_id = 1;
constexpr size_t color_num = 5;
const auto color_arr = [] {
  std::array<HBRUSH, color_num> results{};
  for (size_t i = 0; i < color_num; ++i) {
    results[i] = CreateBrush(color_num, i);
  }
  return results;
}();

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
  wcex.style = CS_PARENTDC;
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
    ShowWindow(child_hwnd, SW_NORMAL);
    // ShowWindow(main_hwnd, SW_NORMAL);
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
  FillRect(ctx->hdc, &rc, color_arr[ctx->color_idx]);
  ctx->color_idx = (ctx->color_idx + 1) % color_num;
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
                 rc.bottom - rc.top - 200, true);
      break;
    }
    case WM_CLOSE: {
      PostMessage(GetWindow(hwnd, GW_CHILD), WM_CLOSE, 0, 0);
      DestroyWindow(hwnd);
      break;
    }
    case WM_PAINT: {
      // Sleep(3000);
      PAINTSTRUCT ps;
      auto hdc = BeginPaint(hwnd, &ps);
      // OutputDebugStringA(std::format("MainWnd Repaints, hdc = {}\n",
      //                                reinterpret_cast<size_t>(hdc))
      //                        .c_str());
      EndPaint(hwnd, &ps);
      return 0;
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
      // Sleep(3000);
      PAINTSTRUCT ps;
      auto hdc = BeginPaint(hwnd, &ps);
      // OutputDebugStringA(std::format("ChildWnd Repaints, hdc = {}\n",
      //                                reinterpret_cast<size_t>(hdc))
      //                        .c_str());
      auto ctx = reinterpret_cast<ChildWindowContext*>(
          GetWindowLongPtr(hwnd, GWLP_USERDATA));
      RECT rc;
      GetClientRect(hwnd, &rc);
      FillRect(hdc, &rc, color_arr[ctx->color_idx]);
      ctx->color_idx = (ctx->color_idx + 1) % color_num;
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
