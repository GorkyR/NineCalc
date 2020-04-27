#include "ninecalc.h"

/*
	@TODO:
	- Calculator
	  	- Functions (?)

	- Editor
	  - Copy/Paste
	  - Copy result
	  - Save .txt (with results)

	  - Token formating?
	  - Highlighting?
*/

#define array_count(array) (sizeof(array) / sizeof(array[0]))

inline u64 kibibytes(u64 n) { return(n << 10); }
inline u64 mebibytes(u64 n) { return(n << 20); }
inline u64 gibibytes(u64 n) { return(n << 30); }
inline u64 tebibytes(u64 n) { return(n << 40); }

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

void
draw_rect (Canvas *graphics, s32 min_x, s32 min_y, s32 max_x, s32 max_y, u32 color)
{
	if (min_x > max_x)
		swap(min_x, max_x);
	if (min_y > max_y)
		swap(min_y, max_y);

	u32 x_min = (u32)maximum(min_x, 0);
	u32 y_min = (u32)maximum(min_y, 0);
	u32 x_max = (u32)clamp(max_x, 0, graphics->width);
	u32 y_max = (u32)clamp(max_y, 0, graphics->height);

	u32 stride = graphics->width + x_min - x_max;
	u32 *buffer = (u32*)graphics->buffer + y_min * graphics->width + x_min;
	for (u32 y = y_min; y < y_max; y++)
	{
		for (u32 x = x_min; x < x_max; x++)
			*(buffer++) = alpha_blend(color, *buffer);
		buffer += stride;
	}
}


