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
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "didagle/graph_processor_api.h"
#include "didagle/graph_processor_di.h"
#include "didagle/graph_vertex.h"

namespace didagle {
typedef std::function<void(int)> DoneClosure;
typedef std::function<void(void)> AnyClosure;
typedef std::function<void(AnyClosure&&)> ConcurrentExecutor;
using EventReporter = std::function<void(DAGEvent)>;

struct GraphExecuteOptions {
  ConcurrentExecutor concurrent_executor;
  std::shared_ptr<Params> params;
  EventReporter event_reporter;
  std::function<bool(const std::string&)> check_version;
};

struct SelectCondParamsContext {
  Processor* p = nullptr;
  CondParams param;
};

class GraphContext;
class GraphClusterContext;
class VertexContext {
 private:
  GraphContext* _graph_ctx = nullptr;
  Vertex* _vertex = nullptr;
  std::atomic<uint32_t> _waiting_num;
  std::vector<VertexResult> _deps_results;
  VertexResult _result;
  VertexErrCode _code;
  Processor* _processor = nullptr;
  ProcessorDI* _processor_di = nullptr;
  GraphClusterContext* _subgraph_cluster = nullptr;
  GraphContext* _subgraph_ctx = nullptr;
  std::string _full_graph_name;
  Params _params;
  std::vector<SelectCondParamsContext> _select_contexts;
  uint64_t _exec_start_ustime = 0;
  size_t _child_idx = (size_t)-1;
  const Params* _exec_params = nullptr;
  int _exec_rc = INT_MAX;

 public:
  VertexContext();
  void SetChildIdx(size_t idx) { _child_idx = idx; }
  Vertex* GetVertex() { return _vertex; }
  const Params* GetExecParams();
  ProcessorDI* GetProcessorDI() { return _processor_di; }
  Processor* GetProcessor() { return _processor; }
  VertexResult GetResult() { return _result; }
  void FinishVertexProcess(int code);
  int ExecuteProcessor();
  int ExecuteSubGraph();
  uint32_t SetDependencyResult(Vertex* v, VertexResult r);
  bool Ready();
  int Setup(GraphContext* g, Vertex* v);
  void Reset();
  int Execute();
  ~VertexContext();
};

struct Graph;
struct GraphCluster;
class GraphContext {
 private:
  GraphClusterContext* _cluster;
  Graph* _graph;
  std::unordered_map<Vertex*, std::shared_ptr<VertexContext>> _vertex_context_table;
  std::atomic<uint32_t> _join_vertex_num;
  GraphDataContextPtr _data_ctx;
  DoneClosure _done;
  std::vector<uint8_t> _config_setting_result;
  std::set<DIObjectKey> _all_input_ids;
  std::set<DIObjectKey> _all_output_ids;
  size_t _children_count;

 public:
  GraphContext();

  Graph* GetGraph() { return _graph; }
  GraphCluster* GetGraphCluster();
  GraphClusterContext* GetGraphClusterContext() { return _cluster; }

  VertexContext* FindVertexContext(Vertex* v);
  void OnVertexDone(VertexContext* vertex);

  int Setup(GraphClusterContext* c, Graph* g);
  void Reset();
  void ExecuteReadyVertexs(std::vector<VertexContext*>& ready_vertexs);
  int Execute(DoneClosure&& done);
  GraphDataContext* GetGraphDataContext();
  GraphDataContext& GetGraphDataContextRef() { return *(GetGraphDataContext()); }
  void SetGraphDataContext(GraphDataContext* p);
};

struct ConfigSettingContext {
  Processor* eval_proc = nullptr;
  uint8_t result = 0;
};

class GraphClusterContext {
 private:
  const Params* _exec_params = nullptr;
  std::shared_ptr<GraphCluster> _running_cluster;
  GraphContext* _running_graph = nullptr;
  GraphCluster* _cluster = nullptr;
  std::vector<ConfigSettingContext> _config_settings;
  std::unordered_map<std::string, std::shared_ptr<GraphContext>> _graph_context_table;
  DoneClosure _done;
  GraphDataContext* _extern_data_ctx = nullptr;
  uint64_t _end_ustime = 0;

 public:
  void SetExternGraphDataContext(GraphDataContext* p) { _extern_data_ctx = p; }
  uint64_t GetEndTime() { return _end_ustime; }
  void SetEndTime(const uint64_t end_ustime) { _end_ustime = end_ustime; }
  void SetExecuteParams(const Params* p) { _exec_params = p; }
  const Params* GetExecuteParams() const { return _exec_params; }
  GraphCluster* GetCluster() { return _cluster; }
  GraphContext* GetRunGraph(const std::string& name);
  void SetRunningCluster(std::shared_ptr<GraphCluster> c) { _running_cluster = c; }
  std::shared_ptr<GraphCluster> GetRunningCluster() { return _running_cluster; }
  int Setup(GraphCluster* c);
  void Reset();
  int Execute(const std::string& graph, DoneClosure&& done, GraphContext*& graph_ctx);
  void Execute(GraphDataContext& session_ctx, std::vector<uint8_t>& eval_results);
  ~GraphClusterContext();
};

}  // namespace didagle