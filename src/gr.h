#pragma once
#define internal   static
#define global     static
#define persistent static

typedef signed char  s8;
typedef signed short s16;
typedef signed int   s32;
typedef signed long  s64;

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;

typedef float  f32;
typedef double f64;

typedef s32 bool32;
typedef s64 bool64;

inline s32
minimum (s32 a, s32 b)
{
	s32 result = (a < b)? a : b;
	return(result);
}
inline s32
maximum (s32 a, s32 b)
{
	s32 result = (a < b)? b : a;
	return(result);
}
#define swap(a, b) { auto _ = a; a = b; b = _; }

inline u64 kibibytes(u64 n) { return(n << 10); }
inline u64 mebibytes(u64 n) { return(n << 20); }
inline u64 gibibytes(u64 n) { return(n << 30); }
inline u64 tebibytes(u64 n) { return(n << 40); }

#define array_count(array) (sizeof(array) / sizeof(array[0]))

#ifdef DEBUG
	u8 *__ASSERT_FAILURE__ = 0;
	#define assert(expression) { if (!(expression)) *__ASSERT_FAILURE__ = 0; }
#else
	#define assert(expression)
#endif