#include "../../include/sasha/core/App.h"

namespace
{
	App* app = 0;
}

LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return app->WndProc(hWnd, msg, wParam, lParam);
}


App::App(HINSTANCE hInstance) noexcept
	: _hInstance(hInstance)
{
	app = this;
}

App::~App()
{
}

void App::AppInit()
{
	// Creating and setting up the window class
	WNDCLASS wc{};
	// Extras that we dont need
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	// Setting up the function that will handle the messages sent to the wnd
	wc.lpfnWndProc = MainWndProc;
	// Default Arrow
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	// Name of the wnd class that we will register it as
	wc.lpszClassName = L"CLASS";
	// Background Color
	wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	// Default Icon
	wc.hIcon = LoadIcon(0, IDI_APPLICATION);
	// No menu
	wc.lpszMenuName = 0;
	// Set up the instance of our application to the wnd class
	wc.hInstance = _hInstance;
	// It will redraw the wnd H(orizontally) and V(ertically)
	wc.style = CS_HREDRAW | CS_VREDRAW;

	// Registers the wnd class in order to create the window app
	if (!RegisterClass(&wc))
		ThrowAppException(L"RegisterClass Failed Miserably");

	// Setting up the wnd rectangle
	RECT R = { 0, 0, SCREEN_HEIGHT, SCREEN_WIDTH };
	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
	int width = R.right - R.left;
	int height = R.bottom - R.top;

	// Creating the window
	_wndHandle = CreateWindow(
		// Name of the wnd class
		L"CLASS",
		// Name at the tab bar 
		L"Rifat",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		width, height,
		0, 0,
		_hInstance,
		0
	);

	// Handles if the window isnt created
	if (!_wndHandle)
		ThrowAppException(L"CreateWindow Failed Miserably");

	ShowWindow(_wndHandle, SW_SHOW);
	UpdateWindow(_wndHandle);
	
	_d3dApp = std::make_unique<D3DRenderer>(_wndHandle, SCREEN_WIDTH, SCREEN_HEIGHT);
	_d3dApp->SetInputs(&_kbd, &_mouse);
	_d3dApp->d3dInit();
}

void App::Run()
{
	MSG msg = { 0 };

	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			_timer.Tick();
			if (_isPaused)
				Sleep(100);
			else
			{
				CalculateFPS();
				// Update
				_d3dApp->Update(_timer);
				// Draw
				_d3dApp->RenderFrame(_timer);
			}
			_kbd.EndFrame();
			_mouse.EndFrame();
		}
	}
}

LRESULT App::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE)
			PostQuitMessage(0);
		else
			_kbd.OnKeyDown(wParam);
		break;
	case WM_KEYUP:
		_kbd.OnKeyUp(wParam);
		break;

	case WM_MOUSEMOVE:
		_mouse.OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		break;

	case WM_LBUTTONDOWN:
		_mouse.OnButtonDown(VK_LBUTTON);
		break;
	case WM_LBUTTONUP:
		_mouse.OnButtonUp(VK_LBUTTON);
		break;

	case WM_RBUTTONDOWN:
		_mouse.OnButtonDown(VK_RBUTTON);
		SetCapture(hWnd);
		ShowCursor(FALSE);
		break;
	case WM_RBUTTONUP:
		_mouse.OnButtonUp(VK_RBUTTON);
		ReleaseCapture();
		ShowCursor(TRUE);
		break;

	case WM_MOUSEWHEEL:
		_mouse.OnWheelDelta(GET_WHEEL_DELTA_WPARAM(wParam));
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE)
		{
			_isPaused = true;
			_timer.Stop();
		}
		else
		{
			_isPaused = false;
			_timer.Start();
		}
		break;
	case WM_SIZE:
		if (_d3dApp)
		{
			_d3dApp->SetAppSize(LOWORD(lParam), HIWORD(lParam));

			if (wParam == SIZE_MINIMIZED)
			{
				_isPaused = true;
				_minimized = true;
				_maximized = false;
			}
			else if (wParam == SIZE_MAXIMIZED)
			{
				_isPaused = true;
				_minimized = false;
				_maximized = true;
			}
			else if (wParam == SIZE_RESTORED)
			{
				if (_minimized)
				{
					_isPaused = false;
					_minimized = false;
				}
				else if (_maximized)
				{
					_isPaused = false;
					_maximized = false;
				}
				_d3dApp->OnResize();
			}
		}
		break;
	case WM_EXITSIZEMOVE:
		_isPaused = false;
		_timer.Start();
		_d3dApp->OnResize();
		break;
	case WM_GETMINMAXINFO:
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
		break;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

void App::CalculateFPS() const noexcept
{
	static int frameCount = 0;
	static float timeElapsed = 0.f;
	frameCount++;

	if (_timer.TotalTime() - timeElapsed >= 1.f)
	{
		float fps = (float)frameCount;
		float mspf = 1000.f / fps;

		std::wstring windowName = L"fps: " + std::to_wstring(fps) + L", ms: " + std::to_wstring(mspf);
		SetWindowText(_wndHandle, windowName.c_str());

		frameCount = 0;
		timeElapsed += 1.f;
	}
}
