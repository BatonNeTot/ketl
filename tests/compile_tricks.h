/*🍲Ketl🍲*/
#ifndef compile_tricks_h
#define compile_tricks_h

#define BEFORE_MAIN_ACTION(action) \
namespace {\
	static bool success = action(); \
}

#endif // compile_tricks_h