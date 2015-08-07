#ifndef CAMERA_H_
#define CAMERA_H_

#include <DirectXMath.h>

struct Extent2D;
struct Frustum;

class ICamera
{
public:
	virtual void GetPosition(DirectX::XMFLOAT3* positionOut) = 0;
	virtual void GetViewMatrix(DirectX::XMFLOAT4X4* matrixOut) = 0;
	virtual void GetProjectionMatrix(DirectX::XMFLOAT4X4* matrixOut, const Extent2D& viewportSize) = 0;
	virtual void GetFrustum(Frustum* frustum, const Extent2D& viewportSize) = 0;
};

class SphericalCamera : public ICamera
{
public:
	SphericalCamera();
	SphericalCamera(const DirectX::XMFLOAT3& position, const float yaw, const float pitch,
		const float nearPlane, const float farPlane, const float fieldOfView);

	DirectX::XMFLOAT3 Position;

	float Yaw;
	float Pitch;

	float NearPlane;
	float FarPlane;
	float FieldOfView;

	void GetPosition(DirectX::XMFLOAT3* positionOut) override;
	void GetViewMatrix(DirectX::XMFLOAT4X4* matrixOut) override;
	void GetProjectionMatrix(DirectX::XMFLOAT4X4* matrixOut, const Extent2D& viewportSize) override;
	void GetFrustum(Frustum* frustum, const Extent2D& viewportSize) override;

	void GetForward(DirectX::XMFLOAT3* vecOut);
	void LookAt(const float x, const float y, const float z);
	void LookAt(const DirectX::XMFLOAT3& target);
};

#endif