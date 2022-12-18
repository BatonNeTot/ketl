/*🍲Ketl🍲*/
#ifndef parser_nodes_h
#define parser_nodes_h

#include "parser.h"

#include <algorithm>

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
			case Number::Type::Int32: return "Int32";
			case Number::Type::Int64: return "Int64";
			case Number::Type::Float32: return "Float32";
			case Number::Type::Float64: return "Float64";
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

		UniValue() = default;

		UniValue(const std::string& value)
			: type(Type::String), str(value) {}

		UniValue(int value)
			: type(Type::Number) {
			number.type = Number::Type::Int32;
			number.vInt32 = value;
		}

		UniValue(long value)
			: type(Type::Number) {
			number.type = Number::Type::Int64;
			number.vInt64 = value;
		}

		UniValue(float value)
			: type(Type::Number) {
			number.type = Number::Type::Float32;
			number.vFloat32 = value;
		}

		UniValue(double value)
			: type(Type::Number) {
			number.type = Number::Type::Float64;
			number.vFloat64 = value;
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

	class NodeLeaf : public Node{
	public:

		NodeLeaf(const std::string_view& value)
			: _value(value), _type(ValueType::Operator) {}

		NodeLeaf(const std::string_view& value, nullptr_t)
			: _value(value), _type(ValueType::Id) {}

		NodeLeaf(const std::string_view& value, unsigned int)
			: _value(value), _type(ValueType::Number) {}

		NodeLeaf(const std::string_view& value, char)
			: _value(value), _type(ValueType::String) {}

		ValueType type() const override {
			return _type;
		}

		const std::string_view& value() const override {
			return _value;
		}

	private:
		ValueType _type;
		std::string_view _value;
	};

	class NodeIdHolder : public Node {
	public:
		NodeIdHolder(std::unique_ptr<Node>&& node, std::string_view id)
			: _id(id) {
			_node.emplace_back(std::forward<std::unique_ptr<Node>>(node));
		}

		const std::string_view& id() const override {
			return _id;
		}

		void id(std::string_view id) {
			_id = id;
		}

		std::vector<std::unique_ptr<Node>>& children() override {
			return _node;
		}

		const std::vector<std::unique_ptr<Node>>& children() const override {
			return _node;
		}

	private:

		std::unique_ptr<Node>& node() {
			return _node.front();
		}

		std::vector<std::unique_ptr<Node>> _node;
		std::string_view _id;
	};

	class NodeUnfolder : public Node {
	public:
		NodeUnfolder(std::unique_ptr<Node>&& node) {
			_node.emplace_back(std::forward<std::unique_ptr<Node>>(node));
		}

		bool isUtility() const override { return true; }
		uint64_t countForMerge() const override { return std::max<uint64_t>(1u, node()->countForMerge()); }
		std::unique_ptr<Node> getForMerge(uint64_t index) override { return node()->countForMerge() > 0 ? node()->getForMerge(index) : std::move(node()); }

		std::vector<std::unique_ptr<Node>>& children() override {
			return _node;
		}

		const std::vector<std::unique_ptr<Node>>& children() const override {
			return _node;
		}

	private:

		std::unique_ptr<Node>& node() {
			return _node.front();
		}

		const std::unique_ptr<Node>& node() const {
			return _node.front();
		}

		std::vector<std::unique_ptr<Node>> _node;
	};

	class NodeConcat : public Node {
	public:

		using Args = std::vector<std::unique_ptr<Node>>;

		NodeConcat() {}
		NodeConcat(Args&& args)
			: _args(std::forward<Args>(args)) {}

		uint64_t countForMerge() const override { return _args.size(); }
		std::unique_ptr<Node> getForMerge(uint64_t index) override { return std::move(_args[index]); }

		std::vector<std::unique_ptr<Node>>& children() override {
			return _args;
		}

		const std::vector<std::unique_ptr<Node>>& children() const override {
			return _args;
		}

	private:
		Args _args;
	};

}

#endif /*parser_nodes_h*/