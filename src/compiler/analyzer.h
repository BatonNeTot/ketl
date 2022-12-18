﻿/*🍲Ketl🍲*/
#ifndef analyzer_h
#define analyzer_h

#include "parser.h"
#include "parser_nodes.h"

#include "ketl.h"

#include <string>
#include <list>
#include <vector>
#include <unordered_map>

namespace Ketl {
	class Linker;

	enum class OperatorCodes : uint8_t {
		Plus,
		Minus,
		Multiply,
		Divide,
		Assign,
	};

	enum class TypeCodes : uint8_t {
		Body,
		Const,
		Pointer,
		LRef,
		RRef,
		Function,
	};

	enum class ByteInstruction : uint8_t {
		Variable,
		VariableDefinition,
		Literal,
		FunctionStack,
		UnaryOperator,
		BinaryOperator,
		Function,
		FunctionDefinition,
	};

	class Analyzer {
	public:

		template <class T>
		static void insert(std::vector<uint8_t>& bytes, const T& value) {
			for (auto i = 0u; i < sizeof(T); ++i) {
				bytes.emplace_back();
			}
			*reinterpret_cast<T*>(bytes.data() + bytes.size() - sizeof(T)) = value;
		}

		static void insert(std::vector <uint8_t>& bytes, const char* str) {
			while (*str != '\0') {
				bytes.emplace_back(*str);
				++str;
			}
			bytes.emplace_back('\0');
		}

		class ByteType {
		public:
			virtual ~ByteType() = default;
			virtual void binarize(std::vector<uint8_t>&byteData) const = 0;
		};

		class ByteTypeBody : public ByteType {
		public:
			ByteTypeBody() = default;
			ByteTypeBody(const std::string& id_)
				: id(id_) {}
			void binarize(std::vector<uint8_t>& byteData) const {
				insert(byteData, TypeCodes::Body);
				insert(byteData, id.c_str());
			}

			std::string id;
		};

		class ByteVariable {
		public:
			ByteVariable(uint64_t index_)
				: index(index_) {}
			virtual ~ByteVariable() = default;
			virtual void binarize(std::vector<uint8_t>& byteData) const = 0;
		
			uint64_t index;
		};

		class ByteVarialbeId : public ByteVariable {
		public:
			ByteVarialbeId(uint64_t index)
				: ByteVariable(index) {}
			void binarize(std::vector<uint8_t>& byteData) const override {
				insert(byteData, ByteInstruction::Variable);
				insert(byteData, id.c_str());
			}
			std::string id;
		};

		class ByteVariableLiteralFloat64 : public ByteVariable {
		public:
			ByteVariableLiteralFloat64(uint64_t index)
				: ByteVariable(index) {}
			void binarize(std::vector<uint8_t>& byteData) const override {
				insert(byteData, ByteInstruction::Literal);
				ByteTypeBody("Float64").binarize(byteData);
				insert(byteData, value);
			}
			double value = 0.;
		};

		class ByteVariableBinaryOperator : public ByteVariable {
		public:
			ByteVariableBinaryOperator(uint64_t index)
				: ByteVariable(index) {}
			void binarize(std::vector<uint8_t>& byteData) const override {
				insert(byteData, ByteInstruction::BinaryOperator);
				insert(byteData, code);
				insert(byteData, firstArg);
				insert(byteData, secondArg);
			}

			OperatorCodes code = OperatorCodes::Assign;
			uint64_t firstArg = 0;
			uint64_t secondArg = 0;
		};

		class ByteVariableFunction : public ByteVariable {
		public:
			ByteVariableFunction(uint64_t index)
				: ByteVariable(index) {}
			void binarize(std::vector<uint8_t>& byteData) const override {
				insert(byteData, ByteInstruction::Function);
				insert(byteData, function);
				insert(byteData, uint64_t(args.size()));
				for (auto& arg : args) {
					insert(byteData, arg);
				}
			}

			uint64_t function = 0;
			std::vector<uint64_t> args;
		};

		class ByteVariableDefineFunction : public ByteVariable {
		public:
			ByteVariableDefineFunction(uint64_t index)
				: ByteVariable(index) {}
			void binarize(std::vector<uint8_t>& byteData) const override {
				insert(byteData, ByteInstruction::FunctionDefinition);
				insert(byteData, id.c_str());
				returnType->binarize(byteData);
				insert(byteData, uint64_t(types.size()));
				for (auto& type : types) {
					type->binarize(byteData);
				}
				insert(byteData, uint64_t(bytedata.size()));
				for (auto& data : bytedata) {
					insert(byteData, data);
				}
			}

			std::string id;
			std::unique_ptr<ByteType> returnType;
			std::list<std::unique_ptr<ByteType>> types;
			std::vector<uint8_t> bytedata;
		};

		class ByteVariableFunctionArgument : public ByteVariable {
		public:
			ByteVariableFunctionArgument(uint64_t index)
				: ByteVariable(index) {}
			void binarize(std::vector<uint8_t>& byteData) const override {

			}
		};

		struct Scope {

			struct Info {
				ByteVariable* byteVariable = nullptr;
				uint64_t level = 0;
			};

			std::unordered_map<std::string, Info> map;
			uint64_t currentLevel = 0;
		};

		Analyzer() : _parser() {}

		std::vector<uint8_t> proceed(const std::string& source);

		std::vector<uint8_t> proceed(Scope& scope, const Node* commandsNode);

		std::vector<uint8_t> proceed(Scope& scope, std::list<std::unique_ptr<ByteVariable>>& variables, const Node* commandsNode);

	private:

		Ketl::Parser _parser;

		enum class State : unsigned char {
			Default,
			DeclarationVar,
		};

		ByteVariable* proceedCommands(const Node& nodeId, Scope& scope, std::list<std::unique_ptr<ByteVariable>>& args, std::vector<ByteVariable*>& stack);

		ByteVariable* proceedLtrBinary(
			std::vector<std::unique_ptr<Node>>::const_reverse_iterator begin, std::vector<std::unique_ptr<Node>>::const_reverse_iterator end,
			Scope& scope, std::list<std::unique_ptr<ByteVariable>>& args, std::vector<ByteVariable*>& stack);

		ByteVariable* proceedRtlBinary(
			std::vector<std::unique_ptr<Node>>::const_iterator begin, std::vector<std::unique_ptr<Node>>::const_iterator end, 
			Scope& scope, std::list<std::unique_ptr<ByteVariable>>& args, std::vector<ByteVariable*>& stack);

		std::unique_ptr<ByteType> proceedType(const Node& typeNode);
	};
}

#endif /*analyzer_h*/