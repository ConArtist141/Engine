#ifndef INPUT_HANDLER_H_
#define INPUT_HANDLER_H_

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <set>

enum MouseKey
{
	MOUSE_KEY_LEFT,
	MOUSE_KEY_RIGHT
};

struct MouseEventArgs
{
	MouseKey MouseKey;
	int MouseX;
	int MouseY;
};

struct MouseMoveEventArgs
{
	int MouseX;
	int MouseY;
};

struct KeyEventArgs
{
	WPARAM Key;
};

class InputHandlerBase
{
private:
	std::set<WPARAM> pressedKeys;
	HWND hWindow;

public:
	inline InputHandlerBase(HWND hWindow);

	inline void SetMousePosition(const POINT& pos);
	inline POINT GetMousePosition() const;
	inline RECT GetClientSize() const;
	inline void GetMousePosition(LPPOINT posOut) const;
	inline void GetClientSize(LPRECT extentOut) const;
	inline int ShowMouse(bool bShow);

	inline bool IsKeyDown(const WPARAM key);
	inline bool IsKeyUp(const WPARAM key);

	inline void RegisterKey(const WPARAM key);
	inline void UnregisterKey(const WPARAM key);

	virtual void OnMouseDown(const MouseEventArgs& mouseEvent) = 0;
	virtual void OnMouseUp(const MouseEventArgs& mouseEvent) = 0;
	virtual void OnMouseMove(const MouseMoveEventArgs& mouseEvent) = 0;
	virtual void OnKeyDown(const KeyEventArgs& keyEvent) = 0;
	virtual void OnKeyUp(const KeyEventArgs& keyEvent) = 0;
};

inline InputHandlerBase::InputHandlerBase(HWND hWindow) :
hWindow(hWindow)
{
}

inline void InputHandlerBase::GetMousePosition(LPPOINT posOut) const
{
	GetCursorPos(posOut);
	ScreenToClient(hWindow, posOut);
}

inline void InputHandlerBase::GetClientSize(LPRECT extentOut) const
{
	GetClientRect(hWindow, extentOut);
}

inline int InputHandlerBase::ShowMouse(bool bShow)
{
	return ShowCursor(static_cast<BOOL>(bShow));
}

inline void InputHandlerBase::SetMousePosition(const POINT& pos)
{
	POINT tempPos = pos;
	ClientToScreen(hWindow, &tempPos);
	SetCursorPos(tempPos.x, tempPos.y);
}

inline RECT InputHandlerBase::GetClientSize() const
{
	RECT rect;
	GetClientRect(hWindow, &rect);
	return rect;
}

inline POINT InputHandlerBase::GetMousePosition() const
{
	POINT pos;
	GetCursorPos(&pos);
	ScreenToClient(hWindow, &pos);
	return pos;
}

inline void InputHandlerBase::RegisterKey(const WPARAM key)
{
	pressedKeys.insert(key);
}

inline void InputHandlerBase::UnregisterKey(const WPARAM key)
{
	pressedKeys.erase(key);
}

inline bool InputHandlerBase::IsKeyDown(const WPARAM key)
{
	return pressedKeys.find(key) != pressedKeys.end();
}

inline bool InputHandlerBase::IsKeyUp(const WPARAM key)
{
	return pressedKeys.find(key) == pressedKeys.end();
}

#endif