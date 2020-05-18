#include "ninecalc.h"

/*
	@TODO:
	- Calculator
	  - Units -> Unit conversion
	  - Big numbers

	- Editor
	  - Animate caret
	  - Select & Copy/Cut
	  - Token highlighting

	  - Token formating (?)

	  - Scrollbar (?)
	  - Save .txt (with results (?)) (?)
*/

internal void
update_and_render(Memory_Arena   *arena,
                  Platform       *platform,
                  Canvas         *canvas,
                  Time_Input     *time,
                  Keyboard_Input *keyboard,
                  Mouse_Input    *mouse)
{
	State *state = (State*)arena->data;
	Memory_Arena temp = { arena->data + sizeof(State), kibibytes(50) };

	if (!arena->used)
	{
		allocate_struct(arena, State);
		allocate_bytes(arena, temp.size);

		state->font = platform->load_font("data/fira.ttf", 18);
		state->caret_width = 1;
		state->line_number_bar_width = 40;

		state->document.buffer = make_empty_string(arena, kibibytes(1));
		state->document.line_capacity = 1024;
		state->document.lines  = allocate_array(arena, UTF32_String, state->document.line_capacity);
		recalculate_lines(&state->document);

		keyboard->input_buffer = make_empty_string(arena, 256);
	}

	bool should_snap_scroll = process_keyboard(state, keyboard);
	if (should_snap_scroll)
		recalculate_scroll(state, *canvas);

	if (button_was_pressed(keyboard->paste))
	{
		UTF32_String pasted = platform->pop_from_clipboard();
		u64 length_of_pasted_string = pasted.length;
		u64 c = 0;
		for (u64 i = 0; i < pasted.length; ++i)
		{
			u32 codepoint = pasted[i];
			if (codepoint == '\n' || codepoint_is_in_font(state->font, codepoint))
				pasted[c++] = codepoint;
		}
		pasted.length = c;
		insert_string_if_fits(&state->document.buffer, pasted,
			(state->document.lines[state->cursor_line].data - state->document.buffer.data) + state->cursor_position_in_line);
		arena->used -= length_of_pasted_string * sizeof(u32);
		recalculate_lines(&state->document);
		state->cursor_position_in_line += pasted.length;
		recalculate_cursor_position(state);
	}

	if (button_was_pressed(keyboard->save))
	{
		platform->push_to_clipboard(state->document.buffer);
	}

	s32 horizontal_offset = state->line_number_bar_width;

	// background
	draw_rect(*canvas, horizontal_offset, 0, canvas->width, canvas->height, colorf32(1));
	// line number sidebar
	draw_rect(*canvas, 0, 0, horizontal_offset, canvas->height, colorf32(0.9f));

	Context context = make_context(&temp, 100);

	UTF32_String prev_var = make_string_from_chars(&temp, "prev");
	UTF32_String sum_var  = make_string_from_chars(&temp, "sum");

	// @TODO: clamp to visible area
	u64 min = state->scroll_offset / state->font.line_height;
	u64 max = minimum(min + canvas->height / state->font.line_height + 1, state->document.line_count);

	for (u64 i = 0; i < min; ++i)
	{
		Result evaluation = evaluate_expression(&temp, state->document.lines[i], &context);
		add_or_update_variable(&context, prev_var, evaluation.value);
		add_or_update_variable(&context, sum_var, context[sum_var].value + evaluation.value);
	}
	
	for (u64 i = min; i < max; i++)
	{
		UTF32_String line = state->document.lines[i];

		s32 vertical_offset = (s32)(state->font.line_height * i - state->scroll_offset);
		s32 baseline = vertical_offset + state->font.baseline;

		if (i == state->cursor_line)
		{
			{ 	// line highlight
				draw_rect(*canvas,
					horizontal_offset,vertical_offset,
					canvas->width,    vertical_offset + state->font.line_height,
					colorf32(0.95f));

				// line number highlight
				draw_rect(*canvas,
					0, vertical_offset,
					horizontal_offset, vertical_offset + state->font.line_height,
					colorf32(0.85f));
			}

			// caret
			s32 caret_offset = 0;
			text_width(line, state->font, state->cursor_position_in_line, &caret_offset);
			caret_offset += horizontal_offset;
			draw_rect(*canvas,
				caret_offset, vertical_offset,
				caret_offset + state->caret_width, vertical_offset + state->font.line_height,
				coloru8(0));
		}

		if (mouse->left.is_down)
		{
			if (mouse->y >= vertical_offset && mouse->y < (s32)(vertical_offset + state->font.line_height))
			{
				state->cursor_line = i;
				state->cursor_position_in_line = cursor_position_from_offset(line, (s32)maximum(mouse->x - horizontal_offset, 0), state->font);
			}
		}

		// line numbers
		UTF32_String line_number_str = convert_s64_to_string(&temp, i+1);
		s32 line_number_width = text_width(line_number_str, state->font);
		draw_text(*canvas, state->font, line_number_str,
			horizontal_offset - line_number_width - 5, baseline,
			(i == state->cursor_line)? coloru8(0, 200) : coloru8(0, 128));

	    // line content
	    u32 text_color = coloru8(0);
		if (keyboard->save.is_down)
			text_color = coloru8(100, 100, 255, 200);
		draw_text(*canvas, state->font, line, horizontal_offset, baseline, text_color);

#if DEBUG && 0
		UTF32_String line_length = convert_s64_to_string(&temp, line.length);
		s32 width = text_width(line_length, state->font);
		draw_text(*canvas, state->font, line_length, canvas->width - width, baseline, coloru8(255, 0, 0));
#else
		Token_List tokens = tokenize_expression(&temp, line);
		Result evaluation = evaluate_tree(parse_tokens(&temp, tokens), &context);
		if (evaluation.valid)
		{
			UTF32_String result = convert_f64_to_string(&temp, evaluation.value);
			u32 result_color = coloru8(0, 128);

			if (i == state->cursor_line)
			{
				if (keyboard->copy.is_down)
					result_color = coloru8(100, 100, 255, 255);
				if (button_was_pressed(keyboard->copy))
					platform->push_to_clipboard(result);
			}

			s32 result_width = text_width(result, state->font);
			draw_text(*canvas, state->font, result, canvas->width - result_width, baseline, result_color);

			add_or_update_variable(&context, prev_var, evaluation.value);

			bool used_sum = false;
			for (u64 j = 0; j < tokens.count; ++j)
			{
				Token token = tokens[j];
				if (token.type == Token_Type::Identifier && strings_are_equal(token.text, sum_var))
				{
					used_sum = true;
					break;
				}
			}
			if (used_sum)
				add_or_update_variable(&context, sum_var, 0);
			else
				add_or_update_variable(&context, sum_var, context[sum_var].value + evaluation.value);
		}
#endif
	}

#if DEBUG
	persistent s64 fps_history[100] = {};
	persistent u64 fps_history_index = 0;

	fps_history[fps_history_index++] = (s64)1e6 / (time->delta? time->delta : 1);
	fps_history_index %= array_count(fps_history);

	f32 bar_width = (f32)canvas->width / array_count(fps_history);
	for (u64 i = 0; i < array_count(fps_history); ++i)
	{
		u64 it = (fps_history_index + i) % array_count(fps_history);
		s64 fps_i = fps_history[it];
		// fps_i /= 2;
		draw_rect(*canvas,
			(s32)(bar_width * i      ), canvas->height - (s32)fps_i,
			(s32)(bar_width * (i + 1)), canvas->height - (s32)fps_i + 1,
			colorf32(1, 0, 0));
	}

	const f64 fps_update_period_in_seconds = 0.25;
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
	persistent UTF32_String label2 = make_string_from_chars(arena, "cursor_line: ");
	persistent UTF32_String label1 = make_string_from_chars(arena, "cursor_position: ");
	UTF32_String fps_string = convert_s64_to_string(&temp, fps);
	UTF32_String cursor_line_str = convert_s64_to_string(&temp, (s64)state->cursor_line);
	UTF32_String cursor_position_str = convert_s64_to_string(&temp, (s64)state->cursor_position_in_line);

	UTF32_String info_str = concatenate(&temp, apprx_str, concatenate(&temp, fps_string, unit_str));

	s32 info_width = text_width(info_str, state->font);
	draw_text(*canvas, state->font, info_str,
		canvas->width - info_width, canvas->height - (s32)fps - state->font.line_height + state->font.baseline,
		colorf32(1, 0, 0));

	info_str = concatenate(&temp, label1, cursor_position_str);

	info_width = text_width(info_str, state->font);
	draw_text(*canvas, state->font, info_str,
		canvas->width - info_width, canvas->height - (s32)fps - state->font.line_height*3 + state->font.baseline,
		colorf32(1, 0, 0));

	info_str = concatenate(&temp, label2, cursor_line_str);

	info_width = text_width(info_str, state->font);
	draw_text(*canvas, state->font, info_str,
		canvas->width - info_width, canvas->height - (s32)fps - state->font.line_height*2 + state->font.baseline,
		colorf32(1, 0, 0));

#endif
}

