from lexgen import Tokens
import sys
from string import Template
import os
from lalr import *

if __name__ == '__main__':
	templateFilename = sys.argv[1]
	outputFilename = sys.argv[2]

	templateSrc = ''
	with open(templateFilename, 'r') as templateFile:
		templateSrc = templateFile.read()

	templatingMapping = Model([
			Prod('statements', [ Nonterm('statement'), Nonterm("statements") ]),
			Prod('statements', []),
			Prod('block', [ Term('CURLY_LEFT'), Nonterm("statements"), Term("CURLY_RIGHT") ]),

			Prod('statement', [ Nonterm('expr'), Term("TERMINATION_CHARACTER") ]),
			Prod('statement', [ Term('RETURN'), Term("TERMINATION_CHARACTER") ],
				action="PUSH_HIR_INSTR(KETL_HIR_RETURN);"),
			Prod('statement', [ Term('RETURN'), Nonterm('expr'), Term("TERMINATION_CHARACTER") ],
				action="PUSH_HIR_RETURN_VALUE(STACK_TOP(2).output.var_id);"),

			Prod('expr', [ Term("PARENTHESIS_LEFT"), Nonterm('expr'), Term("PARENTHESIS_RIGHT") ],	
				action="result.var_id = STACK_TOP(2).output.var_id;"),
			Prod('expr', [ Term("ID") ],				
				action="result.var_id = PUSH_TOP_LITERAL_ID();"),
			Prod('expr', [ Term("LITERAL_INTEGER") ],				
				action="result.var_id = PUSH_TOP_LITERAL_NUMBER();"),

			Prod('call_args_tail', [ Term("COMMA"), Nonterm('expr'), Nonterm('call_args_tail') ],	
				action="result.arguments_counter = PUSH_HIR_ARGUMENT(STACK_TOP(2).output.var_id, STACK_TOP(1).output.arguments_counter);"),
			Prod('call_args_tail', []),
			Prod('call_args', [ Nonterm('expr'), Nonterm('call_args_tail') ],	
				action="result.arguments_counter = PUSH_HIR_ARGUMENT(STACK_TOP(2).output.var_id, STACK_TOP(1).output.arguments_counter);"),
			Prod('call_args', []),
			Prod('expr', [ Nonterm('expr'), Term("PARENTHESIS_LEFT"), Nonterm('call_args'), Term('PARENTHESIS_RIGHT') ],	
				operatorPrecedence=0,	
				action="result.var_id = PUSH_HIR_CALL(STACK_TOP(4).output.var_id, STACK_TOP(2).output.arguments_counter);"),
				
			Prod('expr', [ Nonterm('expr'), Term("MULTIPLY"), Nonterm('expr') ],	
				operatorPrecedence=1,	
				action="result.var_id = PUSH_HIR_BINARY_OP(KETL_HIR_MULTY_UNDEF, STACK_TOP(3).output.var_id, STACK_TOP(1).output.var_id);"),
			Prod('expr', [ Nonterm('expr'), Term("PLUS"), Nonterm('expr') ],	
				operatorPrecedence=2,	
				action="result.var_id = PUSH_HIR_BINARY_OP(KETL_HIR_PLUS_UNDEF, STACK_TOP(3).output.var_id, STACK_TOP(1).output.var_id);"),
		], Tokens, 
		['ltr', 'ltr', 'ltr'],
		
		).getCTemplateMapping()

	templateSrc = Template(templateSrc).substitute(templatingMapping)

	if os.path.isfile(outputFilename):
		os.chmod(outputFilename, S_IWUSR|S_IREAD)
	with open(outputFilename, 'w') as outputFile:
		outputFile.write(templateSrc)
	os.chmod(outputFilename, S_IREAD|S_IRGRP|S_IROTH)

