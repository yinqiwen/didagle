/*
 *Copyright (c) 2021, qiyingwang <qiyingwang@tencent.com>
 *All rights reserved.
 *
 *Redistribution and use in source and binary forms, with or without
 *modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of rimos nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 *THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 *BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 *THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "didagle/store/background_worker.h"

#include <gflags/gflags.h>
#include <utility>
#include "folly/Singleton.h"

DEFINE_uint32(didagle_async_reset_worker_num, 2, "async reset worker num");

namespace {
struct PrivateTag {};
}  // namespace
namespace didagle {
static folly::Singleton<AsyncResetWorker, PrivateTag> the_singleton;
std::shared_ptr<AsyncResetWorker> AsyncResetWorker::GetInstance() { return the_singleton.try_get(); }
AsyncResetWorker::AsyncResetWorker() {
  executor_ = std::make_unique<folly::CPUThreadPoolExecutor>(
      FLAGS_didagle_async_reset_worker_num, std::make_shared<folly::NamedThreadFactory>("didagle_reset"));
}

void AsyncResetWorker::SetCustomExecutor(std::function<void(folly::Func&&)>&& func) {
  custom_executor_ = func;
  executor_->stop();
  executor_.reset();
}
void AsyncResetWorker::Post(folly::Func&& func) {
  if (custom_executor_) {
    custom_executor_(std::move(func));
    return;
  }
  executor_->add(std::move(func));
}

AsyncResetWorker::~AsyncResetWorker() {
  if (executor_ != nullptr) {
    executor_->stop();
    executor_.reset();
  }
}
}  // namespace didagle
