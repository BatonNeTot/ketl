/*🍲Ketl🍲*/
#ifndef parser_h
#define parser_h

#include "lexer.h"

#include "ketl.h"

#include <string>
#include <list>
#include <vector>

namespace Ketl {

	struct UniValue {
		enum class Type : uint8_t {
			String,
			Number,
		};

		Type type;

		std::string str;
		struct Number {
			enum class Type : uint8_t {
				Int32,
				Int64,
				Float32,
				Float64
			};

			Type type;

			union {
				int32_t vInt32;
				int64_t vInt64;
				float vFloat32;
				double vFloat64;
			};
		} number;

		std::string asStrType() const {
			switch (number.type) {
			case Number::Type::Int32: return "Int";
			case Number::Type::Int64: return "Int";
			case Number::Type::Float32: return "Float";
			case Number::Type::Float64: return "Float";
			}
			return "";
		}

		std::string strType() const {
			if (type == Type::String) {
				return "string";
			}

			switch (number.type) {
			case Number::Type::Int32: return "int32";
			case Number::Type::Int64: return "int64";
			case Number::Type::Float32: return "float32";
			case Number::Type::Float64: return "float64";
			}

			return "";
		}

		std::string& operator=(const std::string& value) {
			type = Type::String;
			return str = value;
		}

		int32_t& operator=(int value) {
			type = Type::Number;
			number.type = Number::Type::Int32;
			return number.vInt32 = value;
		}

		int64_t& operator=(long value) {
			type = Type::Number;
			number.type = Number::Type::Int64;
			return number.vInt64 = value;
		}

		float& operator=(float value) {
			type = Type::Number;
			number.type = Number::Type::Float32;
			return number.vFloat32 = value;
		}

		double& operator=(double value) {
			type = Type::Number;
			number.type = Number::Type::Float64;
			return number.vFloat64 = value;
		}
	};

	class Parser {
	public:

		struct Node {
			enum class Type {
				Operator,
				Number,
				Id,
			};

			UniValue value;

			Type type;
			std::vector<Node*> args;
			Node* parent = nullptr;
			int expectedArgc;
			bool isFunction = false;

			~Node() {
				for (auto& arg : args) {
					delete arg;
				}
			}

			Node(Type type, int expectedArgc = -1) :
				type(type), expectedArgc(expectedArgc) {}

			void insert(Node* node) {
				node->parent = this;
				args.emplace_back(node);
			}

			void replace(Node* node) {
				auto dad = parent;
				node->insert(this);
				if (dad != nullptr) {
					node->parent = dad;
					dad->args.back() = node;
				}
			}

			void bakeArgc() {
				if (this != nullptr) {}
				expectedArgc = static_cast<int>(args.size());
			}

			int getDepth() {
				return parent == nullptr ? 0 : parent->getDepth() + 1;
			}

			Node* findToken(const std::string& token) {
				if (value.str == token) {
					return this;
				}
				if (parent == nullptr) {
					return nullptr;
				}
				return parent->findToken(token);
			}

			Node* getLast(int level = -1) {
				if (this == nullptr) {
					return nullptr;
				}

				if (level == -1) {
					for (auto& it : args) {
						auto result = it->getLast();
						if (result != nullptr) {
							return result;
						}
					}
					return expectedArgc == -1 || expectedArgc > args.size() ? this : nullptr;
				}
				else {
					return level == 0 ? this : args.back()->getLast(level - 1);
				}
			}

			bool isCompleted() const {
				if (this == nullptr) {
					return false;
				}

				if (expectedArgc == -1 || expectedArgc < args.size()) {
					return false;
				}

				for (auto& arg : args) {
					bool result = arg->isCompleted();
					if (!result) {
						return false;
					}
				}
				return true;
			}

			bool isOperator() const {
				return type == Type::Operator && value.str != "()";
			}
		};

		Parser(const std::string& source) : _lexer(source) {}

		const Node& getNext();

		bool hasNext() {
			return _lexer.hasNext();
		}

	private:

		Node* checkId(const Lexer::Token& token, std::list<std::string>& typeTokens);

		Node* constructNumber(const Lexer::Token& token);

		Lexer _lexer;

		std::list<Node*> _comands;

		Lexer::Token _lastToken;
	};

}

#endif /*parser_h*/