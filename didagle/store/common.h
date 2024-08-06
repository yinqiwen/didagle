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

#pragma once
#include <sys/time.h>
#include <functional>
#include <memory>

#include "folly/synchronization/Latch.h"

#include "didagle/graph/params.h"
#include "didagle/processor/processor.h"
#include "didagle/trace/event.h"

namespace didagle {

using AnyClosure = std::function<void(void)>;
using AsyncExecutor = std::function<void(AnyClosure&&)>;
using EventReporter = std::function<void(DAGEvent)>;

struct Latch {
  virtual void CountDown(ptrdiff_t n = 1) = 0;
  virtual void Wait() noexcept = 0;
  virtual ~Latch() {}
};

struct FollyLatch : public Latch {
  explicit FollyLatch(ptrdiff_t expected) { l = std::make_unique<folly::Latch>(expected); }
  void CountDown(ptrdiff_t n) override { l->count_down(n); }
  void Wait() noexcept override { l->wait(); }
  std::unique_ptr<folly::Latch> l;
};
inline std::unique_ptr<Latch> new_folly_latch(ptrdiff_t n) {
  std::unique_ptr<Latch> p(new FollyLatch(n));
  return p;
}

using LatchCreator = std::function<std::unique_ptr<Latch>(ptrdiff_t)>;
struct GraphExecuteOptions {
  AsyncExecutor async_executor;
  LatchCreator latch_creator;
  std::shared_ptr<Params> params;
  EventReporter event_reporter;
};
using GraphExecuteOptionsPtr = std::shared_ptr<GraphExecuteOptions>;

struct SelectCondParamsContext {
  Processor* p = nullptr;
  CondParams param;
};

inline uint64_t ustime() {
  struct timeval tv;
  uint64_t ust;
  gettimeofday(&tv, nullptr);
  ust = ((uint64_t)tv.tv_sec) * 1000000;
  ust += tv.tv_usec;
  return ust;
}

}  // namespace didagle