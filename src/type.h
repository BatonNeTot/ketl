/*🍲Ketl🍲*/
#ifndef type_h
#define type_h

#include "ketl.h"

namespace Ketl {

	class BasicTypeBody {
	public:

		BasicTypeBody(const std::string& id, uint64_t size)
			: _id(id), _size(size) {}

		const std::string& id() const {
			return _id;
		}

		Type::FunctionInfo deduceConstructor(const std::vector<std::unique_ptr<const Type>>& argumentTypes) const;

		const FunctionImpl* construct(uint64_t funcIndex) const {
			return &_cstrs[funcIndex].func;
		}

		uint64_t contructorCount() const {
			return _cstrs.size();
		}

		const FunctionImpl* destruct(StackAllocator& stack, uint8_t* stackPtr, uint8_t* returnPtr) const {
			return &_dstr;
		}

		uint64_t sizeOf() const {
			return _size;
		}

	public:

		std::string _id;
		struct Constructor {
			Constructor() {}
			FunctionImpl func;
			std::vector<std::unique_ptr<const Type>> argTypes;
		};
		std::vector<Constructor> _cstrs;
		FunctionImpl _dstr;
		uint64_t _size = 0;

	};

	class TypeOfType : public Type {
	public:
		TypeOfType(const BasicTypeBody* body)
			: _body(body) {}
		std::string id() const override { return "Type"; };
		FunctionInfo deduceFunction(const std::vector<std::unique_ptr<const Type>>& argumentTypes) const override {
			// TODO constructor
			return FunctionInfo{};
		}
	private:

		uint64_t sizeOfImpl() const override { return sizeof(BasicTypeBody); };
		std::unique_ptr<Type> clone() const override {
			return std::make_unique<TypeOfType>(*this);
		}

		const BasicTypeBody* _body;
	};

	class BasicType : public Type {
	public:

		BasicType(const BasicTypeBody* body)
			: _body(body) {}
		BasicType(const BasicTypeBody* body, bool isConst, bool isRef, bool hasAddress)
			: Type(isConst, isRef, hasAddress), _body(body) {}

		std::string id() const override {
			return _body->id();
		}

		FunctionInfo deduceFunction(const std::vector<std::unique_ptr<const Type>>& argumentTypes) const override {
			return _body->deduceConstructor(argumentTypes);
		}

	private:

		uint64_t sizeOfImpl() const override {
			return _body->sizeOf();
		}

		std::unique_ptr<Type> clone() const override {
			return std::make_unique<BasicType>(*this);
		}

		const BasicTypeBody* _body;
	};

	class FunctionType : public Type {
	public:
		FunctionType() {}
		FunctionType(const FunctionType& function)
			: info(function.info) {}

		std::string id() const override {
			auto& func = info;
			auto id = func.returnType->id() + "(*)(";
			for (uint64_t i = 0u; i < func.argTypes.size(); ++i) {
				if (i != 0) {
					id += ", ";
				}
				id += func.argTypes[i]->id();
			}
			return id;
		}
		FunctionInfo deduceFunction(const std::vector<std::unique_ptr<const Type>>& argumentTypes) const override {
			FunctionInfo outputInfo;

			if (argumentTypes.size() != info.argTypes.size()) {
				return Type::deduceFunction(argumentTypes);
			}
			bool next = false;
			for (uint64_t typeIt = 0u; typeIt < info.argTypes.size(); ++typeIt) {
				if (!argumentTypes[typeIt]->convertableTo(*info.argTypes[typeIt])) {
					next = true;
					break;
				}
			}
			if (next) {
				return Type::deduceFunction(argumentTypes);
			}

			outputInfo.returnType = Type::clone(info.returnType);
			outputInfo.argTypes = &info.argTypes;

			return outputInfo;
		}

		struct Info {
			Info() {}
			Info(const Info& info)
				: returnType(Type::clone(info.returnType)), argTypes(info.argTypes.size()) {
				for (uint64_t i = 0u; i < info.argTypes.size(); ++i) {
					argTypes[i] = Type::clone(info.argTypes[i]);
				}
			}

			std::unique_ptr<const Type> returnType;
			std::vector<std::unique_ptr<const Type>> argTypes;
		};
		Info info;

	private:
		uint64_t sizeOfImpl() const override {
			return sizeof(FunctionImpl);
		}
		std::unique_ptr<Type> clone() const override {
			return std::make_unique<FunctionType>(*this);
		}
	};
}

#endif /*type_h*/