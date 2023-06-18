//🍲ketl
#include "bnf_scheme.h"

#include "bnf_node.h"
#include "ketl/object_pool.h"

#include <string.h>

#define CREATE_CHILD(node, childName) KETLBnfNode* childName = (node)->firstChild = ketlGetFreeObjectFromPool(bnfNodePool)
#define CREATE_SIBLING(node) (node) = (node)->nextSibling = ketlGetFreeObjectFromPool(bnfNodePool)


KETLBnfNode* ketlBuildBnfScheme(KETLObjectPool* bnfNodePool) {
	ketlResetPool(bnfNodePool);

	//// forward declarated some stuff
	KETLBnfNode* command = ketlGetFreeObjectFromPool(bnfNodePool);
	KETLBnfNode* expression = ketlGetFreeObjectFromPool(bnfNodePool);
	KETLBnfNode* severalCommands = ketlGetFreeObjectFromPool(bnfNodePool);

	//// expression
	//// primary
	KETLBnfNode* primary = ketlGetFreeObjectFromPool(bnfNodePool);
	primary->nextSibling = NULL;
	primary->type = KETL_BNF_NODE_TYPE_OR;
	{
		CREATE_CHILD(primary, orChild);
		orChild->type = KETL_BNF_NODE_TYPE_ID;

		CREATE_SIBLING(orChild);
		orChild->type = KETL_BNF_NODE_TYPE_NUMBER;

		CREATE_SIBLING(orChild);
		orChild->type = KETL_BNF_NODE_TYPE_STRING;

		CREATE_SIBLING(orChild);
		orChild->type = KETL_BNF_NODE_TYPE_CONCAT;
		{
			CREATE_CHILD(orChild, concatChild);
			concatChild->type = KETL_BNF_NODE_TYPE_SERVICE_CONSTANT;
			concatChild->value = "(";

			CREATE_SIBLING(concatChild);
			concatChild->type = KETL_BNF_NODE_TYPE_REF;
			concatChild->ref = expression;

			CREATE_SIBLING(concatChild);
			concatChild->type = KETL_BNF_NODE_TYPE_SERVICE_CONSTANT;
			concatChild->value = ")";

			concatChild->nextSibling = NULL;
		}

		orChild->nextSibling = NULL;
	}

	//// precedenceExpression1
	KETLBnfNode* precedenceExpression1 = ketlGetFreeObjectFromPool(bnfNodePool);
	precedenceExpression1->nextSibling = NULL;
	precedenceExpression1->type = KETL_BNF_NODE_TYPE_CONCAT;
	{
		CREATE_CHILD(precedenceExpression1, concatChild);
		concatChild->type = KETL_BNF_NODE_TYPE_REF;
		concatChild->ref = primary;

		CREATE_SIBLING(concatChild);
		concatChild->type = KETL_BNF_NODE_TYPE_REPEAT;
		{
			CREATE_CHILD(concatChild, repeatChild);
			repeatChild->nextSibling;
			repeatChild->type = KETL_BNF_NODE_TYPE_CONCAT;
			{
				CREATE_CHILD(repeatChild, innerConcatChild);
				innerConcatChild->type = KETL_BNF_NODE_TYPE_CONSTANT;
				innerConcatChild->value = ".";

				CREATE_SIBLING(innerConcatChild);
				innerConcatChild->type = KETL_BNF_NODE_TYPE_REF;
				innerConcatChild->ref = primary;

				innerConcatChild->nextSibling = NULL;
			}
			repeatChild->nextSibling = NULL;
		}
		concatChild->nextSibling = NULL;
	}

	//// precedenceExpression4
	KETLBnfNode* precedenceExpression4 = expression;
	precedenceExpression4->nextSibling = NULL;
	precedenceExpression4->type = KETL_BNF_NODE_TYPE_CONCAT;
	{
		CREATE_CHILD(precedenceExpression4, concatChild);
		concatChild->type = KETL_BNF_NODE_TYPE_REF;
		concatChild->ref = precedenceExpression1;

		CREATE_SIBLING(concatChild);
		concatChild->type = KETL_BNF_NODE_TYPE_REPEAT;
		{
			CREATE_CHILD(concatChild, repeatChild);
			repeatChild->nextSibling;
			repeatChild->type = KETL_BNF_NODE_TYPE_CONCAT;
			{
				CREATE_CHILD(repeatChild, innerConcatChild);
				innerConcatChild->type = KETL_BNF_NODE_TYPE_OR;
				{
					CREATE_CHILD(innerConcatChild, orChild);
					orChild->type = KETL_BNF_NODE_TYPE_CONSTANT;
					orChild->value = "+";

					CREATE_SIBLING(orChild);
					orChild->type = KETL_BNF_NODE_TYPE_CONSTANT;
					orChild->value = "-";

					orChild->nextSibling = NULL;
				}

				CREATE_SIBLING(innerConcatChild);
				innerConcatChild->type = KETL_BNF_NODE_TYPE_REF;
				innerConcatChild->ref = precedenceExpression1;

				innerConcatChild->nextSibling = NULL;
			}
			repeatChild->nextSibling = NULL;
		}
		concatChild->nextSibling = NULL;
	}

	//// define_variable_assignment
	KETLBnfNode* define_variable_assigment = ketlGetFreeObjectFromPool(bnfNodePool);
	define_variable_assigment->nextSibling = NULL;
	define_variable_assigment->type = KETL_BNF_NODE_TYPE_CONCAT;
	{
		CREATE_CHILD(define_variable_assigment, concatChild);
		concatChild->type = KETL_BNF_NODE_TYPE_SERVICE_CONSTANT;
		concatChild->value = "let";

		CREATE_SIBLING(concatChild);
		concatChild->type = KETL_BNF_NODE_TYPE_ID;

		CREATE_SIBLING(concatChild);
		concatChild->type = KETL_BNF_NODE_TYPE_SERVICE_CONSTANT;
		concatChild->value = ":=";

		CREATE_SIBLING(concatChild);
		concatChild->type = KETL_BNF_NODE_TYPE_REF;
		concatChild->ref = expression;

		CREATE_SIBLING(concatChild);
		concatChild->type = KETL_BNF_NODE_TYPE_SERVICE_CONSTANT;
		concatChild->value = ";";

		concatChild->nextSibling = NULL;
	}

	//// if_else

	KETLBnfNode* if_else = ketlGetFreeObjectFromPool(bnfNodePool);
	if_else->nextSibling = NULL;
	if_else->type = KETL_BNF_NODE_TYPE_CONCAT;
	{
		CREATE_CHILD(if_else, currentChild);
		currentChild->type = KETL_BNF_NODE_TYPE_SERVICE_CONSTANT;
		currentChild->value = "if";

		CREATE_SIBLING(currentChild);
		currentChild->type = KETL_BNF_NODE_TYPE_SERVICE_CONSTANT;
		currentChild->value = "(";

		CREATE_SIBLING(currentChild);
		currentChild->type = KETL_BNF_NODE_TYPE_REF;
		currentChild->ref = expression;

		CREATE_SIBLING(currentChild);
		currentChild->type = KETL_BNF_NODE_TYPE_SERVICE_CONSTANT;
		currentChild->value = ")";

		CREATE_SIBLING(currentChild);
		currentChild->type = KETL_BNF_NODE_TYPE_REF;
		currentChild->ref = command;

		CREATE_SIBLING(currentChild);
		currentChild->type = KETL_BNF_NODE_TYPE_OPTIONAL;

		{
			CREATE_CHILD(currentChild, optional);
			optional->nextSibling = NULL;
			optional->type = KETL_BNF_NODE_TYPE_CONCAT;

			CREATE_CHILD(optional, concatChild);
			concatChild->type = KETL_BNF_NODE_TYPE_SERVICE_CONSTANT;
			concatChild->value = "else";

			CREATE_SIBLING(concatChild);
			concatChild->type = KETL_BNF_NODE_TYPE_REF;
			concatChild->ref = command;

			concatChild->nextSibling = NULL;
		}

		currentChild->nextSibling = NULL;
	}

	//// command
	
	command->nextSibling = NULL;
	command->type = KETL_BNF_NODE_TYPE_OR;

	{
		CREATE_CHILD(command, currentChild);
		currentChild->type = KETL_BNF_NODE_TYPE_REF;
		currentChild->ref = if_else;

		/*
		CREATE_SIBLING(currentChild);
		currentChild->type = KETL_BNF_NODE_TYPE_REF;
		currentChild->ref = NULL; // TODO whileElse

		CREATE_SIBLING(currentChild);
		currentChild->type = KETL_BNF_NODE_TYPE_REF;
		currentChild->ref = NULL; // TODO return

		CREATE_SIBLING(currentChild);
		currentChild->type = KETL_BNF_NODE_TYPE_REF;
		currentChild->ref = NULL; // TODO defineVariable
		*/

		CREATE_SIBLING(currentChild);
		currentChild->type = KETL_BNF_NODE_TYPE_REF;
		currentChild->ref = define_variable_assigment;

		/*
		CREATE_SIBLING(currentChild);
		currentChild->type = KETL_BNF_NODE_TYPE_REF;
		currentChild->ref = NULL; // TODO defineFunction

		CREATE_SIBLING(currentChild);
		currentChild->type = KETL_BNF_NODE_TYPE_REF;
		currentChild->ref = NULL; // TODO defineStruct
		*/

		CREATE_SIBLING(currentChild);
		currentChild->type = KETL_BNF_NODE_TYPE_CONCAT;
		{
			CREATE_CHILD(currentChild, concatChild);
			concatChild->type = KETL_BNF_NODE_TYPE_OPTIONAL;
			concatChild->firstChild = ketlGetFreeObjectFromPool(bnfNodePool);
			concatChild->firstChild->nextSibling = NULL;
			concatChild->firstChild->type = KETL_BNF_NODE_TYPE_REF;
			concatChild->firstChild->ref = expression;

			CREATE_SIBLING(concatChild);
			concatChild->type = KETL_BNF_NODE_TYPE_SERVICE_CONSTANT;
			concatChild->value = ";";

			concatChild->nextSibling = NULL;
		}

		CREATE_SIBLING(currentChild);
		currentChild->type = KETL_BNF_NODE_TYPE_CONCAT;
		{
			CREATE_CHILD(currentChild, concatChild);
			concatChild->type = KETL_BNF_NODE_TYPE_SERVICE_CONSTANT;
			concatChild->value = "{";

			CREATE_SIBLING(concatChild);
			concatChild->type = KETL_BNF_NODE_TYPE_REF;
			concatChild->ref = severalCommands;

			CREATE_SIBLING(concatChild);
			concatChild->type = KETL_BNF_NODE_TYPE_SERVICE_CONSTANT;
			concatChild->value = "}";

			concatChild->nextSibling = NULL;
		}

		currentChild->nextSibling = NULL;
	}

	//// severalCommands

	severalCommands->nextSibling = NULL;
	severalCommands->type = KETL_BNF_NODE_TYPE_REPEAT;

	{
		CREATE_CHILD(severalCommands, severalCommands_child);
		severalCommands_child->type = KETL_BNF_NODE_TYPE_REF;
		severalCommands_child->ref = command;
		severalCommands_child->nextSibling = NULL;
	}

	KETLObjectPoolIterator bnfIterator;
	ketlInitPoolIterator(&bnfIterator, bnfNodePool);
	while (ketlIteratorPoolHasNext(&bnfIterator)) {
		KETLBnfNode* current = ketlIteratorPoolGetNext(&bnfIterator);
		switch (current->type) {
		case KETL_BNF_NODE_TYPE_CONSTANT:
		case KETL_BNF_NODE_TYPE_SERVICE_CONSTANT:
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

#undef CREATE_SIBLING
#undef CREATE_CHILD
