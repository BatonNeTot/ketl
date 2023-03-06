/*🍲Ketl🍲*/
#ifndef type_h
#define type_h

#include "ketl.h"

namespace Ketl {

	class Context;

	class InterfaceTypeObject : public TypeObject {
	public:
		InterfaceTypeObject(const std::string& id)
			: _id(id) {};
		virtual ~InterfaceTypeObject() = default;

		virtual std::string id() const { return _id; }

		virtual uint64_t sizeOf() const { return 0; }

		virtual bool isLight() const { return true; }

	private:

		void* rootPtr() override { return this; }

		std::string _id;
	};

	class ClassTypeObject : public TypeObject {
	public:
		ClassTypeObject(const std::string& id, uint64_t size)
			: _id(id), _size(size) {};
		ClassTypeObject(const std::string& id, uint64_t size, std::vector<InterfaceTypeObject*>&& interfaces)
			: _id(id), _size(size), _interfaces(std::move(interfaces)) {};
		virtual ~ClassTypeObject() = default;

		virtual std::string id() const { return _id; }

		virtual uint64_t sizeOf() const { return _size; }

		virtual bool isLight() const { return true; }

	private:

		friend Context;

		void* rootPtr() override { return this; }

		std::string _id;
		uint64_t _size;
		std::vector<InterfaceTypeObject*> _interfaces;
	};

	class PrimitiveTypeObject : public TypeObject {
	public:

		PrimitiveTypeObject(const std::string& id, uint64_t size)
			: _id(id), _size(size) {};
		virtual ~PrimitiveTypeObject() = default;

		virtual std::string id() const { return _id; }

		virtual uint64_t sizeOf() const { return _size; }

		virtual bool isLight() const { return false; }

	private:

		void* rootPtr() override { return this; }

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
			registerLink(&_returnType);
			for (auto& parameter : _parameters) {
				registerLink(&parameter.type);
			}
		};
		virtual ~FunctionTypeObject() = default;

		virtual std::string id() const { 
			// TODO calculate it once and cache
			auto idStr = _returnType->id() + "(";
			auto parameterIt = _parameters.begin(), parameterEnd = _parameters.end();
			if (parameterIt != parameterEnd) {
				idStr += parameterIt->type->id();
			}
			for (++parameterIt; parameterIt != parameterEnd; ++parameterIt) {
				idStr += ", " + parameterIt->type->id();
			}
			idStr += ")";
			return idStr;
		}

		virtual uint64_t sizeOf() const { return sizeof(FunctionImpl); }

		virtual bool isLight() const { return true; }

	private:

		void* rootPtr() override { return this; }

		const TypeObject* _returnType;
		std::vector<Parameter> _parameters;
	};
}

#endif /*type_h*/