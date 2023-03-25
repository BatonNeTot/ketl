/*🍲Ketl🍲*/
#include "semantic_analyzer.h"

#include "context.h"

#include <sstream>

namespace Ketl {

	RawInstruction& InstructionSequence::addInstruction() {
		if (_hasReturnStatement && !_raisedAfterReturnError) {
			_raisedAfterReturnError = true;
			_context.pushErrorMsg("[WARNING] Statements after return");
		}

		return _rawInstructions.emplace_back();
	}

	void InstructionSequence::addReturnStatement(UndeterminedDelegate expression) {
		if (_hasReturnStatement) {
			_context.pushErrorMsg("[ERROR] Multiple return statements");
			return;
		}

		_hasReturnStatement = true;
		_returnExpression = std::move(expression);
	}

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

	AnalyzerVar* SemanticAnalyzer::deduceUnaryOperatorCall(OperatorCode code, const UndeterminedDelegate& var, InstructionSequence& instructions) {
		// TODO
		return nullptr;
	}
	AnalyzerVar* SemanticAnalyzer::deduceBinaryOperatorCall(OperatorCode code, const UndeterminedDelegate& lhs, const UndeterminedDelegate& rhs, InstructionSequence& instructions) {
		// TODO actual deducing
		std::string argumentsNotation = std::string("Int64,Int64");
		auto primaryOperatorPair = context().deducePrimaryOperator(code, argumentsNotation);

		if (primaryOperatorPair.first == Instruction::Code::None) {

			return nullptr;
		}

		auto& instruction = instructions.addInstruction();
		instruction.code = primaryOperatorPair.first;
		instruction.firstVar = lhs.getUVar().getVarAsItIs();
		instruction.secondVar = rhs.getUVar().getVarAsItIs();

		auto& longType = *context().getVariable("Int64").as<TypeObject>();
		instruction.outputVar = createTempVar(longType);

		return instruction.outputVar;
	}
	UndeterminedDelegate SemanticAnalyzer::deduceFunctionCall(const UndeterminedDelegate& caller, const std::vector<UndeterminedDelegate>& arguments, InstructionSequence& instructions) {
		auto& variants = caller.getUVar().getVariants();
		auto& delegatedArguments = caller.getArguments(); 
		
		if (variants.empty()) {
			pushErrorMsg("[ERROR] No satisfying function");
			return &_undefinedVar;
		}

		std::vector<UndeterminedDelegate> totalArguments;
		totalArguments.reserve(delegatedArguments.size() + arguments.size());

		for (const auto& argument : delegatedArguments) {
			totalArguments.emplace_back(argument);
		}
		for (const auto& argument : arguments) {
			totalArguments.emplace_back(argument);
		}

		std::map<uint64_t, AnalyzerVar*> deducedVariants;

		for (const auto& callerVar : variants) {
			auto type = callerVar->getType();
			deducedVariants.emplace(type->deduceOperatorCall(callerVar, OperatorCode::Call, totalArguments));
		}

		auto bestVariantsIt = deducedVariants.begin();
		auto bestVariantCost = bestVariantsIt->first;

		if (bestVariantCost == std::numeric_limits<uint64_t>::max()) {
			pushErrorMsg("[ERROR] No satisfying function");
			return &_undefinedVar;
		}

		auto deducedEnd = deducedVariants.end();
		auto bestVariantsCount = 0u;

		for (;bestVariantsIt != deducedEnd && bestVariantsIt->first == bestVariantCost; ++bestVariantsIt) {
			if (bestVariantsIt->second != nullptr) {
				++bestVariantsCount;
			}
		}

		if (bestVariantsCount == 0) {
			// specifically a copy
			auto newDelegateCaller = caller.getUVar();
			return UndeterminedDelegate(std::move(newDelegateCaller), std::move(totalArguments));
		}

		if (bestVariantsCount > 1) {
			pushErrorMsg("[ERROR] Coundn't decide call");
			return &_undefinedVar;
		}

		auto functionVar = deducedVariants.begin()->second;
		auto functionType = functionVar->getType();

		auto& returnType = *functionType->getReturnType();
		auto outputVar = createTempVar(returnType);

		// allocating stack
		auto& defineInstruction = instructions.addInstruction();
		defineInstruction.code = Instruction::Code::AllocateFunctionStack;

		defineInstruction.firstVar = functionVar;

		// evaluating arguments
		auto& parameters = functionType->getParameters();
		for (auto i = 0u; i < totalArguments.size(); ++i) {
			auto& argumentVar = totalArguments[i];
			auto& parameter = parameters[i];

			auto parameterVar = argumentVar;
			auto isRef = parameter.isRef;
			if (!isRef) {
				// TODO create copy for the function call if needed, for now it's reference only
			}

			auto& defineInstruction = instructions.addInstruction();
			defineInstruction.code = Instruction::Code::DefineFuncParameter;

			defineInstruction.firstVar = createFunctionArgumentVar(i, *argumentVar.getUVar().getVarAsItIs()->getType());
			defineInstruction.secondVar = parameterVar.getUVar().getVarAsItIs();
		}

		// calling the function
		auto& instruction = instructions.addInstruction();
		instruction.code = Instruction::Code::CallFunction;
		instruction.firstVar = functionVar;
		instruction.outputVar = outputVar;

		return instruction.outputVar;
	}
	const TypeObject* SemanticAnalyzer::deduceCommonType(const std::vector<UndeterminedDelegate>& vars) {
		// TODO
		return nullptr;
	}

