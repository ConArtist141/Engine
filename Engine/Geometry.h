#ifndef GEOMETRY_H_
#define GEOMETRY_H_

#include <DirectXMath.h>

struct Bounds
{
	DirectX::XMFLOAT3 Lower;
	DirectX::XMFLOAT3 Upper;
};

struct Plane
{
	DirectX::XMFLOAT3 Normal;
	float Distance;
};

struct Frustum
{
	Plane Planes[6];
};

void ConstructPlaneFromNormalAndPoint(const DirectX::XMVECTOR& point,
	const DirectX::XMVECTOR& normal, Plane* planeOut);

void ConstructPlaneFromPoints(const DirectX::XMVECTOR& p1, const DirectX::XMVECTOR& p2,
	const DirectX::XMVECTOR& p3, Plane* planeOut);

bool IsOutsideFrustum(const Bounds& bounds, const Frustum& frustum);

void ConstructFrustum(const float fieldOfView, const float farPlane, const float nearPlane,
	const DirectX::XMFLOAT3& cameraPosition, const DirectX::XMFLOAT3& cameraTarget,
	const DirectX::XMFLOAT3& cameraUp, const float aspectRatio, Frustum* frustumOut);

void TransformBounds(const DirectX::XMMATRIX& matrix, const Bounds& bounds, Bounds* boundsOut);

#endif