#pragma once
#include "grs.h"

struct Memory
{
	void *contents;
	u64 size;
	u64 used;
};

internal void *
allocate_bytes(Memory *arena, u64 size)
{
	assert(arena->used + size <= arena->size);
	void *memory = (u8*)arena->contents + arena->used;
	arena->used += size;
	return(memory);
}
#define allocate_struct(arena, type)       (type *)allocate_bytes(arena, sizeof(type))
#define allocate_array(arena, type, count) (type *)allocate_bytes(arena, sizeof(type) * count)
#define allocate_arena(arena, size)        Memory{ allocate_bytes(arena, size), size, 0 }

#define free_arena(arena) { arena.used = 0; }