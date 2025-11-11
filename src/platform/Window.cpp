#include "Window.h"
#include <stdexcept>


void Window::create(const wchar_t* title, int w, int h)
{
	WNDCLASSEX wc{ sizeof(WNDCLASSEX) };
	wc.lpfnWndProc = WndProc; wc.hInstance = GetModuleHandle(nullptr); wc.lpszClassName = L"Dx11MeshViewerWnd";
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	RegisterClassEx(&wc);


	DWORD style = WS_OVERLAPPEDWINDOW;
	RECT r{ 0,0,w,h }; AdjustWindowRect(&r, style, FALSE);


	m_hwnd = CreateWindow(wc.lpszClassName, title, style, CW_USEDEFAULT, CW_USEDEFAULT,
		r.right - r.left, r.bottom - r.top, nullptr, nullptr, wc.hInstance, this);
	if (!m_hwnd) throw std::runtime_error("CreateWindow failed");
	width = w; height = h;
}


void Window::show() { ShowWindow(m_hwnd, SW_SHOW); }


bool Window::pump()
{
	MSG msg{};
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
		if (msg.message == WM_QUIT) return false;
		TranslateMessage(&msg); DispatchMessage(&msg);
	}
	return true;
}


LRESULT CALLBACK Window::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	Window* self = nullptr;
	if (msg == WM_NCCREATE) {
		CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
		self = reinterpret_cast<Window*>(cs->lpCreateParams);
		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)self);
	}
	else {
		self = reinterpret_cast<Window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
	}


	if (!self) return DefWindowProc(hWnd, msg, wParam, lParam);


	switch (msg) {
	case WM_SIZE: {
		int w = LOWORD(lParam), h = HIWORD(lParam);
		self->width = w; self->height = h;
		if (self->onResize) self->onResize(w, h);
		return 0;
	}
	case WM_LBUTTONDOWN: self->mouseDown[0] = true; self->lastMouse = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) }; SetCapture(hWnd); return 0;
	case WM_RBUTTONDOWN: self->mouseDown[1] = true; self->lastMouse = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) }; SetCapture(hWnd); return 0;
	case WM_MBUTTONDOWN: self->mouseDown[2] = true; self->lastMouse = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) }; SetCapture(hWnd); return 0;
	case WM_LBUTTONUP: self->mouseDown[0] = false; ReleaseCapture(); return 0;
	case WM_RBUTTONUP: self->mouseDown[1] = false; ReleaseCapture(); return 0;
	case WM_MBUTTONUP: self->mouseDown[2] = false; ReleaseCapture(); return 0;
	case WM_MOUSEMOVE: {
		POINT p{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		float dx = float(p.x - self->lastMouse.x);
		float dy = float(p.y - self->lastMouse.y);
		self->lastMouse = p;
		int btn = self->mouseDown[0] ? 0 : self->mouseDown[1] ? 1 : self->mouseDown[2] ? 2 : -1;
		if (btn >= 0 && self->onMouseDrag) self->onMouseDrag(dx, dy, btn);
		return 0;
	}
	case WM_MOUSEWHEEL: {
		short d = GET_WHEEL_DELTA_WPARAM(wParam);
		if (self->onWheel) self->onWheel(d / 120.0f);
		return 0;
	}
	case WM_DESTROY: PostQuitMessage(0); return 0;
	case WM_KEYDOWN: {
		if (self->onKeyDown) self->onKeyDown((UINT)wParam); return 0;
	}
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}