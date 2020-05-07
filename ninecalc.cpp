#include "ninecalc.h"

/*
	@TODO:
	- Calculator
	  - Functions (?)

	- Editor
	  - Token highlighting
	  - Token formating (?)
	
	  - Select & Copy/Cut

	  - Save .txt (with results (?)) (?)
	  - Scrollbar (?)
*/

#define array_count(array) (sizeof(array) / sizeof(array[0]))

internal inline u64 kibibytes(u64 n) { return(n << 10); }
internal inline u64 mebibytes(u64 n) { return(n << 20); }
internal inline u64 gibibytes(u64 n) { return(n << 30); }
internal inline u64 tebibytes(u64 n) { return(n << 40); }

internal inline u32
coloru8(u8 red, u8 green, u8 blue, u8 alpha = 255)
{
	u32 color =
		((u32)alpha << 24) |
		((u32)red   << 16) |
		((u32)green <<  8) |
		((u32)blue  <<  0);
	return(color);
}
internal inline u32
coloru8(u8 luminosity, u8 alpha = 255)
{
	u32 color = coloru8(luminosity, luminosity, luminosity, alpha);
	return(color);
}

internal inline u32
colorf32(f32 red, f32 green, f32 blue, f32 alpha = 1.0f)
{
	assert(red   >= 0 && red   <= 1.0f);
	assert(green >= 0 && green <= 1.0f);
	assert(blue  >= 0 && blue  <= 1.0f);
	assert(alpha >= 0 && alpha <= 1.0f);

	u32 color = coloru8(
		(u8)(red   * 255.0f),
		(u8)(green * 255.0f),
		(u8)(blue  * 255.0f),
		(u8)(alpha * 255.0f));
	return(color);
}

internal inline u32
colorf32(f32 luminosity, f32 alpha = 1.0f)
{
	u32 color = colorf32(luminosity, luminosity, luminosity, alpha);
	return color;
}

internal inline u32
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


internal void
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


