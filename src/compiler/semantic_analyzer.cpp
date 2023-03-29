/*🍲Ketl🍲*/
#include "semantic_analyzer.h"

#include "context.h"

#include <sstream>

namespace Ketl {

	bool UndeterminedVar::canBeOverloadedWith(const TypeObject& type) const {
		if (_potentialVars.empty()) {
			return true;
		}
		return _potentialVars[0].argument->getType()->doesSupportOverload() && type.doesSupportOverload();
	};

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

	SemanticAnalyzer::SemanticAnalyzer(Context& context, SemanticAnalyzer* parentContext)
		: _context(context), _localScope(parentContext != nullptr)
		, _undefinedArgument(context), _undefinedVar(&_undefinedArgument, true, false, false) {
		_rootLocal = &_localVars.emplace_back();
		_currentLocal = _rootLocal;

		_localsByName.emplace_back();
	}

	void SemanticAnalyzer::pushScope() {
		// TODO
	}
	void SemanticAnalyzer::popScope() {
		// TODO
	}

	RawArgument* SemanticAnalyzer::deduceUnaryOperatorCall(OperatorCode code, const UndeterminedDelegate& var, InstructionSequence& instructions) {
		// TODO
		return nullptr;
	}
	CompilerVar SemanticAnalyzer::deduceBinaryOperatorCall(OperatorCode code, const UndeterminedDelegate& lhs, const UndeterminedDelegate& rhs, InstructionSequence& instructions) {
		// TODO actual deducing
		std::string argumentsNotation = std::string("Int64,Int64");
		auto primaryOperatorPair = context().deducePrimaryOperator(code, argumentsNotation);

		if (primaryOperatorPair.first == Instruction::Code::None) {

			return CompilerVar();
		}

		auto& instruction = instructions.addInstruction();
		instruction.code = primaryOperatorPair.first;
		instruction.firstVar = lhs.getUVar().getVarAsItIs().argument;
		instruction.secondVar = rhs.getUVar().getVarAsItIs().argument;

		auto& longType = *context().getVariable("Int64").as<TypeObject>();
		instruction.outputVar = createTempVar(longType);

		return CompilerVar(instruction.outputVar, false, false, false);
	}
	UndeterminedDelegate SemanticAnalyzer::deduceFunctionCall(const UndeterminedDelegate& caller, const std::vector<UndeterminedDelegate>& arguments, InstructionSequence& instructions) {
		auto& variants = caller.getUVar().getVariants();
		auto& delegatedArguments = caller.getArguments(); 
		
		if (variants.empty()) {
			pushErrorMsg("[ERROR] No satisfying function");
			return _undefinedVar;
		}

		std::vector<UndeterminedDelegate> totalArguments;
		totalArguments.reserve(delegatedArguments.size() + arguments.size());

		for (const auto& argument : delegatedArguments) {
			totalArguments.emplace_back(argument);
		}
		for (const auto& argument : arguments) {
			totalArguments.emplace_back(argument);
		}

		std::map<uint64_t, RawArgument*> deducedVariants;

		for (const auto& callerVar : variants) {
			auto type = callerVar.argument->getType();
			if (type->getReturnType()) {
				// it is a function
				auto& parameters = type->getParameters();

				if (totalArguments.size() > parameters.size()) {
					deducedVariants.emplace(std::make_pair<uint64_t, RawArgument*>(std::numeric_limits<uint64_t>::max(), nullptr));
					continue;
				}

				// TODO do cast checking and counting
				if (totalArguments.size() < parameters.size()) {
					deducedVariants.emplace(std::make_pair<uint64_t, RawArgument*>(0u, nullptr));
					continue;
				}

				deducedVariants.emplace(std::make_pair(0u, callerVar.argument));
				continue;
			}

			auto& callOperators = type->getCallFunctions();
			for (auto& callOperator : callOperators) {
				// TODO
			}
		}

		auto bestVariantsIt = deducedVariants.begin();
		auto bestVariantCost = bestVariantsIt->first;

		if (bestVariantCost == std::numeric_limits<uint64_t>::max()) {
			pushErrorMsg("[ERROR] No satisfying function");
			return _undefinedVar;
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
			return _undefinedVar;
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

			defineInstruction.firstVar = createFunctionArgumentVar(i, *argumentVar.getUVar().getVarAsItIs().argument->getType());
			defineInstruction.secondVar = parameterVar.getUVar().getVarAsItIs().argument;
		}

		// calling the function
		auto& instruction = instructions.addInstruction();
		instruction.code = Instruction::Code::CallFunction;
		instruction.firstVar = functionVar;
		instruction.outputVar = outputVar;

		return CompilerVar(instruction.outputVar, false, false, false);
	}
	const TypeObject* SemanticAnalyzer::deduceCommonType(const std::vector<UndeterminedDelegate>& vars) {
		// TODO
		return nullptr;
	}

