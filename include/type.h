/*🍲Ketl🍲*/
#ifndef type_h
#define type_h

#include "function.h"
#include "common.h"

#include <vector>

namespace Ketl {

	static const std::vector<VarTraits> emptyParameters;

	class TypeObject {
	public:
		TypeObject() = default;
		virtual ~TypeObject() = default;

		virtual const std::string& id() const = 0;

		virtual uint64_t actualSizeOf() const = 0;

		virtual bool isLight() const { return false; }

		uint64_t sizeOf() const { return isLight() ? sizeof(void*) : actualSizeOf(); }

		virtual const TypeObject* getReturnType() const { return nullptr; }

		virtual const std::vector<VarTraits>& getParameters() const {
			return emptyParameters;
		};

		struct Field {
			Field(const std::string_view& id_, const TypeObject* type_)
				: type(type_), id(id_) {}

			const TypeObject* type = nullptr;
			uint64_t offset = 0u;
			std::string id;
		};

		struct StaticField {
			// TODO replace with TypedPtr?
			const TypeObject* type = nullptr;
			void* ptr;
			std::string id;
		};

		virtual bool doesSupportOverload() const { return false; }

		virtual const std::vector<TypedPtr>& getCallFunctions() const {
			static std::vector<TypedPtr> empty;
			return empty;
		};

		friend bool operator==(const TypeObject& lhs, const TypeObject& rhs) {
			return lhs.id() == rhs.id();
		}

	};

	class InterfaceTypeObject : public TypeObject {
	public:
		InterfaceTypeObject(const std::string_view& id)
			: _id(id) {}

		const std::string& id() const override { return _id; }

		uint64_t actualSizeOf() const override { return 0; }

		bool isLight() const override { return true; }

		bool doesSupportOverload() const override { return false; };

	private:

		std::string _id;
	};

	class ClassTypeObject : public TypeObject {
	public:
		ClassTypeObject(const std::string_view& id, uint64_t size)
			: _id(id), _size(size) {}
		ClassTypeObject(const std::string_view& id, uint64_t size, std::vector<InterfaceTypeObject*>&& interfaces)
			: _id(id), _size(size), _interfaces(std::move(interfaces)) {}

		const std::string& id() const override { return _id; }

		uint64_t actualSizeOf() const override { return _size; }

		bool isLight() const override { return true; }

		bool doesSupportOverload() const override { return false; };

		void addInterface(InterfaceTypeObject* interface) {
			_interfaces.emplace_back(interface);
		}

	private:

		std::string _id;
		uint64_t _size;
		std::vector<InterfaceTypeObject*> _interfaces;
	};

	class PrimitiveTypeObject : public TypeObject {
	public:

		PrimitiveTypeObject(const std::string_view& id, uint64_t size)
			: _id(id), _size(size) {}

		const std::string& id() const override { return _id; }

		uint64_t actualSizeOf() const override { return _size; }

		bool isLight() const override { return false; }

		bool doesSupportOverload() const override { return false; };

	private:

		std::string _id;
		uint64_t _size;
	};

	class FunctionTypeObject : public TypeObject {
	public:

		FunctionTypeObject(const TypeObject& returnType, std::vector<VarTraits>&& parameters)
			: _returnType(&returnType), _parameters(std::move(parameters)) {
			_id = _returnType->id() + "(";
			auto parameterIt = _parameters.begin(), parameterEnd = _parameters.end();
			if (parameterIt != parameterEnd) {
				_id += parameterIt->type->id();
				for (++parameterIt; parameterIt != parameterEnd; ++parameterIt) {
					_id += ", " + parameterIt->type->id();
				}
			}
			_id += ")";
		}

		const std::string& id() const override { return _id; }

		uint64_t actualSizeOf() const override { return sizeof(FunctionObject); }

		bool isLight() const override { return true; }

		bool doesSupportOverload() const override { return true; };

		const TypeObject* getReturnType() const override { return _returnType; }

		const std::vector<VarTraits>& getParameters() const override { return _parameters; };

	private:

		std::string _id;
		const TypeObject* _returnType;
		std::vector<VarTraits> _parameters;
	};

	class StructTypeObject : public TypeObject {
	public:

		StructTypeObject(const std::string_view& id, std::vector<Field>&& fields, std::vector<StaticField>&& staticFields)
			: _id(id), _fields(std::move(fields)), _staticFields(std::move(staticFields)) {
			// calculate size and fields offsets
			uint64_t currentOffset = 0u;
			for (auto& field : _fields) {
				// TODO align
				currentOffset = currentOffset;
				field.offset = currentOffset;
				currentOffset += field.type->sizeOf();
			}
			// TODO align
			currentOffset = currentOffset;
			_size = currentOffset;
		}

		const std::string& id() const override { return _id; }

		uint64_t actualSizeOf() const override { return _size; }

	private:
		std::string _id;
		uint64_t _size = 0u;
		std::vector<Field> _fields;
		std::vector<StaticField> _staticFields;
	};
}

#endif /*type_h*/