void
draw_bitmap(Canvas *graphics, u32 *buffer, s32 left, s32 top, u32 width, u32 height)
{
	u32 min_x = (u32)maximum(left, 0);
	u32 min_y = (u32)maximum(top, 0);
	u32 offscreen_left = min_x - left;
	u32 offscreen_top = min_y - top;

	u32 max_x = (u32)minimum(left + width , graphics->width);
	u32 max_y = (u32)minimum(top  + height, graphics->height);
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


Glyph *
get_glyph(Font *font, u32 codepoint)
{
	Glyph *glyph = 0;
	u32 glyph_offset = 0;
	for (u32 i = 0; i < font->n_ranges; i++)
	{
		u32 *range = font->ranges[i];
		if (codepoint >= range[0] && codepoint <= range[1])
		{
			glyph = font->glyphs + glyph_offset + codepoint - range[0];
			break;
		}
	}
	return(glyph);
}

s32
draw_glyph(Canvas *graphics, Font *font, u32 codepoint, s32 left, s32 baseline, u32 color)
{
	s32 advance = 0;
	Glyph *glyph = get_glyph(font, codepoint);
	if (glyph)
	{
		advance = glyph->advance;

		left -= glyph->x;
		s32 top  = baseline - glyph->y;
		s32 width  = glyph->width;
		s32 height = glyph->height;

		s32 min_x = (s32)maximum(left, 0);
		s32 min_y = (s32)maximum(top , 0);
		s32 offscreen_left = min_x - left;
		s32 offscreen_top  = min_y - top;

		s32 max_x = (s32)minimum(left + width , graphics->width);
		s32 max_y = (s32)minimum(top  + height, graphics->height);
		s32 offscreen_right  = width  + left - max_x;
		s32 offscreen_bottom = height + top  - max_y;

		u8 *source = glyph->buffer + offscreen_top * width + offscreen_left;
		u32 source_stride = offscreen_right + offscreen_left;

		u32 *destination = (u32*)graphics->buffer + min_y * graphics->width + min_x;
		u32 destination_stride = graphics->width + min_x - max_x;

		for (s32 y = min_y; y < max_y; y++)
		{
			for (s32 x = min_x; x < max_x; x++)
			{
				u8 corrected_alpha = ((u32)*(source++) * (color >> 24)) / 255;
				*(destination++) = alpha_blend((color & 0x00FFFFFF) | (corrected_alpha << 24), *destination);
			}
			source += source_stride;
			destination += destination_stride;
		}
	}
	return(advance);
}

internal s32
draw_text(Canvas *graphics, Font *font, U32_String text, s32 x, s32 y, u32 color)
{
	s32 offset = 0;
	for (u64 i = 0; i < text.length; i++)
	{
		offset += draw_glyph(graphics, font, text[i], x + offset, y, color);
	}
	return(offset);
}

internal s32
get_text_width(Font* font, U32_String text, u64 cursor_position = 0, s32 *caret_offset = 0)
{
	u32 position = 0;
	s32 span = 0;
	for (u64 i = 0; i < text.length; i++)
	{
		if (caret_offset && position++ == cursor_position)
			*caret_offset = span;
		Glyph* glyph = get_glyph(font, text[i]);
		span += glyph->advance;
	}
	if (caret_offset && position == cursor_position)
		*caret_offset = span;
	return(span);
}

/*internal void
draw_bitmap(Canvas *canvas, Bitmap *bitmap, Bounding_Box box, Vec2 position)
{
  // all positions relative to the canvas

  // clamp boinding box to canvas
  box.position = {
    maximum(box.min.x, 0),
    maximum(box.min.y, 0)
  };
  box->width  = minimum(box.width,  canvas->width  - box.position.x);
  box.height = minimum(box.height, canvas.height - box.position.y);

  Vec2 bitmap_min = {
    maximum(-(position.x - box.x), 0),
    maximum(-(position.y - box.y), 0)
  };

  Vec2 canvas_min = {
    maximum(position.x + bitmap_min.x, 0),
    maximum(position.y + bitmap_min.y, 0)
  };

  canvas_offset = maximum(position.y + bitmap_min.y, 0) * canvas.width + maximum(position.x + bitmap_min.x, 0);
  bitmap_offset = bitmap_min.y * bitmap.width + bitmap_min.x;

  blit_width  = maximum(box.position.x + box.width  - position.x, 0) - bitmap_min.x;
  blit_height = maximum(box.position.y + box.height - position.y, 0) - bitmap_min.y;

  u32 *source      = bitmap.buffer + bitmap_offset;
  u32 *destination = canvas.buffer + canvas_offset;

  u32 source_step      = bitmap.width - blit_width;
  u32 destination_step = canvas.width - blit_width;

  for (u32 y = 0; y < blit_height; y++)
  {
    for (u32 x = 0; x < blit_width; x++)
    {
      *(destination++) = alpha_blend(*(source++), *destination);
    }
    source      += source_step;
    destination += destination_step;
  }
}*/

void recalculate_lines(Document *document)
{
	u64 i = 0;
	u32 line = 0;
	u64 line_start = 0;
	U32_String *buffer = &document->buffer;
	for (; i < buffer->length; i++)
	{
		if ((*buffer)[i] == '\n')
		{
			assert((line + 1) < document->line_capacity);

			U32_String *doc_line = document->lines + line;
			doc_line->data = buffer->data + line_start;
			doc_line->capacity = buffer->capacity - line_start;
			doc_line->length = i - line_start;

			line_start = i + 1;
			++line;
		}
	}
	U32_String *doc_line = document->lines + line;
	doc_line->data = buffer->data + line_start;
	doc_line->capacity = buffer->capacity - line_start;
	doc_line->length = i - line_start;

	document->line_count = line + 1;
}

void recalculate_scroll(State *state, u32 height)
{
	u64 scroll_into_cursor = state->cursor_line * state->font.line_height;
	if (scroll_into_cursor < state->scroll_offset)
		state->scroll_offset = scroll_into_cursor;
	else if ((scroll_into_cursor + state->font.line_height) >= state->scroll_offset + height)
		state->scroll_offset = scroll_into_cursor + state->font.line_height - height;
}

internal u64
get_cursor_position_from_offset(Font *font, U32_String line, s32 offset)
{
	u64 i = 0;
	s32 span = 0;
	for (; i < line.length; i++)
	{
		Glyph* glyph = get_glyph(font, line[i]);
		span += glyph->advance;
		if (span > offset)
			return(i);
	}
	return(i);
}

inline internal void
clear_button(Input_Button *input)
{
	input->transitions = 0;
}

inline internal bool32
is_or_was_clicked(Input_Button input)
{
	return(input.is_down || input.transitions > 1);
}

internal void
update_and_render(Memory_Arena *arena, Platform *platform, Canvas *canvas, Keyboard_Input *keyboard, Mouse_Input *mouse)
{
	State *state = (State*)arena->data;
	Memory_Arena temp = { arena->data + sizeof(State), kibibytes(50) };

	if (!arena->used)
	{
		allocate_struct(arena, State);
		allocate_bytes(arena, temp.size);

		state->font = platform->load_font(arena, "data/fira.ttf", 20);
		state->caret_width = 1;
		state->line_number_bar_width = 40;

		state->document.buffer = make_empty_string(arena, kibibytes(1));
		state->document.line_capacity = 1024;
		state->document.lines  = allocate_array(arena, U32_String, state->document.line_capacity);
		recalculate_lines(&state->document);
	}

	s32 horizontal_offset = state->line_number_bar_width;

	// background
	draw_rect(canvas, horizontal_offset, 0, canvas->width, canvas->height, colorf32(1));

	// line number sidebar
	draw_rect(canvas, 0, 0, horizontal_offset, canvas->height, colorf32(0.9f));

	Context context = make_context(&temp, 100);

	U32_String prev_var = make_string_from_chars(&temp, "prev");
	U32_String sum_var  = make_string_from_chars(&temp, "sum");

	// @TODO: clamp to visible area
	for (u64 i = 0; i < state->document.line_count; i++)
	{
		U32_String line = state->document.lines[i];

		s32 vertical_offset = (s32)(state->font.line_height * i - state->scroll_offset);
		s32 baseline = vertical_offset + state->font.baseline_from_top;

		if (i == state->cursor_line)
		{
			{ 	// line highlight
				draw_rect(canvas,
					horizontal_offset,vertical_offset,
					canvas->width,    vertical_offset + state->font.line_height,
					colorf32(0.95f));

				// line number highlight
				draw_rect(canvas,
					0, vertical_offset,
					horizontal_offset, vertical_offset + state->font.line_height,
					colorf32(0.85f));
			}

			// caret
			s32 caret_offset = 0;
			get_text_width(&state->font, line, state->cursor_position_in_line, &caret_offset);
			caret_offset += horizontal_offset;
			draw_rect(canvas,
				caret_offset, vertical_offset,
				caret_offset + state->caret_width, vertical_offset + state->font.line_height,
				coloru8(0));
		}

		if (is_or_was_clicked(mouse->left) && mouse->y >= vertical_offset && mouse->y < (s32)(vertical_offset + state->font.line_height))
		{
			state->cursor_line = i;
			state->cursor_position_in_line = get_cursor_position_from_offset(&state->font, line, (s32)maximum(mouse->x - horizontal_offset, 0));
		}

		// line numbers ???
		U32_String line_number = convert_s64_to_string(&temp, i+1);
		s32 line_number_width = get_text_width(&state->font, line_number);
		draw_text(canvas, &state->font, line_number,
			horizontal_offset - line_number_width - 5, baseline,
			(i == state->cursor_line)? coloru8(0, 200) : coloru8(0, 128));

	    // line content
		draw_text(canvas, &state->font, line, horizontal_offset, baseline, coloru8(0));

		Result evaluation = evaluate_expression(&temp, line, &context);
		if (evaluation.valid)
		{
			U32_String result = convert_f64_to_string(&temp, evaluation.value);
			s32 result_width = get_text_width(&state->font, result);
			draw_text(canvas, &state->font, result, canvas->width - result_width, baseline, coloru8(0, 128));

			add_or_update_variable(&context, prev_var, evaluation.value);
			add_or_update_variable(&context, sum_var, context[sum_var].value + evaluation.value);
		}
	}

	clear_button(&mouse->left);
}