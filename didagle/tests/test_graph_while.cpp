// Copyright (c) 2024, Tencent Inc.
// All rights reserved.

#include <fmt/core.h>
#include <gtest/gtest.h>
#include <stdint.h>
#include <string>
#include <vector>

#include "didagle/graph/params.h"
#include "didagle/processor/api.h"
#include "didagle/tests/test_common.h"
using namespace didagle;

GRAPH_OP_BEGIN(test0)
GRAPH_OP_OUTPUT(std::string, test0)
int OnExecute(const Params& args) override {
  test0 = "test0";
  return 0;
}
GRAPH_OP_END

GRAPH_OP_BEGIN(test1)
GRAPH_OP_OUTPUT(std::string, test1)
GRAPH_OP_OUTPUT(int, loop_count)
int OnExecute(const Params& args) override {
  test1 = "test1";
  return loop_count++ < 5 ? 0 : -1;
}
GRAPH_OP_END

GRAPH_OP_BEGIN(test2)
GRAPH_OP_OUTPUT(std::string, test2)
GRAPH_OP_OUTPUT(int, test2_loop_count)
int OnExecute(const Params& args) override {
  test2.append("test2");
  test2_loop_count++;
  return 0;
}
GRAPH_OP_END

TEST(While, simple) {
  std::string content = R"(
name="test"
[[graph]]
name="test"
gen_while_subgraph=true
[[graph.vertex]]
processor = "test0"
start=true
[[graph.vertex]]
while = "test1"
processor="test2"
deps=["test0"]
  )";
  TestContext ctx;
  auto handle = ctx.store->LoadString(content);
  ASSERT_TRUE(handle != nullptr);

  ParamsPtr paras_str = Params::New();
  (*paras_str)["exp"]["id"].SetInt(1001);

  for (int i = 0; i < 10; i++) {
    auto data_ctx = GraphDataContext::New();
    int rc = ctx.store->SyncExecute(data_ctx, "test", "test", paras_str);
    ASSERT_EQ(rc, 0);

    auto test2 = data_ctx->Get<std::string>("test2");
    ASSERT_TRUE(test2 != nullptr);
    ASSERT_EQ(*test2, "test2test2test2test2test2");
  }
}

// TEST(While, expr) {
//   std::string content = R"(
// name="test"
// default_expr_processor="didagle_expr"
// [[graph]]
// name="test"
// gen_while_subgraph=true
// [[graph.vertex]]
// processor = "test0"
// start=true
// [[graph.vertex]]
// while = "$LOOP_IDX<5"
// processor="test2"
// deps=["test0"]
//   )";
//   TestContext ctx;
//   auto handle = ctx.store->LoadString(content);
//   for (int i = 0; i < 10; i++) {
//     auto data_ctx = GraphDataContext::New();
//     int rc = ctx.store->SyncExecute(data_ctx, "test", "test");
//     ASSERT_EQ(rc, 0);

//     auto test2 = data_ctx->Get<std::string>("test2");
//     ASSERT_TRUE(test2 != nullptr);
//     ASSERT_EQ(*test2, "test2test2test2test2test2");
//   }
// }
