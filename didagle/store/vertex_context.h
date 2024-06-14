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

#include "didagle/graph/vertex.h"
#include "didagle/processor/processor.h"
#include "didagle/processor/processor_di.h"
#include "didagle/store/common.h"

namespace didagle {

class GraphContext;
class GraphClusterContext;
class GraphStore;
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
  std::string_view _exec_mathced_cond;
  int _exec_rc = INT_MAX;

  std::vector<VertexContext*> _successor_ctxs;
  std::vector<int> _successor_dep_idxs;

  void SetupSuccessors();

  friend class GraphContext;

 public:
  VertexContext();
  inline void SetChildIdx(size_t idx) { _child_idx = idx; }
  inline Vertex* GetVertex() { return _vertex; }
  const Params* GetExecParams(std::string_view* matched_cond);
  inline ProcessorDI* GetProcessorDI() { return _processor_di; }
  inline Processor* GetProcessor() { return _processor; }
  inline VertexResult GetResult() { return _result; }
  void FinishVertexProcess(int code);
  int ExecuteProcessor();
  int ExecuteSubGraph();
  inline uint32_t SetDependencyResult(int idx, VertexResult r) {
    // int idx = _vertex->GetDependencyIndex(v);
    VertexResult last_result_val = _deps_results[idx];
    _deps_results[idx] = r;
    if (last_result_val == V_RESULT_INVALID) {
      return _waiting_num.fetch_sub(1);
    } else {
      return _waiting_num.load();
    }
  }
  inline bool Ready() { return 0 == _waiting_num.load(); }
  int Setup(GraphContext* g, Vertex* v);
  void Reset();
  int Execute();
  ~VertexContext();
};

}  // namespace didagle
