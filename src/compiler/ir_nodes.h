/*🍲Ketl🍲*/
#ifndef compiler_ir_nodes_h
#define compiler_ir_nodes_h

#include "parser.h"

#include "semantic_analyzer.h"

namespace Ketl {

	std::unique_ptr<IRNode> createBlockTree(const ProcessNode* info);

	std::unique_ptr<IRNode> proxyTree(const ProcessNode*);
	std::unique_ptr<IRNode> emptyTree(const ProcessNode*);

	std::unique_ptr<IRNode> createDefineVariableByAssignmentTree(const ProcessNode* info);
	std::unique_ptr<IRNode> createDefineFunctionTree(const ProcessNode* info);

	std::unique_ptr<IRNode> createRtlTree(const ProcessNode* info);
	std::unique_ptr<IRNode> createLtrTree(const ProcessNode* info);

	std::unique_ptr<IRNode> createVariable(const ProcessNode* info);

	std::unique_ptr<IRNode> createLiteral(const ProcessNode* info);

}

#endif /*compiler_ir_nodes_h*/