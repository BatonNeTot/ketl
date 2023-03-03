/*🍲Ketl🍲*/
#include "semantic_analyzer.h"

#include "context.h"

#include <sstream>

namespace Ketl {

	void SemanticAnalyzer::pushScope() {
		scopeOffsets.push(currentStackOffset);
		++scopeLayer;
	}
	void SemanticAnalyzer::popScope() {
		if (scopeLayer == 0) {
			// TODO error
			return;
		}

		currentStackOffset = scopeOffsets.top();
		scopeOffsets.pop();

		propagateScopeDestructors();

		--scopeLayer;
	}

	class AnalyzerLiteralVar : public AnalyzerVar {
	public:
		AnalyzerLiteralVar(const std::string_view& value)
			: _value(value) {}

		std::pair<Argument::Type, Argument> getArgument(SemanticAnalyzer& context) const override {
			auto type = Argument::Type::Literal;
			Argument argument;
			argument.uinteger = std::stoull(std::string(_value));
			return std::make_pair(type, argument);
		}

		std::string_view _value;
	};

	AnalyzerVar* SemanticAnalyzer::createLiteralVar(const std::string_view& value) {
		auto& ptr = vars.emplace_back(std::make_unique<AnalyzerLiteralVar>(value));

		return ptr.get();
	}

	class AnalyzerGlobalVar : public AnalyzerVar {
	public:
		AnalyzerGlobalVar(const std::string_view& id)
			: _id(id) {}

		std::pair<Argument::Type, Argument> getArgument(SemanticAnalyzer& context) const override {
			auto type = Argument::Type::Global;
			Argument argument;
			argument.globalPtr = context.context().getVariable(std::string(_id)).as<void>();
			return std::make_pair(type, argument);
		}

		std::string_view _id;
	};

	AnalyzerVar* SemanticAnalyzer::getVar(const std::string_view& value) {
		auto scopeIt = scopeVarsByNames.find(value);
		if (scopeIt != scopeVarsByNames.end()) {
			// TODO
		}

		auto& ptr = vars.emplace_back(std::make_unique<AnalyzerGlobalVar>(value));

		auto idStr = std::string(value);
		if (!_context.getVariable(idStr).as<void>()) {
			pushErrorMsg("[ERROR] Variable " + idStr + " doesn't exists");
			return ptr.get();
		}

		return ptr.get();
	}

	AnalyzerVar* SemanticAnalyzer::createVar(const std::string_view& value, std::unique_ptr<IRNode>&& type) {
		if (isLocalScope()) {
			auto id = std::string(value);

			auto& scopedNames = scopeVarsByNames[value];
			auto [varIt, success] = scopedNames.try_emplace(scopeLayer, nullptr);
			if (!success) {
				pushErrorMsg("[ERROR] Variable " + id + " already exists in local scope");
				return varIt->second;
			}

			auto tempVar = createTempVar(std::move(type));
			varIt->second = tempVar;

			return varIt->second;
		}
		else {
			auto& ptr = vars.emplace_back(std::make_unique<AnalyzerGlobalVar>(value));

			auto id = std::string(value);
			if (_context.getVariable(id).as<void>()) {
				pushErrorMsg("[ERROR] Variable " + id + " already exists in global scope");
				return ptr.get();
			}

			newGlobalVars.try_emplace(value, ptr.get());

			return ptr.get();
		}
	}

	class AnalyzerTemporaryVar : public AnalyzerVar {
	public:
		AnalyzerTemporaryVar(uint64_t offset) 
			: _offset(offset) {}

		std::pair<Argument::Type, Argument> getArgument(SemanticAnalyzer& context) const override {
			auto type = Argument::Type::Stack;
			Argument argument;
			argument.stack = _offset;
			return std::make_pair(type, argument);
		}

		uint64_t _offset;
	};

	AnalyzerVar* SemanticAnalyzer::createTempVar(std::unique_ptr<IRNode>&& type) {
		// TODO alignment
		auto offset = currentStackOffset;

		// TODO get size from type
		uint64_t size = 8;

		currentStackOffset += size;
		maxOffsetValue = std::max(currentStackOffset, maxOffsetValue);

		auto& ptr = vars.emplace_back(std::make_unique<AnalyzerTemporaryVar>(offset));

		auto& scopeVar = scopeVars.emplace();

		scopeVar.scopeLayer = scopeLayer;
		scopeVar.type = std::move(type);
		scopeVar.var = ptr.get();

		return ptr.get();
	}

	std::string SemanticAnalyzer::compilationErrors() const {
		std::stringstream ss;
		for (const auto& msg : _compilationErrors) {
			ss << msg << std::endl;
		}
		return ss.str();
	}

	void SemanticAnalyzer::bakeContext() {
		for (auto& var : newGlobalVars) {
			auto id = std::string(var.first);
			if (_context.getVariable(id).as<void>()) {
				pushErrorMsg("[ERROR] Variable " + id + " already exists");
				continue;
			}

			auto& longType = *_context.getVariable("Int64").as<TypeObject>();
			auto ptr = _context.allocateGlobal(longType);
			_context.declareGlobal(id, ptr, longType);
		}
	}

	std::variant<FunctionImpl, std::string> SemanticAnalyzer::compile(std::unique_ptr<IRNode>&& block)&& {
		std::vector<RawInstruction> rawInstructions;

		std::move(*block).produceInstructions(rawInstructions, *this);

		FunctionImpl function(context()._alloc, maxOffsetValue, rawInstructions.size());

		bakeContext();
		if (hasCompilationErrors()) {
			return compilationErrors();
		}

		for (auto i = 0u; i < function._instructionsCount; ++i) {
			rawInstructions[i].propagadeInstruction(function._instructions[i], *this);
		}

		return function;
	}
}