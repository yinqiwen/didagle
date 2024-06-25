/*
** BSD 3-Clause License
**
** Copyright (c) 2024, qiyingwang <qiyingwang@tencent.com>, the respective contributors, as shown by the AUTHORS file.
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are met:
** * Redistributions of source code must retain the above copyright notice, this
** list of conditions and the following disclaimer.
**
** * Redistributions in binary form must reproduce the above copyright notice,
** this list of conditions and the following disclaimer in the documentation
** and/or other materials provided with the distribution.
**
** * Neither the name of the copyright holder nor the names of its
** contributors may be used to endorse or promote products derived from
** this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
** AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
** DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
** SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
** CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
** OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
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
  // auto bbb = group1->Add([&](const Params& args) -> int {
  //   counter++;
  //   return 0;
  // });
  // aaa->Precede(bbb);
  auto d = group0->Add(group1);

  c->Precede(d);

  int rc = ctx.store->SyncExecute(group0);
  ASSERT_EQ(rc, 0);

  ASSERT_EQ(counter, 4);
}

TEST(Task, if_else) {
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
