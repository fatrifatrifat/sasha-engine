#pragma once
#include <Windows.h>
#include <memory>
#include <exception>
#include "../renderer/D3DRenderer.h"

#define SCREEN_HEIGHT 800
#define SCREEN_WIDTH 800

#define ThrowAppException(info) throw AppException(__FUNCTIONW__, __FILEW__, __LINE__, info)

class App
{
public:
	class AppException
	{
	public:
		AppException(const std::wstring& functionName,
			const std::wstring& fileName,
			UINT lineNumber,
			const std::wstring& extraInfo) noexcept
			: _functionName(functionName)
			, _fileName(fileName)
			, _lineNumber(lineNumber)
			, _extraInfo(extraInfo)
		{}

		std::wstring ToString() const noexcept
		{
			std::wstring result;
			result += L"Error: " + _extraInfo + L"\n";
			result += L"Function: " + _functionName + L"\n";
			result += L"File: " + _fileName + L"\n";
			result += L"Line: " + std::to_wstring(_lineNumber);
			return result;
		}

	private:
		std::wstring _functionName;
		std::wstring _fileName;
		UINT _lineNumber;
		std::wstring _extraInfo;
	};

public:
	explicit App(HINSTANCE hInstance) noexcept;
	~App();

	App(const App&) = delete;
	App& operator=(const App&) = delete;

	void AppInit();
	void Run();
	LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
	void CalculateFPS() const noexcept;

private:
	bool _isPaused = false;
	Timer _timer;
	HINSTANCE _hInstance;
	HWND _wndHandle;
	bool _minimized = false;
	bool _maximized = false;

	std::unique_ptr<D3DRenderer> _d3dApp;
};