internal inline u32
coloru8(u8 red, u8 green, u8 blue, u8 alpha)
{
	u32 color = ((u32)alpha << 24) |
                ((u32)red   << 16) |
                ((u32)green <<  8) |
                ((u32)blue  <<  0);
	return color;
}

internal inline u32
coloru8(u8 luminosity, u8 alpha)
{
	u32 color = coloru8(luminosity, luminosity, luminosity, alpha);
	return color;
}

internal inline u32
colorf32(f32 red, f32 green, f32 blue, f32 alpha)
{
	assert(red   >= 0 && red   <= 1.0f);
	assert(green >= 0 && green <= 1.0f);
	assert(blue  >= 0 && blue  <= 1.0f);
	assert(alpha >= 0 && alpha <= 1.0f);

	u32 color = coloru8((u8)(red   * 255.0f),
                        (u8)(green * 255.0f),
                        (u8)(blue  * 255.0f),
                        (u8)(alpha * 255.0f));
	return color;
}

internal inline u32
colorf32(f32 luminosity, f32 alpha)
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

	return result;
}

internal Glyph *
glyph_from_codepoint(Font font, u32 codepoint)
{
	Glyph *glyph = 0;
	u32 glyph_offset = 0;
	for (u32 i = 0; i < font.range_count; i++)
	{
		u32 *range = font.ranges[i];
		if (codepoint >= range[0] && codepoint <= range[1])
		{
			glyph = font.glyphs + glyph_offset + codepoint - range[0];
			break;
		}
	}
	return glyph;
}

