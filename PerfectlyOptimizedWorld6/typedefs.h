#pragma once
#pragma warning( disable : 26812 )

#define _USE_MATH_DEFINES
#define _CRT_SECURE_NO_WARNINGS

#include <stdint.h>

typedef char           int8;
typedef short          int16;
typedef int            int32;
typedef unsigned char  uint8;
typedef unsigned short uint16;
typedef unsigned       uint32;
typedef uint64_t       uint64;
typedef float          float32;
typedef double         float64;

#define STATIC_ASSERT(expr) typedef char __static_assert_t[(expr) != 0]
