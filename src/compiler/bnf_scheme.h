//🍲ketl
#ifndef compiler_bnf_scheme_h
#define compiler_bnf_scheme_h

#include "ketl/common.h"

KETL_FORWARD(KETLBnfNode);
KETL_FORWARD(KETLObjectPool);

KETLBnfNode* ketlBuildBnfScheme(KETLObjectPool* bnfNodePool);

#endif /*compiler_bnf_scheme_h*/
