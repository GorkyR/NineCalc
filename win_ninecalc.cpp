#include "ninecalc.cpp"
#include "stb_truetype.h"

#include <windows.h>
#include <stdio.h>

struct Windows_Graphics
{
	BITMAPINFO bitmap_info;
	u8  bytes_per_pixel;
	Canvas canvas;
};

         int     WinMain(HINSTANCE, HINSTANCE, LPSTR, int);
internal LRESULT win_callback(HWND, UINT, WPARAM, LPARAM);

internal u8*  win_allocate(u64, void* =0);
internal void win_free    (void*);

internal void win_resize_backbuffer(Windows_Graphics *, u32, u32);
internal void win_update_window(HWND, HDC, Windows_Graphics*);
internal void win_update_window(HWND, Windows_Graphics*);

internal Memory_Arena win_allocate_arena(u64, void* =0);
internal void         win_free_arena    (Memory_Arena*);

internal u8* win_read_file(char*);

internal inline s64 win_monitor_refresh_rate(HWND);

internal inline s64 win_ticks_per_second();
internal inline s64 win_tick();
internal inline s64 win_microseconds_elapsed (s64, s64);
internal inline s64 win_microseconds_elapsed_since(s64);

internal inline void update_button(Input_Button*, bool, bool =false);
internal inline void reset_button        (Input_Button*);
internal inline void reset_keyboard_input(Keyboard_Input*);
internal inline void reset_mouse_input   (Mouse_Input*);

internal Platform_Load_Font          win_load_font;
internal Platform_Unload_Font        win_unload_font;
internal Platform_Push_To_Clipboard  win_push_to_clipboard;
internal Platform_Pop_From_Clipboard win_pop_from_clipboard;

//-------------------------------------------------------------------

global bool             application_is_running;
global Windows_Graphics win_graphics;
global Memory_Arena     arena;
global State            *state;
global Platform         win_platform;
global Time_Input       time;
global Keyboard_Input   keyboard;
global Mouse_Input      mouse;

global s64 start_timestamp;
global s64 timestamp;

int
WinMain (HINSTANCE instance, HINSTANCE _, LPSTR command_line, int __)
{
	start_timestamp = win_tick();

	#ifdef DEBUG
		u64 memory_base = tebibytes(2);
	#else
		u64 memory_base = 0;
	#endif

	arena = win_allocate_arena(mebibytes(5), (void*)memory_base);
	state = (State*)arena.data;

	win_platform.load_font          = win_load_font;
	win_platform.push_to_clipboard  = win_push_to_clipboard;
	win_platform.pop_from_clipboard = win_pop_from_clipboard;
	
	WNDCLASSEX window_class = {};
	window_class.cbSize = sizeof(window_class);
	window_class.lpfnWndProc = win_callback;
	window_class.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	window_class.hInstance = instance;
	window_class.hIcon = 0;
	window_class.hCursor = LoadCursor(0, IDC_IBEAM);
	window_class.lpszClassName = "NineCalcClass";

	if (RegisterClassEx(&window_class))
	{
		HWND window = CreateWindowEx(WS_EX_APPWINDOW, window_class.lpszClassName,
			/*window title:*/ "NineCalc",
			/*styles:*/ WS_OVERLAPPEDWINDOW | WS_VISIBLE /*| WS_VSCROLL/**/,
			/*position:*/ CW_USEDEFAULT, CW_USEDEFAULT,
			/*size:*/ 480, 360,
			0, 0, instance, 0);

		if (window)
		{
			timeBeginPeriod(1);

			// s64 target_frame_rate = win_monitor_refresh_rate(window);
			s64 target_frame_rate = 30;
			s64 target_microseconds_per_frame = 1000000 / target_frame_rate;
			s64 target_milliseconds_per_frame = 1000 / target_frame_rate;

			timestamp = start_timestamp;
			s64 delta_microseconds = 0;

			application_is_running = true;

			MSG message;
			while (application_is_running && GetMessage(&message, window, 0, 0))
			{
				do
				{
					TranslateMessage(&message);
					DispatchMessage(&message);
				} while (PeekMessage(&message, window, 0, 0, PM_REMOVE));

				time = {};
				time.elapsed = win_microseconds_elapsed(start_timestamp, timestamp),
				time.delta   = delta_microseconds;

				update_and_render(&arena, &win_platform, &win_graphics.canvas, &time, &keyboard, &mouse);
				reset_keyboard_input(&keyboard);
				reset_mouse_input(&mouse);

				delta_microseconds = win_microseconds_elapsed_since(timestamp);
				while (delta_microseconds < target_microseconds_per_frame)
				{
					s64 sleep_milliseconds = (target_microseconds_per_frame - delta_microseconds) / 1000;
					Sleep((u32)sleep_milliseconds);
					delta_microseconds = win_microseconds_elapsed_since(timestamp);
				}
				timestamp = win_tick();

				win_update_window(window, &win_graphics);
			}
		}
	}
}

