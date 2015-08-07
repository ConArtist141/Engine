#include "Camera.h"
#include "RenderWindow.h"
#include "Geometry.h"

using namespace DirectX;

#define DEFAULT_NEAR_PLANE 1.0f
#define DEFAULT_FAR_PLANE 500.0f
#define DEFAULT_FOV XM_PI / 3.0f

void SphericalCamera::GetForward(XMFLOAT3* vecOut)
{
	vecOut->x = cos(Yaw) * sin(Pitch);
	vecOut->y = cos(Pitch);
	vecOut->z = sin(Yaw)* sin(Pitch);
}

void SphericalCamera::GetPosition(DirectX::XMFLOAT3* positionOut)
{
	*positionOut = Position;
}

void SphericalCamera::GetViewMatrix(XMFLOAT4X4* matrixOut)
{
	XMVECTOR eye = XMLoadFloat3(&Position);
	XMVECTOR target = XMVectorSet(cos(Yaw) * sin(Pitch), cos(Pitch), sin(Yaw) * sin(Pitch), 0.0f);
	target += eye;
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMStoreFloat4x4(matrixOut, XMMatrixLookAtLH(eye, target, up));
}

void SphericalCamera::GetProjectionMatrix(XMFLOAT4X4* matrixOut, const Extent2D& viewportSize)
{
	float aspectRatio = (float)viewportSize.Width / (float)viewportSize.Height;
	XMStoreFloat4x4(matrixOut, XMMatrixPerspectiveFovLH(FieldOfView, aspectRatio, NearPlane, FarPlane));
}

void SphericalCamera::LookAt(const float x, const float y, const float z)
{
	LookAt(XMFLOAT3(x, y, z));
}

void SphericalCamera::LookAt(const XMFLOAT3& target)
{
	XMVECTOR eye = XMLoadFloat3(&Position);
	XMVECTOR targetVec = XMLoadFloat3(&target);
	XMVECTOR directionVec = targetVec - eye;
	directionVec = XMVector3Normalize(directionVec);
	XMFLOAT3 direction;
	XMStoreFloat3(&direction, directionVec);
	
	Yaw = atan2(direction.z, direction.x);
	Pitch = acos(direction.y);
}

void SphericalCamera::GetFrustum(Frustum* frustum, const Extent2D& viewportSize)
{
	XMFLOAT3 forward;
	GetForward(&forward);
	XMFLOAT3 target;
	XMStoreFloat3(&target, XMLoadFloat3(&forward) + XMLoadFloat3(&Position));

	float aspectRatio = (float)viewportSize.Width / (float)viewportSize.Height;

	ConstructFrustum(FieldOfView, FarPlane, NearPlane,
		Position, target, XMFLOAT3(0.0f, 1.0f, 0.0f),
		aspectRatio, frustum);
}

SphericalCamera::SphericalCamera() :
NearPlane(DEFAULT_NEAR_PLANE),
FarPlane(DEFAULT_FAR_PLANE),
FieldOfView(DEFAULT_FOV),
Position(0.0f, 0.0f, 0.0f),
Yaw(0.0f),
Pitch(XM_PI / 2.0f)
{
}

SphericalCamera::SphericalCamera(const XMFLOAT3& position, const float yaw, const float pitch,
	const float nearPlane, const float farPlane, const float fieldOfView) :
	NearPlane(nearPlane),
	FarPlane(farPlane),
	FieldOfView(fieldOfView),
	Position(position),
	Yaw(yaw),
	Pitch(pitch)
{
}