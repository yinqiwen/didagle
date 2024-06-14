// -*- coding: utf-8-unix; -*-
// Copyright (c) 2023 Tencent, Inc.
//     All rights reserved.
//
// Author: faithzeng@tencent.com
// Date: 2023/6/25 15:54
// Description:
//

#include <gflags/gflags.h>
#include <memory>

// #include "trpc/coroutine/fiber.h"
// #include "trpc/coroutine/fiber_latch.h"

#include "didagle/log/log.h"
#include "didagle/processor/processor.h"
#include "folly/String.h"

GRAPH_OP_BEGIN(didagle_while)
Processor* _eval;
std::string _cond;     // NOLINT
std::string _cluster;  // NOLINT
std::string _graph;    // NOLINT
bool _async;
int OnSetup(const didagle::Params& args) override {
  _eval = didagle::ProcessorFactory::GetProcessor("didagle_expr");
  if (nullptr != _eval) {
    didagle::Params param;
    _cond = folly::toStdString(args[std::string(didagle::kExprParamKey)].String());
    param.SetString(_cond);
    if (0 != _eval->Setup(param)) {
      DIDAGLE_ERROR("Failed to setup ispine_while_processor, cond is {}", _cond);
      return -1;
    }
  } else {
    DIDAGLE_ERROR("No processor found for ispine_didagle_expr");
    return -1;
  }
  _cluster = folly::toStdString(args[std::string(didagle::kWhileExecCluterParamKey)].String());
  _graph = folly::toStdString(args[std::string(didagle::kWhileExecGraphParamKey)].String());
  _async = args[std::string(didagle::kWhileAsyncExecParamKey)].Bool();
  DIDAGLE_DEBUG("while processor init succ, _cond:{}, _cluster:{}, _graph:{}, _async:{}", _cond, _cluster, _graph,
                _async);
  return 0;
}

int CalcLoopIdx(didagle::GraphDataContext& ctx, const didagle::Params& args) {
  int loop_idx = 0;
  do {
    didagle::Params sub_params;
    sub_params.SetParent(&args);
    sub_params["loop_idx"].SetInt(loop_idx);
    _eval->SetDataContext(&ctx);
    int eval_rc = _eval->Execute(sub_params);
    if (eval_rc != 0) {
      break;
    }
    loop_idx++;
  } while (true);
  return loop_idx;
}

void AsyncExecute(didagle::GraphDataContext& ctx, const didagle::Params& args) {
  // 异步执行，因为执行顺序不确定，参数不能做动态更新，提前计算好执行次数
  int loop_idx = CalcLoopIdx(ctx, args);
  // trpc::FiberLatch latch(loop_idx);
  for (int i = 0; i < loop_idx; ++i) {
    std::shared_ptr<didagle::Params> sub_params = std::make_shared<didagle::Params>();
    sub_params->SetParent(&args);
    (*sub_params.get())["loop_idx"].SetInt(i);
    didagle::GraphDataContextPtr ctx_ptr = didagle::GraphDataContext::New();
    ctx_ptr->SetParent(&ctx);
    // trpc::StartFiberDetached([ctx_ptr, sub_params, &latch, this]() mutable {
    //   ispine::DAGScriptManager::GetInstance()->ExecuteDidagleScript(
    //       ctx_ptr, _cluster, _graph, sub_params.get(), [ctx_ptr, sub_params, &latch]() { latch.CountDown(); });
    // });
  }
  // latch.Wait();
}

void SyncExecute(didagle::GraphDataContext& ctx, const didagle::Params& args) {
  int loop_idx = 0;
  do {
    std::shared_ptr<didagle::Params> sub_params = std::make_shared<didagle::Params>();
    sub_params->SetParent(&args);
    (*sub_params.get())["loop_idx"].SetInt(loop_idx);
    didagle::GraphDataContextPtr ctx_ptr = didagle::GraphDataContext::New();
    ctx_ptr->SetParent(&ctx);
    _eval->SetDataContext(ctx_ptr.get());
    int eval_rc = _eval->Execute(*sub_params.get());
    if (eval_rc != 0) {
      break;
    }

    // trpc::FiberLatch latch(1);
    // ispine::DAGScriptManager::GetInstance()->ExecuteDidagleScript(ctx_ptr, _cluster, _graph, sub_params.get(),
    //                                                               [ctx_ptr, sub_params, &latch] { latch.CountDown();
    //                                                               });
    // latch.Wait();
    loop_idx++;
  } while (true);
}

int OnExecute(const didagle::Params& args) override {
  didagle::GraphDataContext& ctx = GetDataContext();
  if (_async) {
    AsyncExecute(ctx, args);
  } else {
    SyncExecute(ctx, args);
  }
  return 0;
}
GRAPH_OP_END