	class AnalyzerLiteralVar : public AnalyzerVar {
	public:
		// TODO
		AnalyzerLiteralVar(const std::string_view& value, SemanticAnalyzer& context)
			: _value(value) {
			_type = context.context().getVariable("Int64").as<TypeObject>();
		}

		std::pair<Argument::Type, Argument> getArgument() const override {
			auto type = Argument::Type::Literal;
			Argument argument;
			argument.uinteger = std::stoull(std::string(_value));
			return std::make_pair(type, argument);
		}

		const TypeObject* getType() const override {
			return _type;
		}

	private:

		const TypeObject* _type = nullptr;
		std::string_view _value;
		std::string_view _id;
	};

	AnalyzerVar* SemanticAnalyzer::createLiteralVar(const std::string_view& value) {
		auto& ptr = vars.emplace_back(std::make_unique<AnalyzerLiteralVar>(value, *this));

		return ptr.get();
	}
	
	class AnalyzerReturnVar : public AnalyzerVar {
	public:
		AnalyzerReturnVar(AnalyzerVar* value, SemanticAnalyzer& context)
			: _value(value) {
			_type = _value ? _value->getType() : context.context().getVariable("Void").as<TypeObject>();
		}

		std::pair<Argument::Type, Argument> getArgument() const override {
			auto type = Argument::Type::Return;
			Argument argument;
			return std::make_pair(type, argument);
		}

		const TypeObject* getType() const override {
			return _type;
		}

	private:

		AnalyzerVar* _value;
		const TypeObject* _type;
	};

	AnalyzerVar* SemanticAnalyzer::createReturnVar(AnalyzerVar* expression) {
		auto& ptr = vars.emplace_back(std::make_unique<AnalyzerReturnVar>(expression, *this));

		return ptr.get();
	}

	class AnalyzerLiteralClassVar : public AnalyzerVar {
	public:
		AnalyzerLiteralClassVar(void* ptr, const TypeObject& type)
			: _ptr(ptr), _type(&type) {}

		std::pair<Argument::Type, Argument> getArgument() const override {
			auto type = Argument::Type::Literal;
			Argument argument;
			argument.pointer = _ptr;
			return std::make_pair(type, argument);
		}

		const TypeObject* getType() const override {
			return _type;
		}

	private:

		void* _ptr;
		const TypeObject* _type;
	};

	AnalyzerVar* SemanticAnalyzer::createLiteralClassVar(void* ptr, const TypeObject& type) {
		resultRefs.emplace_back(ptr);
		auto& var = vars.emplace_back(std::make_unique<AnalyzerLiteralClassVar>(ptr, type));

		return var.get();
	}


	class AnalyzerFunctionParameterVar : public AnalyzerVar {
	public:
		AnalyzerFunctionParameterVar(uint64_t index, const TypeObject& type)
			: _index(index), _type(&type) {}

		std::pair<Argument::Type, Argument> getArgument() const override {
			auto type = Argument::Type::Literal;
			Argument argument;
			argument.stack = _index * sizeof(void*);
			return std::make_pair(type, argument);
		}

		const TypeObject* getType() const override {
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
		AnalyzerGlobalVar(const TypeObject& type)
			: _type(&type) {}
		AnalyzerGlobalVar(void* ptr, const TypeObject& type)
			: _ptr(ptr), _type(&type) {}

		void bake(void* ptr) override {
			_ptr = ptr;
		}

		std::pair<Argument::Type, Argument> getArgument() const override {
			auto type = Argument::Type::Global;
			Argument argument;
			argument.globalPtr = _ptr;
			return std::make_pair(type, argument);
		}

		const TypeObject* getType() const override {
			return _type;
		}

	private:

		const TypeObject* _type;
		void* _ptr = nullptr;
	};