	class LiteralArgument : public RawArgument {
	public:
		// TODO
		LiteralArgument(const std::string_view& value, SemanticAnalyzer& context)
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

	CompilerVar SemanticAnalyzer::createLiteralVar(const std::string_view& value) {
		auto ptr = vars.emplace_back(std::make_unique<LiteralArgument>(value, *this)).get();

		return CompilerVar(ptr, true, true, false);
	}
	
	class ReturnArgument : public RawArgument {
	public:
		ReturnArgument(RawArgument* value, SemanticAnalyzer& context)
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

		RawArgument* _value;
		const TypeObject* _type;
	};

	RawArgument* SemanticAnalyzer::createReturnVar(RawArgument* expression) {
		auto& ptr = vars.emplace_back(std::make_unique<ReturnArgument>(expression, *this));

		return ptr.get();
	}

	class LiteralClassArgument : public RawArgument {
	public:
		LiteralClassArgument(void* ptr, const TypeObject& type)
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

	CompilerVar SemanticAnalyzer::createLiteralClassVar(void* ptr, const TypeObject& type) {
		resultRefs.emplace_back(ptr);
		auto var = vars.emplace_back(std::make_unique<LiteralClassArgument>(ptr, type)).get();

		return CompilerVar(var, true, true, false);
	}

	class StackArgument : public RawArgument {
	public:
		StackArgument(const TypeObject& type)
			: _type(&type) {}

		void bake(void* ptr) override {
			_ptr = ptr;
		}

		inline void setOffset(uint64_t offset) {
			_offset = offset;
		}

		std::pair<Argument::Type, Argument> getArgument() const override {
			if (_ptr != nullptr) {
				auto type = Argument::Type::Global;
				Argument argument;
				argument.globalPtr = _ptr;
				return std::make_pair(type, argument);
			}
			else {
				auto type = Argument::Type::Stack;
				Argument argument;
				argument.stack = _offset;
				return std::make_pair(type, argument);
			}
		}

		const TypeObject* getType() const override {
			return _type;
		}

	private:

		const TypeObject* _type = nullptr;
		uint64_t _offset = 0u;
		void* _ptr = nullptr;
	};

	RawArgument* SemanticAnalyzer::createTempVar(const TypeObject& type) {
		auto stackVar = std::make_unique<StackArgument>(type);
		auto stackPtr = stackVar.get();
		auto ptr = vars.emplace_back(std::move(stackVar)).get();

		auto localVar = &_localVars.emplace_back();
		localVar->argument = stackPtr;

		localVar->parent = _currentLocal->parent;
		_currentLocal->nextSibling = localVar;
		_currentLocal = localVar;

		return ptr;
	}

	class ParameterArgument : public RawArgument {
	public:
		ParameterArgument(uint64_t index, const TypeObject& type)
			: _type(&type), _index(index) {}

