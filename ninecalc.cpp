#include <windows.h>

#define internal   static
#define global     static
#define persistent static

typedef signed char 	i8;
typedef short 			i16;
typedef int 			i32;
typedef long 			i64;

typedef char 			u8;
typedef unsigned short 	u16;
typedef unsigned int 	u32;
typedef unsigned long 	u64;

typedef i32 			bool32;
typedef i64 			bool64;

struct Graphics
{
	void *buffer;
	BITMAPINFO bitmap_info;
	u8 bytes_per_pixel;
	u32 width;
	u32 height;
	u64 size;
};

global bool32	global_running  = true;
global Graphics	global_graphics = {};

internal void
win_resize_graphics_buffer(u32 width, u32 height)
{
	if (global_graphics.buffer)
	{
		VirtualFree(global_graphics.buffer, 0, MEM_RELEASE);
	}

	global_graphics.bytes_per_pixel = 4;
	global_graphics.width  = width;
	global_graphics.height = height;

	BITMAPINFOHEADER header = {};
	header.biSize 		= sizeof(header);
	header.biWidth 		= width;
	header.biHeight 	= -(i64)height;
	header.biPlanes 	= 1;
	header.biBitCount 	= global_graphics.bytes_per_pixel * 8;
	header.biCompression = BI_RGB;
	global_graphics.bitmap_info.bmiHeader = header;

	u32 size = width * height * global_graphics.bytes_per_pixel;
	global_graphics.buffer = VirtualAlloc(0, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

internal void
win_update_window(HWND window, HDC device_context)
{
	RECT client_rect;
	GetClientRect(window, &client_rect);
	u32 dest_width  = client_rect.right - client_rect.left;
	u32 dest_height = client_rect.bottom - client_rect.top;
	StretchDIBits(device_context,
		/*destination x, y, w, h:*/ 0, 0, dest_width, dest_height,
		/*source x, y, w, h:*/      0, 0, global_graphics.width, global_graphics.height,
		global_graphics.buffer, &global_graphics.bitmap_info,
		DIB_RGB_COLORS, SRCCOPY);
}

internal void
win_update_window(HWND window)
{
	HDC device_context = GetDC(window);
	win_update_window(window, device_context);
	ReleaseDC(window, device_context);
}

internal LRESULT
win_callback (HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	LRESULT result = 0;
	switch (message)
	{
		case WM_CLOSE:
		case WM_QUIT:
		case WM_DESTROY:
		{
			global_running = false;
		} break;
		case WM_SIZE:
		{
			RECT client_rect;
			GetClientRect(window, &client_rect);
			u32 width  = client_rect.right - client_rect.left;
			u32 height = client_rect.bottom - client_rect.top;
			win_resize_graphics_buffer(width, height);
		} break;
		case WM_PAINT:
		{
			PAINTSTRUCT paint;
			HDC device_context = BeginPaint(window, &paint);
			win_update_window(window, device_context);
			EndPaint(window, &paint);
		} break;
		default:
		{
			result = DefWindowProc(window, message, wparam, lparam);
		} break;
	}
	return(result);
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
			while (global_running)
			{
				MSG message;
				while (PeekMessage(&message, window, 0, 0, PM_REMOVE))
				{
					if (message.message == WM_QUIT)
						global_running = false;
					else 
						DispatchMessage(&message);
				}
				for (u32 y = 0; y < global_graphics.height; y++)
					for (u32 x = 0; x < global_graphics.width; x++)
						((u32*)global_graphics.buffer)[y * global_graphics.width + x] = 0x00ffffff;
				win_update_window(window);
			}
		}
	}
}