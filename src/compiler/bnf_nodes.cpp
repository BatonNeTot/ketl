/*🍲Ketl🍲*/
#include "bnf_nodes.h"

#include "parser_nodes.h"

#include <debugapi.h>

namespace Ketl {

	std::pair<bool, std::unique_ptr<Node>> BnfNodeLiteral::parse(std::list<Lexer::Token>::iterator& it, const std::list<Lexer::Token>::iterator& end, uint64_t& offset) const {
		if (value().empty()) {
			return std::make_pair<bool, std::unique_ptr<Node>>(true, {});
		}

		if (it == end) {
			return std::make_pair<bool, std::unique_ptr<Node>>(false, {});
		}

		switch (it->type) {
		case Lexer::Token::Type::Other: {
			if (value().length() > it->value.length() - offset) {
				return std::make_pair<bool, std::unique_ptr<Node>>(false, {});
			}

			for (auto i = 0u; i < value().length(); ++i) {
				if (value()[i] != it->value[i + offset]) {
					return std::make_pair<bool, std::unique_ptr<Node>>(false, {});
				}
			}

			if (value().length() == it->value.length() - offset) {
				++it;
				offset = 0;
			}
			else {
				offset += value().length();
			}
			break;
		}
		case Lexer::Token::Type::Id: {
			if (value().length() != it->value.length() || offset != 0) {
				return std::make_pair<bool, std::unique_ptr<Node>>(false, {});
			}

			for (auto i = 0u; i < value().length(); ++i) {
				if (value()[i] != it->value[i + offset]) {
					return std::make_pair<bool, std::unique_ptr<Node>>(false, {});
				}
			}

			++it;
			break;
		}
		default: {
			return std::make_pair<bool, std::unique_ptr<Node>>(false, {});
		}
		}

		if (_utility) {
			return std::make_pair<bool, std::unique_ptr<Node>>(true, {});
		}
		else {
			return std::make_pair<bool, std::unique_ptr<Node>>(true, std::make_unique<NodeLeaf>(value()));
		}
	}

	bool BnfNodeLiteral::process(std::list<Lexer::Token>::iterator& it, const std::list<Lexer::Token>::iterator& end,
		uint64_t& offset, ProcessNode& parentProcess) const {
		auto pair = parse(it, end, offset);
		if (!pair.first) {
			return false;
		}

		ProcessNode processNode(&parentProcess, this);

		ProcessNode* processParent = &processNode;
		auto nextNode = next(processParent);

		if (nextNode) {
			if (nextNode->process(it, end, offset, *processParent)) {
				if (pair.second) {
					parentProcess.outputChildrenNodes.emplace_front(std::move(pair.second));
				}
				return true;
			}
			return false;
		}

		return it == end;
	}

	std::pair<bool, std::unique_ptr<Node>> BnfNodeLeaf::parse(std::list<Lexer::Token>::iterator& it, const std::list<Lexer::Token>::iterator& end) const {
		if (it == end) {
			return std::make_pair<bool, std::unique_ptr<Node>>(false, {});
		}

		switch (it->type) {
		case Lexer::Token::Type::Id: {
			if (_type != Type::Id) {
				return std::make_pair<bool, std::unique_ptr<Node>>(false, {});
			}
			auto value = it->value;
			++it;
			return std::make_pair<bool, std::unique_ptr<Node>>(true, std::make_unique<NodeLeaf>(std::move(value), nullptr));
		}
		case Lexer::Token::Type::Number: {
			if (_type != Type::Number) {
				return std::make_pair<bool, std::unique_ptr<Node>>(false, {});
			}
			auto value = it->value;
			++it;
			return std::make_pair<bool, std::unique_ptr<Node>>(true, std::make_unique<NodeLeaf>(std::move(value), 0u));
		}
		case Lexer::Token::Type::Literal: {
			if (_type != Type::String) {
				return std::make_pair<bool, std::unique_ptr<Node>>(false, {});
			}
			auto value = it->value;
			++it;
			return std::make_pair<bool, std::unique_ptr<Node>>(true, std::make_unique<NodeLeaf>(std::move(value), '\0'));
		}
		case Lexer::Token::Type::Other: {
			return std::make_pair<bool, std::unique_ptr<Node>>(false, {});
		}
		default: {
			return std::make_pair<bool, std::unique_ptr<Node>>(false, {});
		}
		}
	}

