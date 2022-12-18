/*🍲Ketl🍲*/
#include "ketl.h"

#include "functions/unary_op.h"
#include "functions/binary_op.h"

namespace Ketl {

	const Type Type::Void;

	Environment::Environment() {
		registerFunction<Ketl::AddFunction<int64_t>>("operator +");
		registerFunction<Ketl::SubFunction<int64_t>>("operator -");
		registerFunction<Ketl::MultiFunction<int64_t>>("operator *");
		registerFunction<Ketl::DivideFunction<int64_t>>("operator /");

		registerFunction<Ketl::AddFunction<double>>("operator +");
		registerFunction<Ketl::SubFunction<double>>("operator -");
		registerFunction<Ketl::MultiFunction<double>>("operator *");
		registerFunction<Ketl::DivideFunction<double>>("operator /");

		registerCast<int64_t, double>();

		registerFunction<Ketl::AddFunction<int64_t>>("add");
	}

}