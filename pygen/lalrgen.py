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

			Prod('expr', [ Term("PARENTHESIS_LEFT"), Nonterm('expr'), Term("PARENTHESIS_RIGHT") ],	
				action="result.var_id = stack_top(2).var_id;"),
			Prod('expr', [ Term("ID") ],				
				action="result.var_id = call(push_literal_id, stack_top(1));"),
			Prod('expr', [ Term("LITERAL_INTEGER") ],				
				action="result.var_id = call(push_literal_number, stack_top(1));"),

			Prod('call_args_tail', [ Term("COMMA"), Nonterm('expr'), Nonterm('call_args_tail') ],	
				action="result.arguments_counter = call(push_hir_argument, stack_top(2).var_id, stack_top(1).arguments_counter);"),
			Prod('call_args_tail', []),
			Prod('call_args', [ Nonterm('expr'), Nonterm('call_args_tail') ],	
				action="result.arguments_counter = call(push_hir_argument, stack_top(2).var_id, stack_top(1).arguments_counter);"),
			Prod('call_args', []),
			Prod('expr', [ Nonterm('expr'), Term("PARENTHESIS_LEFT"), Nonterm('call_args'), Term('PARENTHESIS_RIGHT') ],	
				operatorPrecedence=0,	
				action="result.var_id = call_with_pos(push_hir_call, stack_top(4).var_id, stack_top(2).arguments_counter);"),
				

			Prod('statement', [ Nonterm('expr'), Term("TERMINATION_CHARACTER") ]),
			Prod('statement', [ Nonterm('type'), Term('ID'), Term('ASSIGN'), Nonterm('expr'), Term("TERMINATION_CHARACTER") ],
				action="call_with_pos(push_hir_variable_declaration, stack_top(4), stack_top(5).type_index, stack_top(2).var_id);"),
				
			Prod('statement', [ Nonterm('expr'), Term('ASSIGN'), Nonterm('expr'), Term("TERMINATION_CHARACTER") ],
				operatorPrecedence=3,	
				action="call_with_pos(push_hir_assign, stack_top(4).var_id, stack_top(2).var_id);"),

			Prod('expr', [ Nonterm('expr'), Term("MULTIPLY"), Nonterm('expr') ],	
				operatorPrecedence=1,	
				action="result.var_id = call_with_pos(push_hir_binary_op, KETL_HIR_MULTY_UNDEF, stack_top(3).var_id, stack_top(1).var_id);"),
			Prod('expr', [ Nonterm('expr'), Term("DIVIDE"), Nonterm('expr') ],	
				operatorPrecedence=1,	
				action="result.var_id = call_with_pos(push_hir_binary_op, KETL_HIR_DIV_UNDEF, stack_top(3).var_id, stack_top(1).var_id);"),
			Prod('expr', [ Nonterm('expr'), Term("PLUS"), Nonterm('expr') ],	
				operatorPrecedence=2,	
				action="result.var_id = call_with_pos(push_hir_binary_op, KETL_HIR_PLUS_UNDEF, stack_top(3).var_id, stack_top(1).var_id);"),
			Prod('expr', [ Nonterm('expr'), Term("MINUS"), Nonterm('expr') ],	
				operatorPrecedence=2,	
				action="result.var_id = call_with_pos(push_hir_binary_op, KETL_HIR_MINUS_UNDEF, stack_top(3).var_id, stack_top(1).var_id);"),

			Prod('type', [ Term('ID') ], action="result.type_index = call(find_type, stack_top(1));"),
			Prod('type', [ Term('I8') ], action="result.type_index = call(find_type, stack_top(1));"),
			Prod('type', [ Term('I16') ], action="result.type_index = call(find_type, stack_top(1));"),
			Prod('type', [ Term('I32') ], action="result.type_index = call(find_type, stack_top(1));"),
			Prod('type', [ Term('I64') ], action="result.type_index = call(find_type, stack_top(1));"),
			Prod('type', [ Term('U8') ], action="result.type_index = call(find_type, stack_top(1));"),
			Prod('type', [ Term('U16') ], action="result.type_index = call(find_type, stack_top(1));"),
			Prod('type', [ Term('U32') ], action="result.type_index = call(find_type, stack_top(1));"),
			Prod('type', [ Term('U64') ], action="result.type_index = call(find_type, stack_top(1));"),
			Prod('type', [ Term('F32') ], action="result.type_index = call(find_type, stack_top(1));"),
			Prod('type', [ Term('F64') ], action="result.type_index = call(find_type, stack_top(1));"),

			Prod('statement', [ Term('RETURN'), Term("TERMINATION_CHARACTER") ],
				action="call_with_pos(push_hir_instr, KETL_HIR_RETURN);"),
			Prod('statement', [ Term('RETURN'), Nonterm('expr'), Term("TERMINATION_CHARACTER") ],
				action="call_with_pos(push_hir_return_value, stack_top(2).var_id);"),

		], Tokens, 
		['ltr', 'ltr', 'ltr', 'rtl'],
		
		).getCTemplateMapping()

	templateSrc = Template(templateSrc).substitute(templatingMapping)

	if os.path.isfile(outputFilename):
		os.chmod(outputFilename, S_IWUSR|S_IREAD)
	with open(outputFilename, 'w') as outputFile:
		outputFile.write(templateSrc)
	os.chmod(outputFilename, S_IREAD|S_IRGRP|S_IROTH)

