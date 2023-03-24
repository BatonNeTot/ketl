/*🍲Ketl🍲*/
#ifndef type_h
#define type_h

#include "ketl.h"

namespace Ketl {

	class Context;

	class InterfaceTypeObject : public TypeObject {
	public:
		InterfaceTypeObject(const std::string_view& id)
			: _id(id) {}

		std::string id() const override { return _id; }

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

		std::string id() const override { return _id; }

		uint64_t actualSizeOf() const override { return _size; }

		bool isLight() const override { return true; }

		bool doesSupportOverload() const override { return false; };

	private:

		friend Context;

		std::string _id;
		uint64_t _size;
		std::vector<InterfaceTypeObject*> _interfaces;
	};

	class PrimitiveTypeObject : public TypeObject {
	public:

		PrimitiveTypeObject(const std::string_view& id, uint64_t size)
			: _id(id), _size(size) {}

		std::string id() const override { return _id; }

		uint64_t actualSizeOf() const override { return _size; }

		bool isLight() const override { return false; }

		bool doesSupportOverload() const override { return false; };

	private:

		std::string _id;
		uint64_t _size;
	};

	class FunctionTypeObject : public TypeObject {
	public:

		FunctionTypeObject(const TypeObject& returnType, std::vector<Parameter>&& parameters)
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

		std::string id() const override { 
			return _id;
		}

		uint64_t actualSizeOf() const override { return sizeof(FunctionImpl); }

		bool isLight() const override { return true; }

		bool doesSupportOverload() const override { return true; };

		std::pair<uint64_t, AnalyzerVar*> deduceOperatorCall(AnalyzerVar* caller, OperatorCode code, const std::vector<UndeterminedVar>& arguments) const {
			if (arguments.size() != _parameters.size()) {
				return std::make_pair<uint64_t, AnalyzerVar*>(std::numeric_limits<uint64_t>::max(), nullptr);
			}

			// TODO do cast checking and counting
			return std::make_pair(0u, caller);
		};

		const TypeObject* getReturnType() const override { return _returnType; }

		const std::vector<Parameter>& getParameters() const override { return _parameters; };

	private:

		std::string _id;
		const TypeObject* _returnType;
		std::vector<Parameter> _parameters;
	};

	class StructTypeObject : public TypeObject {
	public:

		StructTypeObject(const std::string_view id, std::vector<Field>&& fields, std::vector<StaticField>&& staticFields)
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

		std::string id() const override {
			return _id;
		}

		uint64_t actualSizeOf() const override { 
			return _size; 
		}

	private:
		std::string _id;
		uint64_t _size = 0u;
		std::vector<Field> _fields;
		std::vector<StaticField> _staticFields;
	};
}

#endif /*type_h*/