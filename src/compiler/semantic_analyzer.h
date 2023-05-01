/*🍲Ketl🍲*/
#ifndef compiler_semantic_analyzer_h
#define compiler_semantic_analyzer_h

#include "compiler/parser.h"
#include "context.h"
#include "common.h"

#include <stack>
#include <map>

namespace Ketl {

	class SemanticAnalyzer;
	class RawArgument;
	class TypeObject;

	struct CompilerVar {
		CompilerVar() = default;
		CompilerVar(RawArgument* argument_, bool isCTK_, VarPureTraits traits_)
			: traits(traits_), isCTK(isCTK_), argument(argument_) {}

		VarPureTraits traits;
		// compile time known
		bool isCTK = false;
		RawArgument* argument = nullptr;
	};

	class UndeterminedVar {
	public:

		UndeterminedVar() = default;
		UndeterminedVar(const CompilerVar& predeterminedVar) {
			_potentialVars.emplace_back(predeterminedVar);
		}
		UndeterminedVar(CompilerVar&& predeterminedVar) {
			_potentialVars.emplace_back(std::move(predeterminedVar));
		}
		UndeterminedVar(std::vector<CompilerVar>&& potentialVars)
			: _potentialVars(std::move(potentialVars)) {}

		bool empty() const {
			return _potentialVars.empty();
		}

		const std::vector<CompilerVar>& getVariants() const {
			return _potentialVars;
		}

		const CompilerVar& getVarAsItIs() const {
			if (_potentialVars.empty() || _potentialVars.size() > 1) {
				static CompilerVar emptyVar;
				return emptyVar;
			}
			return _potentialVars[0];
		}

		bool canBeOverloadedWith(const TypeObject& type) const;

		void overload(const CompilerVar& var) {
			_potentialVars.emplace_back(var);
		}
		void overload(CompilerVar&& var) {
			_potentialVars.emplace_back(std::move(var));
		}

	private:

		std::vector<CompilerVar> _potentialVars;
	};

	class UndeterminedDelegate {
	public:

		UndeterminedDelegate() = default;

		UndeterminedDelegate(const UndeterminedVar& uvar)
			: _uvar(uvar) {}
		UndeterminedDelegate(UndeterminedVar&& uvar)
			: _uvar(std::move(uvar)) {}
		UndeterminedDelegate(UndeterminedVar&& uvar, std::vector<UndeterminedDelegate>&& arguments)
			: _uvar(std::move(uvar)), _arguments(std::move(arguments)) {}

		UndeterminedDelegate(const CompilerVar& predeterminedVar)
			: _uvar(predeterminedVar) {}
		UndeterminedDelegate(CompilerVar&& predeterminedVar)
			: _uvar(std::move(predeterminedVar)) {}
		UndeterminedDelegate(std::vector<CompilerVar>&& potentialVars)
			: _uvar(std::move(potentialVars)) {}

		UndeterminedVar& getUVar() {
			return _uvar;
		}
		const UndeterminedVar& getUVar() const {
			return _uvar;
		}

		std::vector<UndeterminedDelegate>& getArguments() {
			return _arguments;
		}
		const std::vector<UndeterminedDelegate>& getArguments() const {
			return _arguments;
		}

		void addArgument(UndeterminedDelegate&& argument) {
			_arguments.emplace_back(std::move(argument));
		}

		bool hasSavedArguments() const {
			return !_arguments.empty();
		}

		std::vector<VarTraits> canBeCastedTo(const Context& context) const;
		bool canBeCastedTo(const Context& context, VarTraits target) const;

	private:
		UndeterminedVar _uvar;
		std::vector<UndeterminedDelegate> _arguments;
	};

	class InstructionIterator {
	public:
		InstructionIterator(Instruction* ptr) 
			: _ptr(ptr) {}

		InstructionIterator(Instruction* ptr, uint64_t offset)
			: _ptr(ptr), _offset(offset) {}

		Instruction* operator->() {
			return _ptr + _offset;
		}

		Instruction& operator[](uint64_t index) {
			return *(_ptr + _offset + index);
		}

		InstructionIterator operator+(uint64_t offset) {
			return {_ptr, _offset + offset};
		}

		InstructionIterator& operator+=(uint64_t offset) {
			_offset += offset;
			return *this;
		}

		uint64_t offset() const {
			return _offset;
		}

	private:

		Instruction* _ptr = nullptr;
		uint64_t _offset = 0;
	};


	class RawArgument {
	public:
		virtual ~RawArgument() = default;
		virtual void bake(void* ptr) {}
		virtual std::pair<Argument::Type, Argument> getArgument() const = 0;
		virtual const TypeObject* getType() const = 0;
	};