		std::pair<Argument::Type, Argument> getArgument() const override {
			auto type = Argument::Type::FunctionParameter;
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

	RawArgument* SemanticAnalyzer::createFunctionArgumentVar(uint64_t index, const TypeObject& type) {
		auto& ptr = vars.emplace_back(std::make_unique<ParameterArgument>(index, type));

		return ptr.get();
	}

	class GlobalArgument : public RawArgument {
	public:
		GlobalArgument(const TypeObject& type)
			: _type(&type) {}
		GlobalArgument(void* ptr, const TypeObject& type)
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
		for (auto nameScopeIt = _localsByName.rbegin(), end = _localsByName.rend(); nameScopeIt != end; ++nameScopeIt) {
			auto nameIt = nameScopeIt->find(id);
			if (nameIt != nameScopeIt->end()) {
				return nameIt->second;
			}
		}

		auto globalIt = newGlobalVars.find(id);
		if (globalIt != newGlobalVars.end()) {
			return globalIt->second;
		}

		auto globalVar = _context.getVariable(id);
		if (!globalVar.empty()) {
			UndeterminedVar uvar;
			for (auto& var : globalVar._vars) {
				auto& ptr = vars.emplace_back(std::make_unique<GlobalArgument>(var.rawData(), var.type()));
				// TODO get const and ref from var
				uvar.overload(CompilerVar(ptr.get(), false, false, false));
			}
			return uvar;
		}

		auto idStr = std::string(id);
		pushErrorMsg("[ERROR] Variable '" + idStr + "' doesn't exists");
		return _undefinedVar;
	}

	CompilerVar SemanticAnalyzer::createVar(const std::string_view& id, const TypeObject& type, bool isConst, bool isRef) {
		if (isLocalScope()) {
			auto& uvar = _localsByName.back()[id];

			if (!uvar.canBeOverloadedWith(type)) {
				pushErrorMsg("[ERROR] Variable '" + std::string(id) + "' already exists in local scope");
				return _undefinedVar;
			}

			auto tempVar = createTempVar(type);
			CompilerVar var(tempVar, isConst, false, isRef);
			uvar.overload(var);

			return var;
		}
		else {
			if (!_context.getVariable(id).empty()) {
				pushErrorMsg("[ERROR] Variable '" + std::string(id) + "' already exists in global scope");
				return _undefinedVar;
			}

			auto& uvar = newGlobalVars[id];
			if (!uvar.canBeOverloadedWith(type)) {
				pushErrorMsg("[ERROR] Variable '" + std::string(id) + "' already exists in global scope");
				return _undefinedVar;
			}

			auto ptr = vars.emplace_back(std::make_unique<GlobalArgument>(type)).get();
			CompilerVar var(ptr, isConst, false, isRef);
			uvar.overload(var);

			return var;
		}
	}

	CompilerVar SemanticAnalyzer::createFunctionParameterVar(uint64_t index, const std::string_view& id, const TypeObject& type, bool isConst, bool isRef) {
		auto [varIt, success] = _localsByName.back().try_emplace(id);
		if (!success) {
			pushErrorMsg("[ERROR] Parameter " + std::string(id) + " already exists");
			return _undefinedVar;
		}

		++_parametersCount;
		auto ptr = vars.emplace_back(std::make_unique<ParameterArgument>(index, type)).get();
		CompilerVar var(ptr, isConst, false, isRef);
		varIt->second.overload(var);

		return var;
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

	void SemanticAnalyzer::bakeLocalVars() {
		std::stack<uint64_t> stackOffsets;
		uint64_t currentOffset = _parametersCount * sizeof(void*);
		_stackSize = currentOffset;
		stackOffsets.push(currentOffset);

		auto localIt = _rootLocal;
		if (localIt->firstChild) {
			while (localIt->firstChild) {
				localIt = localIt->firstChild;
				stackOffsets.push(currentOffset);
			}
		}
		else {
			localIt = localIt->nextSibling;
		}

		while (localIt) {
			// TODO alignment
			localIt->argument->setOffset(currentOffset);
			currentOffset += localIt->argument->getType()->sizeOf();

			if (currentOffset > _stackSize) {
				_stackSize = currentOffset;
			}

			if (localIt->firstChild) {
				while (localIt->firstChild) {
					localIt = localIt->firstChild;
					stackOffsets.push(currentOffset);
				}
				continue;
			}

			if (localIt->nextSibling) {
				localIt = localIt->nextSibling;
				continue;
			}

			localIt = localIt->parent;
			currentOffset = stackOffsets.top();
			stackOffsets.pop();
		}
	}

	void SemanticAnalyzer::bakeContext() {
		for (auto& var : newGlobalVars) {
			auto& variants = var.second.getVariants();
			for (auto& variant : variants) {
				auto& type = *variant.argument->getType();
				void* ptr = _context.allocateGlobal(var.first, type);
				if (ptr == nullptr) {
					std::string id(var.first);
					pushErrorMsg("[ERROR] Variable " + id + " already exists");
				}
				variant.argument->bake(ptr);
			}
		}
	}

	std::variant<FunctionImpl*, std::string> SemanticAnalyzer::compile(const IRNode& block)&& {
		InstructionSequence mainSequence(*this);

		block.produceInstructions(mainSequence, *this);
		if (hasCompilationErrors()) {
			return compilationErrors();
		}

		bakeLocalVars();

		auto rawInstructions = std::move(mainSequence).buildInstructions();
		auto [functionPtr, functionRefs] = context().createObject<FunctionImpl>(context()._alloc, _stackSize, rawInstructions.size());

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