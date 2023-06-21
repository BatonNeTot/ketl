﻿//🍲ketl
#ifndef compiler_syntax_parser_h
#define compiler_syntax_parser_h

#include "ketl/common.h"

KETL_FORWARD(KETLObjectPool);
KETL_FORWARD(KETLStackIterator);
KETL_FORWARD(KETLSyntaxNode);

KETLSyntaxNode* ketlParseSyntax(KETLObjectPool* syntaxNodePool, KETLStackIterator* bnfStackIterator);

#endif /*compiler_syntax_parser_h*/