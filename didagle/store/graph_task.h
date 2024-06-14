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
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "didagle/processor/processor.h"

namespace didagle {
class Task;
class TaskGroup;
class GraphStore;
using TaskPtr = std::shared_ptr<Task>;
using TaskGroupPtr = std::shared_ptr<TaskGroup>;
class Task {
 public:
  Task(const std::string& processor, const std::string& group_id);
  void Precede(TaskPtr task) { Precede(*task); }
  void Precede(Task& task);
  void Succeed(TaskPtr task) { Succeed(*task); }
  void Succeed(Task& task);

  void Consequent(Task& task);
  void Consequent(TaskPtr task) { Consequent(*task); }
  void Alternative(Task& task);
  void Alternative(TaskPtr task) { Alternative(*task); }

  const std::string& Id() const { return id_; }
  void Name(const std::string& name) { id_ = name; }

 private:
  std::string id_;
  std::string processor_;
  std::string group_id_;
  std::set<std::string> precede_;
  std::set<std::string> succeed_;
  std::set<std::string> consequent_;
  std::set<std::string> alternative_;
  friend class TaskGroup;
  friend class GraphStore;
};

class TaskGroup {
 public:
  static TaskGroupPtr New() { return std::make_shared<TaskGroup>(); }
  TaskGroup();
  TaskPtr Add(ExecFunc&& f);
  TaskPtr Add(TaskGroupPtr group);
  TaskPtr Clone(TaskPtr task);
  const std::string& Id() const { return id_; }
  const std::string& ClusterId() const { return cluster_id_; }
  const std::vector<TaskGroupPtr>& SubTaskGroups() const { return childs_; }
  const std::vector<TaskPtr>& Tasks() const { return tasks_; }

 private:
  std::string cluster_id_;
  std::string id_;
  std::vector<TaskPtr> tasks_;
  std::vector<TaskGroupPtr> childs_;
};
}  // namespace didagle