// Copyright (c) 2021, Tencent Inc.
// All rights reserved.
// Created on 2021/04/17
// Authors: qiyingwang (qiyingwang@tencent.com)
#pragma once

#include <stdint.h>

#include <atomic>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "didagle/store/common.h"
#include "didagle/store/graph_context.h"

namespace didagle {

struct ConfigSettingContext {
  Processor* eval_proc = nullptr;
  uint8_t result = 0;
};
struct GraphClusterHandle;
class GraphStore;
class GraphClusterContext {
 private:
  GraphStore* _store;
  GraphExecuteOptionsPtr _exec_opts;
  const Params* _exec_params = nullptr;
  std::shared_ptr<GraphClusterHandle> _running_cluster;
  GraphContext* _running_graph = nullptr;
  GraphContext* _last_runnin_graph = nullptr;
  GraphCluster* _cluster = nullptr;
  std::vector<ConfigSettingContext> _config_settings;
  folly::F14FastMap<std::string, std::shared_ptr<GraphContext>> _graph_context_table;
  DoneClosure _done;
  GraphDataContext* _extern_data_ctx = nullptr;
  uint64_t _end_ustime = 0;

 public:
  GraphClusterContext(GraphStore* store, GraphExecuteOptionsPtr exec_opts) : _store(store), _exec_opts(exec_opts) {}
  inline GraphStore* GetStore() { return _store; }
  inline GraphExecuteOptionsPtr GetGraphExecuteOptions() { return _exec_opts; }
  inline void SetExternGraphDataContext(GraphDataContext* p) { _extern_data_ctx = p; }
  inline uint64_t GetEndTime() { return _end_ustime; }
  inline void SetEndTime(const uint64_t end_ustime) { _end_ustime = end_ustime; }
  inline void SetExecuteParams(const Params* p) { _exec_params = p; }
  inline const Params* GetExecuteParams() const { return _exec_params; }
  inline GraphCluster* GetCluster() { return _cluster; }
  inline void SetRunningCluster(std::shared_ptr<GraphClusterHandle> c) { _running_cluster = c; }
  inline std::shared_ptr<GraphClusterHandle> GetRunningCluster() { return _running_cluster; }
  GraphContext* GetRunGraph(const std::string& name);
  int Setup(GraphCluster* c);
  void Reset();
  int Execute(const std::string& graph, DoneClosure&& done, GraphContext*& graph_ctx);
  void Execute(GraphDataContext& session_ctx, std::vector<uint8_t>& eval_results);
  ~GraphClusterContext();
};

struct GraphClusterHandle {
  GraphCluster cluster;
  using ContextPool = folly::UMPMCQueue<GraphClusterContext*, false>;
  ContextPool contexts;
  GraphClusterContext* GetContext(GraphStore* store, GraphExecuteOptionsPtr options);
  void ReleaseContext(GraphClusterContext* p);
  int Build(GraphStore* store, GraphExecuteOptionsPtr options);
  ~GraphClusterHandle();
};

}  // namespace didagle
