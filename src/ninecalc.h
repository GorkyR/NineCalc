#pragma once
struct Graphics
{
	void *buffer;
	BITMAPINFO bitmap_info;
	u8 bytes_per_pixel;
	u32 width;
	u32 height;
	u64 size;
};

struct Memory
{
	void *contents;
	u64 size;
	u64 used;
};

struct Glyph {
	u32 width;
	u32 height;
	s32 x;
	s32 y;
	u32 advance;
	u8 *buffer;
};

struct Font
{
	u32 line_height;
	u32 baseline_from_top;

	u32 n_ranges;
	u32 (*ranges)[2];

	Glyph *glyphs;
};

struct State
{
	Font loaded_font;

	u32 current_line;
	u32 current_cursor_position;

	u32 caret_x;
	u32 caret_width;

	u32 line_number_bar_width;

	s16 mouse_x;
	s16 mouse_y;
};