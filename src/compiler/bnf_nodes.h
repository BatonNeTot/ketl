/*🍲Ketl🍲*/
#ifndef bnf_nodes_h
#define bnf_nodes_h

#include "parser.h"

namespace Ketl {

	class BnfNodeLiteral : public BnfNode {
	public:
		virtual ~BnfNodeLiteral() = default;

		BnfNodeLiteral(const std::string_view& value, bool utility = false)
			: _value(value), _utility(utility) {}

		std::pair<bool, std::unique_ptr<Node>> parse(std::list<Lexer::Token>::iterator& it, const std::list<Lexer::Token>::iterator& end, uint64_t& offset) const;

		bool process(std::list<Lexer::Token>::iterator& it, const std::list<Lexer::Token>::iterator& end,
			uint64_t& offset, ProcessNode& parentProcess) const override;

		const std::string_view& value() const {
			return _value;
		}

	private:
		std::string_view _value;
		bool _utility;
	};

	class BnfNodeId : public BnfNode {
	public:

		BnfNodeId(const std::string_view& id, bool unfold, bool placeholder)
			: _id(id), _unfold(unfold), _placeholder(placeholder) {}

		void preprocess(const BnfManager& manager) override;

		bool process(std::list<Lexer::Token>::iterator& it, const std::list<Lexer::Token>::iterator& end,
			uint64_t& offset, ProcessNode& parentProcess) const override;

	private:

		const BnfNode* _node = nullptr;
		std::string _id;
		bool _unfold = false;
		bool _placeholder = false;
	};

	class BnfNodeLeaf : public BnfNode {
	public:

		enum class Type : uint8_t {
			Id,
			Number,
			String,
		};

		BnfNodeLeaf(Type type)
			: _type(type) {}

		std::pair<bool, std::unique_ptr<Node>> parse(std::list<Lexer::Token>::iterator& it, const std::list<Lexer::Token>::iterator& end) const;

		const Type& type() const {
			return _type;
		}

		bool process(std::list<Lexer::Token>::iterator& it, const std::list<Lexer::Token>::iterator& end,
			uint64_t& offset, ProcessNode& parentProcess) const override;

	private:
		Type _type;
	};

	class BnfNodeConcat : public BnfNode {
	public:

		template<class... Args>
		BnfNodeConcat(Args&&... args) 
			: BnfNodeConcat(false, std::forward<Args>(args)...) {}

		template<class... Args>
		BnfNodeConcat(bool weak, Args&&... args)
			: _weak(weak) {
			std::unique_ptr<BnfNode> nodes[] = { std::move(args)... };
			_firstChild = std::move(nodes[0]);
			auto* it = &_firstChild;
			for (auto i = 1u; i < sizeof...(Args); ++i) {
				it->get()->nextSibling = std::move(nodes[i]);
				it = &it->get()->nextSibling;
			}
		}

		void preprocess(const BnfManager& manager) override {
			_firstChild->preprocess(manager);
			BnfNode::preprocess(manager);
		}

		bool process(std::list<Lexer::Token>::iterator& it, const std::list<Lexer::Token>::iterator& end,
			uint64_t& offset, ProcessNode& parentProcess) const override;

	private:

		std::unique_ptr<BnfNode> _firstChild;
		bool _weak = false;
	};

	class BnfNodeOr : public BnfNode {
	public:

		template<class... Args>
		BnfNodeOr(Args&&... args) {
			_children.reserve(sizeof...(Args));
			std::unique_ptr<BnfNode> nodes[] = { std::move(args)... };
			for (auto& node : nodes) {
				_children.emplace_back(std::move(node));
			}
		}

		void preprocess(const BnfManager& manager) override {
			for (auto& child : _children) {
				child->preprocess(manager);
			}
			BnfNode::preprocess(manager);
		}

		bool process(std::list<Lexer::Token>::iterator& it, const std::list<Lexer::Token>::iterator& end,
			uint64_t& offset, ProcessNode& parentProcess) const override;

	private:
		std::vector<std::unique_ptr<BnfNode>> _children;
	};

}

#endif /*bnf_nodes_h*/