#include "framework.h"
#include "Desktop Snake.h"

constexpr static auto MAX_LOADSTRING = 128, WM_TRAYICON = WM_USER;

static HINSTANCE hInst;

static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_TRAYICON:
		if (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP)
		{
			SetForegroundWindow(hWnd);
			POINT pt;
			GetCursorPos(&pt);
			TrackPopupMenu(GetSubMenu(LoadMenu(hInst, MAKEINTRESOURCE(IDC_DESKTOPSNAKE)), 0), 0, pt.x, pt.y, 0, hWnd, nullptr);
			return 0;
		}
		return DefWindowProc(hWnd, message, wParam, lParam);
	case WM_COMMAND:
		if (LOWORD(wParam) == IDM_EXIT)
		{
			DestroyWindow(hWnd);
			return 0;
		}
		return DefWindowProc(hWnd, message, wParam, lParam);
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
}

static void Move()
{
	if (IShellWindows* psw; SUCCEEDED(CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&psw))))
	{
		VARIANT varLoc{ VT_EMPTY };
		long hwnd;
		IDispatch* pdisp;
		assert(psw->FindWindowSW(&varLoc, &varLoc, SWC_DESKTOP, &hwnd, SWFO_NEEDDISPATCH, &pdisp) == S_OK);
		IShellBrowser* psb;
		assert(SUCCEEDED(IUnknown_QueryService(pdisp, SID_STopLevelBrowser, IID_PPV_ARGS(&psb))));
		IShellView* psv;
		assert(SUCCEEDED(psb->QueryActiveShellView(&psv)));
		HWND hWnd;
		assert(SUCCEEDED(psv->GetWindow(&hWnd)));
		RECT rect;
		assert(GetClientRect(hWnd, &rect));
		UINT dpi = GetDpiForWindow(hWnd), width = rect.right * dpi / 96, height = rect.bottom * dpi / 96;
		IFolderView2* pfv;
		assert(SUCCEEDED(psv->QueryInterface(IID_PPV_ARGS(&pfv))));
		POINT pt;
		assert(SUCCEEDED(pfv->GetSpacing(&pt)));
	}
}

static void Control()
{

}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
	hInst = hInstance;
	WCHAR szTitle[MAX_LOADSTRING], szWindowClass[MAX_LOADSTRING];
	my_assert(LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING) > 0);
	my_assert(LoadString(hInstance, IDC_DESKTOPSNAKE, szWindowClass, MAX_LOADSTRING) > 0);
	HICON hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DESKTOPSNAKE));
	my_assert(hIcon);
	{
		HANDLE hMutex = CreateMutex(nullptr, TRUE, szWindowClass);
		my_assert(hMutex);
		if (GetLastError() == ERROR_ALREADY_EXISTS)
			return 0;
	}
	{
		WNDCLASSW wcex{ .lpfnWndProc = WndProc, .hInstance = hInstance, .lpszClassName = szWindowClass };
		my_assert(RegisterClass(&wcex));
	}
	{
		HWND hWnd = CreateWindow(szWindowClass, nullptr, 0, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);
		my_assert(hWnd);
		NOTIFYICONDATA nid{
			.cbSize = sizeof(NOTIFYICONDATA),
			.hWnd = hWnd,
			.uID = 0,
			.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP,
			.uCallbackMessage = WM_TRAYICON,
			.hIcon = hIcon
		};
		std::memcpy(nid.szTip, szTitle, sizeof(nid.szTip));
		assert(Shell_NotifyIcon(NIM_ADD, &nid));
	}
	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return (int)msg.wParam;
}
