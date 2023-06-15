//🍲ketl
#ifndef common_h
#define common_h

#define KETL_STR_VALUE_IMPL(x) #x
#define KETL_STR_VALUE(x) KETL_STR_VALUE_IMPL(x)

#define KETL_FOREVER while(true)

#define KETL_FORWARD(name) \
typedef struct name name

#define KETL_DEFINE(name) struct name

#endif /*common_h*/
