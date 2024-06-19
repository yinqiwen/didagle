
#include <gtest/gtest.h>
#include <vector>

#include "didagle/tests/test_common.h"

using namespace didagle;
TEST(Task, ifelse) {
  TestContext ctx;
  auto group = TaskGroup::New();
  int flag = 1;  // else
  int counter = 0;
  auto a = group->Add([&](const Params& args) -> int { return flag; });
  auto b = group->Add([&](const Params& args) -> int {
    counter = 100;
    return 0;
  });
  auto c = group->Add([&](const Params& args) -> int {
    counter = 200;
    return 0;
  });

  a->Consequent(b);   // if
  a->Alternative(c);  // else

  int rc = ctx.store->SyncExecute(group);
  ASSERT_EQ(rc, 0);

  ASSERT_EQ(counter, 200);

  flag = 0;  // if
  rc = ctx.store->SyncExecute(group);
  ASSERT_EQ(rc, 0);
  ASSERT_EQ(counter, 100);
}
