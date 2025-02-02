//🫖ketl
#ifndef ketl_utils_h
#define ketl_utils_h

#include <limits.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)

    #define KETL_OS_WINDOWS 1
    #define KETL_OS_LINUX 0
    #define KETL_OS_MAC 0

    #ifdef _WIN64
        //define something for Windows (64-bit only)
    #else
        //define something for Windows (32-bit only)
    #endif
#elif __APPLE__
    #include <TargetConditionals.h>
    #if TARGET_IPHONE_SIMULATOR
         // iOS, tvOS, or watchOS Simulator
    #elif TARGET_OS_MACCATALYST

        #define KETL_OS_WINDOWS 0
        #define KETL_OS_LINUX 0
        #define KETL_OS_MAC 1

    #elif TARGET_OS_IPHONE
        // iOS, tvOS, or watchOS device
    #elif TARGET_OS_MAC
    
        #define KETL_OS_WINDOWS 0
        #define KETL_OS_LINUX 0
        #define KETL_OS_MAC 1

    #else
    #   error "Unknown Apple platform"
    #endif
#elif __ANDROID__
    // Below __linux__ check should be enough to handle Android,
    // but something may be unique to Android.
#elif __linux__

    #define KETL_OS_WINDOWS 0
    #define KETL_OS_LINUX 1
    #define KETL_OS_MAC 0

#elif __unix__ // all unices not caught above
    // Unix
#elif defined(_POSIX_VERSION)
    // POSIX
#else
#   error "Unknown compiler"
#endif


#if KETL_OS_WINDOWS
    #include <debugapi.h>
    #define KETL_DEBUGBREAK() __debugbreak()
#else
    #define KETL_DEBUGBREAK() do { __asm__ volatile("int $0x03"); int nothing = 0; (void)nothing; } while(0)
#endif

#define KETL_HASH_DEFAULT(a) ((uint64_t)(a))
#define KETL_EQUAL_DEFAULT(a, b) ((a) == (b))

#define KETL_LESS_DEFAULT(a, b) ((a) < (b))

#define KETL_MAX(a, b) ((a) > (b) ? (a) : (b))
#define KETL_MIN(a, b) ((a) < (b) ? (a) : (b))

#define __KETL_STR_VALUE(x) #x
#define KETL_STR_VALUE(x) __KETL_STR_VALUE(x)

#define __KETL_CONCAT(a,b,c,d,e,f,g,i,j,k,l,m,n,o,p,...) a##b##c##d##e##f##g##i##j##k##l##m##n##o##p
#define KETL_CONCAT(...) __KETL_CONCAT(__VA_ARGS__,,,,,,,,,,,,,,,,,)

#define KETL_FOREVER while(1)

#define KETL_STRUCT_PREFIX _ketl_struct_
#define KETL_FORWARD(name) typedef struct KETL_CONCAT(KETL_STRUCT_PREFIX, name) name
#define KETL_DEFINE(name) KETL_FORWARD(name); struct KETL_CONCAT(KETL_STRUCT_PREFIX, name)

#ifdef NDEBUG
    #if !KETL_OS_WINDOWS
        #define KETL_NODEFAULT() default: __builtin_unreachable();
    #else
        #define KETL_NODEFAULT() default: __assume(0);
    #endif
#else
    // This code is supposed to be unreachable, so assert
    #define KETL_NODEFAULT() default: KETL_DEBUGBREAK(); //TODO assert
#endif

#define KETL_NULL_TERMINATED_LENGTH_32 ((uint32_t)-1)
#define KETL_NULL_TERMINATED_LENGTH_64 ((uint64_t)-1)

#define KETL_ALIGN(size, align) (((size) + ((align) - 1)) & ~((align) - 1))

int64_t ketl_str_to_i64(const char* str, size_t length);

#endif // ketl_utils_h
