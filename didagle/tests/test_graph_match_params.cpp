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
GRAPH_OP_OUTPUT(std::string, test3_0)
GRAPH_OP_OUTPUT(std::string, test3_1)
GRAPH_PARAMS_string(abc, "1111", "e");
GRAPH_PARAMS_string(xyz, "2222", "e");
int OnExecute(const Params& args) override {
  test3_0 = PARAMS_abc.toStdString();
  test3_1 = PARAMS_xyz.toStdString();
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
[[graph.vertex]]
processor = "test0"
[[graph.vertex]]
processor = "test2"
[[graph.vertex]]
processor = "test3"
select_args = [
    { match = "with_exp_1000", args = { abc = "hello1", xyz = "aaa" } },
    { match = "$exp.id==1002", args = { abc = "hello2", xyz = "bbb" } },
]
args = { abc = "default", xyz = "zzz" }
  )";
  TestContext ctx;
  auto handle = ctx.store->LoadString(content);
  ASSERT_TRUE(handle != nullptr);
  auto data_ctx = GraphDataContext::New();

  ParamsPtr paras_str = Params::New();
  (*paras_str)["exp"]["id"].SetInt(1001);
  int rc = ctx.store->SyncExecute(data_ctx, "test", "test", paras_str);
  ASSERT_EQ(rc, 0);

  // default params
  auto test3_0 = data_ctx->Get<std::string>("test3_0");
  ASSERT_TRUE(test3_0 != nullptr);
  ASSERT_EQ(*test3_0, "default");
  auto test3_1 = data_ctx->Get<std::string>("test3_1");
  ASSERT_TRUE(test3_1 != nullptr);
  ASSERT_EQ(*test3_1, "zzz");

  (*paras_str)["exp"]["id"].SetInt(1000);
  rc = ctx.store->SyncExecute(data_ctx, "test", "test", paras_str);
  ASSERT_EQ(rc, 0);
  // match with_exp_1000
  test3_0 = data_ctx->Get<std::string>("test3_0");  // test3 always run event if test2 is skip
  ASSERT_TRUE(test3_0 != nullptr);
  ASSERT_EQ(*test3_0, "hello1");
  test3_1 = data_ctx->Get<std::string>("test3_1");  // test3 always run event if test2 is skip
  ASSERT_TRUE(test3_1 != nullptr);
  ASSERT_EQ(*test3_1, "aaa");

  (*paras_str)["exp"]["id"].SetInt(1002);
  rc = ctx.store->SyncExecute(data_ctx, "test", "test", paras_str);
  ASSERT_EQ(rc, 0);
  // match with_exp_1000
  test3_0 = data_ctx->Get<std::string>("test3_0");  // test3 always run event if test2 is skip
  ASSERT_TRUE(test3_0 != nullptr);
  ASSERT_EQ(*test3_0, "hello2");
  test3_1 = data_ctx->Get<std::string>("test3_1");  // test3 always run event if test2 is skip
  ASSERT_TRUE(test3_1 != nullptr);
  ASSERT_EQ(*test3_1, "bbb");
}
