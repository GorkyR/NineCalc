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

typedef float 			f32;
typedef double			f64;

typedef i32 			bool32;
typedef i64 			bool64;

inline i32
minimum (i32 a, i32 b)
{
	i32 result = (a < b)? a : b;
	return(result);
}
inline i32
maximum (i32 a, i32 b)
{
	i32 result = (a < b)? b : a;
	return(result);
}
#define swap(a, b) { auto _ = a; a = b; b = _; }

struct Graphics
{
	void *buffer;
	BITMAPINFO bitmap_info;
	u8 bytes_per_pixel;
	u32 width;
	u32 height;
	u64 size;
};

struct State
{
	u32 line_height;
	u32 line;

	u32 character_width;
	u32 cursor_width;
	u32 cursor;

	u32 line_number_width;
};

global bool32	global_running  = true;
global Graphics	global_graphics = {};
global State    global_state    = {};

internal void
win_resize_backbuffer(u32 width, u32 height)
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
			win_resize_backbuffer(width, height);
		} break;
		case WM_PAINT:
		{
			PAINTSTRUCT paint;
			HDC device_context = BeginPaint(window, &paint);
			win_update_window(window, device_context);
			EndPaint(window, &paint);
		} break;
		case WM_KEYDOWN:
		{
			switch (wparam)
			{
				case VK_UP:
				{
					global_state.line = maximum(global_state.line - 1, 0);
				} break;
				case VK_DOWN:
				{
					global_state.line++;
				} break;
				case VK_RIGHT:
				{
					global_state.cursor++;
				} break;
				case VK_LEFT:
				{
					global_state.cursor = maximum(global_state.cursor - 1, 0);
				} break;
			}
		} break;
		default:
		{
			result = DefWindowProc(window, message, wparam, lparam);
		} break;
	}
	return(result);
}

internal void
draw_rect (Graphics *graphics, i32 min_x, i32 min_y, i32 max_x, i32 max_y, u32 color)
{
	if (min_x > max_x)
		swap(min_x, max_x);
	if (min_y > max_y)
		swap(min_y, max_y);

	u32 x_min = (u32)maximum(min_x, 0);
	u32 y_min = (u32)maximum(min_y, 0);
	u32 x_max = (u32)minimum(max_x, graphics->width);
	u32 y_max = (u32)minimum(max_y, graphics->height);

	u32 stride = graphics->width + x_min - x_max;
	u32 *buffer = (u32*)graphics->buffer + y_min * graphics->width + x_min;
	for (u32 y = y_min; y < y_max; y++)
	{
		for (u32 x = x_min; x < x_max; x++)
			*(buffer++) = color;
		buffer += stride;
	}
}

inline u32
color(f32 red, f32 green, f32 blue, f32 alpha = 1.0f)
{
	u32 result = 
		  ((u32)(alpha * 255) << 24)
		| ((u32)(red   * 255) << 16)
		| ((u32)(green * 255) << 8)
		| ((u32)(blue  * 255) << 0);

	return(result);
}

inline u32
color(f32 luminosity, f32 alpha = 1.0f)
{
	u32 result = color(luminosity, luminosity, luminosity, alpha);
	return result;
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
			global_state.line_height = 16;
			global_state.line = 0;

			global_state.character_width = 8;
			global_state.cursor_width = 1;
			global_state.cursor = 0;

			global_state.line_number_width = 30;

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

				u32 width  = global_graphics.width;
				u32 height = global_graphics.height;

				u32 line_number_width	= global_state.line_number_width;
				u32 line_height			= global_state.line_height;
				u32 line				= global_state.line;
				u32 character_width		= global_state.character_width;
				u32 cursor_width		= global_state.cursor_width;
				u32 cursor				= global_state.cursor;

				// background
				draw_rect(&global_graphics, 0, 0, width, height, color(1));

				// line number sidebar
				draw_rect(&global_graphics, 0, 0, line_number_width, height, color(0.9f));

				// line highlight
				draw_rect(&global_graphics,
					line_number_width, line * line_height,
					width 			 , (line + 1) * line_height,
					color(0.95f));

				// line number highlight
				draw_rect(&global_graphics,
					0				 , line * line_height,
					line_number_width, (line + 1) * line_height,
					color(0.85f));

				// caret
				draw_rect(&global_graphics,
					line_number_width + cursor * character_width, line * line_height,
					line_number_width + cursor * character_width + cursor_width, (line + 1) * line_height,
					color(0));

				win_update_window(window);
			}
		}
	}
}