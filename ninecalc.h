#pragma once
#include "grs.h"
#include "string.h"

struct Canvas
{
	u32 *buffer;
	u32 width;
	u32 height;
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
	String text;
	
	u64 cursor_position;

	u32 caret_x;
	u32 caret_width;

	u32 line_number_bar_width;

	s16 mouse_x;
	s16 mouse_y;
};