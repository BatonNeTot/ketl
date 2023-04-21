/*🍲Ketl🍲*/
#ifndef compiler_parser_h
#define compiler_parser_h

#include "compiler/lexer.h"
#include "common.h"

#include <variant>
#include <memory>
#include <unordered_map>

namespace Ketl {

	class TokenList {
	private:
		struct Node {
			Node() : token(nullptr) {}
			Node(const Lexer::Token& token)
				: token(new Lexer::Token(token)) {}

			Lexer::Token* token;
			Node* prev = nullptr;
			Node* next = nullptr;
		};
	public:

		class Iterator {
		public:

			Iterator() : _node(nullptr) {}

			Iterator& operator++() {
				_node = _node->next;
				return *this;
			}

			Iterator operator++(int) {
				Iterator it(_node);
				_node = _node->next;
				return it;
			}

			Iterator& operator--() {
				_node = _node->prev;
				return *this;
			}

			Iterator operator--(int) {
				Iterator it(_node);
				_node = _node->prev;
				return it;
			}

			Lexer::Token* operator->() {
				return _node->token;
			}

			const Lexer::Token* operator->() const {
				return _node->token;
			}

			explicit operator bool() const {
				return _node->token != nullptr;
			}

			friend bool operator==(const Iterator& lhs, const Iterator& rhs) {
				return lhs._node == rhs._node;
			}

		private:
			friend TokenList;

			Iterator(Node* node)
				: _node(node) {}

			Node* _node;
		};

		TokenList() {
			_first = new Node();
			_end = _first;

			_first->next = _first;
			_first->prev = _first;
		}

		void emplace_back(const Lexer::Token& token) {
			auto node = new Node(token);

			if (_first == _end) {
				_first = node;
				
				_end->next = node;
				node->prev = _end;
			}
			else {
				_end->prev->next = node;
				node->prev = _end->prev;
			}

			_end->prev = node;
			node->next = _end;
		}

		Iterator begin() {
			return Iterator(_first);
		}

	private:
		Node* _first;
		Node* _end;
	};

	class BnfIterator {
	public:

		BnfIterator() {}
		BnfIterator(TokenList::Iterator it)
			: _it(std::move(it)) {}

		BnfIterator& operator=(const BnfIterator& other) {
			_it = other._it;
			_itOffset = other._itOffset;
			return *this;
		}

		const std::string_view& value() const {
			static const std::string_view empty;
			return static_cast<bool>(_it) ? _it->value : empty;
		}

		Lexer::Token::Type type() const {
			static auto def = Lexer::Token::Type::Other;
			return static_cast<bool>(_it) ? _it->type : def;
		}

		explicit operator bool() const {
			return static_cast<bool>(_it);
		}

		BnfIterator& operator++() {
			++_it;
			return *this;
		}

		bool operator+=(const std::string_view& value) {
			if (value.length() > this->value().length() - _itOffset) {
				return false;
			}

			for (auto i = 0u; i < value.length(); ++i) {
				if (value[i] != this->value()[i + _itOffset]) {
					return false;
				}
			}

			if (value.length() == this->value().length() - _itOffset) {
				++_it;
				_itOffset = 0;
			}
			else {
				_itOffset += value.length();
			}

			return true;
		}

		explicit operator uint64_t() const {
			if (static_cast<bool>(_it)) {
				return _it->offset + _itOffset;
			}
			auto back = _it;
			--back;
			return back->offset + back->value.length();
		}

		friend bool operator<(const BnfIterator& lhs, const BnfIterator& rhs) {
			return static_cast<uint64_t>(lhs) < static_cast<uint64_t>(rhs);
		}

		friend bool operator==(const BnfIterator& lhs, const BnfIterator& rhs) {
			return lhs._it == rhs._it && lhs._itOffset == rhs._itOffset;
		}

	private:
		TokenList::Iterator _it;
		uint64_t _itOffset = 0;
	};

	struct ProcessNode;
	class IRNode;

	class Parser {
	public:

		class Node {
		public:
			using CreatorIRTree = std::unique_ptr<IRNode>(*)(const ProcessNode* info);

			using State = uint64_t;

			Node() = default;
			Node(CreatorIRTree creator) : _creator(creator) {}
			virtual ~Node() = default;
			virtual void postConstruct(const std::unordered_map<std::string_view, std::unique_ptr<Node>>& nodeMap) = 0;
			virtual std::string_view toString() const {
				return {};
			}; 
			virtual std::string_view value(const BnfIterator& iterator) const {
				return {};
			};

			virtual bool iterate(BnfIterator& iterator, State& state) const {
				state = 0;
				return true;
			}

			virtual Node* nextChild(State& state) const = 0;
			virtual bool childRejected(State& state) const = 0;

			virtual bool excludeFromTree(const ProcessNode* info) const {
				return false;
			}

			std::unique_ptr<IRNode> createIRTree(const ProcessNode* info) const;

		private: 

			CreatorIRTree _creator = nullptr;
		};

		Parser();

		std::variant<std::unique_ptr<IRNode>, std::string> parseTree(const std::string_view& str);

	private:

		Node* _root = nullptr;
		std::unordered_map<std::string_view, std::unique_ptr<Node>> _nodes;
	};

}

#endif /*compiler_parser_h*/