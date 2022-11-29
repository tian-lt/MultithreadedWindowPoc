// MultithreadedWindow.cpp : Defines the entry point for the application.
//

#include "MultithreadedWindow.h"

#include <format>
#include <mutex>
#include <string>
#include <thread>

#include "framework.h"

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                      // current instance
WCHAR szTitle[MAX_LOADSTRING];        // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];  // the main window class name
HWND childhwnd;
HWND tophwnds[2];

// Forward declarations of functions included in this code module:
void MyRegisterClass(HINSTANCE hInstance);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK ChildProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

constexpr unsigned TM_BASE = WM_USER + 1000;
constexpr unsigned TM_1 = TM_BASE + 1;
constexpr unsigned TM_2 = TM_BASE + 2;

void dump_msg(HWND hwnd, UINT msg, const std::string& tag) {
  if (msg < TM_BASE) return;
  auto parent = GetParent(childhwnd);
  auto tid = GetCurrentThreadId();
  OutputDebugStringA(
      std::format("tag = {}, tid = {}, hparent = {} hwnd = {}, msg = {}\n", tag,
                  tid, reinterpret_cast<unsigned long long>(parent),
                  reinterpret_cast<unsigned long long>(hwnd), msg)
          .c_str());
}

void test() {
  static int state = 0;
  switch (state) {
    case 0: {
      // post a test message
      PostMessage(childhwnd, TM_1, 0, 0);
      state = 1;
    } break;
    case 1: {
      // move child to window 1.
      auto from = GetWindowThreadProcessId(tophwnds[0], nullptr);
      auto to = GetWindowThreadProcessId(tophwnds[1], nullptr);
      if (!AttachThreadInput(from, to, TRUE)) {
        auto err = GetLastError();
        throw err;
      }
      SetParent(childhwnd, tophwnds[1]);
      state = 2;
    } break;
    case 2: {
      // post a test message
      PostMessage(childhwnd, TM_1, 0, 0);
    } break;
  }
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                      _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine,
                      _In_ int nCmdShow) {
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);
  hInst = hInstance;

  // TODO: Place code here.

  // Initialize global strings
  LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
  LoadStringW(hInstance, IDC_MULTITHREADEDWINDOW, szWindowClass,
              MAX_LOADSTRING);
  MyRegisterClass(hInstance);

  std::once_flag onceflag;
  auto childproc = [&] {};

  auto proc = [&](int wid) {
    tophwnds[wid] = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
                                  CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr,
                                  nullptr, hInstance, nullptr);
    if (!tophwnds[wid]) {
      throw "failed to create top window 0.";
    }

    std::call_once(onceflag, [&] {
      childhwnd =
          CreateWindowEx(0, TEXT("ChildWndClass"), L"", WS_CHILD | WS_BORDER, 0,
                         0, 100, 100, tophwnds[0], nullptr, hInst, nullptr);
      if (!childhwnd) {
        throw "failed to create the childwindow.";
      }

      ShowWindow(childhwnd, SW_SHOWDEFAULT);
      UpdateWindow(childhwnd);
    });

    ShowWindow(tophwnds[wid], nCmdShow);
    UpdateWindow(tophwnds[wid]);

    HACCEL hAccelTable =
        LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_MULTITHREADEDWINDOW));

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
      if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
        dump_msg(msg.hwnd, msg.message, std::format("Topwnd[{}]", wid));
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
    }
  };

  std::jthread t1{proc, 0};
  std::jthread t2{proc, 1};
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
void MyRegisterClass(HINSTANCE hInstance) {
  {
    WNDCLASSEXW wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MULTITHREADEDWINDOW));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_MULTITHREADEDWINDOW);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
    if (!RegisterClassExW(&wcex)) {
      throw "failed to register wndclass.";
    }
  }
  {
    WNDCLASSEX wcex = {};
    wcex.cbSize = sizeof(wcex);
    wcex.lpszClassName = TEXT("ChildWndClass");
    wcex.lpfnWndProc = static_cast<WNDPROC>(ChildProc);
    wcex.hInstance = hInstance;
    if (!RegisterClassEx(&wcex)) {
      throw "failed to register child-wndclass.";
    }
  }
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam,
                         LPARAM lParam) {
  switch (message) {
    case WM_COMMAND: {
      int wmId = LOWORD(wParam);
      // Parse the menu selections:
      switch (wmId) {
        case IDM_ABOUT:
          test();
          break;
        case IDM_EXIT:
          PostMessage(hWnd, TM_2, 0, 0);
          break;
        default:
          return DefWindowProc(hWnd, message, wParam, lParam);
      }
    } break;
    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hWnd, &ps);
      // TODO: Add any drawing code that uses hdc here...
      EndPaint(hWnd, &ps);
    } break;
    case WM_DESTROY:
      PostQuitMessage(0);
      break;
    default:
      return DefWindowProc(hWnd, message, wParam, lParam);
  }
  return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
  UNREFERENCED_PARAMETER(lParam);
  switch (message) {
    case WM_INITDIALOG:
      return (INT_PTR)TRUE;

    case WM_COMMAND:
      if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
        return (INT_PTR)TRUE;
      }
      break;
  }
  return (INT_PTR)FALSE;
}

LRESULT CALLBACK ChildProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  dump_msg(hwnd, msg, "ChildProc");
  return DefWindowProc(hwnd, msg, wparam, lparam);
}
