#pragma once
#include "grs.h"
#include "memory_arena.h"

struct UTF32_String
{
	u32 *data;
	u64 capacity;
	u64 length;

	u32 &operator[](u64 index);
};

struct UTF32_String_List
{
	UTF32_String *data;
	u32 count;

	UTF32_String &operator[](u64 index);
};

UTF32_String make_empty_string(Memory_Arena *memory, u64 capacity);
UTF32_String make_string_from_chars(Memory_Arena *memory, char *text);

bool32 strings_are_equal(UTF32_String str1, UTF32_String str2);

UTF32_String substring(UTF32_String text, u64 offset, u64 size);
UTF32_String substring(UTF32_String text, u64 offset);

UTF32_String_List split_lines(Memory_Arena *memory, UTF32_String text);

UTF32_String convert_s64_to_string(Memory_Arena *arena, s64 value);
UTF32_String convert_s64_to_string(Memory_Arena *arena, s64 value, bool32 negative);
UTF32_String convert_f64_to_string(Memory_Arena *arena, f64 value);
s64 parse_integer(UTF32_String text);
f64 parse_float(UTF32_String text);

void reverse_string_in_place(UTF32_String text);
bool32 insert_character_if_fits(UTF32_String *into, u32 character, u64 at);
bool32 insert_string_if_fits(UTF32_String *into, UTF32_String other, u64 at);
UTF32_String concatenate(Memory_Arena *arena, UTF32_String a, UTF32_String b);
void remove_from_string(UTF32_String *from, u64 at, u64 count);

////////////////////////////////////

u32 &
UTF32_String::operator[](u64 index)
{
	assert(index < this->capacity);
	return(this->data[index]);
}

UTF32_String &
UTF32_String_List::operator[](u64 index)
{
	assert(index < this->count);
	return(this->data[index]);
}

UTF32_String
make_empty_string(Memory_Arena *memory, u64 capacity)
{
	UTF32_String string;
	string.data = allocate_array(memory, u32, capacity);
	string.capacity = capacity;
	string.length = 0;
	return(string);
}

UTF32_String
make_string_from_chars(Memory_Arena *memory, char *text)
{
	u64 text_length = 0;
	for (char *ch = text; *ch; ch++)
		text_length++;

	UTF32_String string;
	string = make_empty_string(memory, text_length);
	string.length = text_length;

	for (u64 i = 0; i < string.length; i++)
	{
		string.data[i] = (u32)text[i];
	}

	return(string);
}

UTF32_String
substring(UTF32_String text, u64 offset, u64 size)
{
	assert(offset + size <= text.length);
	UTF32_String substring;
	substring.data = text.data + offset;
	substring.length = size;
	substring.capacity = text.capacity - offset;
	return(substring);
}

UTF32_String
substring(UTF32_String text, u64 offset)
{
	assert(offset <= text.length);
	UTF32_String substring;
	substring.data = text.data + offset;
	substring.length = text.length - offset;
	substring.capacity = text.capacity - offset;
	return(substring);
}

UTF32_String_List
split_lines(Memory_Arena *arena, UTF32_String text)
{
	UTF32_String_List substrings = {};
	substrings.data = (UTF32_String *)((u8*)arena->data + arena->used);

	u64 last_line = 0;
	for (u64 i = 0; i < text.length + 1; ++i)
	{
		if (i == text.length || text[i] == '\n')
		{
			UTF32_String *current_substring = allocate_struct(arena, UTF32_String);
			current_substring->data   = text.data + last_line;
			current_substring->length = i - last_line;
			current_substring->capacity = text.capacity - last_line;

			last_line = i + 1;
			++substrings.count;
		}
	}

	return(substrings);
}

bool32
strings_are_equal(UTF32_String str1, UTF32_String str2)
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

void
reverse_string_in_place(UTF32_String text)
{
	u64 head = 0;
	u64 tail = text.length - 1;
	for (; tail > head; ++head, --tail)
		swap(text[head], text[tail]);
}

UTF32_String
convert_s64_to_string(Memory_Arena *arena, s64 value, bool32 negative)
{
	UTF32_String result = {};
	result.data = (u32*)((u8*)arena->data + arena->used);

	if (negative) value = -value;
	do
	{
		*allocate_struct(arena, u32) = (value % 10) + '0';
		++result.length; ++result.capacity;
		value /= 10;
	} while (value);
	if (negative)
	{
		*allocate_struct(arena, u32) = '-';
		++result.length; ++result.capacity;
	}

	reverse_string_in_place(result);

	return(result);
}