internal Glyph *
get_glyph(Font *font, u32 codepoint)
{
	Glyph *glyph = 0;
	u32 glyph_offset = 0;
	for (u32 i = 0; i < font->range_count; i++)
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

internal s32
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
draw_text(Canvas *graphics, Font *font, UTF32_String text, s32 x, s32 y, u32 color)
{
	s32 offset = 0;
	for (u64 i = 0; i < text.length; i++)
	{
		offset += draw_glyph(graphics, font, text[i], x + offset, y, color);
	}
	return(offset);
}

internal bool32
codepoint_is_in_range(Font font, u32 codepoint)
{
	for (u64 i = 0; i < font.range_count; i++)
	{
		u32 *range = font.ranges[i];
		if (codepoint <= range[1] && codepoint >= range[0])
			return(true);
	}
	return(false);
}

internal s32
get_text_width(Font* font, UTF32_String text, u64 cursor_position = 0, s32 *caret_offset = 0)
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

internal void
recalculate_lines(Document *document)
{
	u64 i = 0;
	u32 line = 0;
	u64 line_start = 0;
	UTF32_String *buffer = &document->buffer;
	for (; i < buffer->length; i++)
	{
		if ((*buffer)[i] == '\n')
		{
			assert((line + 1) < document->line_capacity);

			UTF32_String *doc_line = document->lines + line;
			doc_line->data = buffer->data + line_start;
			doc_line->capacity = buffer->capacity - line_start;
			doc_line->length = i - line_start;

			line_start = i + 1;
			++line;
		}
	}
	UTF32_String *doc_line = document->lines + line;
	doc_line->data = buffer->data + line_start;
	doc_line->capacity = buffer->capacity - line_start;
	doc_line->length = i - line_start;

	document->line_count = line + 1;
}

internal void
recalculate_scroll(State *state, u32 height)
{
	u64 scroll_into_cursor = state->cursor_line * state->font.line_height;
	if (scroll_into_cursor < state->scroll_offset)
		state->scroll_offset = scroll_into_cursor;
	else if ((scroll_into_cursor + state->font.line_height) >= state->scroll_offset + height)
		state->scroll_offset = scroll_into_cursor + state->font.line_height - height;
}

internal u64
get_cursor_position_from_offset(Font *font, UTF32_String line, s32 offset)
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

internal inline bool32
button_was_pressed(Input_Button button) // went from 'up' to 'down' at least once
{
	return(button.transitions > (u8)(button.is_down? 0 : 1));
}

internal bool32
process_keyboard(State *state, Keyboard_Input *keyboard)
{
	bool32 should_snap_scroll = false;
	if (button_was_pressed(keyboard->up))
	{
		if (state->cursor_line > 0)
		{
			--state->cursor_line;
			state->cursor_position_in_line = minimum(
				state->cursor_position_in_line,
				state->document.lines[state->cursor_line].length);
		}
		should_snap_scroll = true;
	}
	if (button_was_pressed(keyboard->down))
	{
		if ((state->cursor_line + 1) < state->document.line_count)
		{
			++state->cursor_line;
			state->cursor_position_in_line = minimum(
				state->cursor_position_in_line,
				state->document.lines[state->cursor_line].length);
		}
		should_snap_scroll = true;
	}
	if (button_was_pressed(keyboard->right))
	{
		if (state->cursor_position_in_line == state->document.lines[state->cursor_line].length)
		{
			if ((state->cursor_line + 1) < state->document.line_count)
			{
				++state->cursor_line;
				state->cursor_position_in_line = 0;
			}
		}
		else
		{
			++state->cursor_position_in_line;
		}
		should_snap_scroll = true;
	}
	if (button_was_pressed(keyboard->left))
	{
		if (state->cursor_position_in_line == 0)
		{
			if (state->cursor_line != 0)
			{
				--state->cursor_line;
				state->cursor_position_in_line = state->document.lines[state->cursor_line].length;
			}
		}
		else
		{
			--state->cursor_position_in_line;
		}
		should_snap_scroll = true;
	}

	if (button_was_pressed(keyboard->enter))
	{
		insert_character_if_fits(&state->document.buffer, '\n',
			(state->document.lines[state->cursor_line].data - state->document.buffer.data) + state->cursor_position_in_line);
		recalculate_lines(&state->document);
		++state->cursor_line;
		state->cursor_position_in_line = 0;
		should_snap_scroll = true;
	}
	if (button_was_pressed(keyboard->backspace))
	{
		if (state->cursor_position_in_line > 0 || state->cursor_line > 0)
		{
			u64 cursor_line = state->cursor_line;
			u64 cursor_position_in_line = state->cursor_position_in_line;
			if (state->cursor_position_in_line == 0)
			{
				--state->cursor_line;
				state->cursor_position_in_line = state->document.lines[state->cursor_line].length;
			}
			else
				--state->cursor_position_in_line;
			remove_from_string(&state->document.buffer,
				(state->document.lines[cursor_line].data - state->document.buffer.data) + cursor_position_in_line - 1, 1);
			recalculate_lines(&state->document);
		}
		should_snap_scroll = true;
	}
	if (button_was_pressed(keyboard->del))
	{
		if (state->cursor_position_in_line < state->document.lines[state->cursor_line].length ||
			(state->cursor_line + 1) < state->document.line_count)
		{
			remove_from_string(&state->document.buffer,
				(state->document.lines[state->cursor_line].data - state->document.buffer.data) + state->cursor_position_in_line, 1);
			recalculate_lines(&state->document);
		}
		should_snap_scroll = true;
	}
	if (button_was_pressed(keyboard->home))
	{
		state->cursor_position_in_line = 0;
		should_snap_scroll = true;
	}
	else if (button_was_pressed(keyboard->end))
	{
		state->cursor_position_in_line = state->document.lines[state->cursor_line].length;
		should_snap_scroll = true;
	}

	if (keyboard->input_buffer.length)
	{
		insert_string_if_fits(&state->document.buffer, keyboard->input_buffer,
			(state->document.lines[state->cursor_line].data - state->document.buffer.data) + state->cursor_position_in_line);
		state->cursor_position_in_line += keyboard->input_buffer.length;
		keyboard->input_buffer.length = 0;
		recalculate_lines(&state->document);
		should_snap_scroll = true;
	}

	return(should_snap_scroll);
}

internal void
update_and_render(Memory_Arena *arena, Platform *platform, Canvas *canvas, Time_Input *time, Keyboard_Input *keyboard, Mouse_Input *mouse)
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
		state->document.lines  = allocate_array(arena, UTF32_String, state->document.line_capacity);
		recalculate_lines(&state->document);

		keyboard->input_buffer = make_empty_string(arena, 256);
	}

	bool32 should_snap_scroll = process_keyboard(state, keyboard);
	if (should_snap_scroll)
		recalculate_scroll(state, canvas->height);

	if (button_was_pressed(keyboard->paste))
	{
		UTF32_String pasted = platform->pop_from_clipboard(arena);
		insert_string_if_fits(&state->document.buffer, pasted,
			(state->document.lines[state->cursor_line].data - state->document.buffer.data) + state->cursor_position_in_line);
		state->cursor_position_in_line += pasted.length;
		arena->used -= pasted.length * sizeof(u32);
		recalculate_lines(&state->document);
	}

	s32 horizontal_offset = state->line_number_bar_width;

	// background
	draw_rect(canvas, horizontal_offset, 0, canvas->width, canvas->height, colorf32(1));
	// line number sidebar
	draw_rect(canvas, 0, 0, horizontal_offset, canvas->height, colorf32(0.9f));

	Context context = make_context(&temp, 100);

	UTF32_String prev_var = make_string_from_chars(&temp, "prev");
	UTF32_String sum_var  = make_string_from_chars(&temp, "sum");

	// @TODO: clamp to visible area
	u64 min = state->scroll_offset / state->font.line_height;
	u64 max = minimum(min + canvas->height / state->font.line_height + 1, state->document.line_count);
	for (u64 i = min; i < max; i++)
	{
		UTF32_String line = state->document.lines[i];

		s32 vertical_offset = (s32)(state->font.line_height * i - state->scroll_offset);
		s32 baseline = vertical_offset + state->font.baseline;

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

		if (mouse->left.is_down)
		{
			if (mouse->y >= vertical_offset && mouse->y < (s32)(vertical_offset + state->font.line_height))
			{
				state->cursor_line = i;
				state->cursor_position_in_line = get_cursor_position_from_offset(&state->font, line, (s32)maximum(mouse->x - horizontal_offset, 0));
			}
		}

		// line numbers
		UTF32_String line_number = convert_s64_to_string(&temp, i+1);
		s32 line_number_width = get_text_width(&state->font, line_number);
		draw_text(canvas, &state->font, line_number,
			horizontal_offset - line_number_width - 5, baseline,
			(i == state->cursor_line)? coloru8(0, 200) : coloru8(0, 128));

	    // line content
		draw_text(canvas, &state->font, line, horizontal_offset, baseline, coloru8(0));

		Result evaluation = evaluate_expression(&temp, line, &context);
		if (evaluation.valid)
		{
			UTF32_String result = convert_f64_to_string(&temp, evaluation.value);
			u32 result_color = coloru8(0, 128);

			if (i == state->cursor_line)
			{
				if (keyboard->copy.is_down)
					result_color = coloru8(100, 100, 255, 200);
				if (button_was_pressed(keyboard->copy))
					platform->push_to_clipboard(result);
			}

			s32 result_width = get_text_width(&state->font, result);
			draw_text(canvas, &state->font, result, canvas->width - result_width, baseline, result_color);

			add_or_update_variable(&context, prev_var, evaluation.value);
			add_or_update_variable(&context, sum_var, context[sum_var].value + evaluation.value);
		}
	}

