/*🍲Ketl🍲*/
#include "ir_nodes.h"

#include "bnf_nodes.h"

namespace Ketl {

	class IRBlock : public IRNode {
	public:

		IRBlock(std::vector<std::unique_ptr<IRNode>>&& commands)
			: _commands(std::move(commands)) {}

		uint64_t childCount() const override {
			return _commands.size();
		}

		const std::unique_ptr<IRNode>& child(uint64_t index) const override {
			return _commands[index];
		}

		AnalyzerVar* produceInstructions(std::vector<RawInstruction>& instructions, AnalyzerContext& context) const override {
			for (auto& command : _commands) {
				command->produceInstructions(instructions, context);
			}
			return {};
		};

	private:
		std::vector<std::unique_ptr<IRNode>> _commands;
	};

	std::unique_ptr<IRNode> createBlockTree(const ProcessNode* info) {
		std::vector<std::unique_ptr<IRNode>> commands;

		auto it = info->firstChild;
		while (it) {
			commands.emplace_back(it->node->createIRTree(it));
			it = it->nextSibling;
		}

		return std::make_unique<IRBlock>(std::move(commands));
	}

	std::unique_ptr<IRNode> proxyTree(const ProcessNode* info) {
		auto child = info->firstChild;
		return child->node->createIRTree(child);
	}

	std::unique_ptr<IRNode> emptyTree(const ProcessNode*) {
		return {};
	}



	class IRDefineVariable : public IRNode {
	public:

		IRDefineVariable(std::string_view id, std::unique_ptr<IRNode>&& type, std::unique_ptr<IRNode>&& expression)
			: _id(id), _type(std::move(type)), _expression(std::move(expression)) {}

		uint64_t childCount() const override {
			return _expression ? 1 : 0;
		}

		const std::unique_ptr<IRNode>& child(uint64_t index) const override {
			static const std::unique_ptr<IRNode> empty;
			return index == 1 ? _expression : empty;
		}

		AnalyzerVar* produceInstructions(std::vector<RawInstruction>& instructions, AnalyzerContext& context) const override {
			auto var = context.createGlobalVar(_id);
			auto expression = _expression->produceInstructions(instructions, context);

			auto& instruction = instructions.emplace_back();
			instruction.firstVar = var;
			instruction.secondVar = expression;
			instruction.code = Instruction::Code::Define;
			
			return {};
		};

	private:
		std::string_view _id;
		std::unique_ptr<IRNode> _type; // TODO ?
		std::unique_ptr<IRNode> _expression;
	};

	std::unique_ptr<IRNode> createDefineVariableByAssignmentTree(const ProcessNode* info) {
		auto concat = info->firstChild;

		auto typeNode = concat->firstChild;
		auto type = typeNode->node->createIRTree(typeNode);

		auto idNode = typeNode->nextSibling;
		auto id = idNode->node->value(idNode->iterator);

		auto expressionNode = idNode->nextSibling;
		auto expression = expressionNode->node->createIRTree(expressionNode);

		return std::make_unique<IRDefineVariable>(id, std::move(type), std::move(expression));
	}

	std::unique_ptr<IRNode> createDefineFunctionTree(const ProcessNode* info) {
		auto concat = info->firstChild;

		auto outputTypeNode = concat->firstChild;
		auto outputType = outputTypeNode->node->createIRTree(outputTypeNode);

		auto idNode = outputTypeNode->nextSibling;
		auto id = idNode->node->value(idNode->iterator);

		auto blockNode = idNode->nextSibling;
		auto block = blockNode->node->createIRTree(blockNode);

		return nullptr;
	}


	static AnalyzerVar* deduceOperatorCall(AnalyzerVar* lhs, AnalyzerVar* rhs, OperatorCode op, std::vector<RawInstruction>& instructions, AnalyzerContext& context) {
		std::string argumentsNotation = std::string("Int64,Int64");
		auto primaryOperatorPair = context.context().deducePrimaryOperator(op, argumentsNotation);

		if (primaryOperatorPair.first != Instruction::Code::None) {
			auto& instruction = instructions.emplace_back();
			instruction.code = primaryOperatorPair.first;
			instruction.firstVar = lhs;
			instruction.secondVar = rhs;
			instruction.outputVar = context.createTemporaryVar(sizeof(int64_t));

			return instruction.outputVar;
		}

		return nullptr;
	}


