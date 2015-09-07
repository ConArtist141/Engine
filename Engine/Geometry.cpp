#include "Geometry.h"

#include <limits>

using namespace std;
using namespace DirectX;

inline void ConstructPlaneFromNormalAndPoint(const DirectX::XMVECTOR& point,
	const DirectX::XMVECTOR& normal, Plane* planeOut)
{
	XMStoreFloat3(&planeOut->Normal, normal);
	XMStoreFloat(&planeOut->Distance, XMVector3Dot(point, normal));
}

inline void ConstructPlaneFromPoints(const DirectX::XMVECTOR& p1, const DirectX::XMVECTOR& p2,
	const DirectX::XMVECTOR& p3, Plane* planeOut)
{
	auto normal = XMVector3Cross(p2 - p1, p3 - p1);
	normal = XMVector3Normalize(normal);

	XMStoreFloat3(&planeOut->Normal, normal);
	XMStoreFloat(&planeOut->Distance, XMVector3Dot(normal, p1));
}

bool IsOutsideFrustum(const Bounds& bounds, const Frustum& frustum)
{
	XMFLOAT3 testPoints[] =
	{
		{ bounds.Lower.x, bounds.Lower.y, bounds.Lower.z },
		{ bounds.Lower.x, bounds.Lower.y, bounds.Upper.z },
		{ bounds.Lower.x, bounds.Upper.y, bounds.Lower.z },
		{ bounds.Lower.x, bounds.Upper.y, bounds.Upper.z },
		{ bounds.Upper.x, bounds.Lower.y, bounds.Lower.z },
		{ bounds.Upper.x, bounds.Lower.y, bounds.Upper.z },
		{ bounds.Upper.x, bounds.Upper.y, bounds.Lower.z },
		{ bounds.Upper.x, bounds.Upper.y, bounds.Upper.z }
	};

	for (int i = 0; i < 6; ++i)
	{
		int sum = 0;

		for (int j = 0; j < 8; ++j)
		{
			sum += ((testPoints[j].x * frustum.Planes[i].Normal.x +
				testPoints[j].y * frustum.Planes[i].Normal.y +
				testPoints[j].z * frustum.Planes[i].Normal.z) > frustum.Planes[i].Distance);

			if (sum == 8)
				return true;
		}
	}

	return false;
}

void ConstructFrustum(const float fieldOfView, const float farPlane, const float nearPlane,
	const DirectX::XMFLOAT3& cameraPosition, const DirectX::XMFLOAT3& cameraTarget,
	const DirectX::XMFLOAT3& cameraUp, const float aspectRatio, Frustum* frustumOut)
{
	XMVECTOR position = XMLoadFloat3(&cameraPosition);
	XMVECTOR target = XMLoadFloat3(&cameraTarget);
	XMVECTOR up = XMLoadFloat3(&cameraUp);

	XMVECTOR forward = target - position;

	XMVECTOR left = XMVector3Cross(up, forward);
	up = XMVector3Cross(forward, left);

	forward = XMVector3Normalize(forward);
	left = XMVector3Normalize(left);
	up = XMVector3Normalize(up);

	XMVECTOR nearCenter = position + forward * nearPlane;
	XMVECTOR farCenter = position + forward * farPlane;

	// Front plane
	ConstructPlaneFromNormalAndPoint(nearCenter, -forward, &frustumOut->Planes[0]);

	// Back plane
	ConstructPlaneFromNormalAndPoint(farCenter, forward, &frustumOut->Planes[1]);

	float a = 2.0f * (float)tan(fieldOfView * 0.5f);
	float nearHeight = nearPlane * a;
	float farHeight = farPlane * a;
	float nearWidth = aspectRatio * nearHeight;
	float farWidth = aspectRatio * farHeight;

	XMVECTOR farTopLeft = farCenter + 0.5f * farWidth * left + 0.5f * farHeight * up;
	XMVECTOR farBottomLeft = farTopLeft - farHeight * up;
	XMVECTOR farTopRight = farTopLeft - farWidth * left;
	XMVECTOR farBottomRight = farTopRight - farHeight * up;

	XMVECTOR nearTopLeft = nearCenter + 0.5f * nearWidth * left + 0.5f * nearHeight * up;
	XMVECTOR nearBottomLeft = nearTopLeft - nearHeight * up;
	XMVECTOR nearTopRight = nearTopLeft - nearWidth * left;
	XMVECTOR nearBottomRight = nearTopRight - nearHeight * up;

	// Top plane
	ConstructPlaneFromPoints(farTopLeft, nearTopLeft, farTopRight, &frustumOut->Planes[2]);

	// Bottom plane
	ConstructPlaneFromPoints(farBottomLeft, farBottomRight, nearBottomLeft, &frustumOut->Planes[3]);

	// Left plane
	ConstructPlaneFromPoints(farBottomLeft, nearBottomLeft, farTopLeft, &frustumOut->Planes[4]);

	// Right plane
	ConstructPlaneFromPoints(farBottomRight, farTopRight, nearBottomRight, &frustumOut->Planes[5]);
}

void TransformBounds(const XMMATRIX& matrix, const Bounds& bounds, Bounds* boundsOut)
{
	XMVECTOR vecs[8] =
	{
		XMVectorSet(bounds.Lower.x, bounds.Lower.y, bounds.Lower.z, 1.0f),
		XMVectorSet(bounds.Lower.x, bounds.Lower.y, bounds.Upper.z, 1.0f),
		XMVectorSet(bounds.Lower.x, bounds.Upper.y, bounds.Lower.z, 1.0f),
		XMVectorSet(bounds.Lower.x, bounds.Upper.y, bounds.Upper.z, 1.0f),
		XMVectorSet(bounds.Upper.x, bounds.Lower.y, bounds.Lower.z, 1.0f),
		XMVectorSet(bounds.Upper.x, bounds.Lower.y, bounds.Upper.z, 1.0f),
		XMVectorSet(bounds.Upper.x, bounds.Upper.y, bounds.Lower.z, 1.0f),
		XMVectorSet(bounds.Upper.x, bounds.Upper.y, bounds.Upper.z, 1.0f)
	};

	auto infinity = numeric_limits<float>::infinity();
	boundsOut->Lower = { infinity, infinity, infinity };
	boundsOut->Upper = { -infinity, -infinity, -infinity };

	XMFLOAT3 vecResult;

	for (size_t i = 0; i < 8; ++i)
	{
		vecs[i] = XMVector4Transform(vecs[i], matrix);
		XMStoreFloat3(&vecResult, vecs[i]);
	
		if (boundsOut->Lower.x > vecResult.x)
			boundsOut->Lower.x = vecResult.x;
		if (boundsOut->Lower.y > vecResult.y)
			boundsOut->Lower.y = vecResult.y;
		if (boundsOut->Lower.z > vecResult.z)
			boundsOut->Lower.z = vecResult.z;

		if (boundsOut->Upper.x < vecResult.x)
			boundsOut->Upper.x = vecResult.x;
		if (boundsOut->Upper.y < vecResult.y)
			boundsOut->Upper.y = vecResult.y;
		if (boundsOut->Upper.z < vecResult.z)
			boundsOut->Upper.z = vecResult.z;
	}
}