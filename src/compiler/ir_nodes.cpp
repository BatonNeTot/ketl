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


	class IRBinaryOperator : public IRNode {
	public:

		IRBinaryOperator(OperatorCode op, bool ltr, std::unique_ptr<IRNode>&& lhs, std::unique_ptr<IRNode>&& rhs)
			: _op(op), _ltr(ltr), _lhs(std::move(lhs)), _rhs(std::move(rhs)) {}

		bool resolveType(Context& context) override {
			auto lhsSuccess = _lhs->resolveType(context);
			auto rhsSuccess = _rhs->resolveType(context);

			if (!lhsSuccess && rhsSuccess && _op == OperatorCode::Assign) {
				auto varId = _lhs->id();
				if (!varId.empty()) {
					auto test = 0;
					return true;
				}
			}

			if (!lhsSuccess || !rhsSuccess) {
				return false;
			}
			auto test = 0;


			return true;
		};

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
			AnalyzerVar* lhsVar;
			AnalyzerVar* rhsVar;

			if (_ltr) {
				lhsVar = _lhs->produceInstructions(instructions, context);
				rhsVar = _rhs->produceInstructions(instructions, context);
			}
			else {
				rhsVar = _rhs->produceInstructions(instructions, context);
				lhsVar = _lhs->produceInstructions(instructions, context);
			}

			auto& instruction = instructions.emplace_back();
			instruction.firstVar = lhsVar;
			instruction.secondVar = rhsVar;
			instruction.outputVar = context.createTemporaryVar(sizeof(int64_t));

			switch (_op) {
			case OperatorCode::Plus: {
				instruction.code = Instruction::Code::AddInt64;
				break;
			}
			case OperatorCode::Multiply: {
				instruction.code = Instruction::Code::MultyInt64;
				break;
			}
			case OperatorCode::Assign: {
				instruction.code = Instruction::Code::Assign;
				break;
			}
			default: {
				auto test = 0;
				(void)test;
				break;
			}
			}
			return instruction.outputVar;
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
			(void)op; // TODO deduce operator code

			auto code = OperatorCode::Assign;

			auto leftArgNode = opNode->prevSibling;
			auto leftArg = leftArgNode->node->createIRTree(leftArgNode);

			rightArg = std::make_unique<IRBinaryOperator>(code, false, std::move(leftArg), std::move(rightArg));

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
			(void)op; // TODO deduce operator code

			auto code = OperatorCode::Plus;
			if (op == "*") {
				code = OperatorCode::Multiply;
			}

			auto rightArgNode = opNode->nextSibling;
			auto rightArg = rightArgNode->node->createIRTree(rightArgNode);

			leftArg = std::make_unique<IRBinaryOperator>(code, true, std::move(leftArg), std::move(rightArg));

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