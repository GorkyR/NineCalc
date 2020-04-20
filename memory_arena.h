#pragma once
#include "grs.h"

struct Memory_Arena
{
	u8* data;
	u64 size;
	u64 used;
};

internal void *allocate_bytes(Memory_Arena *arena, u64 size);
#define allocate_struct(arena, type)       (type *)allocate_bytes(arena, sizeof(type))
#define allocate_array(arena, type, count) (type *)allocate_bytes(arena, sizeof(type) * count)
#define allocate_arena(arena, size)        Memory_Arena{ allocate_bytes(arena, size), size, 0 }

//////////////////////

internal void *
allocate_bytes(Memory_Arena *arena, u64 size)
{
	assert(arena->used + size <= arena->size);
	void *memory = (u8*)arena->data + arena->used;
	arena->used += size;
	return(memory);
}