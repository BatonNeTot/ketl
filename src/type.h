/*🍲Ketl🍲*/
#ifndef type_h
#define type_h

#include "ketl.h"

namespace Ketl {

	class Context;

	class InterfaceTypeObject : public TypeObject {
	public:
		InterfaceTypeObject(const std::string& id)
			: _id(id) {}

		std::string id() const override { return _id; }

		uint64_t sizeOf() const override { return 0; }

		bool isLight() const override { return true; }

	private:

		std::string _id;
	};

	class ClassTypeObject : public TypeObject {
	public:
		ClassTypeObject(const std::string& id, uint64_t size)
			: _id(id), _size(size) {}
		ClassTypeObject(const std::string& id, uint64_t size, std::vector<InterfaceTypeObject*>&& interfaces)
			: _id(id), _size(size), _interfaces(std::move(interfaces)) {}

		std::string id() const override { return _id; }

		uint64_t sizeOf() const override { return _size; }

		bool isLight() const override { return true; }

	private:

		friend Context;

		std::string _id;
		uint64_t _size;
		std::vector<InterfaceTypeObject*> _interfaces;
	};

	class PrimitiveTypeObject : public TypeObject {
	public:

		PrimitiveTypeObject(const std::string& id, uint64_t size)
			: _id(id), _size(size) {}

		std::string id() const override { return _id; }

		uint64_t sizeOf() const override { return _size; }

		bool isLight() const override { return false; }

	private:

		std::string _id;
		uint64_t _size;
	};

	class FunctionTypeObject : public TypeObject {
	public:

		struct Parameter {
			bool isConst = false;
			bool isRef = false;
			const TypeObject* type = nullptr;
		};

		FunctionTypeObject(const TypeObject& returnType, std::vector<Parameter>&& parameters)
			: _returnType(&returnType), _parameters(std::move(parameters)) {
			_id = _returnType->id() + "(";
			auto parameterIt = _parameters.begin(), parameterEnd = _parameters.end();
			if (parameterIt != parameterEnd) {
				_id += parameterIt->type->id();
			}
			for (++parameterIt; parameterIt != parameterEnd; ++parameterIt) {
				_id += ", " + parameterIt->type->id();
			}
			_id += ")";
		}

		std::string id() const override { 
			return _id;
		}

		uint64_t sizeOf() const override { return sizeof(FunctionImpl); }

		bool isLight() const override { return true; }

		const std::vector<Parameter>& getParameters() const { return _parameters; };

	private:

		std::string _id;
		const TypeObject* _returnType;
		std::vector<Parameter> _parameters;
	};
}

#endif /*type_h*/