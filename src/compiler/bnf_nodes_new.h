/*🍲Ketl🍲*/
#ifndef bnf_nodes_new_h
#define bnf_nodes_new_h

#include "parser_new.h"

#include <format>

namespace Ketl {

	struct ProcessNodeNew {
	public:
		// active phase
		ProcessNodeNew* parent = nullptr;
		ParserNew::Node* node = nullptr;
		ParserNew::Node::State state = 0;
		BnfIteratorNew iterator;

		// tree construction phase
		ProcessNodeNew* firstChild = nullptr;
		ProcessNodeNew* prevSibling = nullptr;
		ProcessNodeNew* nextSibling = nullptr;
	};

	class NodeLeafNew : public ParserNew::Node {
	public:

		enum class Type : uint8_t {
			Id,
			Number,
			String,
		};

		NodeLeafNew(Type type)
			: _type(type) {}
		NodeLeafNew(CreatorIRTree creator, Type type)
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

		std::string_view value(const BnfIteratorNew& iterator) const override {
			return iterator.value();
		};

		bool iterate(BnfIteratorNew& it, State& state) const override {
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

	class NodeLiteralNew : public ParserNew::Node {
	public:
		NodeLiteralNew(bool technical, const std::string_view& value)
			: _value(value), _technical(technical) {}

		void postConstruct(const std::unordered_map<std::string_view, std::unique_ptr<Node>>& nodeMap) override {}

		std::string_view toString() const override {
			return _value;
		}

		std::string_view value(const BnfIteratorNew&) const override {
			return _value;
		};

		bool iterate(BnfIteratorNew& it, State& state) const override {
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

		bool excludeFromTree(const ProcessNodeNew* info) const override {
			return _technical;
		}

	private:
		std::string_view _value;
		bool _technical;
	};

	class NodeIdNew : public ParserNew::Node {
	public:

		NodeIdNew(bool optional, const std::string_view& id)
			: _id(id), _optional(optional) {}
		NodeIdNew(CreatorIRTree creator, bool optional, const std::string_view& id)
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

		bool excludeFromTree(const ProcessNodeNew* info) const override {
			return !info->firstChild || (_optional && !info->firstChild->nextSibling);
		}

	private:
		std::string_view _id;
		ParserNew::Node* _node = nullptr;
		bool _optional = false;
	};

	class NodeConcatNew : public ParserNew::Node {
	public:

		template<class... Args>
		NodeConcatNew(bool weak, Args&&... args) : _weak(weak) {
			_children.reserve(sizeof...(Args));
			std::unique_ptr<ParserNew::Node> nodes[] = { std::move(args)... };
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

		bool excludeFromTree(const ProcessNodeNew* info) const override {
			return _weak || !info->firstChild || !info->firstChild->nextSibling;
		}

	private:
		std::vector<std::unique_ptr<ParserNew::Node>> _children;
		bool _weak;
	};

	class NodeOrNew : public ParserNew::Node {
	public:

		template<class... Args>
		NodeOrNew(Args&&... args) {
			_children.reserve(sizeof...(Args));
			std::unique_ptr<ParserNew::Node> nodes[] = { std::move(args)... };
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

		bool excludeFromTree(const ProcessNodeNew*) const override {
			return true;
		}

	private:
		std::vector<std::unique_ptr<ParserNew::Node>> _children;
	};

	class NodeRepeatNew : public ParserNew::Node {
	public:

		NodeRepeatNew(std::unique_ptr<ParserNew::Node>&& node)
			: _node(std::move(node)) {}
		NodeRepeatNew(CreatorIRTree creator, std::unique_ptr<ParserNew::Node>&& node)
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

		bool excludeFromTree(const ProcessNodeNew* info) const override {
			return true;
		}

	private:
		std::unique_ptr<ParserNew::Node> _node;
	};

	class NodeConditionalNew : public ParserNew::Node {
	public:

		template<class... Args>
		NodeConditionalNew(Args&&... args) {
			_children.reserve(sizeof...(Args));
			std::unique_ptr<ParserNew::Node> nodes[] = { std::move(args)... };
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

		bool excludeFromTree(const ProcessNodeNew*) const override {
			return true;
		}

	private:
		std::vector<std::unique_ptr<ParserNew::Node>> _children;
	};
}

#endif /*bnf_nodes_new_h*/