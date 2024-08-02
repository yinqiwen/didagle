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
GRAPH_OP_OUTPUT(std::string, test0_a)
GRAPH_OP_OUTPUT(int, test0_b)
int OnExecute(const Params& args) override {
  test0_a = "test0";
  test0_b = 101;
  return 0;
}
GRAPH_OP_END

GRAPH_OP_BEGIN(test1)
GRAPH_OP_INPUT(std::string, test0_a)
GRAPH_OP_OUTPUT(std::string, test1)
int OnExecute(const Params& args) override {
  if (nullptr != test0_a) {
    test1.append(*test0_a);
  }
  test1.append("#").append("test1");
  return 0;
}
GRAPH_OP_END

GRAPH_OP_BEGIN(test2)
GRAPH_OP_INPUT(int, test0_b)
GRAPH_OP_OUTPUT(int, test2)
int OnExecute(const Params& args) override {
  if (nullptr != test0_b) {
    test2 = *test0_b + 100;
  } else {
    test2 = -1;
  }
  return 0;
}
GRAPH_OP_END

GRAPH_OP_BEGIN(test3)
GRAPH_OP_INPUT(std::string, test1)
GRAPH_OP_INPUT(int, test2)
GRAPH_OP_EXTERN_INPUT(int, extern_in);
GRAPH_OP_OUTPUT(std::string, test3_a)
GRAPH_OP_OUTPUT(int, test3_b)
GRAPH_OP_OUTPUT(int, test3_c)
int OnExecute(const Params& args) override {
  if (nullptr != test1) {
    test3_a.append(*test1);
  }
  test3_a.append("#").append("test3");
  if (nullptr != test2) {
    test3_b = *test2 + 100;
  } else {
    test3_b = -1;
  }
  if (nullptr != extern_in) {
    test3_c = *extern_in + 100;
  } else {
    test3_c = 502;
  }
  return 0;
}
GRAPH_OP_END

TEST(EarlyExitGraph, simple) {
  std::string content = R"(
name="test"
[[graph]]
name="test"
[[graph.vertex]]
processor = "test0"
[[graph.vertex]]
processor = "test1"
[[graph.vertex]]
processor = "test2"
[[graph.vertex]]
processor = "test3"
  )";
  TestContext ctx;
  auto handle = ctx.store->LoadString(content);
  ASSERT_TRUE(handle != nullptr);
  auto data_ctx = GraphDataContext::New();
  int rc = ctx.store->SyncExecute(data_ctx, "test", "test");
  ASSERT_EQ(rc, 0);

  auto test0_b = data_ctx->Get<int>("test0_b");
  ASSERT_TRUE(test0_b != nullptr);
  ASSERT_EQ(*test0_b, 101);

  auto test3_b = data_ctx->Get<int>("test3_b");
  ASSERT_TRUE(test3_b != nullptr);
  ASSERT_EQ(*test3_b, 301);

  auto test3_a = data_ctx->Get<std::string>("test3_a");
  ASSERT_TRUE(test3_a != nullptr);
  ASSERT_EQ(*test3_a, "test0#test1#test3");
}