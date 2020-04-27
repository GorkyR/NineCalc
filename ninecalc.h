#pragma once
#include "grs.h"
#include "memory_arena.h"
#include "u32_string.h"
#include "math_evaluation.h"

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

struct Document
{
	U32_String buffer;
	U32_String *lines;
	u32 line_count;
	u32 line_capacity;
};

struct State
{
	Font font;
	
	u32 caret_x;
	u32 caret_width;

	u32 line_number_bar_width;

	Document document;
	u64 cursor_line;
	u64 cursor_position_in_line;

	u64 scroll_offset;
};


struct Canvas
{
	u32 *buffer;
	u32 width;
	u32 height;
};

struct Input_Button
{
	bool32 is_down;
	u32 transitions;
};

struct Keyboard_Input
{
	Input_Button up;
	Input_Button right;
	Input_Button down;
	Input_Button left;

	Input_Button enter;
	Input_Button backspace;
	Input_Button del;
	Input_Button home;
	Input_Button end;

	U32_String buffered_input;
};

struct Mouse_Input
{
	s16 x;
	s16 y;

	Input_Button left;
	Input_Button right;
	Input_Button middle;
};

typedef Font platform_load_font(Memory_Arena*, char*, u32);

struct Platform
{
	platform_load_font *load_font;
};

internal void update_and_render(Memory_Arena*, Platform*, Canvas*, Keyboard_Input*, Mouse_Input*);