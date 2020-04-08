#include <windows.h>
#include "gr.h"
#include "ninecalc.h"
#include "stb_truetype.h"

global bool32	global_running  = true;
global Graphics	global_graphics;
global State *state;

internal Memory
win_allocate_memory(u64 size, u64 base = 0)
{
	Memory memory = {};
	memory.contents = VirtualAlloc((LPVOID)base, size,
		MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (memory.contents)
	{
		memory.size = size;
	}
	return(memory);
}
internal void
win_free_memory(Memory *memory)
{
	VirtualFree(memory->contents, 0, MEM_RELEASE);
	*memory = {};
}

internal void *
allocate_bytes(Memory *arena, u64 size)
{
	assert(arena->used + size <= arena->size);
	void *memory = (u8*)arena->contents + arena->used;
	arena->used += size;
	return(memory);
}
#define allocate_struct(arena, type) (type *)allocate_bytes(arena, sizeof(type))
#define allocate_array(arena, type, count) (type *)allocate_bytes(arena, sizeof(type) * count)

internal void
free_bytes(Memory *arena, u64 size)
{
	assert(arena->used - size >= 0);
	arena->used -= size;
}

internal void
win_resize_backbuffer(Graphics *graphics, u32 width, u32 height)
{
	if (graphics->buffer)
	{
		VirtualFree(graphics->buffer, 0, MEM_RELEASE);
	}

	graphics->bytes_per_pixel = 4;
	graphics->width  = width;
	graphics->height = height;

	BITMAPINFOHEADER header = {};
	header.biSize 		 = sizeof(header);
	header.biWidth 		 = width;
	header.biHeight 	 = -(s64)height;
	header.biPlanes 	 = 1;
	header.biBitCount 	 = graphics->bytes_per_pixel * 8;
	header.biCompression = BI_RGB;
	graphics->bitmap_info.bmiHeader = header;

	u32 size = width * height * graphics->bytes_per_pixel;
	graphics->buffer = VirtualAlloc(0, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

internal void
win_update_window(HWND window, HDC device_context, Graphics *graphics)
{
	RECT client_rect;
	GetClientRect(window, &client_rect);
	u32 dest_width  = client_rect.right - client_rect.left;
	u32 dest_height = client_rect.bottom - client_rect.top;
	StretchDIBits(device_context,
		/*destination x, y, w, h:*/ 0, 0, dest_width, dest_height,
		/*source x, y, w, h:*/      0, 0, graphics->width, graphics->height,
		graphics->buffer, &graphics->bitmap_info,
		DIB_RGB_COLORS, SRCCOPY);
}

internal void
win_update_window(HWND window, Graphics *graphics)
{
	HDC device_context = GetDC(window);
	win_update_window(window, device_context, graphics);
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
			win_resize_backbuffer(&global_graphics, width, height);
		} break;
		case WM_PAINT:
		{
			PAINTSTRUCT paint;
			HDC device_context = BeginPaint(window, &paint);
			win_update_window(window, device_context, &global_graphics);
			EndPaint(window, &paint);
		} break;
		case WM_KEYDOWN:
		{
			switch (wparam)
			{
				case VK_UP:
				{
					state->current_line = maximum(state->current_line - 1, 0);
				} break;
				case VK_DOWN:
				{
					state->current_line++;
				} break;
				case VK_RIGHT:
				{
					state->current_cursor_position++;
				} break;
				case VK_LEFT:
				{
					state->current_cursor_position = maximum(state->current_cursor_position - 1, 0);
				} break;
			}
		} break;
		case WM_MOUSEMOVE:
		{
			state->mouse_x = (s16)(lparam & 0xffff);
			state->mouse_y = (s16)((lparam >> 16) & 0xffff);
		} break;
		default:
		{
			result = DefWindowProc(window, message, wparam, lparam);
		} break;
	}
	return(result);
}

inline u32
coloru8(u8 red, u8 green, u8 blue, u8 alpha = 255)
{
	u32 color =
		((u32)alpha << 24) |
		((u32)red   << 16) |
		((u32)green <<  8) |
		((u32)blue  <<  0);
	return(color);
}
inline u32
coloru8(u8 luminosity, u8 alpha = 255)
{
	u32 color = coloru8(luminosity, luminosity, luminosity, alpha);
	return(color);
}

inline u32
colorf32(f32 red, f32 green, f32 blue, f32 alpha = 1.0f)
{
	assert(red   <= 1.0f);
	assert(green <= 1.0f);
	assert(blue  <= 1.0f);
	assert(alpha <= 1.0f);

	u32 color = coloru8(
		(u8)(red   * 255.0f),
		(u8)(green * 255.0f),
		(u8)(blue  * 255.0f),
		(u8)(alpha * 255.0f));
	return(color);
}

inline u32
colorf32(f32 luminosity, f32 alpha = 1.0f)
{
	u32 color = colorf32(luminosity, luminosity, luminosity, alpha);
	return color;
}

inline u32
alpha_blend(u32 source, u32 destination)
{
	u32 result;

	u32 source_alpha		= (source      >> 24) & 0xFF;
	u32 destination_alpha	= (destination >> 24) & 0xFF;
	u32 complement_alpha	= destination_alpha * (255 - source_alpha) / 255;
	u32 out_alpha			= source_alpha + complement_alpha;

	if (out_alpha)
	{
		u32 source_red   = (source >> 16) & 0xFF;
		u32 source_green = (source >>  8) & 0xFF;
		u32 source_blue  = (source >>  0) & 0xFF;

		u32 destination_red   = (destination >> 16) & 0xFF;
		u32 destination_green = (destination >>  8) & 0xFF;
		u32 destination_blue  = (destination >>  0) & 0xFF;

		u8 out_red   = (u8)((source_red   * source_alpha + destination_red   * complement_alpha) / out_alpha);
		u8 out_green = (u8)((source_green * source_alpha + destination_green * complement_alpha) / out_alpha);
		u8 out_blue  = (u8)((source_blue  * source_alpha + destination_blue  * complement_alpha) / out_alpha);

		result = coloru8(out_red, out_green, out_blue, (u8)out_alpha);
	}
	else
	{
		result = coloru8(0, 0);
	}

	return(result);
}

internal void
draw_rect (Graphics *graphics, s32 min_x, s32 min_y, s32 max_x, s32 max_y, u32 color)
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
			*(buffer++) = alpha_blend(color, *buffer);
		buffer += stride;
	}
}

