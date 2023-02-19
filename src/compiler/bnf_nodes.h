/*🍲Ketl🍲*/
#ifndef compiler_bnf_nodes_h
#define compiler_bnf_nodes_h

#include "parser.h"

#include <format>

namespace Ketl {

	struct ProcessNode {
	public:
		// active phase
		ProcessNode* parent = nullptr;
		Parser::Node* node = nullptr;
		Parser::Node::State state = 0;
		BnfIterator iterator;

		// tree construction phase
		ProcessNode* firstChild = nullptr;
		ProcessNode* prevSibling = nullptr;
		ProcessNode* nextSibling = nullptr;
	};

	class NodeLeaf : public Parser::Node {
	public:

		enum class Type : uint8_t {
			Id,
			Number,
			String,
		};

		NodeLeaf(Type type)
			: _type(type) {}
		NodeLeaf(CreatorIRTree creator, Type type)
			: Node(creator), _type(type) {}

		void postConstruct(const std::unordered_map<std::string_view, std::unique_ptr<Node>>& nodeMap) override {}

		std::string_view toString() const override {
			switch (_type) {
			case Type::Id: {
				return "id";
			}
			default: {
				return "expression";
			}
			}
		}

		std::string_view value(const BnfIterator& iterator) const override {
			return iterator.value();
		};

		bool iterate(BnfIterator& it, State& state) const override {
			if (!it) {
				return false;
			}

			switch (it.type()) {
			case Lexer::Token::Type::Id: {
				if (_type != Type::Id) {
					return false;
				}
				break;
			}
			case Lexer::Token::Type::Number: {
				if (_type != Type::Number) {
					return false;
				}
				break;
			}
			case Lexer::Token::Type::Literal: {
				if (_type != Type::String) {
					return false;
				}
				break;
			}
			case Lexer::Token::Type::Other: {
				return false;
			}
			default: {
				return false;
			}
			}
			++it;
			return true;
		}

		Node* nextChild(State& state) const override {
			return nullptr;
		}

		bool childRejected(State& state) const override {
			return false;
		}

	private:
		Type _type;
	};

	class NodeLiteral : public Parser::Node {
	public:
		NodeLiteral(bool technical, const std::string_view& value)
			: _value(value), _technical(technical) {}
		NodeLiteral(CreatorIRTree creator, bool technical, const std::string_view& value)
			: Node(creator), _value(value), _technical(technical) {}

		void postConstruct(const std::unordered_map<std::string_view, std::unique_ptr<Node>>& nodeMap) override {}

		std::string_view toString() const override {
			return _value;
		}

		std::string_view value(const BnfIterator&) const override {
			return _value;
		};

		bool iterate(BnfIterator& it, State& state) const override {
			if (_value.empty()) {
				return false;
			}

			if (!it) {
				return false;
			}

			switch (it.type()) {
			case Lexer::Token::Type::Other: {
				if (!(it += _value)) {
					return false;
				}
				break;
			}
			case Lexer::Token::Type::Id: {
				if (_value.length() != it.value().length() || !(it += _value)) {
					return false;
				}
				break;
			}
			default: {
				return false;
			}
			}

			return true;
		}

		Node* nextChild(State& state) const override {
			return nullptr;
		}

		bool childRejected(State& state) const override {
			return false;
		}

		bool excludeFromTree(const ProcessNode* info) const override {
			return _technical;
		}

	private:
		std::string_view _value;
		bool _technical;
	};

	class NodeId : public Parser::Node {
	public:

		NodeId(bool optional, const std::string_view& id)
			: _id(id), _optional(optional) {}
		NodeId(CreatorIRTree creator, bool optional, const std::string_view& id)
			: Node(creator), _id(id), _optional(optional) {}

		void postConstruct(const std::unordered_map<std::string_view, std::unique_ptr<Node>>& nodeMap) override {
			_node = nodeMap.find(_id)->second.get();
		}

		Node* nextChild(State& state) const override {
			if (state > 0) {
				return nullptr;
			}
			++state;
			return _node;
		}

		bool childRejected(State& state) const override {
			return false;
		}

