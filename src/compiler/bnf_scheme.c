//🍲ketl
#include "bnf_scheme.h"

#include "bnf_node.h"
#include "ketl/object_pool.h"

#include <string.h>

#define CURRENT_NODE __current

#define SET_CURRENT(node) __current = (node)

#define CREATE_ROOT(node) KETLBnfNode* node = CURRENT_NODE = ketlGetFreeObjectFromPool(bnfNodePool);\
CURRENT_NODE->nextSibling = NULL
#define CREATE_ROOT_OF(node, copy) KETLBnfNode* node = CURRENT_NODE = (copy);\
CURRENT_NODE->nextSibling = NULL

#define CREATE_CHILD() \
KETLBnfNode* __temp = CURRENT_NODE;\
KETLBnfNode* CURRENT_NODE = __temp->firstChild = ketlGetFreeObjectFromPool(bnfNodePool);\
CURRENT_NODE->builder = KETL_SYNTAX_BUILDER_TYPE_NONE; do {

#define CREATE_SIBLING() CURRENT_NODE = CURRENT_NODE->nextSibling = ketlGetFreeObjectFromPool(bnfNodePool);\
CURRENT_NODE->builder = KETL_SYNTAX_BUILDER_TYPE_NONE

#define CLOSE_CHILD() CURRENT_NODE->nextSibling = NULL; } while (0) 

#define SET_TYPE(_type) CURRENT_NODE->type = (_type)

#define SET_CONSTANT(_value) SET_TYPE(KETL_BNF_NODE_TYPE_CONSTANT);\
CURRENT_NODE->value = (_value)

#define SET_REF(_ref) SET_TYPE(KETL_BNF_NODE_TYPE_REF);\
CURRENT_NODE->ref = (_ref)


KETLBnfNode* ketlBuildBnfScheme(KETLObjectPool* bnfNodePool) {
	ketlResetPool(bnfNodePool);
	KETLBnfNode* __current = NULL;

	//// forward declarated some stuff
	CREATE_ROOT(command);
	CREATE_ROOT(expression);
	CREATE_ROOT(severalCommands);

	//// expression
	//// primary
	CREATE_ROOT(primary);
	primary->builder = KETL_SYNTAX_BUILDER_TYPE_PRIMARY_EXPRESSION;
	primary->type = KETL_BNF_NODE_TYPE_OR;
	{
		CREATE_CHILD();
		SET_TYPE(KETL_BNF_NODE_TYPE_ID);

		CREATE_SIBLING();
		SET_TYPE(KETL_BNF_NODE_TYPE_NUMBER);

		CREATE_SIBLING();
		SET_TYPE(KETL_BNF_NODE_TYPE_STRING);

		CREATE_SIBLING();
		SET_TYPE(KETL_BNF_NODE_TYPE_CONCAT);
		{
			CREATE_CHILD();
			SET_CONSTANT("(");

			CREATE_SIBLING();
			SET_REF(expression);

			CREATE_SIBLING();
			SET_CONSTANT(")");

			CLOSE_CHILD();
		}

		CLOSE_CHILD();
	}

	//// precedenceExpression1
	CREATE_ROOT(precedenceExpression1);
	precedenceExpression1->builder = KETL_SYNTAX_BUILDER_TYPE_PRECEDENCE_EXPRESSION_1;
	precedenceExpression1->type = KETL_BNF_NODE_TYPE_CONCAT;
	{
		CREATE_CHILD();
		SET_REF(primary);

		CREATE_SIBLING();
		SET_TYPE(KETL_BNF_NODE_TYPE_REPEAT);
		{
			CREATE_CHILD();
			SET_TYPE(KETL_BNF_NODE_TYPE_CONCAT);
			{
				CREATE_CHILD();
				SET_CONSTANT(".");

				CREATE_SIBLING();
				SET_REF(primary);

				CLOSE_CHILD();
			}

			CLOSE_CHILD();
		}
		CLOSE_CHILD();
	}

	//// precedenceExpression2
	CREATE_ROOT(precedenceExpression2);
	precedenceExpression2->builder = KETL_SYNTAX_BUILDER_TYPE_PRECEDENCE_EXPRESSION_2;
	precedenceExpression2->type = KETL_BNF_NODE_TYPE_CONCAT;
	{
		CREATE_CHILD();
		SET_REF(precedenceExpression1);

		CREATE_SIBLING();
		SET_TYPE(KETL_BNF_NODE_TYPE_REPEAT);
		{
			CREATE_CHILD();
			SET_TYPE(KETL_BNF_NODE_TYPE_CONCAT);
			{
				CREATE_CHILD();
				SET_CONSTANT("(");

				CREATE_SIBLING();
				SET_TYPE(KETL_BNF_NODE_TYPE_OPTIONAL);
				{
					CREATE_CHILD();
					SET_REF(precedenceExpression1);

					CREATE_SIBLING();
					SET_TYPE(KETL_BNF_NODE_TYPE_REPEAT);
					{
						CREATE_CHILD();
						SET_CONSTANT(",");

						CREATE_SIBLING();
						SET_REF(precedenceExpression1);

						CLOSE_CHILD();
					}

					CLOSE_CHILD();
				}

				CREATE_SIBLING();
				SET_CONSTANT(")");

				CLOSE_CHILD();
			}

			CLOSE_CHILD();
		}
		CLOSE_CHILD();
	}

	//// precedenceExpression4
	CREATE_ROOT(precedenceExpression4);
	precedenceExpression4->builder = KETL_SYNTAX_BUILDER_TYPE_PRECEDENCE_EXPRESSION_4;
	precedenceExpression4->type = KETL_BNF_NODE_TYPE_CONCAT;
	{
		CREATE_CHILD();
		SET_REF(precedenceExpression2);

		CREATE_SIBLING();
		SET_TYPE(KETL_BNF_NODE_TYPE_REPEAT);
		{
			CREATE_CHILD();
			SET_TYPE(KETL_BNF_NODE_TYPE_CONCAT);
			{
				CREATE_CHILD();
				SET_TYPE(KETL_BNF_NODE_TYPE_OR);
				{
					CREATE_CHILD();
					SET_CONSTANT("+");

					CREATE_SIBLING();
					SET_CONSTANT("-");

					CLOSE_CHILD();
				}

				CREATE_SIBLING();
				SET_REF(precedenceExpression2);

				CLOSE_CHILD();
			}
			CLOSE_CHILD();
		}
		CLOSE_CHILD();
	}

	//// precedenceExpression5
	CREATE_ROOT(precedenceExpression5);
	precedenceExpression5->builder = KETL_SYNTAX_BUILDER_TYPE_PRECEDENCE_EXPRESSION_5;
	precedenceExpression5->type = KETL_BNF_NODE_TYPE_CONCAT;
	{
		CREATE_CHILD();
		SET_REF(precedenceExpression4);

		CREATE_SIBLING();
		SET_TYPE(KETL_BNF_NODE_TYPE_REPEAT);
		{
			CREATE_CHILD();
			SET_TYPE(KETL_BNF_NODE_TYPE_CONCAT);
			{
				CREATE_CHILD();
				SET_TYPE(KETL_BNF_NODE_TYPE_OR);
				{
					CREATE_CHILD();
					SET_CONSTANT("*");

					CREATE_SIBLING();
					SET_CONSTANT("/");

					CLOSE_CHILD();
				}

				CREATE_SIBLING();
				SET_REF(precedenceExpression4);

				CLOSE_CHILD();
			}
			CLOSE_CHILD();
		}
		CLOSE_CHILD();
	}

	//// precedenceExpression6
	CREATE_ROOT_OF(precedenceExpression6, expression);
	precedenceExpression6->builder = KETL_SYNTAX_BUILDER_TYPE_PRECEDENCE_EXPRESSION_6;
	precedenceExpression6->type = KETL_BNF_NODE_TYPE_CONCAT;
	{
		CREATE_CHILD();
		SET_REF(precedenceExpression5);

		CREATE_SIBLING();
		SET_TYPE(KETL_BNF_NODE_TYPE_REPEAT);
		{
			CREATE_CHILD();
			SET_TYPE(KETL_BNF_NODE_TYPE_CONCAT);
			{
				CREATE_CHILD();
				SET_TYPE(KETL_BNF_NODE_TYPE_OR);
				{
					CREATE_CHILD();
					SET_CONSTANT(":=");

					CLOSE_CHILD();
				}

				CREATE_SIBLING();
				SET_REF(precedenceExpression5);

				CLOSE_CHILD();
			}
			CLOSE_CHILD();
		}
		CLOSE_CHILD();
	}

	//// defineVariableAssigment
	CREATE_ROOT(defineVariableAssigment);
	defineVariableAssigment->builder = KETL_SYNTAX_BUILDER_TYPE_DEFINE_WITH_ASSIGNMENT;
	defineVariableAssigment->type = KETL_BNF_NODE_TYPE_CONCAT;
	{
		CREATE_CHILD();
		SET_CONSTANT("let");

		CREATE_SIBLING();
		SET_TYPE(KETL_BNF_NODE_TYPE_ID);

		CREATE_SIBLING();
		SET_CONSTANT(":=");

		CREATE_SIBLING();
		SET_REF(expression);

		CREATE_SIBLING();
		SET_CONSTANT(";");

		CLOSE_CHILD();
	}

	//// ifElse
	CREATE_ROOT(ifElse);
	ifElse->type = KETL_BNF_NODE_TYPE_CONCAT;
	{
		CREATE_CHILD();
		SET_CONSTANT("if");

		CREATE_SIBLING();
		SET_CONSTANT("(");

		CREATE_SIBLING();
		SET_REF(expression);

		CREATE_SIBLING();
		SET_CONSTANT(")");

		CREATE_SIBLING();
		SET_REF(command);

		CREATE_SIBLING();
		SET_TYPE(KETL_BNF_NODE_TYPE_OPTIONAL);
		{
			CREATE_CHILD();
			SET_TYPE(KETL_BNF_NODE_TYPE_CONCAT);
			{
				CREATE_CHILD();
				SET_CONSTANT("else");

				CREATE_SIBLING();
				SET_REF(command);

				CLOSE_CHILD();
			}

			CLOSE_CHILD();
		}

		CLOSE_CHILD();
	}
	
	//// returnNode
	CREATE_ROOT(returnNode);
	returnNode->builder = KETL_SYNTAX_BUILDER_TYPE_RETURN;
	returnNode->type = KETL_BNF_NODE_TYPE_CONCAT;
	{
		CREATE_CHILD();
		SET_CONSTANT("return");

		CREATE_SIBLING();
		SET_TYPE(KETL_BNF_NODE_TYPE_OPTIONAL);
		{
			CREATE_CHILD();
			SET_REF(expression);

			CLOSE_CHILD();
		}

		CREATE_SIBLING();
		SET_CONSTANT(";");

		CLOSE_CHILD();
	}

	//// command
	SET_CURRENT(command);
	command->builder = KETL_SYNTAX_BUILDER_TYPE_COMMAND;
	command->type = KETL_BNF_NODE_TYPE_OR;
	{
		CREATE_CHILD();
		SET_REF(ifElse);

		/*
		CREATE_SIBLING(currentChild);
		currentChild->type = KETL_BNF_NODE_TYPE_REF;
		currentChild->ref = NULL; // TODO whileElse
		*/

		CREATE_SIBLING();
		SET_REF(returnNode);

		/*
		CREATE_SIBLING(currentChild);
		currentChild->type = KETL_BNF_NODE_TYPE_REF;
		currentChild->ref = NULL; // TODO defineVariable
		*/

		CREATE_SIBLING();
		SET_REF(defineVariableAssigment);

		/*
		CREATE_SIBLING(currentChild);
		currentChild->type = KETL_BNF_NODE_TYPE_REF;
		currentChild->ref = NULL; // TODO defineFunction

		CREATE_SIBLING(currentChild);
		currentChild->type = KETL_BNF_NODE_TYPE_REF;
		currentChild->ref = NULL; // TODO defineStruct
		*/

		CREATE_SIBLING();
		SET_TYPE(KETL_BNF_NODE_TYPE_CONCAT);
		{
			CREATE_CHILD();
			SET_TYPE(KETL_BNF_NODE_TYPE_OPTIONAL);
			{
				CREATE_CHILD();
				SET_REF(expression);

				CLOSE_CHILD();
			}

			CREATE_SIBLING();
			SET_CONSTANT(";");

			CLOSE_CHILD();
		}

		CREATE_SIBLING();
		SET_TYPE(KETL_BNF_NODE_TYPE_CONCAT);
		{
			CREATE_CHILD();
			SET_CONSTANT("{");

			CREATE_SIBLING();
			SET_REF(severalCommands);

			CREATE_SIBLING();
			SET_CONSTANT("}");

			CLOSE_CHILD();
		}

		CLOSE_CHILD();
	}

	//// severalCommands
	SET_CURRENT(severalCommands);
	severalCommands->builder = KETL_SYNTAX_BUILDER_TYPE_BLOCK;
	severalCommands->type = KETL_BNF_NODE_TYPE_REPEAT;
	{
		CREATE_CHILD();
		SET_REF(command);

		CLOSE_CHILD();
	}

	KETLObjectPoolIterator bnfIterator;
	ketlInitPoolIterator(&bnfIterator, bnfNodePool);
	while (ketlIteratorPoolHasNext(&bnfIterator)) {
		KETLBnfNode* current = ketlIteratorPoolGetNext(&bnfIterator);
		switch (current->type) {
		case KETL_BNF_NODE_TYPE_CONSTANT:
			current->size = (uint32_t)strlen(current->value);
			break;
		case KETL_BNF_NODE_TYPE_CONCAT:
		case KETL_BNF_NODE_TYPE_OR:
		case KETL_BNF_NODE_TYPE_OPTIONAL:
		case KETL_BNF_NODE_TYPE_REPEAT: {
			current->size = 0;
			KETLBnfNode* it = current->firstChild;
			while (it) {
				++current->size;
				it = it->nextSibling;
			}
			break;
		}
		}
	}

	return severalCommands;
}
