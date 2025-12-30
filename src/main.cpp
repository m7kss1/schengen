#include <arrow/api.h>

#include <yaclib/async/run.hpp>
#include <yaclib/runtime/fair_thread_pool.hpp>

#include <cstdlib>
#include <iostream>
#include <memory>

namespace {

arrow::Result<std::shared_ptr<arrow::Int64Array>> MakeSampleArray() {
  arrow::Int64Builder builder;
  for (std::int64_t value = 0; value < 5; ++value) {
    ARROW_RETURN_NOT_OK(builder.Append(value));
  }

  std::shared_ptr<arrow::Array> array;
  ARROW_RETURN_NOT_OK(builder.Finish(&array));
  return std::static_pointer_cast<arrow::Int64Array>(array);
}

}  // namespace

int main() {
  auto maybe_array = MakeSampleArray();
  if (!maybe_array.ok()) {
    std::cerr << "Failed to build Arrow Int64Array: " << maybe_array.status().ToString() << '\n';
    return EXIT_FAILURE;
  }

  auto array = maybe_array.ValueOrDie();

  yaclib::FairThreadPool thread_pool{/*threads=*/2};
  auto future = yaclib::Run(thread_pool, [array] {
    std::int64_t sum = 0;
    for (std::int64_t i = 0; i < array->length(); ++i) {
      if (!array->IsNull(i)) {
        sum += array->Value(i);
      }
    }
    return sum;
  });

  auto result = std::move(future).Get();
  thread_pool.Stop();
  thread_pool.Wait();

  if (!result) {
    std::cerr << "YACLib computation failed\n";
    return EXIT_FAILURE;
  }

  const auto sum = std::move(result).Ok();
  std::cout << "Sum computed via Arrow+YACLib: " << sum << '\n';
  return EXIT_SUCCESS;
}
