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
		// TODO
		AnalyzerLiteralVar(const std::string_view& value)
			: _value(value) {}

		std::pair<Argument::Type, Argument> getArgument(SemanticAnalyzer& context) const override {
			auto type = Argument::Type::Literal;
			Argument argument;
			argument.uinteger = std::stoull(std::string(_value));
			return std::make_pair(type, argument);
		}

		const TypeObject* getType(SemanticAnalyzer& context) const override {
			return context.context().getVariable("Int64").as<TypeObject>();
		}

	private:

		std::string_view _value;
		std::string_view _id;
	};

	AnalyzerVar* SemanticAnalyzer::createLiteralVar(const std::string_view& value) {
		auto& ptr = vars.emplace_back(std::make_unique<AnalyzerLiteralVar>(value));

		return ptr.get();
	}

	class AnalyzerFunctionVar : public AnalyzerVar {
	public:
		AnalyzerFunctionVar(FunctionImpl& function, const TypeObject& type)
			: _function(&function), _type(&type) {}

		std::pair<Argument::Type, Argument> getArgument(SemanticAnalyzer& context) const override {
			auto type = Argument::Type::Literal;
			Argument argument;
			argument.pointer = _function;
			return std::make_pair(type, argument);
		}

		const TypeObject* getType(SemanticAnalyzer& context) const override {
			return _type;
		}

	private:

		FunctionImpl* _function;
		const TypeObject* _type;
	};

	AnalyzerVar* SemanticAnalyzer::createFunctionVar(FunctionImpl* functionPtr, const TypeObject& type) {
		resultRefs.emplace_back(functionPtr);
		auto& ptr = vars.emplace_back(std::make_unique<AnalyzerFunctionVar>(*functionPtr, type));

		return ptr.get();
	}


	class AnalyzerFunctionParameterVar : public AnalyzerVar {
	public:
		AnalyzerFunctionParameterVar(uint64_t index, const TypeObject& type)
			: _index(index), _type(&type) {}

		std::pair<Argument::Type, Argument> getArgument(SemanticAnalyzer& context) const override {
			auto type = Argument::Type::Literal;
			Argument argument;
			argument.stack = _index * sizeof(void*);
			return std::make_pair(type, argument);
		}

		const TypeObject* getType(SemanticAnalyzer& context) const override {
			return _type;
		}

	private:

		const TypeObject* _type;
		uint64_t _index;
	};

	AnalyzerVar* SemanticAnalyzer::createFunctionArgumentVar(uint64_t index, const TypeObject& type) {
		auto& ptr = vars.emplace_back(std::make_unique<AnalyzerFunctionParameterVar>(index, type));

		return ptr.get();
	}

	class AnalyzerGlobalVar : public AnalyzerVar {
	public:
		AnalyzerGlobalVar(const std::string_view& id, const TypeObject& type)
			: _id(id), _type(&type) {}

		std::pair<Argument::Type, Argument> getArgument(SemanticAnalyzer& context) const override {
			auto type = Argument::Type::Global;
			Argument argument;
			argument.globalPtr = context.context().getVariable(std::string(_id)).as<void>();
			return std::make_pair(type, argument);
		}

		const TypeObject* getType(SemanticAnalyzer& context) const override {
			return _type;
		}

	private:

		const TypeObject* _type;
		std::string_view _id;
	};

	AnalyzerVar* SemanticAnalyzer::getVar(const std::string_view& id) {
		auto globalIt = newGlobalVars.find(id);
		if (globalIt != newGlobalVars.end()) {
			return globalIt->second;
		}

		auto scopeIt = scopeVarsByNames.find(id);
		if (scopeIt != scopeVarsByNames.end()) {
			auto it = scopeIt->second.rbegin();
			return it->second;
		}

		auto globalVar = _context.getVariable(id);
		if (globalVar.as<void>()) {
			auto& ptr = vars.emplace_back(std::make_unique<AnalyzerGlobalVar>(id, globalVar.type()));
			return ptr.get();
		}

		auto idStr = std::string(id);
		pushErrorMsg("[ERROR] Variable " + idStr + " doesn't exists");
		// TODO return something not null
		return nullptr;
	}

	AnalyzerVar* SemanticAnalyzer::createVar(const std::string_view& id, const TypeObject& type) {
		if (isLocalScope()) {

			auto& scopedNames = scopeVarsByNames[id];
			auto [varIt, success] = scopedNames.try_emplace(scopeLayer, nullptr);
			if (!success) {
				pushErrorMsg("[ERROR] Variable " + std::string(id) + " already exists in local scope");
				return varIt->second;
			}

			auto tempVar = createTempVar(type);
			varIt->second = tempVar;

			return varIt->second;
		}
		else {
			auto& ptr = vars.emplace_back(std::make_unique<AnalyzerGlobalVar>(id, type));

			if (_context.getVariable(id).as<void>()) {
				pushErrorMsg("[ERROR] Variable " + std::string(id) + " already exists in global scope");
				return ptr.get();
			}

			newGlobalVars.try_emplace(id, ptr.get());

			return ptr.get();
		}
	}

	class AnalyzerParameterVar : public AnalyzerVar {
	public:
		AnalyzerParameterVar(uint64_t offset, const TypeObject& type)
			: _offset(offset), _type(&type) {}

		std::pair<Argument::Type, Argument> getArgument(SemanticAnalyzer& context) const override {
			auto type = Argument::Type::FunctionParameter;
			Argument argument;
			argument.stack = _offset;
			return std::make_pair(type, argument);
		}

		const TypeObject* getType(SemanticAnalyzer& context) const override {
			return _type;
		}

	private:

		const TypeObject* _type;
		uint64_t _offset;
	};

	AnalyzerVar* SemanticAnalyzer::createFunctionParameterVar(const std::string_view& id, const TypeObject& type) {
		auto& scopedNames = scopeVarsByNames[id];
		auto [varIt, success] = scopedNames.try_emplace(scopeLayer, nullptr);
		if (!success) {
			pushErrorMsg("[ERROR] Parameter " + std::string(id) + " already exists");
			return varIt->second;
		}

		auto offset = currentStackOffset;
		uint64_t size = sizeof(void*);

		currentStackOffset += size;
		maxOffsetValue = std::max(currentStackOffset, maxOffsetValue);

		auto& ptr = vars.emplace_back(std::make_unique<AnalyzerParameterVar>(offset, type));
		varIt->second = ptr.get();

		return ptr.get();
	}

	class AnalyzerTemporaryVar : public AnalyzerVar {
	public:
		AnalyzerTemporaryVar(uint64_t offset, const TypeObject& type)
			: _offset(offset), _type(&type) {}

		std::pair<Argument::Type, Argument> getArgument(SemanticAnalyzer& context) const override {
			auto type = Argument::Type::Stack;
			Argument argument;
			argument.stack = _offset;
			return std::make_pair(type, argument);
		}

		const TypeObject* getType(SemanticAnalyzer& context) const override {
			return _type;
		}

	private:

		const TypeObject* _type;
		uint64_t _offset;
	};

	AnalyzerVar* SemanticAnalyzer::createTempVar(const TypeObject& type) {
		// TODO alignment
		auto offset = currentStackOffset;

		uint64_t size = type.sizeOf();

		currentStackOffset += size;
		maxOffsetValue = std::max(currentStackOffset, maxOffsetValue);

		auto& ptr = vars.emplace_back(std::make_unique<AnalyzerTemporaryVar>(offset, type));

		auto& scopeVar = scopeVars.emplace();

		scopeVar.scopeLayer = scopeLayer;
		scopeVar.type = &type;
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

	Variable SemanticAnalyzer::evaluate(const IRNode& node) {
		// TODO evalutaion right now is quite raw
		return context().getVariable("Void")._var;
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

	std::variant<FunctionImpl*, std::string> SemanticAnalyzer::compile(const IRNode& block)&& {
		std::vector<RawInstruction> rawInstructions;

		block.produceInstructions(rawInstructions, *this);
		if (hasCompilationErrors()) {
			return compilationErrors();
		}

		auto [functionPtr, functionRefs] = context().createObject<FunctionImpl>(context()._alloc, maxOffsetValue, rawInstructions.size());

		bakeContext();
		if (hasCompilationErrors()) {
			return compilationErrors();
		}

		for (const auto& ref : resultRefs) {
			functionRefs->registerAbsLink(ref);
		}

		for (auto i = 0u; i < functionPtr->_instructionsCount; ++i) {
			rawInstructions[i].propagadeInstruction(functionPtr->_instructions[i], *this);
		}

		return functionPtr;
	}
}