// Copyright (c) 2021, Tencent Inc.
// All rights reserved.
// Created on 2021/04/17
// Authors: qiyingwang (qiyingwang@tencent.com)
#pragma once

#include <stdint.h>

#include "folly/concurrency/UnboundedQueue.h"
#include "folly/container/F14Map.h"

#include "didagle/graph/graph.h"
#include "didagle/store/cluster_context.h"
#include "didagle/store/common.h"
#include "didagle/store/graph_task.h"

namespace didagle {

class GraphStore {
 public:
  explicit GraphStore(const GraphExecuteOptions& options);
  std::shared_ptr<GraphClusterHandle> Load(const std::string& file);
  std::shared_ptr<GraphClusterHandle> FindGraphClusterByName(const std::string& name);
  GraphClusterContext* GetGraphClusterContext(const std::string& cluster);
  int Execute(GraphDataContextPtr data_ctx, const std::string& cluster, const std::string& graph, const Params* params,
              DoneClosure&& done, uint64_t time_out_ms = 0);
  bool Exists(const std::string& cluster, const std::string& graph);

  int AsyncExecute(TaskGroupPtr graph, DoneClosure&& done, uint64_t time_out_ms = 0);
  int SyncExecute(TaskGroupPtr graph, uint64_t time_out_ms = 0);

 private:
  Graph BuildGraphByTaskGroup(TaskGroupPtr graph);
  std::shared_ptr<GraphClusterHandle> LoadTaskGroup(TaskGroupPtr graph);
  using ClusterGraphTable = folly::F14NodeMap<std::string, folly::atomic_shared_ptr<GraphClusterHandle>>;
  ClusterGraphTable _graphs;
  GraphExecuteOptionsPtr _exec_options;
  std::mutex _graphs_mutex;
  GraphExecFunc _graph_exec_func_;
};

}  // namespace didagle
