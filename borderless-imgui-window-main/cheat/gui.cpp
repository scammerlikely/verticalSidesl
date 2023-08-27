#include "gui.h"
#include <string>
#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_dx9.h"
#include "../imgui/imgui_impl_win32.h"
#include <iostream>
#include <string>
#include <Windows.h> // Include Windows.h for HWND and ShowWindow
#include <vector>


extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND window,
	UINT message,
	WPARAM wideParameter,
	LPARAM longParameter
);

long __stdcall WindowProcess(
	HWND window,
	UINT message,
	WPARAM wideParameter,
	LPARAM longParameter)
{
	if (ImGui_ImplWin32_WndProcHandler(window, message, wideParameter, longParameter))
		return true;

	switch (message)
	{
	case WM_SIZE: {
		if (gui::device && wideParameter != SIZE_MINIMIZED)
		{
			gui::presentParameters.BackBufferWidth = LOWORD(longParameter);
			gui::presentParameters.BackBufferHeight = HIWORD(longParameter);
			gui::ResetDevice();
		}
	}return 0;

	case WM_SYSCOMMAND: {
		if ((wideParameter & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
	}break;

	case WM_DESTROY: {
		PostQuitMessage(0);
	}return 0;

	case WM_LBUTTONDOWN: {
		gui::position = MAKEPOINTS(longParameter); // set click points
	}return 0;

	case WM_MOUSEMOVE: {
		if (wideParameter == MK_LBUTTON)
		{
			const auto points = MAKEPOINTS(longParameter);
			auto rect = ::RECT{ };

			GetWindowRect(gui::window, &rect);

			rect.left += points.x - gui::position.x;
			rect.top += points.y - gui::position.y;

			if (gui::position.x >= 0 &&
				gui::position.x <= gui::WIDTH &&
				gui::position.y >= 0 && gui::position.y <= 19)
				SetWindowPos(
					gui::window,
					HWND_TOPMOST,
					rect.left,
					rect.top,
					0, 0,
					SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOZORDER
				);
		}

	}return 0;

	}

	return DefWindowProc(window, message, wideParameter, longParameter);
}

void gui::CreateHWindow(const char* windowName) noexcept
{
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_CLASSDC;
	windowClass.lpfnWndProc = WindowProcess;
	windowClass.cbClsExtra = 0;
	windowClass.cbWndExtra = 0;
	windowClass.hInstance = GetModuleHandleA(0);
	windowClass.hIcon = 0;
	windowClass.hCursor = 0;
	windowClass.hbrBackground = 0;
	windowClass.lpszMenuName = 0;
	windowClass.lpszClassName = "class001";
	windowClass.hIconSm = 0;

	RegisterClassEx(&windowClass);

	window = CreateWindowEx(
		0,
		"class001",
		windowName,
		WS_POPUP,
		100,
		100,
		WIDTH,
		HEIGHT,
		0,
		0,
		windowClass.hInstance,
		0
	);

	ShowWindow(window, SW_SHOWDEFAULT);
	UpdateWindow(window);
}

void gui::DestroyHWindow() noexcept
{
	DestroyWindow(window);
	UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
}

bool gui::CreateDevice() noexcept
{
	d3d = Direct3DCreate9(D3D_SDK_VERSION);

	if (!d3d)
		return false;

	ZeroMemory(&presentParameters, sizeof(presentParameters));

	presentParameters.Windowed = TRUE;
	presentParameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
	presentParameters.BackBufferFormat = D3DFMT_UNKNOWN;
	presentParameters.EnableAutoDepthStencil = TRUE;
	presentParameters.AutoDepthStencilFormat = D3DFMT_D16;
	presentParameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

	if (d3d->CreateDevice(
		D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL,
		window,
		D3DCREATE_HARDWARE_VERTEXPROCESSING,
		&presentParameters,
		&device) < 0)
		return false;

	return true;
}

void gui::ResetDevice() noexcept
{
	ImGui_ImplDX9_InvalidateDeviceObjects();

	const auto result = device->Reset(&presentParameters);

	if (result == D3DERR_INVALIDCALL)
		IM_ASSERT(0);

	ImGui_ImplDX9_CreateDeviceObjects();
}

void gui::DestroyDevice() noexcept
{
	if (device)
	{
		device->Release();
		device = nullptr;
	}

	if (d3d)
	{
		d3d->Release();
		d3d = nullptr;
	}
}

void gui::CreateImGui() noexcept
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ::ImGui::GetIO();

	io.IniFilename = NULL;

	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(window);
	ImGui_ImplDX9_Init(device);
}

void gui::DestroyImGui() noexcept
{
	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void gui::BeginRender() noexcept
{
	MSG message;
	while (PeekMessage(&message, 0, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&message);
		DispatchMessage(&message);

		if (message.message == WM_QUIT)
		{
			isRunning = !isRunning;
			return;
		}
	}

	// Start the Dear ImGui frame
	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}


void gui::EndRender() noexcept
{
	ImGui::EndFrame();

	device->SetRenderState(D3DRS_ZENABLE, FALSE);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

	device->Clear(0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_RGBA(0, 0, 0, 255), 1.0f, 0);

	if (device->BeginScene() >= 0)
	{
		ImGui::Render();
		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
		device->EndScene();
	}
	const auto result = device->Present(0, 0, 0, 0);

	// Handle loss of D3D9 device
	if (result == D3DERR_DEVICELOST && device->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
		ResetDevice();
}

const std::vector<std::string> correctKeys = {
	"LOxRkd392YsmXrXw823XMwia83",
	"DevLo333kdas&6324^%%%^&2284",
	"KirandsadRHuXbwt236NgwYbqnWHRUW6s",
	"cracked&&^^***%$$$$",
	"nigger"
};

// Define a variable to store the user-entered key
std::string enteredKey;

// Function to check if the entered key is correct
bool IsKeyCorrect()
{
	for (const std::string& correctKey : correctKeys)
	{
		if (enteredKey == correctKey)
		{
			return true;
		}
	}
	return false;
}

void ShowNotification(const char* message)
{
	// You can customize this function to display a notification in your GUI.
	// For simplicity, I'll use std::cout for demonstration purposes.
	std::cout << "Notification: " << message << std::endl;
}



void gui::Render() noexcept
{
	// Show the console window

	// Hide the console window
	HWND hwnd = GetConsoleWindow();
	ShowWindow(hwnd, SW_HIDE);
	static bool showMainWindow = true;
	static bool showUniversalWindow = false; // Set this flag to false initially

	
	// Inside your ImGui window code
	if (showMainWindow && ImGui::Begin("Login"))
	{
		static char textBuffer[256] = ""; // Buffer to store the text input

		// You can add additional logic here based on the cheatActivated flag to enable/disable cheat features.

		ImGui::InputText("Key", textBuffer, sizeof(textBuffer), ImGuiInputTextFlags_Password);

		// Button to delete the "Main Window" and show the "Universal" window
		if (ImGui::Button("Enter"))
		{
			enteredKey = textBuffer;

			if (IsKeyCorrect())
			{
				showMainWindow = false;
				showUniversalWindow = true;
			}
			else
			{
				ShowNotification("Wrong key");
			}
		}

		ImGui::End();
	}


	if (showUniversalWindow && ImGui::Begin("Universal"))
	{
		// Your existing code for the "Universal" window...
		if (ImGui::BeginTabBar("cheat"))
		{   
			

			if (ImGui::BeginTabItem("Main"))
			{
				

				static bool enableToggle = false;
				ImGui::Checkbox("Enable", &enableToggle);
				if (ImGui::Button("Bypass Anti-Silent"))
				{
					ImGui::OpenPopup("Injected");
				}

				if (ImGui::BeginPopupModal("Injected", NULL, ImGuiWindowFlags_AlwaysAutoResize))
				{
					// Set the next window size to be wider
					ImGui::SetNextWindowSize(ImVec2(800, 300));

					ImGui::Text("Bypassed");
					ImGui::Separator();

					if (ImGui::Button("Close"))
						ImGui::CloseCurrentPopup();

					ImGui::EndPopup();
				}

				static bool showFOVToggle = false;
				ImGui::Checkbox("Show FOV", &showFOVToggle);

				static float fovSize = 3.f; // Default size for the FOV circle
				ImGui::SliderFloat("FOV", &fovSize, 1, 125);

				// Your other code here...

				// Here, we'll add code to draw the FOV circle when "Show FOV" is checked.
				if (showFOVToggle && enableToggle)
				{
					ImVec2 cursorPos = ImGui::GetMousePos(); // Get the current mouse position

					// Calculate the circle center position based on the cursor position.
					ImVec2 circleCenter(cursorPos.x, cursorPos.y);

					// Calculate the window position and size.
					ImVec2 windowPos = ImGui::GetWindowPos();
					ImVec2 windowSize = ImGui::GetWindowSize();

					// Adjust the circle center position if it goes outside of the window.
					if (circleCenter.x < windowPos.x) circleCenter.x = windowPos.x;
					if (circleCenter.y < windowPos.y) circleCenter.y = windowPos.y;
					if (circleCenter.x > windowPos.x + windowSize.x) circleCenter.x = windowPos.x + windowSize.x;
					if (circleCenter.y > windowPos.y + windowSize.y) circleCenter.y = windowPos.y + windowSize.y;

					// Draw the circle using ImGui's drawing functions.
					// You can adjust the circle color and other properties as you like.
					ImGui::GetForegroundDrawList()->AddCircle(circleCenter, fovSize, IM_COL32(255, 0, 0, 255), 12, 2.0f);
				}

				static auto f4 = 0.f;
				ImGui::SliderFloat("Wide Prediction", &f4, -9, 9);
				static auto f2 = 0.f;
				ImGui::SliderFloat("Circle Prediction", &f2, -9, 9);
				static auto f5 = 0.f;

				ImGui::SliderFloat("Movement Prediction", &f5, -9, 9);
				static auto f = 0.f;
				ImGui::SliderFloat("Stance", &f, -100, 100);
				// Your other sliders and checkboxes...

				ImGui::EndTabItem();
			}

			// Add other tabs and their content here...

			ImGui::EndTabBar();
		}

		ImGui::End();
	}
}


