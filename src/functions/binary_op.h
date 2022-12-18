/*🍲Ketl🍲*/
#ifndef functions_binary_op_h
#define functions_binary_op_h

#include "ketl.h"

namespace Ketl {

	template <class T>
	class BinaryFunction : public FunctionTypeless {
	public:
		Type type() const override {
			Ketl::Type mType;

			mType.result = std::make_unique<Ketl::Type>();
			mType.result->baseType = Type::baseTypeTemplate<T>();
			mType.args.emplace_back().baseType = Type::baseTypeTemplate<T>();
			mType.args.emplace_back().baseType = Type::baseTypeTemplate<T>();

			return mType;
		}
	};

	template <class T>
	class AddFunction : public BinaryFunction<T> {
	public:
		void call(ValueProvider& result, ValueProvider** argv, uint32_t argc) const override {
			result.get().get<T>() = argv[0]->get().get<T>() + argv[1]->get().get<T>();
		}
	};

	template <class T>
	class SubFunction : public BinaryFunction<T> {
	public:
		void call(ValueProvider& result, ValueProvider** argv, uint32_t argc) const override {
			result.get().get<T>() = argv[0]->get().get<T>() - argv[1]->get().get<T>();
		}
	};

	template <class T>
	class MultiFunction : public BinaryFunction<T> {
	public:
		void call(ValueProvider& result, ValueProvider** argv, uint32_t argc) const override {
			result.get().get<T>() = argv[0]->get().get<T>() * argv[1]->get().get<T>();
		}
	};

	template <class T>
	class DivideFunction : public BinaryFunction<T> {
	public:
		void call(ValueProvider& result, ValueProvider** argv, uint32_t argc) const override {
			result.get().get<T>() = argv[0]->get().get<T>() / argv[1]->get().get<T>();
		}
	};

}

#endif /*functions_binary_op_h*/