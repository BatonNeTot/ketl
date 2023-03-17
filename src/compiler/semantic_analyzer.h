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

	class AnalyzerVar {
	public:
		virtual ~AnalyzerVar() = default;
		virtual std::pair<Argument::Type, Argument> getArgument(SemanticAnalyzer& context) const = 0;
		virtual const TypeObject* getType(SemanticAnalyzer& context) const = 0;
	};

	class UndeterminedVar {
	public:

		UndeterminedVar() = default;
		UndeterminedVar(AnalyzerVar* predeterminedVar)
			: _predeterminedVar(predeterminedVar) {}

		std::vector<const TypeObject*> typeVariants(SemanticAnalyzer& context) const {
			if (_predeterminedVar) {
				return { _predeterminedVar->getType(context) };
			}
			std::vector<const TypeObject*> types;
			types.reserve(_potentialFunctions.size());
			for (const auto& functionPair : _potentialFunctions) {
				types.emplace_back(functionPair.second);
			}
			return types;
		}

		// TODO raise error if there is no var
		AnalyzerVar* getVarAsItIs() const {
			return _predeterminedVar;
		}

	private:

		AnalyzerVar* _predeterminedVar = nullptr;
		std::vector<std::pair<FunctionImpl*, const TypeObject*>> _potentialFunctions;
	};

	class RawInstruction {
	public:
		Instruction::Code code = Instruction::Code::None;
		AnalyzerVar* outputVar;
		AnalyzerVar* firstVar;
		AnalyzerVar* secondVar;

		void propagadeInstruction(Instruction& instruction, SemanticAnalyzer& context);
	};

	class InstructionSequence {
	public:

		InstructionSequence(SemanticAnalyzer& context)
			: _context(context) {}

		RawInstruction& addInstruction();

		InstructionSequence									createIfBranch(AnalyzerVar* expression);
		std::pair<InstructionSequence, InstructionSequence> createIfElseBranches(AnalyzerVar* expression);
		InstructionSequence									createWhileBranch(AnalyzerVar* expression, const std::string_view& id);


		void addReturnStatement(UndeterminedVar expression);

		std::vector<RawInstruction> buildInstructions()&& {
			return std::move(_rawInstructions);
		}

	private:

		bool _hasReturnStatement = false;
		bool _raisedAfterReturnError = false;
		UndeterminedVar _returnExpression;
		SemanticAnalyzer& _context;
		std::vector<RawInstruction> _rawInstructions;

	};

	// Intermediate representation node
	class IRNode {
	public:

		virtual ~IRNode() = default;
		virtual UndeterminedVar produceInstructions(InstructionSequence& instructions, SemanticAnalyzer& context) const { return {}; }
	};

	class SemanticAnalyzer {
	public:

		SemanticAnalyzer(Context& context, bool localScope = false)
			: _context(context), _localScope(localScope) {}
		~SemanticAnalyzer() {}

		std::variant<FunctionImpl*, std::string> compile(const IRNode& block)&&;

		void bakeContext();

		AnalyzerVar* createTempVar(const TypeObject& type);

		AnalyzerVar* createLiteralVar(const std::string_view& value);

		AnalyzerVar* createReturnVar(AnalyzerVar* expression);

		AnalyzerVar* createFunctionVar(FunctionImpl* function, const TypeObject& type);

		AnalyzerVar* createFunctionArgumentVar(uint64_t index, const TypeObject& type);
		AnalyzerVar* createFunctionParameterVar(const std::string_view& id, const TypeObject& type);


		AnalyzerVar* deduceUnaryOperatorCall(OperatorCode code, const UndeterminedVar& var, InstructionSequence& instructions);
		AnalyzerVar* deduceBinaryOperatorCall(OperatorCode code, const UndeterminedVar& lhs, const UndeterminedVar& rhs, InstructionSequence& instructions);
		AnalyzerVar* deduceFunctionCall(const UndeterminedVar& caller, const std::vector<UndeterminedVar>& arguments, InstructionSequence& instructions);
		const TypeObject* deduceCommonType(const std::vector<UndeterminedVar>& vars);


		AnalyzerVar* getVar(const std::string_view& id);
		AnalyzerVar* createVar(const std::string_view& id, const TypeObject& type);

		Variable evaluate(const IRNode& node);

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

		uint64_t scopeLayer = 0u;
		uint64_t currentStackOffset = 0u;
		uint64_t maxOffsetValue = 0u;

		std::vector<std::unique_ptr<AnalyzerVar>> vars;

		struct ScopeVar {
			AnalyzerVar* var; 
			const TypeObject* type;
			uint64_t scopeLayer;
		};

		std::stack<ScopeVar> scopeVars;
		std::stack<uint64_t> scopeOffsets;
		std::unordered_map<std::string_view, std::map<uint64_t, AnalyzerVar*>> scopeVarsByNames;
		std::unordered_map<std::string_view, AnalyzerVar*> newGlobalVars;

		std::vector<const void*> resultRefs;

		Context& _context;

		std::vector<std::string> _compilationErrors;

		bool _localScope;
	};

}

#endif /*compiler_semantic_analyzer_h*/