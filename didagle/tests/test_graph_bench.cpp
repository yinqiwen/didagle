// Copyright (c) 2024, Tencent Inc.
// All rights reserved.

#include <benchmark/benchmark.h>
#include <fmt/core.h>
#include <stdint.h>
#include <string>
#include "didagle/processor/api.h"
#include "didagle/tests/test_common.h"
using namespace didagle;

GRAPH_OP_BEGIN(test0)
GRAPH_OP_OUTPUT(std::string, test0_a)
GRAPH_OP_OUTPUT(int, test0_b)
int OnSetup(const Params& args) override { return 0; }
int OnExecute(const Params& args) override {
  test0_a = "test0";
  test0_b = 101;
  return 0;
}
GRAPH_OP_END

GRAPH_OP_BEGIN(test1)
GRAPH_OP_INPUT(std::string, test0_a)
GRAPH_OP_OUTPUT(std::string, test1)
int OnSetup(const Params& args) override { return 0; }
int OnExecute(const Params& args) override {
  if (nullptr != test0_a) {
    test1.append(*test0_a);
  }
  test1.append("#").append("test1");
  return 0;
}
GRAPH_OP_END

GRAPH_OP_BEGIN(test2)
GRAPH_OP_INPUT(std::string, test1)
GRAPH_OP_OUTPUT(std::string, test2)
int OnSetup(const Params& args) override { return 0; }
int OnExecute(const Params& args) override {
  if (nullptr != test1) {
    test2.append(*test1);
  }
  test2.append("#").append("test2");
  return 0;
}
GRAPH_OP_END

GRAPH_OP_BEGIN(test3)
GRAPH_OP_INPUT(std::string, test2)
GRAPH_OP_OUTPUT(std::string, test3)
int OnSetup(const Params& args) override { return 0; }
int OnExecute(const Params& args) override {
  if (nullptr != test2) {
    test3.append(*test2);
  }
  test3.append("#").append("test3");
  return 0;
}
GRAPH_OP_END

GRAPH_OP_BEGIN(test4)
GRAPH_OP_INPUT(std::string, test3)
GRAPH_OP_OUTPUT(std::string, test4)
int OnSetup(const Params& args) override { return 0; }
int OnExecute(const Params& args) override {
  if (nullptr != test3) {
    test4.append(*test3);
  }
  test4.append("#").append("test4");
  return 0;
}
GRAPH_OP_END

static void BM_test_graph_run(benchmark::State& state) {
  TestContext ctx;
  std::string content = R"(
name = "test"
strict_dsl = true

[[graph]]
name = "test"
[[graph.vertex]]
processor = "test0"
start=true
[[graph.vertex]]
processor = "test1"
[[graph.vertex]]
processor = "test2"
[[graph.vertex]]
processor = "test3"
[[graph.vertex]]
processor = "test4"
  )";
  auto handle = ctx.store->LoadString(content);
  for (auto _ : state) {
    auto data_ctx = GraphDataContext::New();
    data_ctx->EnableEventTracker();
    ctx.store->SyncExecute(data_ctx, "test", "test");
  }
}
// Register the function as a benchmark
BENCHMARK(BM_test_graph_run);
// Run the benchmark
BENCHMARK_MAIN();