UTF32_String
convert_s64_to_string(Memory_Arena *arena, s64 value)
{
	bool32 is_negative = value < 0;
	return(convert_s64_to_string(arena, is_negative? -value : value, is_negative));
}

internal s16
normalize_f64(f64 *value)
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

UTF32_String
convert_f64_to_string(Memory_Arena *arena, f64 value)
{
	UTF32_String result = {};
	result.data = (u32*)((u8*)arena->data + arena->used);

	bool32 was_negative = value < 0;
	if (was_negative) value = -value;

	s16 exponent = normalize_f64(&value);

	s64 integral_part = (s64)value;
	UTF32_String integral_string = convert_s64_to_string(arena, was_negative? -integral_part : integral_part, was_negative);

	s64 decimal_part = (s64)((value - integral_part) * 1e15 + 0.5);
	u8 width = 15;
	if (decimal_part)
	{
		for (; decimal_part && decimal_part % 10 == 0 ; decimal_part /= 10)
			--width;
	}
	else
	{
		return(integral_string);
	}

	UTF32_String decimal_string = {};
	decimal_string.data = (u32*)((u8*)arena->data + arena->used);
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

	result.length = result.capacity = (u32*)((u8*)arena->data + arena->used) - result.data;

	return(result);
}

UTF32_String
concatenate(Memory_Arena *arena, UTF32_String a, UTF32_String b)
{
	UTF32_String concatenation = make_empty_string(arena, a.length + b.length);
	concatenation.length = concatenation.capacity;
	for (u64 i = 0; i < a.length; i++)
		concatenation[i] = a[i];
	for (u64 i = 0; i < b.length; i++)
		concatenation[i + a.length] = b[i];
	return(concatenation);
}

bool32
insert_character_if_fits(UTF32_String *into, u32 character, u64 at)
{
	if (into->capacity > into->length + 1)
	{
		assert(at <= into->length); // should be contiguous

		for (u64 i = into->length, c = 0;
			c < into->length - at; --i, ++c)
		{
			(*into)[i] = (*into)[into->length - c - 1];
		}

		(*into)[at] = character;

		into->length += 1;
		return(true);
	}
	return(false);
}

bool32
insert_string_if_fits(UTF32_String *into, UTF32_String other, u64 at)
{
	if (into->capacity > (into->length + other.length))
	{
		assert(at <= into->length); // should be contiguous

		for (u64 i = into->length + other.length - 1, c = 0;
			c < into->length - at; --i, ++c)
		{
			(*into)[i] = (*into)[into->length - c - 1];
		}

		for (u64 i = 0; i < other.length; ++i)
		{
			(*into)[at + i] = other[i];
		}

		into->length += other.length;
		return(true);
	}
	return(false);
}

void
remove_from_string(UTF32_String *from, u64 at, u64 count)
{
	assert(at < from->length);
	count = minimum(count, from->length - at);

	for (u64 i = at + count, c = 0; i < from->length; ++i, ++c)
		from->data[at + c] = from->data[i];

	from->length -= count;
}

s64
parse_integer(UTF32_String text)
{
	assert(text[0] != '-');    // assume always positive
	assert(text.length <= 18); // max 18-digit integer (1 quintillion - 1)

	s64 result = 0;
	for (u64 i = 0; i < text.length; i++)
	{
		u32 codepoint = text[i];
		if (codepoint >= '0' && codepoint <= '9') {
			result *= 10;
			result += codepoint - '0';
		} else if (codepoint != '_')
			throw("Parse failure: invalid literal");
	}

	return(result);
}

f64
parse_float(UTF32_String text)
{
	assert(text[0] != '-');
	u64 decimal_offset = 0;
	bool32 has_decimal = false;
	for (; decimal_offset < text.length; ++decimal_offset)
	{
		if (text[decimal_offset] == '.')
		{
			has_decimal = true;
			break;
		}
	}

	f64 result = (f64)parse_integer(substring(text, 0, decimal_offset));

	if (has_decimal)
	{
		f64 pow_ten = 1;
		for (u64 i = decimal_offset + 1; i < text.length; ++i)
		{
			if (text[i] >= '0' && text[i] <= '9')
				pow_ten *= 10;
			else if (text[i] != '_')
				throw("Parse failure: invalid literal");
		}
		result += parse_integer(substring(text, decimal_offset + 1)) / pow_ten;
	}

	return(result);
}