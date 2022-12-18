/*🍲Ketl🍲*/
#ifndef parser_h
#define parser_h

#include "lexer.h"

#include <string>
#include <memory>
#include <unordered_map>
#include <list>
#include <vector>
#include <algorithm>

namespace Ketl {

	class BnfManager;

	class Node;
	class BnfNode;

	struct ProcessNode {
		ProcessNode() {}
		ProcessNode(ProcessNode* parent_, const BnfNode* node_)
			: parent(parent_), node(node_) {}

		ProcessNode* parent = nullptr;
		const BnfNode* node = nullptr;
		std::list<std::unique_ptr<Node>> outputChildrenNodes;
	};

	class BnfIterator {
	public:

		BnfIterator(std::list<Lexer::Token>::iterator&& it, const std::list<Lexer::Token>::iterator& end)
			: _it(std::move(it)), _end(end) {}

		BnfIterator& operator=(const BnfIterator& other) {
			_it = other._it;
			_itOffset = other._itOffset;
			return *this;
		}

		const std::string_view& value() const {
			static const std::string_view empty;
			return _it != _end ? _it->value : empty;
		}

		Lexer::Token::Type type() const {
			static auto def = Lexer::Token::Type::Other;
			return _it != _end ? _it->type : def;
		}

		explicit operator bool() const {
			return _it != _end;
		}

		explicit operator char() const {
			return _it != _end ? _it->value[_itOffset] : '\0';
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
			if (_it != _end) {
				return _it->offset + _itOffset;
			}
			auto back = _end;
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
		std::list<Lexer::Token>::iterator _it;
		const std::list<Lexer::Token>::iterator& _end;
		uint64_t _itOffset = 0;
	};

	class BnfNode {
	public:
		virtual ~BnfNode() = default;
		virtual void preprocess(const BnfManager& manager) {
			if (nextSibling) {
				nextSibling->preprocess(manager);
			}
		}

		virtual bool process(BnfIterator& it, ProcessNode& parentProcess) const = 0;

		const BnfNode* next(ProcessNode*& parentProcess) const {
			while (parentProcess != nullptr && parentProcess->node != nullptr) {
				auto& nextSib = parentProcess->node->nextSibling;
				parentProcess = parentProcess->parent;
				if (nextSib) {
					return nextSib.get();
				}
			}

			return nullptr;
		}

		virtual std::string_view errorMsg() const { return {}; }
		
		std::unique_ptr<BnfNode> nextSibling;
	};

	class BnfManager {
	public:

		const BnfNode* getById(const std::string& id) const {
			auto it = _ids.find(id);
			return it != _ids.end() ? it->second.get() : nullptr;
		}

		void insert(const std::string&id, std::unique_ptr<BnfNode>&& node) {
			_ids.try_emplace(id, std::forward<std::unique_ptr<BnfNode>>(node));
		}

		void preprocessNodes() {
			for (auto& pair : _ids) {
				pair.second->preprocess(*this);
			}
		}

	private:
		std::unordered_map<std::string, std::unique_ptr<BnfNode>> _ids;
	};

	enum class ValueType : uint8_t {
		Operator,
		Id,
		Number,
		String,
	};

	class Node {
	public:

		virtual ~Node() = default;
		virtual bool isUtility() const { return false; }
		virtual uint64_t countForMerge() const { return 0; }
		virtual std::unique_ptr<Node> getForMerge(uint64_t index) { return {}; }
		virtual const std::string_view& id() const {
			static const std::string_view empty;
			return empty;
		}
		virtual const std::string_view& value() const {
			static const std::string_view empty;
			return empty;
		}

		virtual std::vector<std::unique_ptr<Node>>& children() {
			static std::vector<std::unique_ptr<Node>> empty;
			return empty;
		}

		virtual const std::vector<std::unique_ptr<Node>>& children() const {
			static const std::vector<std::unique_ptr<Node>> empty;
			return empty;
		}

		virtual ValueType type() const {
			return ValueType::Operator;
		}

	};

	class Parser {
	public:

		Parser();

		std::unique_ptr<Node> proceed(const std::string& str);

		const std::string& errorMsg() const {
			return _error;
		}

	private:

		void insertPredence(std::unique_ptr<BnfNode>&& operators, const std::string& expression,
			const std::string& extra, const std::string& lowExpression);

		BnfManager _manager;

		std::string _error;
	};

}

#endif /*parser_h*/