	UndeterminedVar SemanticAnalyzer::getVar(const std::string_view& id) {
		auto scopeIt = scopeVarsByNames.find(id);
		if (scopeIt != scopeVarsByNames.end()) {
			auto it = scopeIt->second.rbegin();
			return it->second;
		}

		auto globalIt = newGlobalVars.find(id);
		if (globalIt != newGlobalVars.end()) {
			return globalIt->second;
		}

		auto globalVar = _context.getVariable(id);
		if (!globalVar.empty()) {
			UndeterminedVar uvar;
			for (auto& var : globalVar._vars) {
				auto& ptr = vars.emplace_back(std::make_unique<AnalyzerGlobalVar>(var.rawData(), var.type()));
				uvar.overload(ptr.get());
			}
			return uvar;
		}

		auto idStr = std::string(id);
		pushErrorMsg("[ERROR] Variable '" + idStr + "' doesn't exists");
		return &_undefinedVar;
	}

	AnalyzerVar* SemanticAnalyzer::createVar(const std::string_view& id, const TypeObject& type) {
		if (isLocalScope()) {

			auto& scopedNames = scopeVarsByNames[id];
			auto& uvar = scopedNames[scopeLayer];

			if (!uvar.canBeOverloadedWith(type)) {
				pushErrorMsg("[ERROR] Variable '" + std::string(id) + "' already exists in local scope");
				return &_undefinedVar;
			}

			auto tempVar = createTempVar(type);
			uvar.overload(tempVar);

			return tempVar;
		}
		else {
			if (!_context.getVariable(id).empty()) {
				pushErrorMsg("[ERROR] Variable '" + std::string(id) + "' already exists in global scope");
				return &_undefinedVar;
			}

			auto& uvar = newGlobalVars[id];
			if (!uvar.canBeOverloadedWith(type)) {
				pushErrorMsg("[ERROR] Variable '" + std::string(id) + "' already exists in global scope");
				return &_undefinedVar;
			}

			auto& ptr = vars.emplace_back(std::make_unique<AnalyzerGlobalVar>(type));
			uvar.overload(ptr.get());

			return ptr.get();
		}
	}

	class AnalyzerParameterVar : public AnalyzerVar {
	public:
		AnalyzerParameterVar(uint64_t offset, const TypeObject& type)
			: _offset(offset), _type(&type) {}

		std::pair<Argument::Type, Argument> getArgument() const override {
			auto type = Argument::Type::FunctionParameter;
			Argument argument;
			argument.stack = _offset;
			return std::make_pair(type, argument);
		}

		const TypeObject* getType() const override {
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
			return &_undefinedVar;
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

		std::pair<Argument::Type, Argument> getArgument() const override {
			auto type = Argument::Type::Stack;
			Argument argument;
			argument.stack = _offset;
			return std::make_pair(type, argument);
		}

		const TypeObject* getType() const override {
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

		// TODO
		// So, we need to compile and run it, BUT we need to use context which we didn't bake yet
		// what about we separate BAKE and COMPILE, so that we can compile without baking.
		// which is kinda already the thing, because baking only adds names into global name space

		return context().getVariable("Int64");
	}

	const TypeObject* SemanticAnalyzer::evaluateType(const IRNode& node) {
		
		return context().getVariable("Int64").as<TypeObject>();
	}

	void SemanticAnalyzer::bakeContext() {
		for (auto& var : newGlobalVars) {
			auto& variants = var.second.getVariants();
			for (auto& variant : variants) {
				auto& type = *variant->getType();
				void* ptr = _context.allocateGlobal(var.first, type);
				if (ptr == nullptr) {
					std::string id(var.first);
					pushErrorMsg("[ERROR] Variable " + id + " already exists");
				}
				variant->bake(ptr);
			}
		}
	}

	std::variant<FunctionImpl*, std::string> SemanticAnalyzer::compile(const IRNode& block)&& {
		InstructionSequence mainSequence(*this);

		block.produceInstructions(mainSequence, *this);
		if (hasCompilationErrors()) {
			return compilationErrors();
		}

		auto rawInstructions = std::move(mainSequence).buildInstructions();
		auto [functionPtr, functionRefs] = context().createObject<FunctionImpl>(context()._alloc, maxOffsetValue, rawInstructions.size());

		bakeContext();
		if (hasCompilationErrors()) {
			return compilationErrors();
		}

		for (const auto& ref : resultRefs) {
			functionRefs->registerAbsLink(ref);
		}

		for (auto i = 0u; i < functionPtr->_instructionsCount; ++i) {
			rawInstructions[i].propagadeInstruction(functionPtr->_instructions[i]);
		}

		return functionPtr;
	}
}