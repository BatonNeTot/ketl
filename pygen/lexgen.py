import sys
from string import Template
from enum import Enum
import os
from stat import S_IREAD, S_IRGRP, S_IROTH, S_IWUSR 

Tokens = Enum('Tokens', [
	'ID',
	'LITERAL_INTEGER',
	'LITERAL_STRING',
	'LITERAL_CHAR',

	'PARENTHESIS_LEFT', 
	'PARENTHESIS_RIGHT',
	'CURLY_LEFT', 
	'CURLY_RIGHT',
	'SQUARE_LEFT', 
	'SQUARE_RIGHT', 
	'DOT',
	'COMMA',
	'TERNARY_FIRST',
	'TERNARY_SECOND',
	'TERMINATION_CHARACTER',

	'LOGICAL_NOT',
	'LOGICAL_AND',
	'LOGICAL_OR',
	'LESS',
	'LESS_OR_EQUAL',
	'GREATER',
	'GREATER_OR_EQUAL',
	'EQUAL',
	'NOT_EQUAL',

	'BITWISE_NOT',
	'BITWISE_AND',
	'BITWISE_OR',
	'BITWISE_XOR',
	'BITWISE_SHIFT_LEFT',
	'BITWISE_SHIFT_RIGHT',

	'INCREMENT',
	'DECREMENT',
	'PLUS',
	'MINUS',
	'MULTIPLY',
	'DIVIDE',
	'REMAINDER',

	'ASSIGN',
	'ASSIGN_PLUS',
	'ASSIGN_MINUS',
	'ASSIGN_MULTIPLY',
	'ASSIGN_DIVIDE',
	'ASSIGN_REMAINDER',
	'ASSIGN_BITWISE_SHIFT_LEFT',
	'ASSIGN_BITWISE_SHIFT_RIGHT',
	'ASSIGN_BITWISE_AND',
	'ASSIGN_BITWISE_OR',
	'ASSIGN_BITWISE_XOR',

	'MACRO_DEFINE',
	'MACRO_INCLUDE',

	'I8',
	'I16',
	'I32',
	'I64',
	'U8',
	'U16',
	'U32',
	'U64',
	'F32',
	'F64',

	'AUTO',
	'BOOL',
	'BREAK',
	'CASE',
	'CHAR',
	'CONST',
	'CONTINUE',
	'DEFAULT',
	'DO',
	'DOUBLE',
	'ELSE',
	'ENUM',
	'EXTERN',
	'FALSE',
	'FLOAT',
	'FOR',
	'GOTO',
	'IF',
	'INLINE',
	'INT',
	'LONG',
	'REGISTER',
	'RETURN',
	'SHORT',
	'SIGNED',
	'SIZEOF',
	'STATIC',
	'STRUCT',
	'SWITCH',
	'TRUE',
	'TYPEDEF',
	'UNION',
	'UNSIGNED',
	'VOID',
	'VOLATILE',
	'WHILE',
], start=0)


if __name__ == '__main__':
	templateFilename = sys.argv[1]
	outputFilename = sys.argv[2]

	templateSrc = ''
	with open(templateFilename, 'r') as templateFile:
		templateSrc = templateFile.read()

	tokenIds = [(token.name, token.value) for token in Tokens] + [('TOTAL', len(Tokens))]
	tokenMaxSize = len(max(tokenIds, key=lambda pair: len(pair[0]))[0])
	templatingMapping = {
		'tokenIds' : '\n'.join(f'#define KETL_TOKEN_TYPE_{id.ljust(tokenMaxSize)} {value}' for id, value in tokenIds)
    }

	templateSrc = Template(templateSrc).substitute(templatingMapping)

	if os.path.isfile(outputFilename):
		os.chmod(outputFilename, S_IWUSR|S_IREAD)
	with open(outputFilename, 'w') as outputFile:
		outputFile.write(templateSrc)
	os.chmod(outputFilename, S_IREAD|S_IRGRP|S_IROTH)