// Copyright (c) 2021, Tencent Inc.
// All rights reserved.

#include <fmt/core.h>
#include <stdint.h>
#include <string>
#include "didagle/didagle.h"

#include "boost/asio/post.hpp"
#include "boost/asio/thread_pool.hpp"

#include "didagle/store/common.h"
#include "folly/Singleton.h"
#include "folly/synchronization/HazptrThreadPoolExecutor.h"
#include "folly/synchronization/Latch.h"

using namespace didagle;

static void test0(GraphStore& store) {
  auto group = TaskGroup::New();
  int counter = 0;
  auto a = group->Add([&](const Params& args) -> int {
    counter++;
    return 0;
  });
  auto b = group->Add([&](const Params& args) -> int {
    counter++;
    return 0;
  });
  auto c = group->Add([&](const Params& args) -> int {
    counter++;
    return 0;
  });

  a->Precede(b);
  b->Precede(c);

  int rc = store.SyncExecute(group);
  fmt::print("##rc:{}\n", rc);

  fmt::print("##counter:{}\n", counter);
}

static void test_sub_task_group(GraphStore& store) {
  auto group0 = TaskGroup::New();
  int counter = 0;
  auto a = group0->Add([&](const Params& args) -> int {
    counter++;
    return 0;
  });
  auto b = group0->Add([&](const Params& args) -> int {
    counter++;
    return 0;
  });
  auto c = group0->Add([&](const Params& args) -> int {
    counter++;
    return 0;
  });

  a->Precede(b);
  b->Precede(c);

  auto group1 = TaskGroup::New();
  auto aaa = group1->Add([&](const Params& args) -> int {
    counter++;
    return 0;
  });
  auto bbb = group1->Add([&](const Params& args) -> int {
    counter++;
    fmt::print("#####\n");
    return 0;
  });
  aaa->Precede(bbb);
  auto d = group0->Add(group1);

  c->Precede(d);

  int rc = store.SyncExecute(group0);
  fmt::print("##rc:{}\n", rc);

  fmt::print("##counter:{}\n", counter);
}
static void test_if_else(GraphStore& store) {
  auto group = TaskGroup::New();
  int counter = 0;
  auto a = group->Add([&](const Params& args) -> int { return 1; });
  auto b = group->Add([&](const Params& args) -> int {
    counter = 100;
    return 0;
  });
  auto c = group->Add([&](const Params& args) -> int {
    counter = 200;
    return 0;
  });

  a->Consequent(b);
  a->Alternative(c);

  int rc = store.SyncExecute(group);
  fmt::print("##rc:{}\n", rc);

  fmt::print("##counter:{}\n", counter);
}

int main() {
  folly::SingletonVault::singleton()->registrationComplete();
  folly::enable_hazptr_thread_pool_executor();
  boost::asio::thread_pool pool(8);
  GraphExecuteOptions exec_opt;
  exec_opt.async_executor = [&pool](AnyClosure&& r) { boost::asio::post(pool, r); };
  exec_opt.latch_creator = new_folly_latch;
  GraphStore store(exec_opt);
  test0(store);
  test_sub_task_group(store);
  test_if_else(store);
  pool.join();
  return 0;
}
