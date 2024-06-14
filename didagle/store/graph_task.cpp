/*
** BSD 3-Clause License
**
** Copyright (c) 2023, qiyingwang <qiyingwang@tencent.com>, the respective contributors, as shown by the AUTHORS file.
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
#include "didagle/store/graph_task.h"
#include <atomic>
#include <memory>

namespace didagle {
static constexpr std::string_view k_task_prefix = "didagle_task_";
static constexpr std::string_view k_task_group_cluster_prefix = "didagle_task_group_cluster_";
static constexpr std::string_view k_task_group_prefix = "didagle_task_group_";
static constexpr std::string_view k_processor_prefix = "didagle_task_processor_";
static std::atomic<uint64_t> g_task_group_id_seed{0};
static std::atomic<uint64_t> g_task_id_seed{0};
static std::atomic<uint64_t> g_task_group_cluster_id_seed{0};
static std::atomic<uint64_t> g_processor_id_seed{0};
Task::Task(const std::string& processor, const std::string& group_id) {
  processor_ = processor;
  group_id_ = group_id;
  std::string s(k_task_prefix);
  s.append(std::to_string(g_task_id_seed.fetch_add(1)));
  id_ = s;
}
void Task::Precede(Task& task) {
  succeed_.insert(task.id_);
  task.precede_.insert(id_);
}
void Task::Succeed(Task& task) { task.Precede(*this); }

TaskGroup::TaskGroup() {
  std::string s(k_task_group_prefix);
  s.append(std::to_string(g_task_group_id_seed.fetch_add(1)));
  id_ = s;
  std::string ss(k_task_group_cluster_prefix);
  ss.append(std::to_string(g_task_group_cluster_id_seed.fetch_add(1)));
  cluster_id_ = ss;
}

TaskPtr TaskGroup::Add(ExecFunc&& f) {
  std::string s(k_processor_prefix);
  s.append(std::to_string(g_processor_id_seed.fetch_add(1)));
  bool r = register_func_processor(s, std::move(f));
  if (!r) {
    return nullptr;
  }
  auto task = std::make_shared<Task>(s, "");
  tasks_.emplace_back(task);
  return task;
}
TaskPtr TaskGroup::Add(TaskGroupPtr group) {
  childs_.emplace_back(group);
  group->cluster_id_ = cluster_id_;
  auto new_task = std::make_shared<Task>("", group->id_);
  tasks_.emplace_back(new_task);
  return new_task;
}
TaskPtr TaskGroup::Clone(TaskPtr task) {
  auto new_task = std::make_shared<Task>(task->processor_, task->group_id_);
  tasks_.emplace_back(new_task);
  return new_task;
}
}  // namespace didagle