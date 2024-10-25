// MTR https://github.com/ougi-washi/m3u8-to-rtmp

#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

// Base types
typedef uint8_t		    u8;
typedef uint16_t	    u16;
typedef uint32_t    	u32;
typedef uint64_t	    u64;
typedef int8_t	    	i8;
typedef int16_t 		i16;
typedef int32_t		    i32;
typedef int64_t	    	i64;
typedef float   		f32;
typedef double		    f64;
typedef size_t		    sz;
typedef bool		    b8;
typedef char		    c8;

// Platform detection
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
#define PLATFORM_WINDOWS 1
#elif defined(__linux__) || defined(__gnu_linux__)
// Linux OS
#define PLATFORM_LINUX 1
#if defined(__ANDROID__)
#define PLATFORM_ANDROID 1
#endif
#elif defined(__unix__)
// Catch anything not caught by the above.
#define PLATFORM_UNIX 1
#elif defined(_POSIX_VERSION)
// POSIX
#define PLATFORM_POSIX 1
#elif __APPLE__
// Apple platforms
#define PLATFORM_APPLE 1
#include <TargetConditionals.h>
#if TARGET_IPHONE_SIMULATOR
// iOS Simulator
#define PLATFORM_IOS 1
#define PLATFORM_IOS_SIMULATOR 1
#elif TARGET_OS_IPHONE
#define PLATFORM_IOS 1
// iOS device
#elif TARGET_OS_MAC
// Other kinds of Mac OS
#else
#error "Unknown Apple platform"
#endif
#else
#error "Unknown platform"
#endif
