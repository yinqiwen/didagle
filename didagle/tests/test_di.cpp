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
#include <stdint.h>
#include <string>
#include "didagle/di/container.h"
using namespace didagle;
struct TestPOD {
  int a = 101;
};

TEST(DIContainer, POD) {
  DIContainer::RegisterBuilder<TestPOD>("pod", std::make_unique<DIObjectBuilder<TestPOD>>([]() {
                                          static TestPOD t;
                                          t.a = 202;
                                          return &t;
                                        }));
  DIContainer::Init();

  const TestPOD* p = DIContainer::Get<TestPOD>("pod");
  EXPECT_EQ(202, p->a);
}

TEST(DIContainer, SharedPtr) {
  typedef std::shared_ptr<std::string> StringPtr;
  DIContainer::RegisterBuilder<StringPtr>("test_id", std::make_unique<DIObjectBuilder<StringPtr>>([]() {
                                            StringPtr id(new std::string("hello"));
                                            return id;
                                          }));
  DIContainer::Init();

  StringPtr p = DIContainer::Get<StringPtr>("test_id");
  EXPECT_EQ("hello", *p);
}

TEST(DIContainer, UniquePtr) {
  typedef std::unique_ptr<std::string> StringPtr;
  DIContainer::RegisterBuilder<StringPtr>("test_uniq_id", std::make_unique<DIObjectBuilder<StringPtr>>([]() {
                                            StringPtr id(new std::string("world"));
                                            return std::move(id);
                                          }));
  DIContainer::Init();

  StringPtr p = DIContainer::Get<StringPtr>("test_uniq_id");
  EXPECT_EQ("world", *p);
}

struct TestPOD2 {
  int a = 101;
  std::string id = "test";
};

struct TestPOD2Builder : public DIObjectBuilder<TestPOD2> {
  TestPOD2 instance;
  int Init() override {
    instance.a = 10000;
    instance.id = "new_instance";
    return 0;
  }
  const TestPOD2* Get() override { return &instance; }
};

TEST(DIContainer, CustomBuilderClass) {
  DIContainer::RegisterBuilder<TestPOD2>("test_pod2", std::make_unique<TestPOD2Builder>());
  DIContainer::Init();

  const TestPOD2* p = DIContainer::Get<TestPOD2>("test_pod2");
  EXPECT_EQ(10000, p->a);
  EXPECT_EQ("new_instance", p->id);
}

struct TestPOD3 {
  int a = 101;
  std::string id = "test";
};

struct TestPOD4 : public DIObject {
  DI_DEP(TestPOD3, pod3)
  DI_DEP(std::shared_ptr<TestPOD3>, pod33)
};

TEST(DIContainer, DepInject) {
  DIContainer::RegisterBuilder<TestPOD3>("pod3", std::make_unique<DIObjectBuilder<TestPOD3>>([]() {
                                           static TestPOD3 pod;
                                           pod.a = 202;
                                           pod.id = "notest";
                                           return &pod;
                                         }));
  DIContainer::RegisterBuilder<std::shared_ptr<TestPOD3>>(
      "pod33", std::make_unique<DIObjectBuilder<std::shared_ptr<TestPOD3>>>([]() {
        std::shared_ptr<TestPOD3> p(new TestPOD3);
        p->a = 303;
        p->id = "sptest";
        return p;
      }));
  DIContainer::RegisterBuilder<TestPOD4>("pod4", std::make_unique<DIObjectBuilder<TestPOD4>>([]() {
                                           static TestPOD4 pod;
                                           return &pod;
                                         }));
  DIContainer::Init();

  const TestPOD4* p = DIContainer::Get<TestPOD4>("pod4");
  EXPECT_EQ(202, p->pod3->a);
  EXPECT_EQ("notest", p->pod3->id);
  EXPECT_EQ(303, p->pod33->a);
  EXPECT_EQ("sptest", p->pod33->id);
}
