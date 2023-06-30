// Copyright (c) 2020, Tencent Inc.
// All rights reserved.
#include <thread>
#include "didagle/di_container.h"
#include "didagle/didagle_log.h"
#include "didagle/graph_processor_api.h"

GRAPH_FUTURE_OP_BEGIN(future_phase0, "this is test future phase")
GRAPH_OP_OUTPUT(std::string, v0)

folly::Future<int> OnFutureExecute(const didagle::Params& args) override {
  folly::Promise<int> promise;
  folly::Future<int> rc = promise.getFuture();
  // prx->AsyncRpc(client_ctx, *req)
  //     .Then([promise = std::move(promise)](trpc::Future<RpcResponse>&& fut) mutable {
  //       if (fut.IsReady()) {
  //         //
  //         promise.setValue(0);
  //         return;
  //       }
  //       promise.setValue(-1);
  //     });
  return rc;
}

GRAPH_OP_END
