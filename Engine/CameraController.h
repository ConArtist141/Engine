#ifndef CAMERA_CONTROLLER_H_
#define CAMERA_CONTROLLER_H_

#include "InputHandler.h"

class SphericalCamera;

class FirstPersonCameraController
{
protected:
	SphericalCamera* camera;
	InputHandlerBase* inputHandler;

	POINT MouseCapturePosition;
	bool bHasCapturedMouse;

	void CenterMouse();

public:
	FirstPersonCameraController(SphericalCamera* camera, InputHandlerBase* inputHandler);

	float RotationVelocity;
	float Velocity;
	WPARAM ForwardKey;
	WPARAM BackwardKey;
	WPARAM LeftKey;
	WPARAM RightKey;

	void OnMouseDown(const MouseEventArgs& args);
	void OnMouseUp(const MouseEventArgs& args);
	void Update(const float delta);
};

#endif