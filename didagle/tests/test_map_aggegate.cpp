// Copyright (c) 2024, Tencent Inc.
// All rights reserved.

#include <fmt/core.h>
#include <gtest/gtest.h>
#include <stdint.h>
#include <string>
#include <vector>

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
int OnExecute(const Params& args) override {
  test1 = "test1";
  return 0;
}
GRAPH_OP_END

GRAPH_OP_BEGIN(test2)
GRAPH_OP_MAP_INPUT(std::string, inputs)
GRAPH_OP_OUTPUT(int, int_result)
GRAPH_OP_OUTPUT(std::string, str_result)
int OnExecute(const Params& args) override {
  int_result = inputs.size();
  for (auto [_, v] : inputs) {
    str_result.append(*v).append("#");
  }
  return 0;
}
GRAPH_OP_END

TEST(MapAggegate, simple) {
  std::string content = R"(
name="test"
[[graph]]
name="test"
[[graph.vertex]]
processor = "test0"
[[graph.vertex]]
processor = "test1"
[[graph.vertex]]
id="test11"
processor = "test1"
output = [{ field = "test1", id = "test11" }]
[[graph.vertex]]
processor = "test2"
input = [{ field = "inputs", aggregate = ["test0", "test1","test11"] }] 
  )";
  TestContext ctx;
  auto handle = ctx.store->LoadString(content);
  ASSERT_TRUE(handle != nullptr);
  auto data_ctx = GraphDataContext::New();
  int rc = ctx.store->SyncExecute(data_ctx, "test", "test");
  ASSERT_EQ(rc, 0);

  auto test0 = data_ctx->Get<std::string>("test0");
  ASSERT_TRUE(test0 != nullptr);
  ASSERT_EQ(*test0, "test0");
  auto test1 = data_ctx->Get<std::string>("test1");
  ASSERT_TRUE(test1 != nullptr);
  ASSERT_EQ(*test1, "test1");
  auto test11 = data_ctx->Get<std::string>("test11");
  ASSERT_TRUE(test11 != nullptr);
  ASSERT_EQ(*test11, "test1");

  auto int_result = data_ctx->Get<int>("int_result");
  ASSERT_TRUE(int_result != nullptr);
  ASSERT_EQ(*int_result, 3);
  auto str_result = data_ctx->Get<std::string>("str_result");
  ASSERT_TRUE(str_result != nullptr);
  ASSERT_TRUE(str_result->find("test0") != std::string::npos);
  ASSERT_TRUE(str_result->find("test1") != std::string::npos);
}
