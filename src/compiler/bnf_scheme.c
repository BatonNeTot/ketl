//🍲ketl
#include "bnf_scheme.h"

#include "bnf_node.h"
#include "ketl/object_pool.h"

#define CREATE_CHILD(node, childName) KETLBnfNode* childName = (node)->firstChild = ketlGetFreeObjectFromPool(bnfNodePool)
#define CREATE_SIBLING(node) (node) = (node)->nextSibling = ketlGetFreeObjectFromPool(bnfNodePool)


KETLBnfNode* ketlBuildBnfScheme(KETLObjectPool* bnfNodePool) {
	//// forward declarated some stuff
	KETLBnfNode* command = ketlGetFreeObjectFromPool(bnfNodePool);
	KETLBnfNode* expression = ketlGetFreeObjectFromPool(bnfNodePool);
	KETLBnfNode* several_commands = ketlGetFreeObjectFromPool(bnfNodePool);

	//// expression
	KETLBnfNode* primary = expression;
	primary->nextSibling = NULL;
	primary->type = KETL_BNF_NODE_TYPE_OR;
	{
		CREATE_CHILD(primary, orChild);
		orChild->type = KETL_BNF_NODE_TYPE_ID;

		CREATE_SIBLING(orChild);
		orChild->type = KETL_BNF_NODE_TYPE_NUMBER;

		CREATE_SIBLING(orChild);
		orChild->type = KETL_BNF_NODE_TYPE_CONCAT;
		{
			CREATE_CHILD(orChild, concatChild);
			concatChild->type = KETL_BNF_NODE_TYPE_SERVICE_LITERAL;
			concatChild->value = "(";

			CREATE_SIBLING(concatChild);
			concatChild->type = KETL_BNF_NODE_TYPE_REF;
			concatChild->ref = expression;

			CREATE_SIBLING(concatChild);
			concatChild->type = KETL_BNF_NODE_TYPE_SERVICE_LITERAL;
			concatChild->value = ")";

			concatChild->nextSibling = NULL;
		}
	}

	//// define_variable_assignment
	KETLBnfNode* define_variable_assigment = ketlGetFreeObjectFromPool(bnfNodePool);
	define_variable_assigment->nextSibling = NULL;
	define_variable_assigment->type = KETL_BNF_NODE_TYPE_CONCAT;
	{
		CREATE_CHILD(define_variable_assigment, concatChild);
		concatChild->type = KETL_BNF_NODE_TYPE_SERVICE_LITERAL;
		concatChild->value = "let";

		CREATE_SIBLING(concatChild);
		concatChild->type = KETL_BNF_NODE_TYPE_ID;

		CREATE_SIBLING(concatChild);
		concatChild->type = KETL_BNF_NODE_TYPE_SERVICE_LITERAL;
		concatChild->value = "=";

		CREATE_SIBLING(concatChild);
		concatChild->type = KETL_BNF_NODE_TYPE_REF;
		concatChild->ref = expression;

		CREATE_SIBLING(concatChild);
		concatChild->type = KETL_BNF_NODE_TYPE_SERVICE_LITERAL;
		concatChild->value = ";";

		concatChild->nextSibling = NULL;
	}

	//// if_else

	KETLBnfNode* if_else = ketlGetFreeObjectFromPool(bnfNodePool);
	if_else->nextSibling = NULL;
	if_else->type = KETL_BNF_NODE_TYPE_CONCAT;
	{
		CREATE_CHILD(if_else, currentChild);
		currentChild->type = KETL_BNF_NODE_TYPE_SERVICE_LITERAL;
		currentChild->value = "if";

		CREATE_SIBLING(currentChild);
		currentChild->type = KETL_BNF_NODE_TYPE_SERVICE_LITERAL;
		currentChild->value = "(";

		CREATE_SIBLING(currentChild);
		currentChild->type = KETL_BNF_NODE_TYPE_REF;
		currentChild->ref = expression;

		CREATE_SIBLING(currentChild);
		currentChild->type = KETL_BNF_NODE_TYPE_SERVICE_LITERAL;
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
			concatChild->type = KETL_BNF_NODE_TYPE_SERVICE_LITERAL;
			concatChild->value = "else";

			CREATE_SIBLING(concatChild);
			concatChild->type = KETL_BNF_NODE_TYPE_REF;
			concatChild->ref = command;

			concatChild->nextSibling = NULL;
		}
	}

	//// command
	
	command->nextSibling = NULL;
	command->type = KETL_BNF_NODE_TYPE_OR;

	{
		CREATE_CHILD(command, currentChild);
		currentChild->type = KETL_BNF_NODE_TYPE_REF;
		currentChild->ref = if_else;

		CREATE_SIBLING(currentChild);
		currentChild->type = KETL_BNF_NODE_TYPE_REF;
		currentChild->ref = NULL; // TODO whileElse

		CREATE_SIBLING(currentChild);
		currentChild->type = KETL_BNF_NODE_TYPE_REF;
		currentChild->ref = NULL; // TODO return

		CREATE_SIBLING(currentChild);
		currentChild->type = KETL_BNF_NODE_TYPE_REF;
		currentChild->ref = NULL; // TODO defineVariable

		CREATE_SIBLING(currentChild);
		currentChild->type = KETL_BNF_NODE_TYPE_REF;
		currentChild->ref = NULL; // TODO defineVariableAssignment

		CREATE_SIBLING(currentChild);
		currentChild->type = KETL_BNF_NODE_TYPE_REF;
		currentChild->ref = NULL; // TODO defineFunction

		CREATE_SIBLING(currentChild);
		currentChild->type = KETL_BNF_NODE_TYPE_REF;
		currentChild->ref = NULL; // TODO defineStruct

		CREATE_SIBLING(currentChild);
		currentChild->type = KETL_BNF_NODE_TYPE_CONCAT;
		{
			CREATE_CHILD(currentChild, concatChild);
			concatChild->type = KETL_BNF_NODE_TYPE_OPTIONAL;
			concatChild->firstChild = ketlGetFreeObjectFromPool(bnfNodePool);
			concatChild->firstChild->nextSibling = NULL;
			concatChild->firstChild->type = KETL_BNF_NODE_TYPE_REF;
			concatChild->firstChild->ref = NULL; // TODO expression

			CREATE_SIBLING(concatChild);
			concatChild->type = KETL_BNF_NODE_TYPE_SERVICE_LITERAL;
			concatChild->value = ";";

			concatChild->nextSibling = NULL;
		}

		CREATE_SIBLING(currentChild);
		currentChild->type = KETL_BNF_NODE_TYPE_CONCAT;
		{
			CREATE_CHILD(currentChild, concatChild);
			concatChild->type = KETL_BNF_NODE_TYPE_SERVICE_LITERAL;
			concatChild->value = "{";

			CREATE_SIBLING(concatChild);
			concatChild->type = KETL_BNF_NODE_TYPE_REF;
			concatChild->ref = several_commands;

			CREATE_SIBLING(concatChild);
			concatChild->type = KETL_BNF_NODE_TYPE_SERVICE_LITERAL;
			concatChild->value = "}";

			concatChild->nextSibling = NULL;
		}
	}

	//// several_commands

	several_commands->nextSibling = NULL;
	several_commands->type = KETL_BNF_NODE_TYPE_REPEAT;

	{
		CREATE_CHILD(several_commands, several_commands_child);
		several_commands_child->type = KETL_BNF_NODE_TYPE_REF;
		several_commands_child->ref = command;
		several_commands_child->nextSibling = NULL;
	}

	return several_commands;
}

#undef CREATE_SIBLING
#undef CREATE_CHILD
