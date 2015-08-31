#include "RenderWindow.h"
#include "Renderer.h"
#include "ContentPackage.h"
#include "SceneGraph.h"
#include "Camera.h"
#include "InputHandler.h"
#include "CameraController.h"

#include <DirectXMath.h>

#include <iostream>
#include <string>
#include <sstream>
#include <array>

using namespace DirectX;

class InputHandler : public InputHandlerBase
{
private:
	FirstPersonCameraController cameraController;

public:
	InputHandler(SphericalCamera* camera, HWND hwnd) :
		InputHandlerBase(hwnd),
		cameraController(camera, this)
	{
		cameraController.RotationVelocity = 0.005f;
		cameraController.Velocity = 0.1f;
	}

	void OnMouseDown(const MouseEventArgs& args) override
	{
		cameraController.OnMouseDown(args);
	}

	void OnMouseUp(const MouseEventArgs& args) override
	{
		cameraController.OnMouseUp(args);
	}

	void OnMouseMove(const MouseMoveEventArgs& args) override { }
	void OnKeyDown(const KeyEventArgs& args) override { }
	void OnKeyUp(const KeyEventArgs& args) override { }

	void Update(const float delta)
	{
		cameraController.Update(delta);
	}
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR cmdLine, int cmdShow)
{
	RenderParams params;
	params.Extent = { 800, 600 };
	params.UseVSync = true;
	params.Windowed = true;

	HWND hWindow;

	if (InitializeWindow(hInstance, params, &hWindow))
	{
		bool bExit = false;

		// Set camera stuff
		SphericalCamera camera;
		camera.Position = XMFLOAT3(0.0f, 2.0f, 15.0f);
		camera.LookAt(XMFLOAT3(0.0f, 2.0f, 0.0f));
		camera.NearPlane = 0.1f;
		camera.FarPlane = 100.0f;

		Renderer renderer;
		InputHandler inputHandler(&camera, hWindow);

		WindowLinkObjects linkObjects = { &renderer, &inputHandler };
		LinkWindow(hWindow, &linkObjects);

		if (SUCCEEDED(renderer.Initialize(hWindow, params)))
		{
			ContentPackage package(&renderer);
			StaticMesh* mesh1 = nullptr;
			StaticMesh* mesh2 = nullptr;
			ID3D11Resource* texture1 = nullptr;
			ID3D11ShaderResourceView* resourceView1 = nullptr;
			ID3D11Resource* texture2 = nullptr;
			ID3D11ShaderResourceView* resourceView2 = nullptr;
			ID3D11VertexShader* vertexShader = nullptr;
			ID3D11PixelShader* pixelShader = nullptr;
			BytecodeBlob vertexShaderBytecode;

			// Load resources
			package.SetVertexLayout(renderer.GetElementLayoutStaticMeshInstanced());
			package.LoadMesh("ball.DAE", &mesh1);
			package.LoadMesh("stage.DAE", &mesh2);
			package.LoadVertexShader("StandardVertex.cso", &vertexShader, &vertexShaderBytecode);
			package.LoadPixelShader("StandardPixel.cso", &pixelShader);
			package.LoadTexture2D("albedo.dds", &texture1, &resourceView1);
			package.LoadTexture2D("albedo2.dds", &texture2, &resourceView2);

			renderer.CreateStaticMeshInputLayout(vertexShaderBytecode);
			vertexShaderBytecode.Destroy();

			// Create a material
			Material* material1 = new Material;
			CreateStandardMaterial(vertexShader, pixelShader, resourceView1, false, material1);
			package.SetMaterial("Material1", material1);
			Material* material2 = new Material;
			CreateStandardMaterial(vertexShader, pixelShader, resourceView2, false, material2);
			package.SetMaterial("Material2", material2);

			// Create the scene
			SceneNode* scene = CreateSceneGraph();
			XMFLOAT4X4 transform;
			for (int i = -2; i <= 2; ++i)
			{
				for (int j = -2; j <= 2; ++j)
				{
					XMStoreFloat4x4(&transform, XMMatrixTranslation(3.0f * i, 2.0f, 3.0f * j));
					scene->Children.push_back(CreateStaticMeshNode(mesh1, material1, transform));
				}
			}
		
			XMStoreFloat4x4(&transform, XMMatrixIdentity());
			scene->Children.push_back(CreateStaticMeshNode(mesh2, material2, transform));

			// Update the transforms of the scene
			UpdateTransforms(scene, XMMatrixIdentity());
			// Build the bounding volume hierarchy for culling objects
			BuildBoundingVolumeHierarchy(scene, true);

			// Show the window now
			PresentWindow(hWindow, false);
			MSG message;

			while (!bExit)
			{
				while (PeekMessage(&message, NULL, 0, 0, PM_REMOVE))
				{
					TranslateMessage(&message);
					DispatchMessage(&message);

					if (message.message == WM_QUIT)
					{
						ShowWindow(hWindow, SW_HIDE);
						bExit = true;
					}
				}

				inputHandler.Update(1.0f);
				renderer.RenderFrame(scene, &camera);
			}

			package.Destroy();

			if (scene != nullptr)
				DestroySceneGraph(scene);
		}

		params = renderer.GetRenderParams();
		renderer.Destroy();

		DisposeWindow(hInstance, params, false);
	}

	return 0;
}