internal bool
codepoint_is_in_font(Font font, u32 codepoint)
{
	for (u64 i = 0; i < font.range_count; i++)
	{
		u32 *range = font.ranges[i];
		if (codepoint <= range[1] && codepoint >= range[0])
			return true;
	}
	return false;
}

internal void
draw_rect (Canvas canvas, s32 min_x, s32 min_y, s32 max_x, s32 max_y, u32 color)
{
	if (min_x > max_x)
		swap(min_x, max_x);
	if (min_y > max_y)
		swap(min_y, max_y);

	u32 x_min = (u32)maximum(min_x, 0);
	u32 y_min = (u32)maximum(min_y, 0);
	u32 x_max = (u32)clamp(max_x, 0, canvas.width);
	u32 y_max = (u32)clamp(max_y, 0, canvas.height);

	u32 stride = canvas.width + x_min - x_max;
	u32 *buffer = (u32*)canvas.buffer + y_min * canvas.width + x_min;
	for (u32 y = y_min; y < y_max; y++)
	{
		for (u32 x = x_min; x < x_max; x++)
			*(buffer++) = alpha_blend(color, *buffer);
		buffer += stride;
	}
}

internal s32
draw_glyph(Canvas canvas, Glyph glyph, s32 x, s32 baseline, u32 color)
{
	x -= glyph.x;
	s32 y  = baseline - glyph.y;

	s32 min_x = (s32)maximum(x, 0);
	s32 min_y = (s32)maximum(y , 0);
	s32 offscreen_left = min_x - x;
	s32 offscreen_top  = min_y - y;

	s32 max_x = (s32)minimum(x + glyph.width , canvas.width);
	s32 max_y = (s32)minimum(y + glyph.height, canvas.height);
	s32 offscreen_right  = glyph.width  + x - max_x;
	s32 offscreen_bottom = glyph.height + y - max_y;

	u8 *source = glyph.buffer + offscreen_top * glyph.width + offscreen_left;
	u32 source_stride = offscreen_right + offscreen_left;

	u32 *destination = (u32*)canvas.buffer + min_y * canvas.width + min_x;
	u32 destination_stride = canvas.width + min_x - max_x;

	for (s32 _y = min_y; _y < max_y; _y++)
	{
		for (s32 _x = min_x; _x < max_x; _x++)
		{
			u8 corrected_alpha = ((u32)(*(source++)) * (color >> 24)) / 255;
			*(destination++) = alpha_blend((color & 0x00FFFFFF) | (corrected_alpha << 24), *destination);
		}
		source += source_stride;
		destination += destination_stride;
	}
	return glyph.advance;
}