internal u8 *
win_read_file(Memory *memory, char *file_path)
{
	void *buffer = 0;

	HANDLE file = CreateFile(file_path,
	  /*access:*/GENERIC_READ,
	  /*allow other processes to:*/FILE_SHARE_READ,
	  0, OPEN_EXISTING, 0, 0);

	u32 file_size = GetFileSize(file, 0);
	buffer = allocate_bytes(memory, file_size);

	u32 bytes_read;	
	if (!ReadFile(file, buffer, file_size, (LPDWORD)&bytes_read, 0))
	{
		free_bytes(memory, file_size);
		buffer = 0;
	}

	CloseHandle(file);

	return((u8*)buffer);
}

internal void
draw_bitmap(Graphics *graphics, u32 *buffer, s32 left, s32 top, u32 width, u32 height)
{
	u32 min_x = maximum(left, 0);
	u32 min_y = maximum(top, 0);
	u32 offscreen_left = min_x - left;
	u32 offscreen_top = min_y - top;

	u32 max_x = minimum(left + width , graphics->width);
	u32 max_y = minimum(top  + height, graphics->height);
	u32 offscreen_right  = width  + left - max_x;
	u32 offscreen_bottom = height + top  - max_y;

	u32 *source = buffer + offscreen_top * width + offscreen_left;
	u32 source_stride = offscreen_right + offscreen_left;

	u32 *destination = (u32*)graphics->buffer + min_y * graphics->width + min_x;
	u32 destination_stride = graphics->width + min_x - max_x;

	for (u32 y = min_y; y < max_y; y++)
	{
		for (u32 x = min_x; x < max_x; x++)
		{
			*(destination++) = alpha_blend(*(source++), *destination);
		}
		source += source_stride;
		destination += destination_stride;
	}
}

internal void
draw_text_character(Graphics *graphics, Font font, u32 codepoint, s32 x, s32 y, u32 color)
{

}

internal Font 
win_load_font(Memory *memory, char *ttf_filepath, u32 line_height)
{
	Font loaded_font = {};
	// init line_height
	loaded_font.line_height = line_height;

	u8* font_data = win_read_file(memory, ttf_filepath);

	stbtt_fontinfo font;
	stbtt_InitFont(&font, font_data, 0);

	f32 scale = stbtt_ScaleForPixelHeight(&font, (f32)line_height);

	{ // init baseline_from_top
		s32 temp_basline;
		stbtt_GetFontVMetrics(&font, &temp_basline, 0, 0);
		loaded_font.baseline_from_top = (u32)((f32)temp_basline * scale);
	}

	{ // init ranges
		u32 ranges[][2] = { {32, 126} };
		loaded_font.n_ranges = array_count(ranges);
		loaded_font.ranges = (u32 (*)[2])allocate_bytes(memory, sizeof(ranges));
		for (u32 i = 0; i < sizeof(ranges) / 4; i++)
		{
			*((u32*)loaded_font.ranges + i) = ranges[0][i];
		}
	}

	{ // init glyphs
		u32 n_glyphs = 0;
		for (u32 i = 0; i < loaded_font.n_ranges; i++)
		{
			u32 *range = loaded_font.ranges[i];
			n_glyphs += range[1] - range[0] + 1;
		}
		loaded_font.glyphs = allocate_array(memory, Glyph, n_glyphs);
	}

	u32 current_glyph = 0;
	for (u32 j = 0; j < loaded_font.n_ranges; j++)
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

			glyph->buffer = (u8*)allocate_bytes(memory, glyph->width * glyph->height);

			stbtt_MakeGlyphBitmap(&font, glyph->buffer,
				glyph->width, glyph->height,
				/*stride:*/glyph->width,
				scale, scale,
				glyph_index);
		}
	}

	return(loaded_font);
}

