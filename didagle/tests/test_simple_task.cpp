
#include <gtest/gtest.h>
#include <vector>

#include "didagle/tests/test_common.h"

using namespace didagle;
TEST(Task, simple) {
  TestContext ctx;
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

  int rc = ctx.store->SyncExecute(group);
  ASSERT_EQ(rc, 0);

  ASSERT_EQ(counter, 3);
}

TEST(Task, simple_group) {
  TestContext ctx;
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
    return 0;
  });
  aaa->Precede(bbb);
  auto d = group0->Add(group1);

  c->Precede(d);

  int rc = ctx.store->SyncExecute(group0);
  ASSERT_EQ(rc, 0);

  ASSERT_EQ(counter, 5);
}