internal LRESULT
win_callback (HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	LRESULT result = 0;
	switch (message)
	{
		case WM_ACTIVATE:
		{
			// if (wparam == WA_INACTIVE)
			// 	KillTimer(window, 1);
			// else 
			// 	SetTimer(window, 1, (u32)target_milliseconds_per_frame, 0);
		} break;
		case WM_CLOSE:
		case WM_QUIT:
		case WM_DESTROY:
		{
			application_is_running = false;
		} break;
		case WM_SIZE:
		{
			RECT client_rect;
			GetClientRect(window, &client_rect);
			u32 width  = client_rect.right - client_rect.left;
			u32 height = client_rect.bottom - client_rect.top;
			win_resize_backbuffer(&win_graphics, width, height);
		} break;
		case WM_PAINT:
		{
			PAINTSTRUCT paint;
			HDC device_context = BeginPaint(window, &paint);

			s64 new_timestamp = win_tick();
			time.elapsed = win_microseconds_elapsed(start_timestamp, new_timestamp);
			time.delta   = win_microseconds_elapsed(timestamp, new_timestamp);
			timestamp = new_timestamp;
			update_and_render(&arena, &win_platform, &win_graphics.canvas, &time, &keyboard, &mouse);

			win_update_window(window, device_context, &win_graphics);
			EndPaint(window, &paint);
		} break;
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			persistent bool control_key_is_down = false;
			persistent bool shift_key_is_down = false;

			bool key_was_down = (lparam & (1 << 30)) != 0;
			bool key_is_down  = (lparam & (1 << 31)) == 0;

			if (wparam == VK_SHIFT)
				shift_key_is_down = key_is_down;
			else if (wparam == VK_CONTROL)
				control_key_is_down = key_is_down;

			else if (wparam == VK_UP)
			{
				if (shift_key_is_down)
					update_button(&keyboard.select_up, key_is_down, true);
				else
					update_button(&keyboard.up, key_is_down, true);
			}
			else if (wparam == VK_RIGHT)
			{
				if (shift_key_is_down)
					update_button(&keyboard.select_right, key_is_down, true);
				else
					update_button(&keyboard.right, key_is_down, true);
			}
			else if (wparam == VK_DOWN)
			{
				if (shift_key_is_down)
					update_button(&keyboard.select_down, key_is_down, true);
				else
					update_button(&keyboard.down, key_is_down, true);
			}
			else if (wparam == VK_LEFT)
			{
				if (shift_key_is_down)
					update_button(&keyboard.select_left, key_is_down, true);
				else
					update_button(&keyboard.left, key_is_down, true);
			}
			else if (wparam == VK_RETURN) update_button(&keyboard.enter    , key_is_down, true);
			else if (wparam == VK_BACK)   update_button(&keyboard.backspace, key_is_down, true);
			else if (wparam == VK_DELETE) update_button(&keyboard.del      , key_is_down, true);
			else if (wparam == VK_HOME)   update_button(&keyboard.home     , key_is_down, true);
			else if (wparam == VK_END)    update_button(&keyboard.end      , key_is_down, true);

			else if (wparam == 'X') update_button(&keyboard.cut  , key_is_down && control_key_is_down);
			else if (wparam == 'C') update_button(&keyboard.copy , key_is_down && control_key_is_down);
			else if (wparam == 'V') update_button(&keyboard.paste, key_is_down && control_key_is_down);
			else if (wparam == 'S') update_button(&keyboard.save , key_is_down && control_key_is_down);
		} break;
		case WM_UNICHAR:
		case WM_CHAR:
		{
			if (wparam == UNICODE_NOCHAR)
				result = true;
			u32 character = (u32)wparam;
			if (codepoint_is_in_font(state->font, character))
				insert_character_if_fits(&keyboard.input_buffer, character, keyboard.input_buffer.length);
		} break;
		case WM_MOUSEMOVE:
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
		case WM_MBUTTONUP:
		{
			mouse.x = (s16)(lparam & 0xffff);
			mouse.y = (s16)((lparam >> 16) & 0xffff);

			update_button(&mouse.left  , wparam & MK_LBUTTON);
			update_button(&mouse.right , wparam & MK_RBUTTON);
			update_button(&mouse.middle, wparam & MK_MBUTTON);
		} break;
		case WM_MOUSEWHEEL:
		{
			persistent s16 scroll_distance = (s16)(state->font.line_height);
			s16 wheel_delta = GET_WHEEL_DELTA_WPARAM(wparam) * scroll_distance / WHEEL_DELTA;
			state->scroll_offset = (u32)clamp((s64)state->scroll_offset - wheel_delta,
				0, (state->document.line_count - 1) * state->font.line_height);
		} break;
		default:
		{
			result = DefWindowProc(window, message, wparam, lparam);
		} break;
	}
	return result;
}

