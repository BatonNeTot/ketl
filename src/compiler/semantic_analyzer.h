/*🍲Ketl🍲*/
#ifndef compiler_semantic_analyzer_h
#define compiler_semantic_analyzer_h

#include "common.h"
#include "parser.h"
#include "context.h"

#include <stack>
#include <map>

namespace Ketl {

	class SemanticAnalyzer;
	class RawArgument;
	class TypeObject;

	struct CompilerVar {
		CompilerVar() = default;
		CompilerVar(RawArgument* argument_, bool isCTK_, VarTraits traits_)
			: isCTK(isCTK_), traits(traits_), argument(argument_) {}

		// compile time known
		bool isCTK = false;
		VarTraits traits;
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

	private:
		UndeterminedVar _uvar;
		std::vector<UndeterminedDelegate> _arguments;
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
		virtual void propagadeInstruction(Instruction* instructions) const = 0;
		virtual uint64_t countInstructions() const = 0;
		virtual void fillReturnStatements(std::list<UndeterminedDelegate>& returnInstructions) const {};
	};

	class InstructionSequence : public RawInstruction {
	public:

		InstructionSequence(SemanticAnalyzer& context)
			: _context(context) {}

		RawArgument* createFullInstruction(Instruction::Code code, RawArgument* first, RawArgument* second, const TypeObject& outputType);
		CompilerVar createDefine(const std::string_view& id, const TypeObject& type, RawArgument* expression);

		RawArgument* createFunctionCall(RawArgument* caller, std::vector<RawArgument*>&& arguments);
		void createIfElseBranches(const IRNode& condition, const IRNode* trueBlock, const IRNode* falseBlock);

		void createReturnStatement(UndeterminedDelegate expression);

		void propagadeInstruction(Instruction* instructions) const override;

		uint64_t countInstructions() const {
			uint64_t sum = 0u;
			for (const auto& rawInstruction : _rawInstructions) {
				sum += rawInstruction->countInstructions();
			}
			if (!_returnExpression.getUVar().empty()) {
				++sum;
			}
			return sum;
		}

		void fillReturnStatements(std::list<UndeterminedDelegate>& returnInstructions) const override {
			returnInstructions.emplace_back(_returnExpression);
			for (const auto& rawInstruction : _rawInstructions) {
				rawInstruction->fillReturnStatements(returnInstructions);
			}
		}

	private:

		bool verifyReturn();

		bool _hasReturnStatement = false;
		bool _raisedAfterReturnError = false;
		UndeterminedDelegate _returnExpression;
		SemanticAnalyzer& _context;
		std::vector<std::unique_ptr<RawInstruction>> _rawInstructions;

	};

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
		virtual UndeterminedDelegate produceInstructions(InstructionSequence& instructions, SemanticAnalyzer& context) const { return {}; }
	};

	class StackArgument;
	class SemanticAnalyzer {
	public:

		SemanticAnalyzer(Context& context, SemanticAnalyzer* parentContext = nullptr);
		~SemanticAnalyzer() = default;

		std::variant<FunctionImpl*, std::string> compile(const IRNode& block)&&;

		void bakeLocalVars();
		void bakeContext();

		RawArgument* createTempVar(const TypeObject& type);

		CompilerVar createLiteralVar(const std::string_view& value);
		CompilerVar createLiteralClassVar(void* ptr, const TypeObject& type);

		RawArgument* createReturnVar(RawArgument* expression);

		RawArgument* createFunctionArgumentVar(uint64_t index, const TypeObject& type);
		CompilerVar createFunctionParameterVar(uint64_t index, const std::string_view& id, const TypeObject& type, VarTraits traits);


		RawArgument* deduceUnaryOperatorCall(OperatorCode code, const UndeterminedDelegate& var, InstructionSequence& instructions);
		CompilerVar deduceBinaryOperatorCall(OperatorCode code, const UndeterminedDelegate& lhs, const UndeterminedDelegate& rhs, InstructionSequence& instructions);
		UndeterminedDelegate deduceFunctionCall(const UndeterminedDelegate& caller, const std::vector<UndeterminedDelegate>& arguments, InstructionSequence& instructions);
		const TypeObject* deduceCommonType(const std::vector<UndeterminedDelegate>& vars);


		UndeterminedVar getVar(const std::string_view& id);
		CompilerVar createVar(const std::string_view& id, const TypeObject& type, VarTraits traits);

		Variable evaluate(const IRNode& node);
		const TypeObject* evaluateType(const IRNode& node);

		void pushErrorMsg(const std::string& msg) {
			_compilationErrors.emplace_back(msg);
		}

		bool hasCompilationErrors() const { return !_compilationErrors.empty(); }
		std::string compilationErrors() const;

		Context& context() const {
			return _context;
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
			UndefinedArgument(Context& context) {
				_type = context.getVariable("Void").as<TypeObject>();
			}

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

		Context& _context;

		std::vector<std::string> _compilationErrors;

		bool _localScope;
	};

}

#endif /*compiler_semantic_analyzer_h*/