		bool excludeFromTree(const ProcessNode* info) const override {
			return !info->firstChild || (_optional && !info->firstChild->nextSibling);
		}

	private:
		std::string_view _id;
		Parser::Node* _node = nullptr;
		bool _optional = false;
	};

	class NodeConcat : public Parser::Node {
	public:

		template<class... Args>
		NodeConcat(bool weak, Args&&... args) : _weak(weak) {
			_children.reserve(sizeof...(Args));
			std::unique_ptr<Parser::Node> nodes[] = { std::move(args)... };
			for (auto& node : nodes) {
				_children.emplace_back(std::move(node));
			}
		}

		void postConstruct(const std::unordered_map<std::string_view, std::unique_ptr<Node>>& nodeMap) override {
			for (auto& child : _children) {
				child->postConstruct(nodeMap);
			}
		}

		Node* nextChild(State& state) const override {
			return state < _children.size() ? _children[state++].get() : nullptr;
		}

		bool childRejected(State& state) const override {
			--state;
			return false;
		}

		bool excludeFromTree(const ProcessNode* info) const override {
			return _weak || !info->firstChild || !info->firstChild->nextSibling;
		}

	private:
		std::vector<std::unique_ptr<Parser::Node>> _children;
		bool _weak;
	};

	class NodeOr : public Parser::Node {
	public:

		template<class... Args>
		NodeOr(Args&&... args) {
			_children.reserve(sizeof...(Args));
			std::unique_ptr<Parser::Node> nodes[] = { std::move(args)... };
			for (auto& node : nodes) {
				_children.emplace_back(std::move(node));
			}
		}

		void postConstruct(const std::unordered_map<std::string_view, std::unique_ptr<Node>>& nodeMap) override {
			for (auto& child : _children) {
				child->postConstruct(nodeMap);
			}
		}

		Node* nextChild(State& state) const override {
			if (state >= _children.size()) {
				return nullptr;
			}
			state += _children.size();
			return _children[state - _children.size()].get();
		}

		bool childRejected(State& state) const override {
			state -= _children.size();
			++state;
			return state < _children.size();
		}

		bool excludeFromTree(const ProcessNode*) const override {
			return true;
		}

	private:
		std::vector<std::unique_ptr<Parser::Node>> _children;
	};

	class NodeRepeat : public Parser::Node {
	public:

		NodeRepeat(std::unique_ptr<Parser::Node>&& node)
			: _node(std::move(node)) {}
		NodeRepeat(CreatorIRTree creator, std::unique_ptr<Parser::Node>&& node)
			: Node(creator), _node(std::move(node)) {}

		void postConstruct(const std::unordered_map<std::string_view, std::unique_ptr<Node>>& nodeMap) override {
			_node->postConstruct(nodeMap);
		}

		Node* nextChild(State& state) const override {
			if (state == 1) {
				state = 0;
				return nullptr;
			}
			return _node.get();
		}

		bool childRejected(State& state) const override {
			state = 1;
			return true;
		}

		bool excludeFromTree(const ProcessNode* info) const override {
			return true;
		}

	private:
		std::unique_ptr<Parser::Node> _node;
	};

	class NodeConditional : public Parser::Node {
	public:

		template<class... Args>
		NodeConditional(Args&&... args) {
			_children.reserve(sizeof...(Args));
			std::unique_ptr<Parser::Node> nodes[] = { std::move(args)... };
			for (auto& node : nodes) {
				_children.emplace_back(std::move(node));
			}
		}

		void postConstruct(const std::unordered_map<std::string_view, std::unique_ptr<Node>>& nodeMap) override {
			for (auto& child : _children) {
				child->postConstruct(nodeMap);
			}
		}

		Node* nextChild(State& state) const override {
			if (state >= _children.size()) {
				return nullptr;
			}
			state += _children.size();
			return _children[state - _children.size()].get();
		}

		bool childRejected(State& state) const override {
			state -= _children.size();
			++state;
			return state <= _children.size();
		}

		bool excludeFromTree(const ProcessNode*) const override {
			return true;
		}

	private:
		std::vector<std::unique_ptr<Parser::Node>> _children;
	};
}

#endif /*compiler_bnf_nodes_h*/