internal s32
draw_glyph(Canvas canvas, Font font, u32 codepoint, s32 x, s32 baseline, u32 color)
{
	s32 advance = 0;
	Glyph *glyph = glyph_from_codepoint(font, codepoint);
	if (glyph)
		advance = draw_glyph(canvas, *glyph, x, baseline, color);
	return advance;
}

internal s32
draw_text(Canvas graphics, Font font, UTF32_String text, s32 x, s32 y, u32 color)
{
	s32 offset = 0;
	for (u64 i = 0; i < text.length; i++)
		offset += draw_glyph(graphics, font, text[i], x + offset, y, color);
	return offset;
}

internal void
draw_bitmap(Canvas canvas, u32 *buffer, s32 left, s32 top, u32 width, u32 height)
{
	u32 min_x = (u32)maximum(left, 0);
	u32 min_y = (u32)maximum(top, 0);
	u32 offscreen_left = min_x - left;
	u32 offscreen_top = min_y - top;

	u32 max_x = (u32)minimum(left + width , canvas.width);
	u32 max_y = (u32)minimum(top  + height, canvas.height);
	u32 offscreen_right  = width  + left - max_x;
	u32 offscreen_bottom = height + top  - max_y;

	u32 *source = buffer + offscreen_top * width + offscreen_left;
	u32 source_stride = offscreen_right + offscreen_left;

	u32 *destination = (u32*)canvas.buffer + min_y * canvas.width + min_x;
	u32 destination_stride = canvas.width + min_x - max_x;

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

internal s32
text_width(UTF32_String text, Font font, u64 cursor_position, s32 *caret_offset)
{
	u32 position = 0;
	s32 span = 0;
	for (u64 i = 0; i < text.length; i++)
	{
		if (caret_offset && position++ == cursor_position)
			*caret_offset = span;
		Glyph* glyph = glyph_from_codepoint(font, text[i]);
		span += glyph->advance;
	}
	if (caret_offset && position == cursor_position)
		*caret_offset = span;
	return span;
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
recalculate_scroll(State *state, Canvas canvas)
{
	u32 height = canvas.height;
	u64 scroll_into_cursor = state->cursor_line * state->font.line_height;
	if (scroll_into_cursor < state->scroll_offset)
		state->scroll_offset = scroll_into_cursor;
	else if ((scroll_into_cursor + state->font.line_height) >= state->scroll_offset + height)
		state->scroll_offset = scroll_into_cursor + state->font.line_height - height;
}

internal void
recalculate_cursor_position(State *state)
{
	while (state->cursor_line < state->document.line_count)
	{
		UTF32_String line = state->document.lines[state->cursor_line];
		if (state->cursor_position_in_line > line.length)
		{
			state->cursor_position_in_line -= line.length + 1;
			++state->cursor_line;
		}
		else
			break;
	}
}

internal u64
cursor_position_from_offset(UTF32_String line, s32 offset, Font font)
{
	u64 i = 0;
	s32 span = 0;
	for (; i < line.length; i++)
	{
		Glyph* glyph = glyph_from_codepoint(font, line[i]);
		span += glyph->advance;
		if (span > offset)
			return i;
	}
	return i;
}

internal inline bool
button_was_pressed(Input_Button button) // went from 'up' to 'down' at least once
{
	return button.transitions > (u8)(button.is_down? 0 : 1);
}

internal bool
process_keyboard(State *state, Keyboard_Input *keyboard)
{
	bool should_snap_scroll = false;
	if (button_was_pressed(keyboard->up))
	{
		state->selecting = false;
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
		state->selecting = false;
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
		state->selecting = false;
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
		state->selecting = false;
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

	return should_snap_scroll;
}