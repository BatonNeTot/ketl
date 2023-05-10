/*🍲Ketl🍲*/
#include "semantic_analyzer.h"

#include "raw_instructions.h"

#include "context.h"
#include "ketl.h"

#include <sstream>

namespace Ketl {

	const TypeObject* TypeFabric::createType(SemanticAnalyzer& analyzer) const {
		switch (_type)
		{
		case Ketl::TypeFabric::Type::Regular: {
			return analyzer.vm().getType(_value);
		}
		case Ketl::TypeFabric::Type::Function: {
			auto outputType = _additionalArguments[0].createType(analyzer);
			std::vector<VarTraits> parameters(_additionalArguments.size() - 1);
			for (auto i = 1u; i < _additionalArguments.size(); ++i) {
				parameters[i - 1].type = _additionalArguments[i].createType(analyzer);
			}
			return analyzer.vm().findOrCreateFunctionType(*outputType, std::move(parameters));
		}
		default:
			return nullptr;
		}
	}

	bool UndeterminedVar::canBeOverloadedWith(const TypeObject& type) const {
		if (_potentialVars.empty()) {
			return true;
		}
		return _potentialVars[0].argument->getType()->doesSupportOverload() && type.doesSupportOverload();
	};

	SemanticAnalyzer::SemanticAnalyzer(VirtualMachine& vm, SemanticAnalyzer* parentAnalyzer, bool global)
		: _vm(vm), _localScope(!global)
		, _undefinedArgument(*vm._types.typeOf<void>()), _undefinedVar(&_undefinedArgument, false, {true, false}) {
		_rootLocal = &_localVars.emplace_back();
		_currentLocal = _rootLocal;

		_localsByName.emplace_back();
	}

	SemanticAnalyzer::UndefinedArgument::UndefinedArgument(const TypeObject& voidType) {
		_type = &voidType;
	}

	std::vector<VarTraits> UndeterminedDelegate::canBeCastedTo(const Context& context) const {
		std::vector<VarTraits> casts;

		switch (_type) {
		case Type::NoCall: {
			for (auto& var : _uvar.getVariants()) {
				auto type = var.argument->getType();

				context.canBeCastedTo(*type, var.traits, casts);

				auto returnType = type->getReturnType();
				if (!returnType) {
					// TODO operator call on object?
					// try to find overloaded operator in context
					continue;
				}

				if (type->getParameters().empty()) {
					casts.emplace_back(*returnType);
				}
			}
			break;
		}
		case Type::CallOperator: {
			for (auto& var : _uvar.getVariants()) {
				auto type = var.argument->getType();

				auto returnType = type->getReturnType();
				if (!returnType) {
					// TODO operator call on object?
					// try to find overloaded operator in context
					continue;
				}

				auto& parameters = type->getParameters();
				if (parameters.size() != _arguments.size()) {
					continue;
				}

				auto i = 0u;
				for (; i < _arguments.size(); ++i) {
					auto& argument = _arguments[i];
					auto& parameter = parameters[i];
					if (!argument.canBeCastedTo(context, parameter)) {
						break;
					}
				}

				if (i >= _arguments.size()) {
					casts.emplace_back(*returnType);
				}
			}
			break;
		}
		}

		return casts;
	}

