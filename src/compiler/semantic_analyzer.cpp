/*🍲Ketl🍲*/
#include "semantic_analyzer.h"

#include "context.h"

namespace Ketl {

	class AnalyzerLiteralVar : public AnalyzerVar {
	public:
		AnalyzerLiteralVar(const std::string_view& value)
			: _value(value) {}

		std::pair<Argument::Type, Argument> getArgument(AnalyzerContext& context) const override {
			auto type = Argument::Type::Literal;
			Argument argument;
			argument.uinteger = std::stoull(std::string(_value));
			return std::make_pair(type, argument);
		}

		std::string_view _value;
	};

	AnalyzerVar* AnalyzerContext::createLiteralVar(const std::string_view& value) {
		auto& ptr = vars.emplace_back(std::make_unique<AnalyzerLiteralVar>(value));

		return ptr.get();
	}

	class AnalyzerGlobalVar : public AnalyzerVar {
	public:
		AnalyzerGlobalVar(const std::string_view& id)
			: _id(id) {}

		std::pair<Argument::Type, Argument> getArgument(AnalyzerContext& context) const override {
			auto type = Argument::Type::Global;
			Argument argument;
			argument.globalPtr = context.context().getVariable(std::string(_id)).as<void>();
			return std::make_pair(type, argument);
		}

		std::string_view _id;
	};

	AnalyzerVar* AnalyzerContext::getGlobalVar(const std::string_view& value) {
		return nullptr;
	}

	AnalyzerVar* AnalyzerContext::createGlobalVar(const std::string_view& value) {
		auto& ptr = vars.emplace_back(std::make_unique<AnalyzerGlobalVar>(value));

		globalVars.try_emplace(value, ptr.get());

		return ptr.get();
	}

	class AnalyzerTemporaryVar : public AnalyzerVar {
	public:
		AnalyzerTemporaryVar(uint64_t offset) 
			: _offset(offset) {}

		std::pair<Argument::Type, Argument> getArgument(AnalyzerContext& context) const override {
			auto type = Argument::Type::Stack;
			Argument argument;
			argument.stack = _offset;
			return std::make_pair(type, argument);
		}

		uint64_t _offset;
	};

	AnalyzerVar* AnalyzerContext::createTemporaryVar(uint64_t size) {
		// TODO alignment
		auto offset = currentStackOffset;
		currentStackOffset += size;
		maxOffsetValue = std::max(currentStackOffset, maxOffsetValue);

		auto& ptr = vars.emplace_back(std::make_unique<AnalyzerTemporaryVar>(offset));

		return ptr.get();
	}

	void AnalyzerContext::bakeContext() {
		for (auto& var : globalVars) {
			auto id = std::string(var.first);
			if (_context.getVariable(id).as<void>()) {
				continue;
			}

			auto& longType = *_context.getVariable("Int64").as<TypeObject>();
			auto ptr = _context.allocateGlobal(longType);
			_context.declareGlobal(id, ptr, longType);
		}
	}

	FunctionImpl SemanticAnalyzer::compile(std::unique_ptr<IRNode>&& block, Context& context) {
		AnalyzerContext acontext(context);
		std::vector<RawInstruction> rawInstructions;

		block->produceInstructions(rawInstructions, acontext);

		FunctionImpl function(context._alloc, acontext.maxOffsetValue, rawInstructions.size());

		acontext.bakeContext();

		for (auto i = 0u; i < function._instructionsCount; ++i) {
			rawInstructions[i].propagadeInstruction(function._instructions[i], acontext);
		}

		return function;
	}
}