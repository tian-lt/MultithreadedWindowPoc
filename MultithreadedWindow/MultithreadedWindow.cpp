// MultithreadedWindow.cpp : Defines the entry point for the application.
//

#include "MultithreadedWindow.h"

#include <cassert>
#include <format>
#include <latch>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "framework.h"

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                      // current instance
WCHAR szTitle[MAX_LOADSTRING];        // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];  // the main window class name
HWND childhwnd;
HWND tophwnds[2];
std::latch all_done{3};

// Forward declarations of functions included in this code module:
void MyRegisterClass(HINSTANCE hInstance);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK ChildProc(HWND, UINT, WPARAM, LPARAM);

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

constexpr unsigned TM_BASE = WM_USER + 1000;
constexpr unsigned TM_1 = TM_BASE + 1;
constexpr unsigned TM_2 = TM_BASE + 2;
constexpr unsigned TM_WID = TM_BASE + 1000;
constexpr unsigned TM_CLOSE = TM_BASE + 1001;

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
      SetParent(childhwnd, tophwnds[1]);
      PostMessage(childhwnd, TM_WID, 0, 1);
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

  auto topwnd_proc = [&](int wid) {
    tophwnds[wid] = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
                                  CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr,
                                  nullptr, hInstance, nullptr);
    if (!tophwnds[wid]) {
      throw "failed to create top window 0.";
    }
    ShowWindow(tophwnds[wid], nCmdShow);
    UpdateWindow(tophwnds[wid]);
    PostMessage(tophwnds[wid], TM_WID, 0, wid);

    if (wid == 0) {
      auto childwnd_proc = [hInst = hInst](int wid) {
        assert(wid == 0);
        childhwnd = CreateWindowEx(0, TEXT("ChildWndClass"), L"",
                                   WS_CHILD | WS_BORDER, 0, 0, 100, 100,
                                   tophwnds[wid], nullptr, hInst, nullptr);
        if (!childhwnd) {
          throw "failed to create the childwindow.";
        }

        ShowWindow(childhwnd, SW_SHOWDEFAULT);
        UpdateWindow(childhwnd);
        PostMessage(childhwnd, TM_WID, 0, wid);

        HACCEL hAccelTable =
            LoadAccelerators(hInst, MAKEINTRESOURCE(IDC_MULTITHREADEDWINDOW));
        MSG msg;
        while (GetMessage(&msg, nullptr, 0, 0)) {
          if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
            dump_msg(msg.hwnd, msg.message,
                     std::format("childwnd_proc[{}]", wid));
            TranslateMessage(&msg);
            DispatchMessage(&msg);
          }
        }
        all_done.count_down();
      };
      std::thread{childwnd_proc, 0}.detach();
    }

    HACCEL hAccelTable =
        LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_MULTITHREADEDWINDOW));
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
      if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
        dump_msg(msg.hwnd, msg.message, std::format("topwnd_proc[{}]", wid));
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
    }
    all_done.count_down();
  };

  std::thread{topwnd_proc, 0}.detach();
  std::thread{topwnd_proc, 1}.detach();
  all_done.wait();
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam,
                         LPARAM lParam) {
  static thread_local int wid = -1;
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
    case TM_WID:
      wid = static_cast<int>(lParam);
      break;
    case WM_DESTROY: {
      PostMessage(childhwnd, TM_CLOSE, 0, wid);
      PostQuitMessage(0);
    } break;
    default:
      return DefWindowProc(hWnd, message, wParam, lParam);
  }
  return 0;
}

LRESULT CALLBACK ChildProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  dump_msg(hwnd, msg, "ChildProcCallback");
  static thread_local int wid = -1;
  switch (msg) {
    case TM_WID:
      wid = static_cast<int>(lparam);
      break;
    case WM_LBUTTONUP: {
      auto tid = GetCurrentThreadId();
      OutputDebugStringA(
          std::format("ChildProcCallback: WM_LBUTTONUP, tid = {}\n", tid)
              .c_str());
    } break;
    case TM_CLOSE: {
      auto wid_from = static_cast<int>(lparam);
      if (wid_from == wid) {
        OutputDebugStringA("The childwnd_proc is going to die.\n");
        PostQuitMessage(0);
      }
    } break;
    default:
      return DefWindowProc(hwnd, msg, wparam, lparam);
  }
  return 0;
}
