/*🍲Ketl🍲*/
#ifndef parser_new_h
#define parser_new_h

#include "lexer.h"
#include "common.h"

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

	class BnfIteratorNew {
	public:

		BnfIteratorNew() {}
		BnfIteratorNew(TokenList::Iterator it)
			: _it(std::move(it)) {}

		BnfIteratorNew& operator=(const BnfIteratorNew& other) {
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

		BnfIteratorNew& operator++() {
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

		friend bool operator<(const BnfIteratorNew& lhs, const BnfIteratorNew& rhs) {
			return static_cast<uint64_t>(lhs) < static_cast<uint64_t>(rhs);
		}

		friend bool operator==(const BnfIteratorNew& lhs, const BnfIteratorNew& rhs) {
			return lhs._it == rhs._it && lhs._itOffset == rhs._itOffset;
		}

	private:
		TokenList::Iterator _it;
		uint64_t _itOffset = 0;
	};

	struct ProcessNodeNew;

	class ParserNew {
	public:

		class Node {
		public:
			using CreatorIRTree = std::unique_ptr<IRNode>(*)(const ProcessNodeNew* info);

			using State = uint64_t;

			Node() = default;
			Node(CreatorIRTree creator) : _creator(creator) {}
			virtual ~Node() = default;
			virtual void postConstruct(const std::unordered_map<std::string_view, std::unique_ptr<Node>>& nodeMap) = 0;
			virtual std::string_view toString() const {
				return {};
			}; 
			virtual std::string_view value(const BnfIteratorNew& iterator) const {
				return {};
			};

			virtual bool iterate(BnfIteratorNew& iterator, State& state) const {
				state = 0;
				return true;
			}

			virtual Node* nextChild(State& state) const = 0;
			virtual bool childRejected(State& state) const = 0;

			virtual bool excludeFromTree(const ProcessNodeNew* info) const {
				return false;
			}

			std::unique_ptr<IRNode> createIRTree(const ProcessNodeNew* info) const {
				if (_creator == nullptr) {
					return {};
				}

				return _creator(info);
			}

		private: 

			CreatorIRTree _creator = nullptr;
		};

		ParserNew();

		std::unique_ptr<IRNode> parseTree(const std::string& str);

		const std::string& errorMsg() const {
			return _error;
		}

	private:

		Node* _root = nullptr;
		std::unordered_map<std::string_view, std::unique_ptr<Node>> _nodes;
		std::string _error;
	};

}

#endif /*parser_new_h*/