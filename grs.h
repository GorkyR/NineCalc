#pragma once

#define internal   static
#define global     static
#define persistent static

typedef signed char  s8;
typedef signed short s16;
typedef signed int   s32;
typedef signed long long s64;

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;

typedef float  f32;
typedef long double f64;

typedef u8  bool8;
typedef u32 bool32;
typedef u64 bool64;

#ifdef DEBUG
	#define assert(assertion) { if (!(assertion)) throw("Assertion failure"); }
#else
	#define assert(assertion)
#endif

#define swap(a, b) { auto _ = a; a = b; b = _; }

internal inline s64 minimum(s64 a, s64 b)
{
	return a < b? a : b;
}
internal inline s64 maximum(s64 a, s64 b)
{
	return a < b? b : a;
}

internal inline s64 clamp(s64 value, s64 min, s64 max)
{
	return value < min? min : (value > max? max : value);
}