	class IRBinaryOperator : public IRNode {
	public:

		IRBinaryOperator(OperatorCode op, bool ltr, std::unique_ptr<IRNode>&& lhs, std::unique_ptr<IRNode>&& rhs)
			: _op(op), _ltr(ltr), _lhs(std::move(lhs)), _rhs(std::move(rhs)) {}

		const std::shared_ptr<TypeTemplate>& type() const override {
			return _type;
		}

		uint64_t childCount() const override {
			return 2;
		}

		const std::unique_ptr<IRNode>& child(uint64_t index) const override {
			return index == 0 ? _lhs : _rhs;
		}

		AnalyzerVar* produceInstructions(std::vector<RawInstruction>& instructions, AnalyzerContext& context) const override {
			auto lhsVar = _lhs->produceInstructions(instructions, context);
			auto rhsVar = _rhs->produceInstructions(instructions, context);

			return deduceOperatorCall(lhsVar, rhsVar, _op, instructions, context);
		};

	private:

		OperatorCode _op;
		bool _ltr;
		std::shared_ptr<TypeTemplate> _type;
		std::unique_ptr<IRNode> _lhs;
		std::unique_ptr<IRNode> _rhs;
	};

	std::unique_ptr<IRNode> createRtlTree(const ProcessNode* info) {
		auto rightArgNode = info->firstChild;

		while (rightArgNode->nextSibling) {
			rightArgNode = rightArgNode->nextSibling;
		}

		auto rightArg = rightArgNode->node->createIRTree(rightArgNode);

		do {
			auto opNode = rightArgNode->prevSibling;
			auto op = opNode->node->value(opNode->iterator);

			auto leftArgNode = opNode->prevSibling;
			auto leftArg = leftArgNode->node->createIRTree(leftArgNode);

			rightArg = std::make_unique<IRBinaryOperator>(parseOperatorCode(op), false, std::move(leftArg), std::move(rightArg));

			rightArgNode = leftArgNode;
		} while (rightArgNode->prevSibling);

		return rightArg;
	}

	std::unique_ptr<IRNode> createLtrTree(const ProcessNode* info) {
		auto leftArgNode = info->firstChild;

		auto leftArg = leftArgNode->node->createIRTree(leftArgNode);

		do {
			auto opNode = leftArgNode->nextSibling;
			auto op = opNode->node->value(opNode->iterator);

			auto rightArgNode = opNode->nextSibling;
			auto rightArg = rightArgNode->node->createIRTree(rightArgNode);

			leftArg = std::make_unique<IRBinaryOperator>(parseOperatorCode(op), true, std::move(leftArg), std::move(rightArg));

			leftArgNode = rightArgNode;
		} while (leftArgNode->nextSibling);

		return leftArg;
	}



	class IRVariable : public IRNode {
	public:

		IRVariable(const std::string_view& id)
			: _id(id) {}
		IRVariable(const std::string_view& id, std::shared_ptr<TypeTemplate> type)
			: _id(id), _type(std::move(type)) {}

		const std::shared_ptr<TypeTemplate>& type() const override {
			return _type;
		}

		const std::string& id() const override {
			return _id;
		}

		uint64_t childCount() const override {
			return 0;
		}

		AnalyzerVar* produceInstructions(std::vector<RawInstruction>& instructions, AnalyzerContext& context) const override {
			return context.getGlobalVar(_id);
		};

	private:
		std::string _id;
		std::shared_ptr<TypeTemplate> _type;
	};

	std::unique_ptr<IRNode> createVariable(const ProcessNode* info) {
		return std::make_unique<IRVariable>(info->node->value(info->iterator));
	}



	class IRLiteral : public IRNode {
	public:

		IRLiteral(const std::string_view& value)
			: _value(value) {}

		uint64_t childCount() const override {
			return 0;
		}

		AnalyzerVar* produceInstructions(std::vector<RawInstruction>& instructions, AnalyzerContext& context) const override {
			return context.createLiteralVar(_value);
		};

	private:
		std::string_view _value;
	};

	std::unique_ptr<IRNode> createLiteral(const ProcessNode* info) {
		return std::make_unique<IRLiteral>(info->node->value(info->iterator));
	}
}