	// TODO redu upper in down function through visitor pattern to remove code duplication
	bool UndeterminedDelegate::canBeCastedTo(const Context& context, VarTraits target) const {
		switch (_type) {
		case Type::NoCall: {
			for (auto& var : _uvar.getVariants()) {
				auto type = var.argument->getType();

				if (var.traits.convertableTo(target) && context.canBeCastedTo(*type, *target.type)) {
					return true;
				}

				auto returnType = type->getReturnType();
				if (!returnType) {
					// TODO operator call on object?
					// try to find overloaded operator in context
					continue;
				}

				if (type->getParameters().empty() && returnType == target.type) {
					return true;
				}
			}
			break;
		}
		case Type::CallOperator: {
			for (auto& var : _uvar.getVariants()) {
				auto type = var.argument->getType();

				auto returnType = type->getReturnType();
				if (!returnType) {
					// TODO operator call on object?
					// try to find overloaded operator in context
					continue;
				}

				if (returnType != target.type) {
					continue;
				}

				auto& parameters = type->getParameters();
				if (parameters.size() != _arguments.size()) {
					continue;
				}

				auto i = 0u;
				for (; i < _arguments.size(); ++i) {
					auto& argument = _arguments[i];
					auto& parameter = parameters[i];
					if (!argument.canBeCastedTo(context, parameter)) {
						break;
					}
				}

				if (i >= _arguments.size()) {
					return true;
				}
			}
			break;
		}
		}

		return false;
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
		std::vector<std::vector<VarTraits>> potentialArgs(2);
		std::vector<VarTraits> args(2);

		potentialArgs[0] = lhs.canBeCastedTo(vm()._context);
		potentialArgs[1] = rhs.canBeCastedTo(vm()._context);

		uint64_t variantsIndex = 0;
		uint64_t totalVariantsCount = 1;

		for (auto& potentialArg : potentialArgs) {
			totalVariantsCount *= potentialArg.size();
		}
		std::multiset<Context::DeductionResult<Instruction::Code>> evaluatedSolutions;

		while (variantsIndex < totalVariantsCount) {
			uint64_t variantsIndexCalculation = variantsIndex;
			for (uint64_t i = 0; i < potentialArgs.size(); ++i) {
				auto& potentialArg = potentialArgs[i];
				args[i] = potentialArg[variantsIndexCalculation % potentialArg.size()];
				variantsIndexCalculation /= potentialArg.size();
			}

			evaluatedSolutions.emplace(vm()._context.deduceBuiltInOperator(code, args));
			
			++variantsIndex;
		}

		auto it = evaluatedSolutions.begin(), end = evaluatedSolutions.end();
		while (true) {
			if (it == end) {
				pushErrorMsg("[ERROR] ambiguous call");
				return _undefinedVar;
			}

			if (it->value != Instruction::Code::None) {
				break;
			}

			++it;
		}

		auto almostWinner = it;
		++it;
		if (it != end && it->score == almostWinner->score) {
			pushErrorMsg("[ERROR] ambiguous call");
			return _undefinedVar;
		}

		auto& result = *almostWinner;

		auto lhsVar = castTo(lhs, result.parameters[0], instructions);
		auto rhsVar = castTo(rhs, result.parameters[1], instructions);

		auto instruction = instructions.createFullInstruction();

		instruction->code = result.value;

		RawArgument* output;
		switch (instruction->code) {
			case Instruction::Code::IsStructEqual:
			case Instruction::Code::IsStructNonEqual:
				output = createTempVar(*result.resultType);
				instruction->arguments.emplace_back(createLiteralVar(lhsVar.argument->getType()->sizeOf()).argument);
				instruction->arguments.emplace_back(lhsVar.argument);
				instruction->arguments.emplace_back(rhsVar.argument);
				instruction->arguments.emplace_back(output);
				break;
			case Instruction::Code::Assign:
				output = lhsVar.argument;
				instruction->arguments.emplace_back(createLiteralVar(lhsVar.argument->getType()->sizeOf()).argument);
				instruction->arguments.emplace_back(rhsVar.argument);
				instruction->arguments.emplace_back(output);
				break;
			default:
				output = createTempVar(*result.resultType);
				instruction->arguments.emplace_back(lhsVar.argument);
				instruction->arguments.emplace_back(rhsVar.argument);
				instruction->arguments.emplace_back(output);
				break;
		}

		return CompilerVar(output, false, {});
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
		/*
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
		if (bestVariantsIt == deducedVariants.end()) {
			pushErrorMsg("[ERROR] No satisfying function");
			return _undefinedVar;
		}

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
		*/

		// specifically a copy
		auto newDelegateCaller = caller.getUVar();
		return UndeterminedDelegate(std::move(newDelegateCaller), std::move(totalArguments));
	}
	const TypeObject* SemanticAnalyzer::deduceCommonType(const std::list<UndeterminedDelegate>& vars) {
		if (vars.size() == 1) {
			return vars.begin()->getUVar().getVarAsItIs().argument->getType();
		}
		// TODO
		__debugbreak();
		return nullptr;
	}

	class UnsignedLiteralArgument : public RawArgument {
	public:
		UnsignedLiteralArgument(uint64_t value, SemanticAnalyzer& analyzer)
			: _value(value) {
			_type = analyzer.vm().typeOf<uint64_t>();
		}

		std::pair<Argument::Type, Argument> getArgument() const override {
			auto type = Argument::Type::Literal;
			Argument argument;
			argument.uinteger = _value;
			return std::make_pair(type, argument);
		}

		const TypeObject* getType() const override {
			return _type;
		}

	private:

		const TypeObject* _type = nullptr;
		std::uint64_t _value;
		std::string_view _id;
	};

	CompilerVar SemanticAnalyzer::createLiteralVar(uint64_t value) {
		auto ptr = vars.emplace_back(std::make_unique<UnsignedLiteralArgument>(value, *this)).get();
		return CompilerVar(ptr, true, { true, false });
	}

	class LiteralArgument : public RawArgument {
	public:
		// TODO
		LiteralArgument(const std::string_view& value, SemanticAnalyzer& analyzer)
			: _value(value) {
			_type = analyzer.vm().typeOf<int64_t>();
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
		return CompilerVar(ptr, true, { true, false });
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

		return CompilerVar(var, true, { true, false });
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

	RawArgument* SemanticAnalyzer::createFunctionArgumentVar(uint64_t index) {
		auto& ptr = vars.emplace_back(std::make_unique<UnsignedLiteralArgument>(index * sizeof(void*), *this));

		return ptr.get();
	}

	class NewGlobalArgument : public RawArgument {
	public:
		NewGlobalArgument(const TypeObject& type)
			: _type(&type) {}

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

	class GlobalArgument : public RawArgument {
	public:
		GlobalArgument(void* ptr, const TypeObject& type)
			: _ptr(ptr), _type(&type) {}

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

		auto globalVar = vm().getVariable(id);
		if (!globalVar.empty()) {
			UndeterminedVar uvar;
			for (auto& var : globalVar._vars) {
				auto& ptr = vars.emplace_back(std::make_unique<GlobalArgument>(var.rawData(), var.type()));
				// TODO get const and ref from var
				uvar.overload(CompilerVar(ptr.get(), !isLocalScope(), {}));
			}
			return uvar;
		}

		auto idStr = std::string(id);
		pushErrorMsg("[ERROR] Variable '" + idStr + "' doesn't exists");
		return _undefinedVar;
	}

	CompilerVar SemanticAnalyzer::createVar(const std::string_view& id, VarTraits&& traits) {
		if (traits.type == nullptr) {
			__debugbreak();
		}
		if (isLocalScope()) {
			auto& uvar = _localsByName.back()[id];

			if (!uvar.canBeOverloadedWith(*traits.type)) {
				pushErrorMsg("[ERROR] Variable '" + std::string(id) + "' already exists in local scope");
				return _undefinedVar;
			}

			auto tempVar = createTempVar(*traits.type);
			CompilerVar var(tempVar, false, traits);
			uvar.overload(var);

			return var;
		}
		else {
			if (!vm().getVariable(id).empty()) {
				pushErrorMsg("[ERROR] Variable '" + std::string(id) + "' already exists in global scope");
				return _undefinedVar;
			}

			auto& uvar = newGlobalVars[id];
			if (!uvar.canBeOverloadedWith(*traits.type)) {
				pushErrorMsg("[ERROR] Variable '" + std::string(id) + "' already exists in global scope");
				return _undefinedVar;
			}

			auto ptr = vars.emplace_back(std::make_unique<NewGlobalArgument>(*traits.type)).get();
			CompilerVar var(ptr, false, traits);
			uvar.overload(var);

			return var;
		}
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

	CompilerVar SemanticAnalyzer::createFunctionParameterVar(uint64_t index, const std::string_view& id, VarTraits&& traits) {
		auto [varIt, success] = _localsByName.back().try_emplace(id);
		if (!success) {
			pushErrorMsg("[ERROR] Parameter " + std::string(id) + " already exists");
			return _undefinedVar;
		}

		++_parametersCount;
		auto ptr = vars.emplace_back(std::make_unique<ParameterArgument>(index, *traits.type)).get();
		CompilerVar var(ptr, false, traits);
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

	const TypeObject* SemanticAnalyzer::evaluateType(const IRNode& node) {
		// TODO
		return vm().typeOf<int64_t>();
	}

	CompilerVar SemanticAnalyzer::castTo(const UndeterminedDelegate& source, VarTraits target, InstructionSequence& instructions) {
		switch (source.getType()) {
		case UndeterminedDelegate::Type::NoCall: {
			// simple cast or no parameter casting
			for (const auto& var : source.getUVar().getVariants()) {
				auto& type = *var.argument->getType();

				auto returnType = type.getReturnType();
				if (returnType == target.type) {
					auto outputVar = instructions.createFunctionCall( var.argument, {} );
					return CompilerVar(outputVar, false, {});
				}

				// TODO if not ref, we need to create copy
				if (&type == target.type) {
					return var;
				}

				// TODO check and do actual casting
			}
			break;
		}
		case UndeterminedDelegate::Type::CallOperator: {
			auto& arguments = source.getArguments();
			// creating delegate or calling the function
			for (const auto& var : source.getUVar().getVariants()) {
				// TODO create if delegate needed and create one instead of calling
				auto& type = *var.argument->getType();
				auto returnType = type.getReturnType();
				if (!returnType) {
					continue;
				}

				auto& parameters = type.getParameters();
				if (parameters.size() != arguments.size()) {
					continue;
				}

				std::vector<RawArgument*> callArguments(parameters.size());
				for (auto i = 0u; i < parameters.size(); ++i) {
					callArguments[i] = castTo(arguments[i], parameters[i], instructions).argument;
				}

				auto outputVar = instructions.createFunctionCall(
					var.argument,
					std::move(callArguments)
				);

				return CompilerVar(outputVar, false, {});
			}
			break;
		}
		}

		// TODO we failed somehow, throw error
		return _undefinedVar;
	}

	const TypeObject* SemanticAnalyzer::autoCast(const UndeterminedDelegate& source) const {
		auto casts = source.canBeCastedTo(vm()._context);
		if (casts.empty()) {
			return nullptr;
		}

		return casts[0].type;
	}

	void SemanticAnalyzer::forceCall(const UndeterminedDelegate& delegate, InstructionSequence& instructions) {
		if (delegate.getType() == UndeterminedDelegate::Type::NoCall) {
			return;
		}

		auto& arguments = delegate.getArguments();
		// creating delegate or calling the function
		for (const auto& var : delegate.getUVar().getVariants()) {
			// TODO create if delegate needed and create one instead of calling
			auto& type = *var.argument->getType();
			auto returnType = type.getReturnType();
			if (!returnType) {
				continue;
			}

			auto& parameters = type.getParameters();
			if (parameters.size() != arguments.size()) {
				continue;
			}

			std::vector<RawArgument*> callArguments(parameters.size());
			for (auto i = 0u; i < parameters.size(); ++i) {
				callArguments[i] = castTo(arguments[i], parameters[i], instructions).argument;
			}

			instructions.createFunctionCall(
				var.argument,
				std::move(callArguments)
			);
			return;
		}

		pushErrorMsg("[ERROR] function call can't be resolved");
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
				void* ptr = _vm.allocateGlobal(var.first, type);
				if (ptr == nullptr) {
					std::string id(var.first);
					pushErrorMsg("[ERROR] Variable " + id + " already exists");
				}
				variant.argument->bake(ptr);
			}
		}
	}

	SemanticAnalyzer::CompilationProduct SemanticAnalyzer::compile(const IRNode& block, const TypeObject* returnType)&& {
		InstructionSequence mainSequence(*this);

		block.produceInstructions(mainSequence, *this);
		if (hasCompilationErrors()) {
			return compilationErrors();
		}

		std::list<UndeterminedDelegate> returnExpressions;
		auto allBranchesHasReturn = mainSequence.collectReturnStatements(returnExpressions);
		if (!allBranchesHasReturn && returnExpressions.empty()) {
			mainSequence.createReturnStatement({});
			allBranchesHasReturn = true;
		}

		if (!allBranchesHasReturn) {
			pushErrorMsg("[ERROR] Not all branches return value");
			return compilationErrors();
		}

		if (!returnType) {
			returnType = vm().typeOf<void>();
			if (!returnExpressions.empty()) {
				returnType = deduceCommonType(returnExpressions);

				if (!returnType) {
					return compilationErrors();
				}
			}
		}

		if (!mainSequence.bakeReturnType(*returnType)) {
			return compilationErrors();
		}

		bakeLocalVars();

		auto rawInstructionCount = mainSequence.countInstructions();

		auto intructions = vm().createArray<Instruction>(rawInstructionCount).first;

		auto [functionPtr, functionRefs] = vm().createObject<FunctionObject>(false, _stackSize, intructions, static_cast<uint32_t>(rawInstructionCount));
		functionRefs->registerAbsLink(intructions);

		bakeContext();
		if (hasCompilationErrors()) {
			return compilationErrors();
		}

		for (const auto& ref : resultRefs) {
			functionRefs->registerAbsLink(ref);
		}

		mainSequence.propagadeInstruction(functionPtr->_instructions);

		return std::make_pair(functionPtr, returnType);
	}
}