internal u8 *
win_allocate(u64 size, void *base)
{
	u8 *data = (u8*)VirtualAlloc((LPVOID)base, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	return data;
}

internal void
win_free(void *buffer)
{
	VirtualFree(buffer, 0, MEM_RELEASE);
}

internal void
win_resize_backbuffer(Windows_Graphics *graphics, u32 width, u32 height)
{
	if (graphics->canvas.buffer)
		win_free(graphics->canvas.buffer);

	graphics->bytes_per_pixel = 4;
	graphics->canvas.width  = width;
	graphics->canvas.height = height;

	BITMAPINFOHEADER header = {};
	header.biSize 		 = sizeof(header);
	header.biWidth 		 = width;
	header.biHeight 	 = -(s64)height;
	header.biPlanes 	 = 1;
	header.biBitCount 	 = graphics->bytes_per_pixel * 8;
	header.biCompression = BI_RGB;
	graphics->bitmap_info.bmiHeader = header;

	u32 size = width * height * graphics->bytes_per_pixel;
	graphics->canvas.buffer = (u32*)win_allocate(size);
}

internal void
win_update_window(HWND window, HDC device_context, Windows_Graphics *graphics)
{
	RECT client_rect;
	GetClientRect(window, &client_rect);
	u32 dest_width  = client_rect.right - client_rect.left;
	u32 dest_height = client_rect.bottom - client_rect.top;
	StretchDIBits(device_context,
		/*destination x, y, w, h:*/ 0, 0, dest_width, dest_height,
		/*source x, y, w, h:*/      0, 0, graphics->canvas.width, graphics->canvas.height,
		graphics->canvas.buffer, &graphics->bitmap_info,
		DIB_RGB_COLORS, SRCCOPY);
}

internal void
win_update_window(HWND window, Windows_Graphics *graphics)
{
	HDC device_context = GetDC(window);
	win_update_window(window, device_context, graphics);
	ReleaseDC(window, device_context);
}

internal Memory_Arena
win_allocate_arena(u64 size, void *base)
{
	Memory_Arena newly_allocated_arena = {};
	newly_allocated_arena.data = win_allocate(size, base);
	if (newly_allocated_arena.data)
	{
		newly_allocated_arena.size = size;
	}
	return newly_allocated_arena;
}

internal void
win_free_arena(Memory_Arena *memory_arena)
{
	win_free((void*)memory_arena->data);
	*memory_arena = {};
}

internal u8 *
win_read_file(char *file_path)
{
	void *buffer = 0;

	HANDLE file = CreateFile(file_path,
	  /*access:*/GENERIC_READ,
	  /*allow other processes to:*/FILE_SHARE_READ,
	  0, OPEN_EXISTING, 0, 0);

	u32 file_size = GetFileSize(file, 0);
	buffer = win_allocate(file_size);

	u32 bytes_read;	
	if (!ReadFile(file, buffer, file_size, (LPDWORD)&bytes_read, 0))
	{
		buffer = 0;
	}

	CloseHandle(file);

	return (u8*)buffer;
}

internal Font 
win_load_font(/*Memory_Arena *memory_arena,*/ char *ttf_filepath, u32 line_height)
{
	Font loaded_font = {};
	// init line_height
	loaded_font.line_height = line_height;

	u8* font_data = win_read_file(ttf_filepath);

	stbtt_fontinfo font;
	stbtt_InitFont(&font, font_data, 0);

	f32 scale = stbtt_ScaleForPixelHeight(&font, (f32)line_height);

	{ // init baseline
		s32 temp_basline;
		stbtt_GetFontVMetrics(&font, &temp_basline, 0, 0);
		loaded_font.baseline = (u32)((f32)temp_basline * scale);
	}

	{ // init ranges
		u32 ranges[][2] = { {32, 126} };
		loaded_font.range_count = array_count(ranges);
		// loaded_font.ranges = (u32 (*)[2])allocate_bytes(memory_arena, sizeof(ranges));
		loaded_font.ranges = (u32 (*)[2])win_allocate(sizeof(ranges));
		for (u32 i = 0; i < sizeof(ranges) / 4; i++)
		{
			*((u32*)loaded_font.ranges + i) = ranges[0][i];
		}
	}

	{ // init glyphs
		for (u32 i = 0; i < loaded_font.range_count; i++)
		{
			u32 *range = loaded_font.ranges[i];
			loaded_font.glyph_count += range[1] - range[0] + 1;
		}
		// loaded_font.glyphs = allocate_array(memory_arena, Glyph, loaded_font.glyph_count);
		loaded_font.glyphs = (Glyph *)win_allocate(sizeof(Glyph) * loaded_font.glyph_count);
	}

	u32 current_glyph = 0;
	for (u32 j = 0; j < loaded_font.range_count; j++)
	{
		for (u32 i = loaded_font.ranges[j][0]; i <= loaded_font.ranges[j][1]; i++)
		{
			Glyph* glyph = loaded_font.glyphs + (current_glyph++);

			u32 glyph_index = stbtt_FindGlyphIndex(&font, i);

			s32 x0, y0, x1, y1;
			stbtt_GetGlyphBitmapBox(&font, glyph_index, scale, scale, &x0, &y0, &x1, &y1);
			glyph->x = -x0;
			glyph->y = -y0;
			glyph->width = x1 - x0;
			glyph->height = y1 - y0;

			{ // init glyph.advance
				s32 advance, lsb;
				stbtt_GetGlyphHMetrics(&font, glyph_index, &advance, &lsb);
				glyph->advance = (u32)((f32)advance * scale);
			}

			{
				// glyph->buffer = allocate_bytes(memory_arena, glyph->width * glyph->height);
				glyph->buffer = win_allocate(glyph->width * glyph->height);

				stbtt_MakeGlyphBitmap(&font, glyph->buffer,
					glyph->width, glyph->height,
					/*stride:*/glyph->width,
					scale, scale,
					glyph_index);
			}
		}
	}

	win_free(font_data);

	return loaded_font;
}

internal void
win_unload_font(Font *font)
{
	for (u64 i = 0; i < font->glyph_count; ++i)
		win_free(font->glyphs[i].buffer);
	win_free(font->glyphs);
	win_free(font->ranges);
	*font = {};
}

internal bool
win_push_to_clipboard(UTF32_String text)
{
	if (!OpenClipboard(0))
		return false;
	EmptyClipboard();
	HGLOBAL global_handle = GlobalAlloc(GMEM_MOVEABLE, text.length + 1);
	if (!global_handle)
	{
		CloseClipboard();
		return false;
	}
	LPTSTR copied_string = (LPTSTR)GlobalLock(global_handle);
	for (u64 i = 0; i < text.length; ++i)
		copied_string[i] = (u8)text[i];
	copied_string[text.length] = 0;
	GlobalUnlock(global_handle);
	SetClipboardData(CF_TEXT, global_handle);
	CloseClipboard();
	return true;
}

internal UTF32_String
win_pop_from_clipboard()
{
	UTF32_String result = {};
	if (OpenClipboard(0))
	{
		HGLOBAL global_handle = GetClipboardData(CF_TEXT);
		if (global_handle)
		{
			LPTSTR clipboard_string = (LPTSTR)GlobalLock(global_handle);
			if (clipboard_string)
			{
				// result = make_string_from_chars(memory_arena, clipboard_string);
				{
					u64 length = 0;
					while (clipboard_string[length])
						++length;
					result.data = (u32 *)win_allocate(sizeof(u32) * length);
					result.capacity = result.length = length;
					for (u64 i = 0; i < length; ++i)
						result.data[i] = clipboard_string[i];
				}
				GlobalUnlock(global_handle);
			}
		}
		CloseClipboard();
	}
	return result;
}

internal inline s64
win_monitor_refresh_rate(HWND window)
{
	HDC dc = GetDC(window);
	s64 refresh_rate = GetDeviceCaps(dc, VREFRESH);
	if (refresh_rate <= 1)
		refresh_rate = 30;
	ReleaseDC(window, dc);
	return refresh_rate;
}

internal inline s64
win_ticks_per_second()
{
	LARGE_INTEGER result;
	QueryPerformanceFrequency(&result);
	return result.QuadPart;
}

internal inline s64
win_tick()
{
	LARGE_INTEGER result;
	QueryPerformanceCounter(&result);
	return result.QuadPart;
}

internal inline s64
win_microseconds_elapsed(s64 start, s64 end)
{
	persistent s64 ticks_per_microsecond = win_ticks_per_second() / 1000000;
	s64 microseconds = (end - start) / ticks_per_microsecond;
	return microseconds;
}

internal inline s64
win_microseconds_elapsed_since(s64 start)
{
	s64 microseconds = win_microseconds_elapsed(start, win_tick());
	return microseconds;
}


internal inline void
update_button(Input_Button *button, bool is_down, bool repeat_key)
{
	if (button->is_down != is_down)
	{
		++button->transitions;
		button->is_down = is_down;
	}
	else if (repeat_key && button->is_down && is_down)
		button->transitions += 2;
}

internal inline void
reset_button(Input_Button *button)
{
	button->transitions = 0;
}

internal inline void
reset_keyboard_input(Keyboard_Input *input)
{
	reset_button(&input->up);
	reset_button(&input->right);
	reset_button(&input->down);
	reset_button(&input->left);
	reset_button(&input->enter);
	reset_button(&input->backspace);
	reset_button(&input->del);
	reset_button(&input->home);
	reset_button(&input->end);
	reset_button(&input->cut);
	reset_button(&input->copy);
	reset_button(&input->paste);
	reset_button(&input->save);
	input->input_buffer.length = 0;
}

internal inline void
reset_mouse_input(Mouse_Input *input)
{
	reset_button(&input->left);
	reset_button(&input->right);
	reset_button(&input->middle);
}