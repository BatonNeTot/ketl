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

		std::string _id;
		uint64_t _size;
	};
}

#endif /*type_h*/