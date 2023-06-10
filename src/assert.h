//🍲ketl
#ifndef assert_h
#define assert_h

#include <stdio.h>

// CHECK_VOE - Verify Or Error 
// CHECK_VOEM - Verify Or Error Message

#ifndef NDEBUG

#define KETL_CHECK_VOE(x) (!(x) && (fprintf(stderr, "[KETL::E] %s", #x), true))

#define KETL_CHECK_VOEM(x, message) (!(x) && (fprintf(stderr, "[KETL::E] %s", message), true))

#else

#define KETL_CHECK_VOE(x) (!(x))

#define KETL_CHECK_VOEM(x, message) (!(x))


#endif // NDEBUG

#endif /*assert_h*/
