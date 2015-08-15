#include "RenderWindow.h"
#include "Renderer.h"
#include "InputHandler.h"

#include <windowsx.h>

#define WINDOW_CLASS_NAME "RendererWindowClass"
#define APPLICATION_NAME "DirectX11 Renderer"

LRESULT CALLBACK WindowProc(HWND hWindow, UINT message, WPARAM wparam, LPARAM lparam)
{
	WindowLinkObjects* winObjects = reinterpret_cast<WindowLinkObjects*>(GetWindowLongPtr(hWindow, 
		GWLP_USERDATA));
	Renderer* renderer = nullptr;
	InputHandlerBase* inputHandler = nullptr;

	if (winObjects != nullptr)
	{
		renderer = winObjects->WindowRenderer;
		inputHandler = winObjects->WindowInputHandler;
	}

	switch (message)
	{
	case WM_DESTROY:
	{
		PostQuitMessage(0);
		return 0;
	}

	case WM_CLOSE:
	{
		PostQuitMessage(0);
		return 0;
	}

	case WM_LBUTTONDOWN:
	{
		SetCapture(hWindow);

		if (inputHandler != nullptr)
		{
			MouseEventArgs args;
			args.MouseKey = MOUSE_KEY_LEFT;
			args.MouseX = GET_X_LPARAM(lparam);
			args.MouseY = GET_Y_LPARAM(lparam);
			inputHandler->OnMouseDown(args);
		}
		return 0;
	}

	case WM_LBUTTONUP:
	{
		ReleaseCapture();

		if (inputHandler != nullptr)
		{
			MouseEventArgs args;
			args.MouseKey = MOUSE_KEY_LEFT;
			args.MouseX = GET_X_LPARAM(lparam);
			args.MouseY = GET_Y_LPARAM(lparam);
			inputHandler->OnMouseUp(args);
		}
		return 0;
	}

	case WM_RBUTTONDOWN:
	{
		SetCapture(hWindow);

		if (inputHandler != nullptr)
		{
			MouseEventArgs args;
			args.MouseKey = MOUSE_KEY_RIGHT;
			args.MouseX = GET_X_LPARAM(lparam);
			args.MouseY = GET_Y_LPARAM(lparam);
			inputHandler->OnMouseDown(args);
		}
		return 0;
	}

	case WM_RBUTTONUP:
	{
		ReleaseCapture();

		if (inputHandler != nullptr)
		{
			MouseEventArgs args;
			args.MouseKey = MOUSE_KEY_RIGHT;
			args.MouseX = GET_X_LPARAM(lparam);
			args.MouseY = GET_Y_LPARAM(lparam);
			inputHandler->OnMouseUp(args);
		}
		return 0;
	}

	case WM_MOUSEMOVE:
	{
		if (inputHandler != nullptr)
		{
			MouseMoveEventArgs args;
			args.MouseX = GET_X_LPARAM(lparam);
			args.MouseY = GET_Y_LPARAM(lparam);
			inputHandler->OnMouseMove(args);
		}
		return 0;
	}

	case WM_KEYDOWN:
	{
		if (inputHandler != nullptr)
		{
			inputHandler->RegisterKey(wparam);

			KeyEventArgs args;
			args.Key = wparam;
			inputHandler->OnKeyDown(args);
		}
		if (wparam == VK_ESCAPE)
			PostQuitMessage(0);
		return 0;
	}

	case WM_KEYUP:
	{
		if (inputHandler != nullptr)
		{
			inputHandler->UnregisterKey(wparam);

			KeyEventArgs args;
			args.Key = wparam;
			inputHandler->OnKeyUp(args);
		}
		return 0;
	}

	case WM_KILLFOCUS:
	{
		if (renderer->IsFullscreen())
		{
			auto params = renderer->GetRenderParams();
			params.Windowed = true;
			renderer->Reset(params);
			SendMessage(hWindow, WM_SETREDRAW, TRUE, 0);
		}
		return 0;
	}

	case WM_ENTERSIZEMOVE:
	{
		renderer->SetMoveSizeEntered(true);
		SendMessage(hWindow, WM_SETREDRAW, FALSE, 0);
		return 0;
	}

	case WM_MOVE:
	{
		if (renderer != nullptr)
			if (!renderer->IsFullscreen() && renderer->MoveSizeEntered())
				SendMessage(hWindow, WM_SETREDRAW, FALSE, 0);
		return 0;
	}

	case WM_SIZE:
	{
		if (renderer == nullptr)
			return 0;

		if (renderer->MoveSizeEntered())
		{
			// If the window is moving, just present
			SendMessage(hWindow, WM_SETREDRAW, TRUE, 0);
			renderer->OnResize();
		}
		else
		{
			// Otherwise, we had an instant resize, reset render parameters
			RenderParams params = renderer->GetRenderParams();

			RECT rc;
			GetClientRect(hWindow, &rc);
			params.Extent.Width = rc.right - rc.left;
			params.Extent.Height = rc.bottom - rc.top;

			renderer->Reset(params);
		}

		return 0;
	}

	case WM_EXITSIZEMOVE:
	{
		renderer->SetMoveSizeEntered(false);
		SendMessage(hWindow, WM_SETREDRAW, TRUE, 0);
		renderer->OnResize();

		RenderParams params = renderer->GetRenderParams();

		RECT rc;
		GetClientRect(hWindow, &rc);
		params.Extent.Width = rc.right - rc.left;
		params.Extent.Height = rc.bottom - rc.top;

		renderer->Reset(params);
		return 0;
	}

	default:
		return DefWindowProc(hWindow, message, wparam, lparam);
	}
}

