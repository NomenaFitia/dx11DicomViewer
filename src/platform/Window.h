#pragma once
#include <Windows.h>
#include <windowsx.h>
#include <functional>


class Window {
public:
	using ResizeCB = std::function<void(int, int)>;
	using MouseDragCB = std::function<void(float, float, int)>; // dx, dy, button(0=L,1=R,2=M)
	using MouseWheelCB = std::function<void(float)>;


	void create(const wchar_t* title, int w, int h);
	void show();
	void setOnResize(ResizeCB cb) { onResize = std::move(cb); }
	void setOnMouseDrag(MouseDragCB cb) { onMouseDrag = std::move(cb); }
	void setOnWheel(MouseWheelCB cb) { onWheel = std::move(cb); }
	

	std::function<void(UINT key)> onKeyDown;

	HWND hwnd() const { return m_hwnd; }


	bool pump();
private:
	static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
	HWND m_hwnd{}; int width{}, height{};
	ResizeCB onResize; MouseDragCB onMouseDrag; MouseWheelCB onWheel;
	POINT lastMouse{ 0,0 }; bool mouseDown[3]{ false,false,false };
};