#include<Windows.h>
#include "Media.h"

HINSTANCE hInst;
MFCore *mf;

const UINT32 WM_NOTIFYICON = WM_USER + 1;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

int WINAPI CALLBACK wWinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine,
	_In_ int nShowCmd
)
{
	hInst = hInstance;
	BOOL isCMD = false;

	TCHAR path[MAX_PATH];
	path[0] = 0;
	if (wcslen(lpCmdLine))
	{
		isCMD = true;
		wcscpy_s(path, lpCmdLine);
	}
	else
	{
		OPENFILENAME ofn;
		ZeroMemory(&ofn, sizeof(ofn));
		ofn.lpstrFilter = L"也许能支持的视频文件\0*.asf;*.avi;*.mpg;*.mp4;*.wmv\0所有文件\0*.*\0";
		ofn.hInstance = hInst;
		ofn.lStructSize = sizeof(ofn);
		ofn.lpstrFile = path;
		ofn.nMaxFile = MAX_PATH;
		ofn.Flags = OFN_FILEMUSTEXIST;
		if (!GetOpenFileName(&ofn))
			return 0;
	}

	WNDCLASSEX wc;
	ZeroMemory(&wc, sizeof(wc));
	wc.cbSize = sizeof(wc);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInst;
	wc.lpszClassName = L"WorkerX";
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);

	RegisterClassEx(&wc);

	RECT rt = GetDisplayResolution();

	HWND w = CreateWindow(L"WorkerX", L"Wallpaper VideoPlayer", WS_POPUP | WS_VISIBLE, 0, 0, rt.right, rt.bottom, NULL, NULL, hInst, NULL);
	SetParent(w, wall.GetHWND());
	mf = new MFCore(w);
	if (FAILED(mf->OpenFile(path)))
	{
		if(!isCMD)
			MessageBox(w, L"打不开！", L"错误", NULL);
		delete mf;
		DestroyWindow(w);
		return -1;
	}
	ShowWindow(w, SW_SHOW);
	UpdateWindow(w);

	NOTIFYICONDATA n;
	ZeroMemory(&n, sizeof(n));
	n.cbSize = sizeof(n);
	n.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	if (isCMD)
		n.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
	else
	{
		n.uFlags = NIF_ICON | NIF_INFO | NIF_TIP | NIF_MESSAGE;
		wcscpy_s(n.szInfo, L"右键双击通知栏图标退出");
		wcscpy_s(n.szInfoTitle, L"Wallpaper Video Player");
	}
	n.uCallbackMessage = WM_NOTIFYICON;
	n.hWnd = w;
	wcscpy_s(n.szTip, L"Wallpaper Video Player");
	Shell_NotifyIcon(NIM_ADD, &n);

	MSG msg;

	while (GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	delete mf;
	Shell_NotifyIcon(NIM_DELETE, &n);
	DestroyWindow(w);
	return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
#ifdef _DEBUG
	TCHAR tt[100];
	wsprintf(tt, L"w%d\n", msg);
	OutputDebugString(tt);
#endif
	switch (msg)
	{
	case WM_NOTIFYICON:
		switch (lparam)
		{
		case WM_RBUTTONDBLCLK:
			Sleep(100);
			PostQuitMessage(0);
			return 0;
		}
		break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);
		if(mf)
			mf->Repaint();
		EndPaint(hwnd, &ps);
	}
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);
}