	bool BnfNodeLeaf::process(std::list<Lexer::Token>::iterator& it, const std::list<Lexer::Token>::iterator& end,
		uint64_t& offset, ProcessNode& parentProcess) const {
		auto pair = parse(it, end);
		if (!pair.first) {
			return false;
		}

		ProcessNode processNode(&parentProcess, this);

		ProcessNode* processParent = &processNode;
		auto nextNode = next(processParent);

		if (nextNode) {
			if (nextNode->process(it, end, offset, *processParent)) {
				if (pair.second) {
					parentProcess.outputChildrenNodes.emplace_front(std::move(pair.second));
				}
				return true;
			}
			return false;
		}

		return it == end;
	}

	void BnfNodeId::preprocess(const BnfManager& manager) {
		_node = manager.getById(_id);
		BnfNode::preprocess(manager);
	}

	bool BnfNodeId::process(std::list<Lexer::Token>::iterator& it, const std::list<Lexer::Token>::iterator& end,
		uint64_t& offset, ProcessNode& parentProcess) const {
		ProcessNode processNode(&parentProcess, this);

		if (_node->process(it, end, offset, processNode)) {
			if (!processNode.outputChildrenNodes.empty() && processNode.outputChildrenNodes.back()) {
				auto& outputNode = processNode.outputChildrenNodes.back();
				if (_placeholder) {
					parentProcess.outputChildrenNodes.emplace_front(
						std::move(outputNode));
				}
				else if (_unfold) {
					auto& children = outputNode->children();
					for (auto it = children.rbegin(), end = children.rend(); it != end; ++it) {
						parentProcess.outputChildrenNodes.emplace_front(std::move(*it));
					}
				}
				else {
					parentProcess.outputChildrenNodes.emplace_front(std::make_unique<NodeIdHolder>(
						std::move(outputNode), _id
						));
				}
			}
			return true;
		}
		return false;
	}

	bool BnfNodeConcat::process(std::list<Lexer::Token>::iterator& it, const std::list<Lexer::Token>::iterator& end,
		uint64_t& offset, ProcessNode& parentProcess) const {
		ProcessNode processNode(&parentProcess, this);

		ProcessNode* processParent = &processNode;
		auto nextNode = _firstChild.get();

		if (nextNode) {
			if (nextNode->process(it, end, offset, processNode)) {
				if (!processNode.outputChildrenNodes.empty()) {
					if (processNode.outputChildrenNodes.size() == 1 && _weak) {
						parentProcess.outputChildrenNodes.emplace_front(std::move(processNode.outputChildrenNodes.back()));
					}
					else {
						auto concat = std::make_unique<NodeConcat>();
						for (auto& child : processNode.outputChildrenNodes) {
							concat->children().emplace_back(std::move(child));
						}
						parentProcess.outputChildrenNodes.emplace_front(std::move(concat));
					}
				}

				return true;
			}
			return false;
		}

		return it == end;
	}

	bool BnfNodeOr::process(std::list<Lexer::Token>::iterator& it, const std::list<Lexer::Token>::iterator& end,
		uint64_t& offset, ProcessNode& parentProcess) const {
		ProcessNode processNode(&parentProcess, this);

		for (auto i = 0u; i < _children.size(); ++i) {
			auto tempIt = it;
			auto tempOffset = offset;
			if (_children[i]->process(tempIt, end, tempOffset, processNode)) {
				it = tempIt;
				offset = tempOffset;
				if (!processNode.outputChildrenNodes.empty() && processNode.outputChildrenNodes.front()) {
					parentProcess.outputChildrenNodes.emplace_front(std::move(processNode.outputChildrenNodes.front()));
				}
				return true;
			}
		}

		return false;
	}

}