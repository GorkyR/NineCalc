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
	u32 baseline;

	u32 (*ranges)[2];
	u32 range_count;

	Glyph *glyphs;
	u32 glyph_count;
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

	bool selecting;

	u64 selection_start_line;
	u64 selection_start_position_in_line;

	u64 selection_end_line;
	u64 selection_end_position_in_line;

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
	u32  transitions;
};

struct Keyboard_Input
{
	Input_Button up;
	Input_Button right;
	Input_Button down;
	Input_Button left;

	Input_Button select_up;
	Input_Button select_right;
	Input_Button select_down;
	Input_Button select_left;

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

struct Time_Input
{
	s64 elapsed;
	s64 delta;
};

typedef Font Platform_Load_Font(char*, u32);
typedef void Platform_Unload_Font(Font*);
typedef bool Platform_Push_To_Clipboard(UTF32_String);
typedef UTF32_String Platform_Pop_From_Clipboard();

struct Platform
{
	Platform_Load_Font          *load_font;
	Platform_Unload_Font        *unload_font;
	Platform_Push_To_Clipboard  *push_to_clipboard;
	Platform_Pop_From_Clipboard *pop_from_clipboard;
};

internal void update_and_render(Memory_Arena*, Platform*, Canvas*, Time_Input*, Keyboard_Input*, Mouse_Input*);

internal inline u32 coloru8 (u8 red,  u8 green,  u8 blue,  u8 alpha=255);
internal inline u32 coloru8 (u8 luminosity,  u8 alpha=255);
internal inline u32 colorf32(f32 red, f32 green, f32 blue, f32 alpha=1.0f);
internal inline u32 colorf32(f32 luminosity, f32 alpha=1.0f);

internal inline u32 alpha_blend(u32 source, u32 destination);

internal Glyph * glyph_from_codepoint(Font, u32 codepoint);
internal bool    codepoint_is_in_font(Font, u32);

internal void draw_rect  (Canvas, s32 minx, s32 miny, s32 maxx, s32 maxy, u32 color);
internal s32  draw_glyph (Canvas, Glyph, s32 x, s32 baseline, u32 color);
internal s32  draw_glyph (Canvas, Font, u32 codepoint, s32 x, s32 baseline, u32 color);
internal s32  draw_text  (Canvas, Font, UTF32_String, s32 x, s32 baseline, u32 color);
internal void draw_bitmap(Canvas, u32* bitmap, s32 x, s32 y, u32 width, u32 height);

internal s32 text_width(UTF32_String, Font, u64=0, s32* =0);
internal u64 cursor_position_from_offset(UTF32_String, s32, Font);

internal void recalculate_lines(Document*);
internal void recalculate_scroll(State*, Canvas);
internal void recalculate_cursor_position(State*);

internal inline bool button_was_pressed(Input_Button);
internal inline bool process_keyboard(State*, Keyboard_Input*);