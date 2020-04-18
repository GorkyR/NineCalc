#include "grs.h"
#include "memory.h"
#include "ninecalc.cpp"

#include "stb_truetype.h"
#include <windows.h>
#include <stdio.h>

#include "calculator.h"

struct WIN_Graphics
{
	BITMAPINFO bitmap_info;
	u8  bytes_per_pixel;
	Canvas canvas;
};

global bool32 global_running  = true;
global WIN_Graphics win_graphics;
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

internal void
win_resize_backbuffer(WIN_Graphics *graphics, u32 width, u32 height)
{
	if (graphics->canvas.buffer)
	{
		VirtualFree(graphics->canvas.buffer, 0, MEM_RELEASE);
	}

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
	graphics->canvas.buffer = (u32*)VirtualAlloc(0, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

internal void
win_update_window(HWND window, HDC device_context, WIN_Graphics *graphics)
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
win_update_window(HWND window, WIN_Graphics *graphics)
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
			win_resize_backbuffer(&win_graphics, width, height);
		} break;
		case WM_PAINT:
		{
			PAINTSTRUCT paint;
			HDC device_context = BeginPaint(window, &paint);
			win_update_window(window, device_context, &win_graphics);
			EndPaint(window, &paint);
		} break;
		case WM_KEYDOWN:
		{
			switch (wparam)
			{
				case VK_UP:
				{
					String text = state->text;
					u64 offset = 0;

					u64 prev_line_start = 0, line_start = 0;
					bool32 is_first_line = true;
					for (u64 i = 0; i < text.length; i++)
					{
						if (i == state->cursor_position)
						{
							offset = i - line_start;
							break;
						}
						if (text[i] == '\n')
						{
							prev_line_start = line_start;
							line_start = i + 1;
							is_first_line = false;
						}
					}
					if (state->cursor_position == text.length)
					{
						offset = text.length - line_start;
					}

					if (!is_first_line)
					{
						state->cursor_position = minimum(prev_line_start + offset, line_start - 1);
					}
				} break;
				case VK_DOWN:
				{
					String text = state->text;
					u64 offset = 0;

					u64 line_start = 0, next_line_start = 0, next_line_end = text.length;
					bool32 current_line_found = false, next_line_found = false;
					bool32 is_last_line = true;
					for (u64 i = 0; i < text.length; i++)
					{
						if (i == state->cursor_position)
						{
							current_line_found = true;
							offset = i - line_start;
						}
						if (text[i] == '\n')
						{
							if (current_line_found)
							{
								if (next_line_found)
								{
									next_line_end = i + 1;
									break;
								}
								next_line_start = i + 1;
								is_last_line = false;
								next_line_found = true;
							}
							else
							{
								line_start = i + 1;
							}
						}
					}

					if (!is_last_line)
					{
						state->cursor_position = minimum(next_line_start + offset, next_line_end);
					}
				} break;/**/
				case VK_RIGHT:
				{
					state->cursor_position = minimum(state->cursor_position + 1, (s32)state->text.length);
				} break;
				case VK_LEFT:
				{
					state->cursor_position = maximum(state->cursor_position - 1, 0);
				} break;
				case VK_RETURN:
				{
					insert_character_if_fits(&state->text, '\n', state->cursor_position++);
				} break;
				case VK_BACK:
				{
					if (state->cursor_position > 0)
					{
						remove(&state->text, --state->cursor_position, 1);
					}
				} break;
				case VK_DELETE:
				{
					if (state->cursor_position < state->text.length)
					{
						remove(&state->text, state->cursor_position, 1);
					}
				} break;
				case VK_HOME:
				{
					// @TODO
				} break;
				case VK_END:
				{
					// @TODO
				} break;
			}
		} break;
		case WM_UNICHAR:
		case WM_CHAR:
		{
			if (wparam == UNICODE_NOCHAR) {
				result = true;
			}
			u32 character = (u32)wparam;

			// If the codepoint is in the loaded font...
			for (u64 i = 0; i < state->loaded_font.n_ranges; i++)
			{
				u32 *range = state->loaded_font.ranges[i];
				if (character <= range[1] && character >= range[0])
				{
					// ...then write it.
					insert_character_if_fits(&state->text, character, state->cursor_position++);
					break;
				}
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
		buffer = 0;
	}

	CloseHandle(file);

	return((u8*)buffer);
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
	state->loaded_font = win_load_font(&memory, "data/fira.ttf", 24);
	state->text = make_string(&memory, kibibytes(1));
	state->line_number_bar_width = 40;
	state->caret_width = 1;

	Memory temp = allocate_arena(&memory, kibibytes(50));

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

			while (global_running)
			{
				MSG message;
				while (PeekMessage(&message, window, 0, 0, PM_REMOVE))
				{
					TranslateMessage(&message);
					DispatchMessage(&message);
				}

				Canvas canvas = win_graphics.canvas;
				u32 width  = canvas.width;
				u32 height = canvas.height;

				u32 line_number_bar_width	= state->line_number_bar_width;
				u32 caret_width			    = state->caret_width;
				u64 cursor_position		    = state->cursor_position;

				Font font = state->loaded_font;

				s32 h_offset = state->line_number_bar_width;

				// background
				draw_rect(&canvas, 0, 0, width, height, colorf32(1));

				// line number sidebar
				draw_rect(&canvas, 0, 0, h_offset, height, colorf32(0.9f));

				u32 current_line = get_current_line(state->text, state->cursor_position);

				{ 	// line highlight
					draw_rect(&canvas,
						h_offset, current_line * font.line_height,
						width 			     , (current_line + 1) * font.line_height,
						colorf32(0.95f));

					// line number highlight
					draw_rect(&canvas,
						0       , current_line * font.line_height,
						h_offset, (current_line + 1) * font.line_height,
						colorf32(0.85f));
				}

				// Write the usage code first!

				String numbers[50];
				for (u32 i = 1; i <= array_count(numbers); i++)
				{
					char buffer[3];
					sprintf_s(&buffer[0], sizeof(buffer), "%d", i);
					numbers[i - 1] = make_string_from_chars(&temp, &buffer[0]);
				}

				u64 n_lines;
				String *lines = split_lines(&temp, state->text, &n_lines);

				for (u64 i = 0; i < n_lines; i++)
				{
					String line = lines[i];

					s32 v_offset = (s32)(font.line_height * i + font.baseline_from_top);

					if (i == current_line)
					{
						// caret
						s32 caret_offset = 0;
						get_text_width(&font, line, cursor_position - (u64)(line.data - state->text.data), &caret_offset);
						draw_rect(&canvas,
							h_offset + caret_offset			     , current_line * font.line_height,
							h_offset + caret_offset + caret_width, (current_line + 1) * font.line_height,
							coloru8(0));
					}

					// line numbers ???
					s32 line_number_width = get_text_width(&font, numbers[i]);
					draw_text(&canvas, &font, numbers[i],
						h_offset - line_number_width, v_offset,
						(i == current_line)? coloru8(0) : coloru8(0, 128));

                    // line content
					draw_text(&canvas, &font, line, h_offset, v_offset, coloru8(0));

					Result evaluation = evaluate(&temp, line);
					if (evaluation.valid)
					{
						String result;
						if (evaluation.type == Number_Type::Integer)
							result = convert_s64_to_string(&temp, evaluation.value.integer);
						else
							result = convert_f64_to_string(&temp, evaluation.value.real);
						s32 result_width = get_text_width(&font, result);
						draw_text(&canvas, &font, result, width - result_width, v_offset, coloru8(0, 128));
					}
				}

				win_update_window(window, &win_graphics);
				free_arena(temp);
			}
		}
	}
}