	class RawInstruction {
	public:
		virtual uint64_t propagadeInstruction(InstructionIterator instructions) const = 0;
		virtual uint64_t countInstructions() const = 0;
		virtual bool collectReturnStatements(std::list<UndeterminedDelegate>& returnInstructions) const { return false; }
		virtual bool bakeReturnType(const TypeObject& type) { return true; }
	};

	class InstructionSequence;

	enum class TypeAccessModifier : uint8_t {
		Public,
		Protected,
		Private
	};

	inline TypeAccessModifier modifierFromString(const std::string_view& value) {
		if (value == "public") {
			return TypeAccessModifier::Public;
		}
		if (value == "protected") {
			return TypeAccessModifier::Protected;
		}
		if (value == "private") {
			return TypeAccessModifier::Private;
		}
		return TypeAccessModifier::Public;
	}
 
	// Intermediate representation node
	class IRNode {
	public:

		virtual ~IRNode() = default;
		virtual UndeterminedDelegate produceInstructions(InstructionSequence& instructions, SemanticAnalyzer& analyzer) const { return {}; }
	};

	class VirtualMachine;
	class StackArgument;
	class SemanticAnalyzer {
	public:

		SemanticAnalyzer(VirtualMachine& vm, SemanticAnalyzer* parentAnalyzer, bool global);
		~SemanticAnalyzer() = default;

		using CompilationProduct = std::variant<std::pair<FunctionObject*, const TypeObject*>, std::string>;
		CompilationProduct compile(const IRNode& block, const TypeObject* returnType)&&;

		void bakeLocalVars();
		void bakeContext();

		RawArgument* createTempVar(const TypeObject& type);

		CompilerVar createLiteralVar(uint64_t value);
		CompilerVar createLiteralVar(const std::string_view& value);
		CompilerVar createLiteralClassVar(void* ptr, const TypeObject& type);

		RawArgument* createFunctionArgumentVar(uint64_t index);
		CompilerVar createFunctionParameterVar(uint64_t index, const std::string_view& id, VarTraits&& traits);


		RawArgument* deduceUnaryOperatorCall(OperatorCode code, const UndeterminedDelegate& var, InstructionSequence& instructions);
		CompilerVar deduceBinaryOperatorCall(OperatorCode code, const UndeterminedDelegate& lhs, const UndeterminedDelegate& rhs, InstructionSequence& instructions);
		UndeterminedDelegate deduceFunctionCall(const UndeterminedDelegate& caller, const std::vector<UndeterminedDelegate>& arguments, InstructionSequence& instructions);
		const TypeObject* deduceCommonType(const std::list<UndeterminedDelegate>& vars);


		UndeterminedVar getVar(const std::string_view& id);
		CompilerVar createVar(const std::string_view& id, VarTraits&& traits);

		const TypeObject* evaluateType(const IRNode& node);

		void pushErrorMsg(const std::string& msg) {
			_compilationErrors.emplace_back(msg);
		}

		bool hasCompilationErrors() const { return !_compilationErrors.empty(); }
		std::string compilationErrors() const;

		VirtualMachine& vm() const {
			return _vm;
		}

		bool isLocalScope() const {
			return _localScope;
		}

		void pushScope();
		void popScope();

		void propagateScopeDestructors() {} // TODO

		uint64_t _parametersCount = 0u;
		uint64_t _stackSize = 0u;

		std::vector<std::unique_ptr<RawArgument>> vars;

		class UndefinedArgument : public RawArgument {
		public:
			UndefinedArgument(const TypeObject& voidType);

		private:
			std::pair<Argument::Type, Argument> getArgument() const override {
				auto type = Argument::Type::None;
				Argument argument;
				return std::make_pair(type, argument);
			}

			const TypeObject* getType() const override {
				return _type;
			}

			const TypeObject* _type = nullptr;
		};

		UndefinedArgument _undefinedArgument;
		CompilerVar _undefinedVar;

		std::unordered_map<std::string_view, UndeterminedVar> newGlobalVars;

		struct LocalVar {
			StackArgument* argument = nullptr;
			LocalVar* parent = nullptr;
			LocalVar* nextSibling = nullptr;
			LocalVar* firstChild = nullptr;
		};

		std::list<std::unordered_map<std::string_view, UndeterminedVar>> _localsByName;
		std::list<LocalVar> _localVars;
		LocalVar* _rootLocal = nullptr;
		LocalVar* _currentLocal = nullptr;

		std::vector<const void*> resultRefs;

		VirtualMachine& _vm;

		std::vector<std::string> _compilationErrors;

		bool _localScope;
	};

}

#endif /*compiler_semantic_analyzer_h*/