#include "CameraController.h"
#include "Camera.h"

#include "Log.h"

#include <DirectXMath.h>
#include <algorithm>

using namespace DirectX;

FirstPersonCameraController::FirstPersonCameraController(SphericalCamera* camera,
	InputHandlerBase* inputHandler) :
	camera(camera),
	inputHandler(inputHandler),
	bHasCapturedMouse(false),
	RotationVelocity(0.0f),
	Velocity(0.0f),
	ForwardKey('W'),
	BackwardKey('S'),
	LeftKey('A'),
	RightKey('D')
{
}

void FirstPersonCameraController::CenterMouse()
{
	RECT rect;
	inputHandler->GetClientSize(&rect);
	POINT pos = { rect.right / 2, rect.bottom / 2 };
	inputHandler->SetMousePosition(pos);
}

void FirstPersonCameraController::OnMouseDown(const MouseEventArgs& args)
{
	if (args.MouseKey == MOUSE_KEY_RIGHT)
	{
		inputHandler->ShowMouse(false);
		bHasCapturedMouse = true;
		MouseCapturePosition = inputHandler->GetMousePosition();
		CenterMouse();
	}
}

void FirstPersonCameraController::OnMouseUp(const MouseEventArgs& args)
{
	if (args.MouseKey == MOUSE_KEY_RIGHT)
	{
		bHasCapturedMouse = false;
		inputHandler->SetMousePosition(MouseCapturePosition);
		inputHandler->ShowMouse(true);
	}
}

void FirstPersonCameraController::Update(const float delta)
{
	if (bHasCapturedMouse)
	{
		POINT pos;
		RECT rect;
		inputHandler->GetMousePosition(&pos);
		inputHandler->GetClientSize(&rect);

		CenterMouse();

		auto dx = static_cast<float>(pos.x - rect.right / 2);
		auto dy = static_cast<float>(pos.y - rect.bottom / 2);

		camera->Yaw -= dx * delta * RotationVelocity;
		camera->Pitch += dy * delta * RotationVelocity;

		camera->Yaw = fmod(camera->Yaw, XM_2PI);
		camera->Pitch = std::min(XM_PI - 0.1f, std::max(0.1f, camera->Pitch));
	}

	XMFLOAT3 forwardDirection;
	camera->GetForward(&forwardDirection);
	XMVECTOR forwardDirectionVec = XMLoadFloat3(&forwardDirection);
	XMVECTOR position = XMLoadFloat3(&camera->Position);
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR sideVec = XMVector3Cross(up, forwardDirectionVec);
	sideVec = XMVector3Normalize(sideVec);

	if (inputHandler->IsKeyDown(ForwardKey))
		position += forwardDirectionVec * Velocity * delta;
	if (inputHandler->IsKeyDown(BackwardKey))
		position -= forwardDirectionVec * Velocity * delta;
	if (inputHandler->IsKeyDown(LeftKey))
		position -= sideVec * Velocity * delta;
	if (inputHandler->IsKeyDown(RightKey))
		position += sideVec * Velocity * delta;

	XMStoreFloat3(&camera->Position, position);
}