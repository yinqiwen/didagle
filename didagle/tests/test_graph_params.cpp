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

GRAPH_OP_OUTPUT(std::string, s_result)
GRAPH_OP_OUTPUT(int64_t, i_result)
GRAPH_OP_OUTPUT(bool, b_result)
GRAPH_OP_OUTPUT(double, d_result)

GRAPH_PARAMS_bool(b_arg, false, "e");
GRAPH_PARAMS_int(i_arg, 1212112, "e");
GRAPH_PARAMS_string(s_arg, "abcde", "e");
GRAPH_PARAMS_double(d_arg, 3.124, "e");
int OnExecute(const Params& args) override {
  s_result = PARAMS_s_arg.toStdString();
  d_result = PARAMS_d_arg;
  i_result = PARAMS_i_arg;
  b_result = PARAMS_b_arg;
  return 0;
}
GRAPH_OP_END

GRAPH_OP_BEGIN(test1)
GRAPH_OP_OUTPUT(std::string, test1)
GRAPH_PARAMS_int_vector(iv_arg, ({1, 2}), "int vector");
GRAPH_PARAMS_double_vector(dv_arg, ({1.1, 2.2}), "double vector");
GRAPH_PARAMS_string_vector(sv_arg, ({"hello", "world"}), "string vector");
GRAPH_PARAMS_bool_vector(bv_arg, ({true, false}), "bool vector");

GRAPH_OP_OUTPUT(std::vector<int64_t>, iv_result);
GRAPH_OP_OUTPUT(std::vector<bool>, bv_result);
GRAPH_OP_OUTPUT(std::vector<double>, dv_result);
GRAPH_OP_OUTPUT(std::vector<ParamsString>, sv_result);
int OnExecute(const Params& args) override {
  test1 = "test1";
  iv_result = PARAMS_iv_arg;
  bv_result = PARAMS_bv_arg;
  dv_result = PARAMS_dv_arg;
  sv_result = PARAMS_sv_arg;
  return 0;
}
GRAPH_OP_END

TEST(Params, simple) {
  std::string content = R"(
name="test"
[[graph]]
name="test"
[[graph.vertex]]
processor = "test0"
start = true
[[graph.vertex]]
processor = "test1"
start = true
  )";
  TestContext ctx;
  auto handle = ctx.store->LoadString(content);
  ASSERT_TRUE(handle != nullptr);
  auto data_ctx = GraphDataContext::New();
  auto params = Params::New();

  bool b_arg = true;
  int64_t i_arg = 1011;
  double d_arg = 2.71;
  std::string s_arg = "hwllo,didagle";

  (*params)["b_arg"].SetBool(b_arg);
  (*params)["i_arg"].SetInt(i_arg);
  (*params)["d_arg"].SetDouble(d_arg);
  (*params)["s_arg"].SetString(s_arg);

  for (size_t i = 0; i < 5; i++) {
    (*params)["bv_arg"].Add().SetBool(true);
    (*params)["iv_arg"].Add().SetInt(i);
    (*params)["dv_arg"].Add().SetDouble(i + 100.0);
    (*params)["sv_arg"].Add().SetString("hello," + std::to_string(i));
  }

  int rc = ctx.store->SyncExecute(data_ctx, "test", "test", params);
  ASSERT_EQ(rc, 0);

  auto s_result = data_ctx->Get<std::string>("s_result");
  ASSERT_TRUE(s_result != nullptr);
  ASSERT_EQ(*s_result, s_arg);
  auto i_result = data_ctx->Get<int64_t>("i_result");
  ASSERT_TRUE(i_result != nullptr);
  ASSERT_EQ(*i_result, i_arg);
  auto d_result = data_ctx->Get<double>("d_result");
  ASSERT_TRUE(d_result != nullptr);
  ASSERT_EQ(*d_result, d_arg);
  auto b_result = data_ctx->Get<bool>("b_result");
  ASSERT_TRUE(b_result != nullptr);
  ASSERT_EQ(*b_result, b_arg);

  auto sv_result = data_ctx->Get<std::vector<ParamsString>>("sv_result");
  ASSERT_TRUE(sv_result != nullptr && sv_result->size() == 5);
  auto iv_result = data_ctx->Get<std::vector<int64_t>>("iv_result");
  ASSERT_TRUE(iv_result != nullptr && iv_result->size() == 5);
  auto dv_result = data_ctx->Get<std::vector<double>>("dv_result");
  ASSERT_TRUE(dv_result != nullptr && dv_result->size() == 5);
  auto bv_result = data_ctx->Get<std::vector<bool>>("bv_result");
  ASSERT_TRUE(bv_result != nullptr && bv_result->size() == 5);
  for (size_t i = 0; i < sv_result->size(); i++) {
    ASSERT_EQ(sv_result->at(i), "hello," + std::to_string(i));
    ASSERT_EQ(iv_result->at(i), i);
    ASSERT_EQ(bv_result->at(i), true);
    ASSERT_EQ(dv_result->at(i), i + 100.0);
  }
}
