#pragma once
#include "grs.h"
#include "memory.h"

struct String
{
	u32 *data;
	u64 capacity;
	u64 length;

	u32 &operator[](u64 index);
};

u32 &String::operator[](u64 index)
{
	assert(index < this->capacity);
	return(this->data[index]);
}

struct String_List 
{
	String *strings;
	u32 count;
};

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
substring(String text, u64 offset, u64 size)
{
	assert(offset + size <= text.length);
	String substring;
	substring.data = text.data + offset;
	substring.length = size;
	substring.capacity = text.capacity - offset;
	return(substring);
}

String
substring(String text, u64 offset)
{
	assert(offset <= text.length);
	String substring;
	substring.data = text.data + offset;
	substring.length = text.length - offset;
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
			current_substring = allocate_struct(memory, String);
		}
	}
	current_substring->data   = text.data + last_line;
	current_substring->length = i - last_line;
	current_substring->capacity = text.capacity - last_line;

	return(substrings);
}

bool32 strings_are_equal(String str1, String str2)
{
	bool32 are_equal = false;
	if (str1.length == str2.length)
	{
		are_equal = true;
		for (u64 i = 0; i < str1.length; i++)
		{
			if (str1[i] != str2[i])
			{
				are_equal = false;
				break;
			}
		}
	}
	return(are_equal);
}

void reverse_string_in_place(String text)
{
	u64 head = 0;
	u64 tail = text.length - 1;
	for (; tail > head; ++head, --tail)
		swap(text[head], text[tail]);
}

String convert_s64_to_string(Memory *arena, s64 value)
{
	String result = {};
	result.data = (u32*)((u8*)arena->contents + arena->used);

	bool32 was_negative = value < 0;
	if (was_negative) value = -value;
	do
	{
		*allocate_struct(arena, u32) = (value % 10) + '0';
		++result.length; ++result.capacity;
		value /= 10;
	} while (value);
	if (was_negative)
	{
		*allocate_struct(arena, u32) = '-';
		++result.length; ++result.capacity;
	}

	reverse_string_in_place(result);

	return(result);
}

s16 normalize_f64(f64 *value)
{
	const f64 large_threshold = 1e16;
	const f64 small_threshold = 1e-14;
	s16 exponent = 0;

	if (*value >= large_threshold)
	{
		if (*value >= 1e256) {
	      *value /= 1e256;
	      exponent += 256;
	    }
	    if (*value >= 1e128) {
	      *value /= 1e128;
	      exponent += 128;
	    }
	    if (*value >= 1e64) {
	      *value /= 1e64;
	      exponent += 64;
	    }
	    if (*value >= 1e32) {
	      *value /= 1e32;
	      exponent += 32;
	    }
	    if (*value >= 1e16) {
	      *value /= 1e16;
	      exponent += 16;
	    }
	    if (*value >= 1e8) {
	      *value /= 1e8;
	      exponent += 8;
	    }
	    if (*value >= 1e4) {
	      *value /= 1e4;
	      exponent += 4;
	    }
	    if (*value >= 1e2) {
	      *value /= 1e2;
	      exponent += 2;
	    }
	    if (*value >= 1e1) {
	      *value /= 1e1;
	      exponent += 1;
	    }
	}

	if (*value > 0 && *value <= small_threshold)
	{
	    if (*value < 1e-255) {
	      *value *= 1e256;
	      exponent -= 256;
	    }
	    if (*value < 1e-127) {
	      *value *= 1e128;
	      exponent -= 128;
	    }
	    if (*value < 1e-63) {
	      *value *= 1e64;
	      exponent -= 64;
	    }
	    if (*value < 1e-31) {
	      *value *= 1e32;
	      exponent -= 32;
	    }
	    if (*value < 1e-15) {
	      *value *= 1e16;
	      exponent -= 16;
	    }
	    if (*value < 1e-7) {
	      *value *= 1e8;
	      exponent -= 8;
	    }
	    if (*value < 1e-3) {
	      *value *= 1e4;
	      exponent -= 4;
	    }
	    if (*value < 1e-1) {
	      *value *= 1e2;
	      exponent -= 2;
	    }
	    if (*value < 1e0) {
	      *value *= 1e1;
	      exponent -= 1;
	    }
	}

	return(exponent);
}

String convert_f64_to_string(Memory *arena, f64 value)
{
	String result = {};
	result.data = (u32*)((u8*)arena->contents + arena->used);

	bool32 was_negative = value < 0;
	if (was_negative) value = -value;

	s16 exponent = normalize_f64(&value);

	s64 integral_part = (s64)value;
	String integral_string = convert_s64_to_string(arena, (was_negative? -integral_part : integral_part));

	s64 decimal_part = (s64)((value - integral_part) * 1e15 + 0.5);
	u8 width = 15;
	if (decimal_part)
	{
		for (; decimal_part && decimal_part % 10 == 0 ; decimal_part /= 10)
			--width;
	}
	else
	{
		width = 1;
	}

	String decimal_string = {};
	decimal_string.data = (u32*)((u8*)arena->contents + arena->used);
	do
	{
		*allocate_struct(arena, u32) = (decimal_part % 10) + '0';
		++decimal_string.length; ++decimal_string.capacity;
		--width;
		decimal_part /= 10;
	} while (decimal_part);
	while (width--)
	{
		*allocate_struct(arena, u32) = '0';
		++decimal_string.length; ++decimal_string.capacity;
	}
	*allocate_struct(arena, u32) = '.';
	++decimal_string.length; ++decimal_string.capacity;

	reverse_string_in_place(decimal_string);

	if (exponent)
	{
		*allocate_struct(arena, u32) = 'e';
		convert_s64_to_string(arena, exponent);
	}

	result.length = result.capacity = (u32*)((u8*)arena->contents + arena->used) - result.data;

	return(result);
}