#pragma once
#include "grs.h"
#include "memory_arena.h"
#include "utf32_string.h"
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

	u32 range_count;
	u32 (*ranges)[2];

	Glyph *glyphs;
};

struct Document
{
	UTF32_String buffer;
	UTF32_String *lines;
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
	bool is_down;
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

	Input_Button cut;
	Input_Button copy;
	Input_Button paste;
	Input_Button save;

	UTF32_String input_buffer;
};

struct Mouse_Input
{
	s16 x;
	s16 y;

	Input_Button left;
	Input_Button right;
	Input_Button middle;
};

typedef Font Platform_Load_Font(Memory_Arena*, char*, u32);
typedef bool32 Platform_Push_To_Clipboard(UTF32_String);
typedef UTF32_String Platform_Pop_From_Clipboard(Memory_Arena*);

struct Platform
{
	Platform_Load_Font          *load_font;
	Platform_Push_To_Clipboard  *push_to_clipboard;
	Platform_Pop_From_Clipboard *pop_from_clipboard;
};

internal void update_and_render(Memory_Arena*, Platform*, Canvas*, Keyboard_Input*, Mouse_Input*);