internal Glyph *
get_glyph(Font font, u32 codepoint)
{
	Glyph *glyph = 0;
	u32 glyph_offset = 0;
	for (u32 i = 0; i < font.n_ranges; i++)
	{
		u32 *range = font.ranges[i];
		if (codepoint >= range[0] && codepoint <= range[1])
		{
			glyph = font.glyphs + glyph_offset + codepoint - range[0];
			break;
		}
	}
	return(glyph);
}

int
WinMain (HINSTANCE instance, HINSTANCE _, LPSTR command_line, int __)
{
	#ifdef DEBUG
		u64 memory_base = tebibytes(2);
	#else
		u64 memory_base = 0;
	#endif

	Memory memory = win_allocate_memory(mebibytes(5), memory_base);
	state  = allocate_struct(&memory, State);

	WNDCLASSEX window_class = {};
	window_class.cbSize = sizeof(window_class);
	window_class.lpfnWndProc = win_callback;
	window_class.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	window_class.hInstance = instance;
	window_class.hIcon = 0;
	window_class.lpszClassName = "NineCalcClass";

	if (RegisterClassEx(&window_class))
	{
		HWND window = CreateWindowEx(WS_EX_APPWINDOW, window_class.lpszClassName,
			/*window title:*/ "NineCalc",
			/*styles:*/ WS_OVERLAPPEDWINDOW | WS_VISIBLE /*| WS_VSCROLL/**/,
			/*position:*/ CW_USEDEFAULT, CW_USEDEFAULT,
			/*size:*/ 640, 480,
			0, 0, instance, 0);

		if (window)
		{
			state->loaded_font = win_load_font(&memory, "data/consola.ttf", 24);
			state->line_number_bar_width = 30;
			state->caret_width = 1;

			while (global_running)
			{
				MSG message;
				while (PeekMessage(&message, window, 0, 0, PM_REMOVE))
				{
					TranslateMessage(&message);
					DispatchMessage(&message);
				}

				u32 width  = global_graphics.width;
				u32 height = global_graphics.height;

				u32 line_number_bar_width	= state->line_number_bar_width;
				u32 line_height			    = state->loaded_font.line_height;
				u32 line				    = state->current_line;
				u32 caret_x					= state->caret_x;
				u32 caret_width			    = state->caret_width;
				u32 cursor_position		    = state->current_cursor_position;

				u32 baseline = state->loaded_font.baseline_from_top;

				// background
				draw_rect(&global_graphics, 0, 0, width, height, colorf32(1));

				// line number sidebar
				draw_rect(&global_graphics, 0, 0, line_number_bar_width, height, colorf32(0.9f));

				// line highlight
				draw_rect(&global_graphics,
					line_number_bar_width, line * line_height,
					width 			 , (line + 1) * line_height,
					colorf32(0.95f));

				// line number highlight
				draw_rect(&global_graphics,
					0				     , line * line_height,
					line_number_bar_width, (line + 1) * line_height,
					colorf32(0.85f));

				// caret
				draw_rect(&global_graphics,
					line_number_bar_width + caret_x			     , line * line_height,
					line_number_bar_width + caret_x + caret_width, (line + 1) * line_height,
					colorf32(0));

				draw_rect(&global_graphics, 0, line * line_height + baseline, global_graphics.width, line * line_height + baseline + 1, colorf32(0));

				u8 test[] = "This is a test line.";//"Ghis is a test line";
#if 0
				Glyph *glyph = get_glyph(state->loaded_font, 'G');
				persistent u32 *buffer = allocate_array(&memory, u32, glyph->width * glyph->height);
				for (u32 i = 0; i < glyph->width * glyph->height; i++)
				{
					buffer[i] = alpha_blend(coloru8(0, 0, 0, glyph->buffer[i]), buffer[i]);
				}
				u32 half_width = glyph->width / 2;
				u32 half_height = glyph->height / 2;
				draw_bitmap(&global_graphics, buffer,
					state->mouse_x - half_width, state->mouse_y - half_height,
					glyph->width, glyph->height);
#else
				u32 current_x = 0;
				for (u8 *codepoint = &test[0]; *codepoint; codepoint++)
				{
					Glyph *glyph = get_glyph(state->loaded_font, (u32)(*codepoint));
					persistent u32 *buffer = allocate_array(&memory, u32, glyph->width * glyph->height * 2);
					for (u32 i = 0; i < glyph->width * glyph->height; i++)
					{
						buffer[i] = coloru8(0, 0);
						buffer[i] = alpha_blend(coloru8(0, 0, 0, glyph->buffer[i]), buffer[i]);
					}
					draw_bitmap(&global_graphics, buffer,
						line_number_bar_width - glyph->x + current_x, line_height * line + baseline - glyph->y,
						glyph->width, glyph->height);
					current_x += glyph->advance;
				}
#endif

				win_update_window(window, &global_graphics);
			}
		}
	}
}