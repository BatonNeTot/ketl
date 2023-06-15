//🍲ketl
#include "token.h"

#include <stdlib.h>

void ketlFreeToken(KETLToken* token) {
	free(token);
}