#if DEBUG
	persistent s64 fps_history[100] = {};
	persistent u64 fps_history_index = 0;

	fps_history[fps_history_index++] = (s64)1e6 / time->delta;
	fps_history_index %= array_count(fps_history);

	s32 bar_width = canvas->width / array_count(fps_history);
	s32 max_height = 0;
	for (u64 i = 0; i < array_count(fps_history); ++i)
	{
		u64 it = (fps_history_index + i) % array_count(fps_history);
		s64 fps_i = fps_history[it];
		// fps_i /= 2;
		max_height = (s32)maximum(max_height, fps_i);
		draw_rect(canvas,
			(s32)(bar_width * i      ), canvas->height - (s32)fps_i,
			(s32)(bar_width * (i + 1)), canvas->height - (s32)fps_i + 1,
			colorf32(1, 0, 0));
	}

	const f64 fps_update_period_in_seconds = 0.5;
	persistent s64 elapsed = 0;
	persistent u32 frames  = 0;
	persistent s64 fps     = 0;

	++frames;
	elapsed += time->delta;
	const s64 fpsup_in_microseconds = (s64)(1e6 * fps_update_period_in_seconds);
	const f64 fpsup_frequency = 1/fps_update_period_in_seconds;
	if (elapsed >= fpsup_in_microseconds)
	{
		fps = (s64)(frames * 1/fps_update_period_in_seconds);
		elapsed = elapsed - fpsup_in_microseconds;
		frames  = 0;
	}

	persistent UTF32_String apprx_str = make_string_from_chars(arena, "~");
	persistent UTF32_String unit_str  = make_string_from_chars(arena, " f/s");
	UTF32_String fps_string = convert_s64_to_string(&temp, fps);

	UTF32_String info_str = concatenate(&temp, apprx_str, concatenate(&temp, fps_string, unit_str));

	s32 info_width = get_text_width(&state->font, info_str);
	draw_text(canvas, &state->font, info_str,
		canvas->width - info_width, canvas->height - max_height - state->font.line_height + state->font.baseline,
		colorf32(1, 0, 0));
#endif
}