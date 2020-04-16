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