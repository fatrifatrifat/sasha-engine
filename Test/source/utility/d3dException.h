#pragma once
#include <Windows.h>
#include <string>
#include <comdef.h>

class D3DException
{
public:
	D3DException(HRESULT hr, const std::wstring& function_name, const std::wstring& file_name, int error_line)
		: _errorCode(hr)
		, _functionName(function_name)
		, _fileName(file_name)
		, _errorLine(error_line)
	{}

	const std::wstring ToString() const
	{
		_com_error error(_errorCode);
		std::wstring errorCode = error.ErrorMessage();
		return L"Error: " + errorCode + L"\nThrown by the function: " + _functionName + L"\nIn the file: " + _fileName + L"\nAt the line: " + std::to_wstring(_errorLine);
	}

	static inline std::wstring AnsiToWString(const std::string& str)
	{
		WCHAR buffer[512];
		MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
		return std::wstring(buffer);
	}

	HRESULT _errorCode;
	std::wstring _functionName;
	std::wstring _fileName;
	int _errorLine;
};

#ifndef ThrowIfFailed
#define ThrowIfFailed(x)                                              \
{                                                                     \
    HRESULT hr__ = (x);                                               \
	std::wstring file = D3DException::AnsiToWString(__FILE__);		  \
    if (FAILED(hr__))                                                 \
    {                                                                 \
        throw D3DException(hr__, L#x, file, __LINE__);                \
    }                                                                 \
}																	
#endif

