#ifndef RENDER_WINDOW_H_
#define RENDER_WINDOW_H_

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

class Renderer;
class InputHandlerBase;

struct Extent2D
{
	int Width;
	int Height;
};

struct RenderParams
{
	Extent2D Extent;
	bool UseVSync;
	bool Windowed;
};

struct WindowLinkObjects
{
	Renderer* WindowRenderer;
	InputHandlerBase* WindowInputHandler;
};

bool InitializeWindow(HINSTANCE hInstance, const RenderParams& renderParams, HWND* hwndOut);
void LinkWindow(HWND hWindow, WindowLinkObjects* windowLinkObjects);
void PresentWindow(HWND hWindow, bool bHideCursor);
void DisposeWindow(HINSTANCE hInstance, const RenderParams& renderParams, HWND hWindow);

#endif