/*🍲Ketl🍲*/
#include "parser.h"

#include "ir_nodes.h"
#include "bnf_nodes.h"

namespace Ketl {

	Parser::Parser() {

		// type
		_nodes.try_emplace("type", std::make_unique<NodeConcat>(false,
			std::make_unique<NodeOr>(
				std::make_unique<NodeLeaf>(NodeLeaf::Type::Id),
				std::make_unique<NodeConcat>(false,
					std::make_unique<NodeLiteral>(false, "const"),
					std::make_unique<NodeId>(false, "type")
					),
				std::make_unique<NodeConcat>(true,
					std::make_unique<NodeLiteral>(true, "("),
					std::make_unique<NodeId>(false, "type"),
					std::make_unique<NodeLiteral>(true, ")")
					)
				),
			std::make_unique<NodeConditional>(
				std::make_unique<NodeLiteral>(false, "&&"),
				std::make_unique<NodeLiteral>(false, "&")
				)
			));

		// expression
		_nodes.try_emplace("primary", std::make_unique<NodeOr>(
			std::make_unique<NodeId>(&createLambda, false, "defineLambda"),
			std::make_unique<NodeLeaf>(&createVariable, NodeLeaf::Type::Id),
			std::make_unique<NodeLeaf>(&createLiteral, NodeLeaf::Type::Number),
			std::make_unique<NodeLeaf>(NodeLeaf::Type::String),
			std::make_unique<NodeConcat>(true,
				std::make_unique<NodeLiteral>(true, "("),
				std::make_unique<NodeId>(true, "expression"),
				std::make_unique<NodeLiteral>(true, ")")
				)
			));

		_nodes.try_emplace("precedence-1-expression", std::make_unique<NodeConcat>(true,
			std::make_unique<NodeId>(true, "primary"),
			std::make_unique<NodeConditional>(
				std::make_unique<NodeConcat>(true,
					std::make_unique<NodeLiteral>(true, "("),
					std::make_unique<NodeConditional>(
						std::make_unique<NodeConcat>(true,
							std::make_unique<NodeId>(true, "expression"),
							std::make_unique<NodeRepeat>(
								std::make_unique<NodeConcat>(true,
									std::make_unique<NodeLiteral>(true, ","),
									std::make_unique<NodeId>(true, "expression")
									)
								)
							)
						),
					std::make_unique<NodeLiteral>(true, ")")
					)
				)
			)); 

		_nodes.try_emplace("precedence-2-expression", std::make_unique<NodeConcat>(true,
			std::make_unique<NodeId>(&proxyTree, true, "precedence-1-expression"),
			std::make_unique<NodeRepeat>(
				std::make_unique<NodeConcat>(true,
					std::make_unique<NodeOr>(
						std::make_unique<NodeLiteral>(false, "*"),
						std::make_unique<NodeLiteral>(false, "/")
						),
					std::make_unique<NodeId>(&proxyTree, true, "precedence-1-expression")
					)
				)
			));

		_nodes.try_emplace("precedence-3-expression", std::make_unique<NodeConcat>(true,
			std::make_unique<NodeId>(&createLtrTree, true, "precedence-2-expression"),
			std::make_unique<NodeRepeat>(
				std::make_unique<NodeConcat>(true,
					std::make_unique<NodeOr>(
						std::make_unique<NodeLiteral>(false, "+"),
						std::make_unique<NodeLiteral>(false, "-")
						),
					std::make_unique<NodeId>(&createLtrTree, true, "precedence-2-expression")
					)
				)
			));

		_nodes.try_emplace("precedence-4-expression", std::make_unique<NodeConcat>(true,
			std::make_unique<NodeId>(&createLtrTree, true, "precedence-3-expression"),
			std::make_unique<NodeRepeat>(
				std::make_unique<NodeConcat>(true,
					std::make_unique<NodeOr>(
						std::make_unique<NodeLiteral>(false, "=="),
						std::make_unique<NodeLiteral>(false, "!=")
						),
					std::make_unique<NodeId>(&createLtrTree, true, "precedence-3-expression")
					)
				)
			));

		_nodes.try_emplace("precedence-5-expression", std::make_unique<NodeConcat>(true,
			std::make_unique<NodeId>(&createLtrTree, true, "precedence-4-expression"),
			std::make_unique<NodeRepeat>(
				std::make_unique<NodeConcat>(true,
					std::make_unique<NodeOr>(
						std::make_unique<NodeLiteral>(false, "=")
						),
					std::make_unique<NodeId>(&createLtrTree, true, "precedence-4-expression")
					)
				)
			));

		_nodes.try_emplace("expression", std::make_unique<NodeId>(&createRtlTree, true, "precedence-5-expression"));

		// define variable by assignment
		_nodes.try_emplace("defineVariableAssignment", std::make_unique<NodeConcat>(false,
			std::make_unique<NodeOr>(
				std::make_unique<NodeLiteral>(&emptyTree, false, "var"),
				std::make_unique<NodeId>(false, "type")
				),
			std::make_unique<NodeLeaf>(NodeLeaf::Type::Id),
			std::make_unique<NodeLiteral>(true, "="),
			std::make_unique<NodeId>(true, "expression"),
			std::make_unique<NodeLiteral>(true, ";")
			));

		// function parameter
		_nodes.try_emplace("functionParameter", std::make_unique<NodeConcat>(false,
			std::make_unique<NodeId>(&createType, false, "type"),
			std::make_unique<NodeConditional>(
				std::make_unique<NodeLeaf>(NodeLeaf::Type::Id)
				)
			));

		// function parameters
		_nodes.try_emplace("functionParameters", std::make_unique<NodeConcat>(false,
			std::make_unique<NodeLiteral>(true, "("),
			std::make_unique<NodeConditional>(
				std::make_unique<NodeConcat>(false,
					std::make_unique<NodeId>(false, "functionParameter"),
					std::make_unique<NodeRepeat>(
						std::make_unique<NodeConcat>(true,
							std::make_unique<NodeLiteral>(true, ","),
							std::make_unique<NodeId>(false, "functionParameter")
							)
						)
					)
				),
			std::make_unique<NodeLiteral>(true, ")")
			));

		// lambda output type
		_nodes.try_emplace("lambdaOutputType", std::make_unique<NodeConditional>(
				std::make_unique<NodeConcat>(false,
					std::make_unique<NodeLiteral>(true, "->"),
					std::make_unique<NodeId>(&createType, false, "type")
					)
			));

		// define lambda
		_nodes.try_emplace("defineLambda", std::make_unique<NodeConcat>(false,
			std::make_unique<NodeId>(false, "functionParameters"),
			std::make_unique<NodeId>(false, "lambdaOutputType"),
			std::make_unique<NodeLiteral>(true, "{"),
			std::make_unique<NodeId>(&createBlockTree, false, "several-commands"),
			std::make_unique<NodeLiteral>(true, "}")
			));

		// define function
		_nodes.try_emplace("defineFunction", std::make_unique<NodeConcat>(false,
			std::make_unique<NodeId>(&createType, false, "type"),
			std::make_unique<NodeLeaf>(NodeLeaf::Type::Id),
			std::make_unique<NodeId>(false, "functionParameters"),
			std::make_unique<NodeLiteral>(true, "{"),
			std::make_unique<NodeId>(&createBlockTree, false, "several-commands"),
			std::make_unique<NodeLiteral>(true, "}")
			));

		_nodes.try_emplace("command", std::make_unique<NodeOr>(
			// expression
			std::make_unique<NodeConcat>(false,
				// return
				std::make_unique<NodeConditional>(
					std::make_unique<NodeLiteral>(false, "return")
				),
				std::make_unique<NodeConditional>(
					std::make_unique<NodeId>(true, "expression")),
				std::make_unique<NodeLiteral>(true, ";")
				),
			// define variable by =
			std::make_unique<NodeId>(&createDefineVariableByAssignment, false, "defineVariableAssignment"),
			// define variable by {}
			std::make_unique<NodeConcat>(false,
				std::make_unique<NodeId>(false, "type"),
				std::make_unique<NodeLeaf>(NodeLeaf::Type::Id),
				std::make_unique<NodeConditional>(
					std::make_unique<NodeConcat>(false,
						std::make_unique<NodeLiteral>(true, "{"),
						std::make_unique<NodeConditional>(
							std::make_unique<NodeConcat>(true,
								std::make_unique<NodeId>(true, "expression"),
								std::make_unique<NodeRepeat>(
									std::make_unique<NodeConcat>(true,
										std::make_unique<NodeLiteral>(true, ","),
										std::make_unique<NodeId>(true, "expression")
										)
									)
								)
							),
						std::make_unique<NodeLiteral>(true, "}")
						)
					),
				std::make_unique<NodeLiteral>(true, ";")
				),
			// define function
			std::make_unique<NodeId>(&createDefineFunction, false, "defineFunction"),
			// block
			std::make_unique<NodeConcat>(false,
				std::make_unique<NodeLiteral>(true, "{"),
				std::make_unique<NodeId>(false, "several-commands"),
				std::make_unique<NodeLiteral>(true, "}")
				)
			));

		_root = _nodes.try_emplace("several-commands", std::make_unique<NodeRepeat>( &createBlockTree,
			std::make_unique<NodeId>(true, "command")
			)).first->second.get();

		for (auto& nodePair : _nodes) {
			nodePair.second->postConstruct(_nodes);
		}
	}

}