bool InitializeWindow(HINSTANCE hInstance, const RenderParams& renderParams, HWND* hwndOut)
{
	WNDCLASSEX wc;
	DEVMODE dmScreenSettings;
	int posX, posY;

	OutputDebugString("Initializing Window...\n");

	// Setup the windows class with default settings.
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = &WindowProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(nullptr, IDI_WINLOGO);
	wc.hIconSm = wc.hIcon;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszMenuName = nullptr;
	wc.lpszClassName = WINDOW_CLASS_NAME;
	wc.cbSize = sizeof(WNDCLASSEX);

	OutputDebugString("Registering Window Class...\n");

	// Register the window class.
	RegisterClassEx(&wc);

	int windowWidth = renderParams.Extent.Width;
	int windowHeight = renderParams.Extent.Height;

	// Setup the screen settings depending on whether it is running in full screen or in windowed mode.
	if (!renderParams.Windowed)
	{
		memset(&dmScreenSettings, 0, sizeof(dmScreenSettings));
		dmScreenSettings.dmSize = sizeof(dmScreenSettings);
		dmScreenSettings.dmPelsWidth = (unsigned long)GetSystemMetrics(SM_CXSCREEN);
		dmScreenSettings.dmPelsHeight = (unsigned long)GetSystemMetrics(SM_CXSCREEN);
		dmScreenSettings.dmBitsPerPel = 32;
		dmScreenSettings.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

		// Change the display settings to full screen.
		ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN);

		// Set the position of the window to the top left corner.
		posX = 0;
		posY = 0;
	}
	else
	{
		// Place the window in the middle of the screen.
		posX = (GetSystemMetrics(SM_CXSCREEN) - windowWidth) / 2;
		posY = (GetSystemMetrics(SM_CYSCREEN) - windowHeight) / 2;

		RECT r = { 0, 0, windowWidth, windowHeight };
		AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);

		windowWidth = r.right - r.left;
		windowHeight = r.bottom - r.top;
	}

	DWORD windowStyle = 0;
	if (!renderParams.Windowed)
		windowStyle = WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_POPUP;
	else
		windowStyle = WS_OVERLAPPEDWINDOW;

	OutputDebugString("Creating Window...\n");

	// Create the window with the screen settings and get the handle to it.
	*hwndOut = CreateWindowEx(WS_EX_APPWINDOW, WINDOW_CLASS_NAME, APPLICATION_NAME,
		windowStyle,
		posX, posY, windowWidth, windowHeight, nullptr, nullptr, hInstance, nullptr);
	ShowWindow(*hwndOut, SW_HIDE);

	if (hwndOut == nullptr)
		return false;

	return true;
}

void PresentWindow(HWND hWindow, bool bHideCursor)
{
	// Bring the window up on the screen and set it as main focus.
	ShowWindow(hWindow, SW_SHOW);
	SetForegroundWindow(hWindow);
	SetFocus(hWindow);

	// Hide the mouse cursor.
	if (bHideCursor)
		ShowCursor(false);
}

void DisposeWindow(HINSTANCE hInstance, const RenderParams& renderParams, HWND hWindow)
{
	ShowCursor(true);

	if (!renderParams.Windowed)
		ChangeDisplaySettings(nullptr, 0);

	OutputDebugString("Disposing Window...\n");

	DestroyWindow(hWindow);

	OutputDebugString("Unregistering Window Class...\n");

	UnregisterClass(WINDOW_CLASS_NAME, hInstance);
}

void LinkWindow(HWND hWindow, WindowLinkObjects* windowLinkObjects)
{
	SetWindowLongPtr(hWindow, GWLP_USERDATA, (LONG_PTR)windowLinkObjects);
}