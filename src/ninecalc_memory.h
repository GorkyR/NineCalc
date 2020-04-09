#pragma once
#include "gr.h"

struct Memory
{
	void *contents;
	u64 size;
	u64 used;
};