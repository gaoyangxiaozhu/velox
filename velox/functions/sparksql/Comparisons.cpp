/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "velox/functions/sparksql/LeastGreatest.h"

#include "velox/expression/EvalCtx.h"
#include "velox/expression/Expr.h"
#include "velox/functions/sparksql/Comparisons.h"
#include "velox/type/Type.h"

namespace facebook::velox::functions::sparksql {
namespace {

template <typename Cmp, TypeKind kind>
class ComparisonFunction final : public exec::VectorFunction {
  using T = typename TypeTraits<kind>::NativeType;

  bool isDefaultNullBehavior() const override {
    return true;
  }

  bool supportsFlatNoNullsFastPath() const override {
    return true;
  }

  void apply(
      const SelectivityVector& rows,
      std::vector<VectorPtr>& args,
      const TypePtr& outputType,
      exec::EvalCtx& context,
      VectorPtr& result) const override {
    context.ensureWritable(rows, BOOLEAN(), result);
    auto* flatResult = result->asFlatVector<bool>();
    const Cmp cmp;

    exec::DecodedArgs decodedArgs(rows, args, context);
    auto decodedArg0 = decodedArgs.at(0);
    auto decodedArg1 = decodedArgs.at(1);
    if (decodedArg0->isIdentityMapping() && decodedArg1->isConstantMapping()) {
      rows.applyToSelected([&](auto i) {
        flatResult->set(
            i, cmp(decodedArg0->valueAt<T>(i), decodedArg1->valueAt<T>(0)));
      });
    } else if (
        decodedArg0->isConstantMapping() && decodedArg1->isIdentityMapping()) {
      rows.applyToSelected([&](auto i) {
        flatResult->set(
            i, cmp(decodedArg0->valueAt<T>(0), decodedArg1->valueAt<T>(i)));
      });
    } else {
      rows.applyToSelected([&](auto i) {
        flatResult->set(
            i, cmp(decodedArg0->valueAt<T>(i), decodedArg1->valueAt<T>(i)));
      });
    }
  }
};

template <template <typename> class Cmp>
std::shared_ptr<exec::VectorFunction> makeImpl(
    const std::string& functionName,
    const std::vector<exec::VectorFunctionArg>& args) {
  VELOX_CHECK_EQ(args.size(), 2);
  for (size_t i = 1; i < args.size(); i++) {
    VELOX_CHECK(*args[i].type == *args[0].type);
  }
  switch (args[0].type->kind()) {
#define SCALAR_CASE(kind)                            \
  case TypeKind::kind:                               \
    return std::make_shared<ComparisonFunction<      \
        Cmp<TypeTraits<TypeKind::kind>::NativeType>, \
        TypeKind::kind>>();
    SCALAR_CASE(BOOLEAN)
    SCALAR_CASE(TINYINT)
    SCALAR_CASE(SMALLINT)
    SCALAR_CASE(INTEGER)
    SCALAR_CASE(BIGINT)
    SCALAR_CASE(HUGEINT)
    SCALAR_CASE(REAL)
    SCALAR_CASE(DOUBLE)
    SCALAR_CASE(VARCHAR)
    SCALAR_CASE(VARBINARY)
    SCALAR_CASE(TIMESTAMP)
    SCALAR_CASE(DATE)
#undef SCALAR_CASE
    default:
      VELOX_NYI(
          "{} does not support arguments of type {}",
          functionName,
          args[0].type->kind());
  }
}

} // namespace

std::shared_ptr<exec::VectorFunction> makeEqualTo(
    const std::string& functionName,
    const std::vector<exec::VectorFunctionArg>& args) {
  return makeImpl<Equal>(functionName, args);
}

std::shared_ptr<exec::VectorFunction> makeLessThan(
    const std::string& functionName,
    const std::vector<exec::VectorFunctionArg>& args) {
  return makeImpl<Less>(functionName, args);
}

std::shared_ptr<exec::VectorFunction> makeGreaterThan(
    const std::string& functionName,
    const std::vector<exec::VectorFunctionArg>& args) {
  return makeImpl<Greater>(functionName, args);
}

std::shared_ptr<exec::VectorFunction> makeLessThanOrEqual(
    const std::string& functionName,
    const std::vector<exec::VectorFunctionArg>& args) {
  return makeImpl<LessOrEqual>(functionName, args);
}

std::shared_ptr<exec::VectorFunction> makeGreaterThanOrEqual(
    const std::string& functionName,
    const std::vector<exec::VectorFunctionArg>& args) {
  return makeImpl<GreaterOrEqual>(functionName, args);
}
} // namespace facebook::velox::functions::sparksql
