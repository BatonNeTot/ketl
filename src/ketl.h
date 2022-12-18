/*🍲Ketl🍲*/
#ifndef eel_h
#define eel_h

#include <cinttypes>
#include <string>
#include <unordered_map>
#include <memory>
#include <optional>

namespace Ketl {

	class FunctionTypeless;

	class ValueTypeless {
	public:
		ValueTypeless() {
			pointer = nullptr;
		}

		union {
			uint64_t uinteger;
			int64_t integer;
			double floating;
			FunctionTypeless* function;
			void* pointer;
		};

		template <class T>
		T& get() = delete;

		template <>
		int64_t& get() {
			return integer;
		}

		template <>
		uint64_t& get() {
			return uinteger;
		}

		template <>
		double& get() {
			return floating;
		}
	};

	using StackOffset = uint32_t;

	class Stack {
	public:

		ValueTypeless& getValue(StackOffset offset) {
			return _stack[offset];
		}

	private:

		ValueTypeless _stack[128];
	};

	struct Type {
		enum class BaseType : uint8_t {
			Void,
			Int64,
			Float64
		};

		Type() = default;
		Type(BaseType baseType_)
			: baseType(baseType_) {}
		Type(const Type& type)
			: baseType(type.baseType)
			, args(type.args) {
			if (type.result) {
				result = std::make_unique<Type>(*type.result);
			}
		}
		Type(const Type&& type) noexcept
			: baseType(type.baseType)
			, args(type.args) {
			if (type.result) {
				result = std::make_unique<Type>(*type.result);
			}
		}

		Type& operator =(const Type& type) {
			baseType = type.baseType;
			if (type.result) {
				result = std::make_unique<Type>(*type.result);
			}
			args = type.args;
			return *this;
		}

		BaseType baseType = BaseType::Void;
		std::unique_ptr<Type> result;
		std::vector<Type> args;

		inline bool isFunction() const {
			return result.operator bool();
		}

		template <class T>
		static BaseType baseTypeTemplate() = delete;

		template <>
		static BaseType baseTypeTemplate<int64_t>() { return BaseType::Int64; }
		template <>
		static BaseType baseTypeTemplate<double>() { return BaseType::Float64; }

		template <class T>
		static const Type& typeTemplate() = delete;

		template <>
		static const Type& typeTemplate<int64_t>() { 
			static Type type(BaseType::Int64);
			return type; 
		}
		template <>
		static const Type& typeTemplate<double>() {
			static Type type(BaseType::Float64);
			return type;
		}

		static const Type Void;

		friend bool operator ==(const Type& lhs, const Type& rhs) {
			return
				lhs.isFunction() == rhs.isFunction() &&
				lhs.baseType == rhs.baseType &&
				(!lhs.isFunction() || (lhs.args == rhs.args));
		}

		std::string toStr() const {
			if (isFunction()) {
				std::string str;
				if (result->isFunction()) {
					str = "(" + result->toStr() + ")(";
				}
				else {
					str = result->toStr() + "(";
				}
				for (auto i = 0; i < args.size(); ++i) {
					if (i != 0) {
						str += ", ";
					}
					str += args[i].toStr();
				}
				str += ")";
				return str;
			}
			else {
				switch (baseType) {
				case BaseType::Void: return "void";
				case BaseType::Int64: return "int";
				case BaseType::Float64: return "double";
				}
				return "void";
			}
		}

		std::string castTargetStr() const {
			std::string castFuncName = toStr();
			if (isFunction()) {
				castFuncName = "(" + castFuncName + ")";
			}
			return "operator " + castFuncName;
		}
	};

}
	template <>
	struct ::std::hash<Ketl::Type> {
		std::size_t operator()(const Ketl::Type& type) const {
			size_t hash = type.isFunction();
			hash = (hash << 1) ^ static_cast<size_t>(type.baseType);

			const auto hasher = std::hash<Ketl::Type>();
			if (type.isFunction()) {
				hash = (hash << 1) ^ hasher(*type.result);
				for (const auto& arg : type.args) {
					hash = (hash << 1) ^ hasher(arg);
				}
			}
			return hash;
		}
	};

namespace Ketl {
	class ValueProvider {
	public:
		virtual ~ValueProvider() = default;

		virtual ValueTypeless& get() = 0;
	};

	class LiteralValueProvider : public ValueProvider {
	public:
		LiteralValueProvider(int32_t value) {
			_value.integer = value;
		}
		LiteralValueProvider(int64_t value) {
			_value.integer = value;
		}
		LiteralValueProvider(uint32_t value) {
			_value.uinteger = value;
		}
		LiteralValueProvider(uint64_t value) {
			_value.uinteger = value;
		}
		LiteralValueProvider(float value) {
			_value.floating = value;
		}
		LiteralValueProvider(double value) {
			_value.floating = value;
		}

		ValueTypeless& get() override {
			return _value;
		}
	private:
		ValueTypeless _value;
	};

	struct StackHolder {

		void updateStack(Stack& stack, StackOffset offset) {
			_stack = &stack;
			_offset = offset;
		}

		ValueTypeless& get(StackOffset offset) {
			return _stack->getValue(_offset + offset);
		}

	private:

		Stack* _stack = nullptr;
		StackOffset _offset = 0;
	};

	class StackValueProvider : public ValueProvider {
	public:

		explicit StackValueProvider(StackHolder& holder, StackOffset offset)
			: _holder(holder), _offset(offset) {}

		ValueTypeless& get() override {
			return _holder.get(_offset);
		}

	private:

		StackHolder& _holder;
		StackOffset _offset;
	};

	class FunctionTypeless {
	public:

		virtual void call(ValueProvider& result, ValueProvider** argv, uint32_t argc) const = 0;

		virtual Type type() const = 0;

	};

	template <class S, class T>
	class CastFunction : public FunctionTypeless {
	public:
		void call(ValueProvider& result, ValueProvider** argv, uint32_t argc) const override {
			result.get().get<T>() = static_cast<T>(argv[0]->get().get<S>());
		}

		Type type() const override {
			Ketl::Type mType;

			mType.result = std::make_unique<Ketl::Type>();
			mType.result->baseType = Type::baseTypeTemplate<T>();
			mType.args.emplace_back().baseType = Type::baseTypeTemplate<S>();

			return mType;
		}
	};

	class Environment {
	public:

		Environment();

		Stack& stack() {
			return _stack;
		}

		struct Variable {
			ValueTypeless value;
			Type type;

			Variable(const Type& type_)
				: type(type_) {}
		};

		Variable& registerGlobal(const std::string& key, const Type& type) {
			auto it = _global.find(key);
			if (it != _global.end()) {
				if (it->second.front().type.isFunction() && type.isFunction()) {
					auto& variable = it->second.emplace_back(type);
					return variable;
				}
				return it->second.front();
			}
			return _global.try_emplace(key).first->second.emplace_back(type);
		}

		ValueTypeless& getGlobalValue(const std::string& key) {
			auto it = _global.find(key);
			return it->second.front().value;
		}

		ValueTypeless& getGlobalFunc(const std::string& key, const std::vector<Type>& args) {
			auto it = _global.find(key);
			for (auto& variable : it->second) {
				if (variable.type.args == args) {
					return variable.value;
				}
			}
			return it->second.front().value;
		}

		const Type& getGlobalType(const std::string& key) const {
			auto it = _global.find(key);
			return it == _global.end() ? Type::Void : (it->second.empty() ? Type::Void : it->second.front().type);
		}

		const Type& estimateFunctionByDecl(const std::string& key, const std::vector<Type>& args) const {
			auto it = _global.find(key);
			if (it == _global.end()) {
				return Type::Void;
			}

			std::vector<uint32_t> castCounts;
			castCounts.reserve(it->second.size());
			for (auto& variable : it->second) {
				auto& counts = castCounts.emplace_back(0);
				if (args.size() != variable.type.args.size()) {
					counts = std::numeric_limits<uint32_t>::max();
					continue;
				}

				for (auto i = 0u; i < variable.type.args.size(); ++i) {
					if (variable.type.args[i] != args[i]) {
						if (hasCast(args[i], variable.type.args[i])) {
							++counts;
						}
						else {
							counts = std::numeric_limits<uint32_t>::max();
							break;
						}
					}
				}
			}

			uint32_t index = std::numeric_limits<uint32_t>::max();
			uint32_t min = std::numeric_limits<uint32_t>::max();
			for (uint32_t i = 0u; i < castCounts.size(); ++i) {
				if (castCounts[i] < min) {
					min = castCounts[i];
					index = i;
				}
			}

			if (index == std::numeric_limits<uint32_t>::max()) {
				return Type::Void;
			}

			return it->second[index].type;
		}

		bool hasCast(const Type& origin, const Type& target) const {
			auto it = _casts.find(origin);
			return it != _casts.end() && std::find(it->second.begin(), it->second.end(), target) != it->second.end();
		}

		template <class T, class... Args>
		void registerFunction(const std::string& key, Args&&... args) {
			auto& function = _functions.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));
			registerGlobal(key, function->type()).value.function = function.get();
		}

		template <class S, class T>
		void registerCast() {
			auto& function = _functions.emplace_back(std::make_unique<CastFunction<S, T>>());
			auto& variable = registerGlobal(Type::typeTemplate<T>().castTargetStr(), function->type());
			variable.value.function = function.get();
			_casts.try_emplace(Type::typeTemplate<S>()).first->second.emplace_back(Type::typeTemplate<T>());
		}

	private:

		Stack _stack;

		std::unordered_map < std::string, std::vector<Variable>> _global;

		std::unordered_map<Type, std::vector<Type>> _casts;

		std::vector<std::unique_ptr<FunctionTypeless>> _functions;
	};

	struct Command {
		Ketl::ValueProvider* _func;
		Ketl::ValueProvider* _result;
		std::vector<Ketl::ValueProvider*> _args;

		void call() {
			auto* function = _func->get().function;
			function->call(*_result, _args.data(), static_cast<uint32_t>(_args.size()));
		}
	};

	class GlobalValueProvider : public ValueProvider {
	public:

		explicit GlobalValueProvider(Environment& env, const std::string& key)
			: _value(&env.getGlobalValue(key)) {}

		explicit GlobalValueProvider(Environment& env, const std::string& key, const std::vector<Type>& args)
			: _value(&env.getGlobalFunc(key, args)) {}

		ValueTypeless& get() override {
			return *_value;
		}

	private:
		ValueTypeless* _value = nullptr;
	};

	struct Block {
		Environment& _env;
		StackHolder holder;
		std::list<Ketl::GlobalValueProvider> _globalProviders;
		std::unordered_map<StackOffset, Ketl::StackValueProvider> _stackProviders;
		std::list<Ketl::LiteralValueProvider> _literalProviders;
		std::vector<Command> _commands;

		Block(Environment& env)
			: _env(env) {}

		void call(Stack& stack, StackOffset offset) {
			holder.updateStack(stack, offset);
			for (auto i = 0u; i < _commands.size(); ++i) {
				_commands[i].call();
			}
		}

		void call() {
			call(_env.stack(), 0);
		}
	};
}

#endif /*eel_h*/