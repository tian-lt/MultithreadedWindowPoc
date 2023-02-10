// clang-format off
#include <Windows.h>
#include <CommCtrl.h>
#include <Richedit.h>
// clang-format on

#include <string_view>
#include <thread>

LRESULT CALLBACK MainProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
constexpr std::wstring_view mainclass = L"main-window-class\0";

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

int APIENTRY wWinMain(_In_ HINSTANCE hinst, _In_opt_ HINSTANCE, _In_ LPWSTR,
                      _In_ int cmdshow) {
  LoadLibraryW(L"Msftedit.dll");
  RegMainWndClass(hinst);
  auto main_hwnd = CreateWindowExW(
      0, mainclass.data(), L"main window", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
      0, CW_USEDEFAULT, 0, nullptr, nullptr, hinst, nullptr);
  auto edit1 = CreateWindowExW(
      0, MSFTEDIT_CLASS, L"RichEdit 1, same thread",
      ES_MULTILINE | WS_VISIBLE | WS_CHILD | WS_BORDER | WS_TABSTOP, 10, 10,
      200, 300, main_hwnd, nullptr, hinst, nullptr);
  auto text_thread = [main_hwnd, hinst] {
    SetWindowLongPtrW(main_hwnd, GWLP_USERDATA,
                      static_cast<LONG_PTR>(GetCurrentThreadId()));
    auto edit2 = CreateWindowExW(
        0, MSFTEDIT_CLASS, L"RichEdit 2, different thread",
        ES_MULTILINE | WS_VISIBLE | WS_CHILD | WS_BORDER | WS_TABSTOP, 220, 10,
        200, 300, main_hwnd, nullptr, hinst, nullptr);
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
      if (msg.message == WM_QUIT) break;
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  };
  ShowWindow(main_hwnd, cmdshow);
  std::jthread t{text_thread};
  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  return 0;
}

LRESULT CALLBACK MainProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  switch (msg) {
    case WM_CLOSE:
      PostThreadMessageW(
          static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWLP_USERDATA)), WM_QUIT,
          0, 0);
      DestroyWindow(hwnd);
      break;
    case WM_DESTROY:
      PostQuitMessage(0);
      break;
    default:
      return DefWindowProcW(hwnd, msg, wparam, lparam);
  }
  return 0;
}
