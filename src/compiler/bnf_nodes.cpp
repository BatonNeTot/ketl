/*🍲Ketl🍲*/
#include "bnf_nodes.h"

#include "parser_nodes.h"

#include <debugapi.h>

namespace Ketl {

	std::pair<bool, std::unique_ptr<Node>> BnfNodeLiteral::parse(BnfIterator& it) const {
		if (value().empty()) {
			return std::make_pair<bool, std::unique_ptr<Node>>(true, {});
		}

		if (!it) {
			return std::make_pair<bool, std::unique_ptr<Node>>(false, {});
		}

		switch (it.type()) {
		case Lexer::Token::Type::Other: {
			if (!(it += value())) {
				return std::make_pair<bool, std::unique_ptr<Node>>(false, {});
			}
			break;
		}
		case Lexer::Token::Type::Id: {
			if (value().length() != it.value().length() || !(it += value())) {
				return std::make_pair<bool, std::unique_ptr<Node>>(false, {});
			}
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

	bool BnfNodeLiteral::process(BnfIterator& it, ProcessNode& parentProcess) const {
		auto pair = parse(it);
		if (!pair.first) {
			parentProcess.node = this;
			return false;
		}

		ProcessNode processNode(&parentProcess, this);

		ProcessNode* processParent = &processNode;
		auto nextNode = next(processParent);

		if (nextNode) {
			if (nextNode->process(it, *processParent)) {
				if (pair.second) {
					parentProcess.outputChildrenNodes.emplace_front(std::move(pair.second));
				}
				return true;
			}
			parentProcess.node = processParent->node;
			return false;
		}

		auto end = static_cast<bool>(it);
		if (end) {
			parentProcess.node = nullptr;
		}
		return !end;
	}

	std::pair<bool, std::unique_ptr<Node>> BnfNodeLeaf::parse(BnfIterator& it) const {
		if (!it) {
			return std::make_pair<bool, std::unique_ptr<Node>>(false, {});
		}

		switch (it.type()) {
		case Lexer::Token::Type::Id: {
			if (_type != Type::Id) {
				return std::make_pair<bool, std::unique_ptr<Node>>(false, {});
			}
			auto value = it.value();
			++it;
			return std::make_pair<bool, std::unique_ptr<Node>>(true, std::make_unique<NodeLeaf>(std::move(value), nullptr));
		}
		case Lexer::Token::Type::Number: {
			if (_type != Type::Number) {
				return std::make_pair<bool, std::unique_ptr<Node>>(false, {});
			}
			auto value = it.value();
			++it;
			return std::make_pair<bool, std::unique_ptr<Node>>(true, std::make_unique<NodeLeaf>(std::move(value), 0u));
		}
		case Lexer::Token::Type::Literal: {
			if (_type != Type::String) {
				return std::make_pair<bool, std::unique_ptr<Node>>(false, {});
			}
			auto value = it.value();
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

	bool BnfNodeLeaf::process(BnfIterator& it, ProcessNode& parentProcess) const {
		auto pair = parse(it);
		if (!pair.first) {
			parentProcess.node = this;
			return false;
		}

		ProcessNode processNode(&parentProcess, this);

		ProcessNode* processParent = &processNode;
		auto nextNode = next(processParent);

		if (nextNode) {
			if (nextNode->process(it, *processParent)) {
				if (pair.second) {
					parentProcess.outputChildrenNodes.emplace_front(std::move(pair.second));
				}
				return true;
			}
			parentProcess.node = processParent->node;
			return false;
		}

		auto end = static_cast<bool>(it);
		if (end) {
			parentProcess.node = nullptr;
		}
		return !end;
	}

	void BnfNodeId::preprocess(const BnfManager& manager) {
		_node = manager.getById(_id);
		BnfNode::preprocess(manager);
	}

	bool BnfNodeId::process(BnfIterator& it, ProcessNode& parentProcess) const {
		ProcessNode processNode(&parentProcess, this);

		if (_node->process(it, processNode)) {
			if (_id == "primary") {
				auto test = 0;
			}
			if (!processNode.outputChildrenNodes.empty() && processNode.outputChildrenNodes.back()) {
				auto& outputNode = processNode.outputChildrenNodes.back();

				if (_weakContent) {

				} else {
					parentProcess.outputChildrenNodes.emplace_front(std::make_unique<NodeIdHolder>(
						std::move(outputNode), _id
						));
				}
			}
			return true;
		}

		parentProcess.node = processNode.node;
		return false;
	}

	bool BnfNodeConcat::process(BnfIterator& it, ProcessNode& parentProcess) const {
		ProcessNode processNode(&parentProcess, this);

		ProcessNode* processParent = &processNode;
		auto nextNode = _firstChild.get();

		if (nextNode) {
			if (nextNode->process(it, processNode)) {
				if (!processNode.outputChildrenNodes.empty()) {
					if (processNode.outputChildrenNodes.size() == 1) {
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
			parentProcess.node = processParent->node;
			return false;
		}

		auto end = static_cast<bool>(it);
		if (end) {
			parentProcess.node = nullptr;
		}
		return !end;
	}

	bool BnfNodeOr::process(BnfIterator& it, ProcessNode& parentProcess) const {
		ProcessNode processNode(&parentProcess, this);

		auto maxIt = it;
		const BnfNode* maxItNode = _children.front().get();
		for (auto i = 0u; i < _children.size(); ++i) {
			auto tempIt = it;
			processNode.node = this;
			if (_children[i]->process(tempIt, processNode)) {
				it = tempIt;
				if (!processNode.outputChildrenNodes.empty() && processNode.outputChildrenNodes.front()) {
					parentProcess.outputChildrenNodes.emplace_front(std::move(processNode.outputChildrenNodes.front()));
				}
				return true;
			}
			if (maxIt == tempIt) {
				maxItNode = processNode.node;
			} else if (maxIt < tempIt) {
				maxIt = tempIt;
				maxItNode = processNode.node;
			}
		}
		it = maxIt;
		parentProcess.node = maxItNode;
		return false;
	}

}