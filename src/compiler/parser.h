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

	class BnfNode {
	public:
		virtual ~BnfNode() = default;
		virtual void preprocess(const BnfManager& manager) {
			if (nextSibling) {
				nextSibling->preprocess(manager);
			}
		}

		virtual bool process(std::list<Lexer::Token>::iterator& it, const std::list<Lexer::Token>::iterator& end,
			uint64_t& offset, ProcessNode& parentProcess) const = 0;

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

	private:

		void insertPredence(std::unique_ptr<BnfNode>&& operators, const std::string& expression,
			const std::string& extra, const std::string& lowExpression);

		BnfManager _manager;
		std::list<std::unique_ptr<Node>> _comands;
	};

}

#endif /*parser_h*/