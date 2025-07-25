#include "core/App.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{
	try
	{
		App app(hInstance);
		app.AppInit();
		app.Run();
	}
	catch (App::AppException& e)
	{
		MessageBox(0, e.ToString().c_str(), 0, 0);
	}
	catch (D3DException& e)
	{
		MessageBox(0, e.ToString().c_str(), 0, 0);
	}
	return 0;
}