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
GRAPH_OP_INPUT(std::string, test0)
GRAPH_OP_OUTPUT(std::string, test1)
int OnExecute(const Params& args) override {
  test1 = "test1";
  return 0;
}
GRAPH_OP_END

GRAPH_OP_BEGIN(test2)
GRAPH_OP_INPUT(std::string, test0)
GRAPH_OP_OUTPUT(std::string, test2)
int OnExecute(const Params& args) override {
  test2 = "test2";
  return 0;
}
GRAPH_OP_END

GRAPH_OP_BEGIN(test3)
GRAPH_OP_INPUT(std::string, test2)
GRAPH_OP_OUTPUT(std::string, test3)
int OnExecute(const Params& args) override {
  test3 = "test3";
  return 0;
}
GRAPH_OP_END

TEST(ExpectConfig, simple) {
  std::string content = R"(
name="test"
default_expr_processor="didagle_expr"
[[config_setting]]
name = "with_exp_1000"
cond = "$exp.id==1000"
[[graph]]
name="test"
vertex_skip_as_error=false
[[graph.vertex]]
expect_config="with_exp_1000"
processor = "test0"
if=["test1"]
else=["test2"]
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

  ParamsPtr paras_str = Params::New();
  (*paras_str)["exp"]["id"].SetInt(1001);
  int rc = ctx.store->SyncExecute(data_ctx, "test", "test", paras_str);
  ASSERT_EQ(rc, 0);
  auto test0 = data_ctx->Get<std::string>("test0");  // test0 skip
  ASSERT_TRUE(test0 == nullptr);
  auto test1 = data_ctx->Get<std::string>("test1");  // test1 skip
  ASSERT_TRUE(test1 == nullptr);
  auto test2 = data_ctx->Get<std::string>("test2");  // test2 skip
  ASSERT_TRUE(test2 == nullptr);
  // ASSERT_EQ(*test2, "test2");
  auto test3 = data_ctx->Get<std::string>("test3");  // test3 always run event if test2 is skip
  ASSERT_TRUE(test3 != nullptr);
  ASSERT_EQ(*test3, "test3");

  (*paras_str)["exp"]["id"].SetInt(1000);
  rc = ctx.store->SyncExecute(data_ctx, "test", "test", paras_str);
  ASSERT_EQ(rc, 0);
  test0 = data_ctx->Get<std::string>("test0");  // test0 run
  ASSERT_TRUE(test0 != nullptr);
  ASSERT_EQ(*test0, "test0");
  test1 = data_ctx->Get<std::string>("test1");  // test1 run
  ASSERT_TRUE(test1 != nullptr);
  ASSERT_EQ(*test1, "test1");
  test2 = data_ctx->Get<std::string>("test2");  // test2 skip
  ASSERT_TRUE(test2 == nullptr);
  test3 = data_ctx->Get<std::string>("test3");  // test3 always run event if test2 is skip
  ASSERT_TRUE(test3 != nullptr);
  ASSERT_EQ(*test3, "test3");
}

TEST(ExpectConfig, expect_cond) {
  std::string content = R"(
name="test"
default_expr_processor="didagle_expr"
[[graph]]
name="test"
vertex_skip_as_error=false
[[graph.vertex]]
expect="$exp.id==1000"
processor = "test0"
if=["test1"]
else=["test2"]
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

  ParamsPtr paras_str = Params::New();
  (*paras_str)["exp"]["id"].SetInt(1001);
  int rc = ctx.store->SyncExecute(data_ctx, "test", "test", paras_str);
  ASSERT_EQ(rc, 0);
  auto test0 = data_ctx->Get<std::string>("test0");  // test0 skip
  ASSERT_TRUE(test0 == nullptr);
  auto test1 = data_ctx->Get<std::string>("test1");  // test1 skip
  ASSERT_TRUE(test1 == nullptr);
  auto test2 = data_ctx->Get<std::string>("test2");  // test2 skip
  ASSERT_TRUE(test2 == nullptr);
  // ASSERT_EQ(*test2, "test2");
  auto test3 = data_ctx->Get<std::string>("test3");  // test3 always run event if test2 is skip
  ASSERT_TRUE(test3 != nullptr);
  ASSERT_EQ(*test3, "test3");

  (*paras_str)["exp"]["id"].SetInt(1000);
  rc = ctx.store->SyncExecute(data_ctx, "test", "test", paras_str);
  ASSERT_EQ(rc, 0);
  test0 = data_ctx->Get<std::string>("test0");  // test0 run
  ASSERT_TRUE(test0 != nullptr);
  ASSERT_EQ(*test0, "test0");
  test1 = data_ctx->Get<std::string>("test1");  // test1 run
  ASSERT_TRUE(test1 != nullptr);
  ASSERT_EQ(*test1, "test1");
  test2 = data_ctx->Get<std::string>("test2");  // test2 skip
  ASSERT_TRUE(test2 == nullptr);
  test3 = data_ctx->Get<std::string>("test3");  // test3 always run event if test2 is skip
  ASSERT_TRUE(test3 != nullptr);
  ASSERT_EQ(*test3, "test3");
}