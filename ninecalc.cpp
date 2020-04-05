#include <windows.h>

LRESULT
win_callback (HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	return(DefWindowProc(window, message, wparam, lparam));
}

int
WinMain (HINSTANCE instance, HINSTANCE _, LPSTR command_line, int __)
{
	WNDCLASSEX window_class = {};
	window_class.cbSize = sizeof(window_class);
	window_class.lpfnWndProc = win_callback;
	window_class.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	window_class.hInstance = instance;
	window_class.hIcon = 0;
	window_class.lpszClassName = "NineCalcWindowClass";

	if (RegisterClassEx(&window_class))
	{
		HWND window = CreateWindowEx(WS_EX_APPWINDOW, window_class.lpszClassName,
			/*window title:*/ "NineCalc",
			/*styles:*/ WS_OVERLAPPEDWINDOW | WS_VISIBLE /*| WS_VSCROLL/**/,
			/*position:*/ CW_USEDEFAULT, CW_USEDEFAULT,
			/*size:*/ 800, 600, 
			0, 0, instance, 0);

		if (window)
		{
			MSG message;
			while (GetMessage(&message, window, 0, 0))
			{
				DispatchMessage(&message);
			}
		}
	}
}