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

#include "folly/container/F14Map.h"

#include "didagle/graph/graph.h"

#include "didagle/store/common.h"
#include "didagle/store/vertex_context.h"

namespace didagle {

class GraphClusterContext;
class GraphContext {
 private:
  GraphClusterContext* _cluster;
  Graph* _graph;
  folly::F14FastMap<Vertex*, std::shared_ptr<VertexContext>> _vertex_context_table;
  std::atomic<uint32_t> _join_vertex_num;
  GraphDataContextPtr _data_ctx;
  DoneClosure _done;
  std::vector<uint8_t> _config_setting_result;
  size_t _children_count;

  std::vector<VertexContext*> _start_ctxs;

 public:
  GraphContext();

  inline Graph* GetGraph() { return _graph; }
  GraphCluster* GetGraphCluster();
  inline GraphClusterContext* GetGraphClusterContext() { return _cluster; }

  VertexContext* FindVertexContext(Vertex* v);
  void OnVertexDone(VertexContext* vertex);

  int Setup(GraphClusterContext* c, Graph* g);
  void Reset();
  void ExecuteReadyVertexs(std::vector<VertexContext*>& ready_vertexs);
  inline void ExecuteReadyVertex(VertexContext* v) { v->Execute(); }
  int Execute(DoneClosure&& done);
  inline GraphDataContext* GetGraphDataContext() { return _data_ctx.get(); }
  inline GraphDataContext& GetGraphDataContextRef() { return *(GetGraphDataContext()); }
  inline void SetGraphDataContext(GraphDataContext* p) { _data_ctx->SetParent(p); }
};

}  // namespace didagle
