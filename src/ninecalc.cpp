#include "ninecalc.h"

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


void
draw_bitmap(Canvas *graphics, u32 *buffer, s32 left, s32 top, u32 width, u32 height)
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

		s32 min_x = maximum(left, 0);
		s32 min_y = maximum(top , 0);
		s32 offscreen_left = min_x - left;
		s32 offscreen_top  = min_y - top;

		s32 max_x = minimum(left + width , graphics->width);
		s32 max_y = minimum(top  + height, graphics->height);
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
draw_text(Canvas *graphics, Font *font, String text, s32 x, s32 y, u32 color)
{
	s32 offset = 0;
	for (u64 i = 0; i < text.length; i++)
	{
		offset += draw_glyph(graphics, font, text[i], x + offset, y, color);
	}
	return(offset);
}

internal s32
get_text_width(Font* font, String text, u32 cursor_position = 0, s32 *caret_offset = 0)
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

internal u32
get_current_line(String text, u32 cursor_position)
{
	u32 current_line = 0;
	for (u64 i = 0; i < text.length; i++)
	{
		if (cursor_position == i)
			break;
		if (text[i] == '\n')
			current_line++;
	}
	return(current_line);
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
  box.width  = minimum(box.width,  canvas.width  - box.position.x);
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

String
make_string(Memory *memory, u64 capacity)
{
	String string;
	string.data = allocate_array(memory, u32, capacity);
	string.capacity = capacity;
	string.length = 0;
	return(string);
}

String
make_string_from_chars(Memory *memory, char *text)
{
	u64 text_length = 0;
	for (char *ch = text; *ch; ch++)
		text_length++;

	String string;
	string = make_string(memory, text_length);
	string.length = text_length;

	for (u64 i = 0; i < string.length; i++)
	{
		string.data[i] = (u32)text[i];
	}

	return(string);
}

String
substring(String text, u32 offset, u32 size)
{
	assert(offset + size <= text.length);
	String substring;
	substring.data = text.data + offset;
	substring.length = size;
	substring.capacity = text.capacity - offset;
	return(substring);
}

String *
split_lines(Memory *memory, String text, u64 *number_of_lines)
{
	*number_of_lines = text.length? 1 : 0;

	String *substrings = allocate_struct(memory, String);
	String *current_substring = substrings;
	u64 last_line = 0;
	u64 i = 0;
	for (; i < text.length; i++)
	{
		if (text[i] == '\n')
		{
			current_substring->data   = text.data + last_line;
			current_substring->length = i - last_line;
			current_substring->capacity = text.capacity - last_line;

			last_line = i + 1;
			(*number_of_lines)++;
			if (last_line < text.length)
				current_substring = allocate_struct(memory, String);
		}
	}
	if (last_line < text.length)
	{
		current_substring->data   = text.data + last_line;
		current_substring->length = i - last_line;
		current_substring->capacity = text.capacity - last_line;
	}

	return(substrings);
}