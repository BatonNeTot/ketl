/*🍲Ketl🍲*/
#ifndef parser_new_h
#define parser_new_h

#include "ebnf.h"

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
				Block,
				Operator,
				Number,
				Id,
			};

			Type type;
			UniValue value;
			std::vector<std::unique_ptr<Node>> args;
			Node* parent = nullptr;
			bool isFunction = false;

			Node(Type type) :
				type(type) {}

			void insert(std::unique_ptr<Node> node) {
				if (!node) {
					return;
				}

				node->parent = this;
				args.emplace_back(std::move(node));
			}
		};

		std::unique_ptr<Node> proceed(const std::string& str);

	private:

		std::unique_ptr<Node> proceed(const ebnf::Ebnf::Token* token);

		std::list<Node*> _comands;
	};

}

#endif /*parser_new_h*/