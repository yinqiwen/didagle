// Copyright (c) 2020, Tencent Inc.
// All rights reserved.
#include "didagle/store/graph_context.h"
#include "didagle/store/cluster_context.h"

namespace didagle {
GraphContext::GraphContext() : _cluster(nullptr), _graph(nullptr), _children_count(0) {
  _join_vertex_num = 0;
  // _data_ctx.reset(new GraphDataContext);
  _data_ctx = GraphDataContext::New();
}

GraphCluster* GraphContext::GetGraphCluster() { return _graph->_cluster; }

int GraphContext::Setup(GraphClusterContext* c, Graph* g) {
  _cluster = c;
  _graph = g;

  size_t child_idx = 0;
  std::set<DIObjectKey> all_output_ids;
  for (auto& pair : g->_nodes) {
    Vertex* v = pair.second;
    auto c = std::make_shared<VertexContext>();
    // std::shared_ptr<VertexContext> c(new VertexContext);
    if (0 != c->Setup(this, v)) {
      DIDAGLE_ERROR("Graph:{} setup vertex:{} failed.", g->name, v->GetDotLable());
      return -1;
    }
    if (!v->cluster.empty()) {
      c->SetChildIdx(child_idx);
      _children_count++;
      child_idx++;
    }
    _vertex_context_table[v] = c;
    ProcessorDI* di = c->GetProcessorDI();
    if (nullptr != di) {
      for (const auto& entry : di->GetOutputIds()) {
        const DIObjectKey& key = entry.info;
        if (all_output_ids.count(key) > 0) {
          DIDAGLE_ERROR("Duplicate output name:{} in graph:{}", key.name, g->name);
          return -1;
        }
        all_output_ids.insert(key);
      }
    }
  }
  for (auto& [_, vetex_ctx] : _vertex_context_table) {
    vetex_ctx->SetupSuccessors();
  }
  _data_ctx->ReserveChildCapacity(_children_count);
  // std::set<DIObjectKey> move_ids;
  for (auto& pair : _vertex_context_table) {
    std::shared_ptr<VertexContext>& c = pair.second;
    ProcessorDI* di = c->GetProcessorDI();
    if (nullptr != di) {
      for (auto& entry : di->GetInputIds()) {
        const DIObjectKey& key = entry.info;
        const GraphData* data = entry.data;
        if (nullptr != data && !data->is_extern && data->aggregate.empty()) {
          if (all_output_ids.count(key) == 0) {
            DIDAGLE_ERROR(
                "Graph:{} have missing output field with name:{}, "
                "type_id:{} for vertex:{}.",
                g->name, key.name, key.id, c->GetVertex()->GetDotLable());
            return -1;
          }
        }
        if (!entry.info.flags.is_extern && !entry.info.flags.is_aggregate) {
          entry.idx = static_cast<int32_t>(_data_ctx->RegisterData(key));
        }
      }
      for (auto& entry : di->GetOutputIds()) {
        const DIObjectKey& key = entry.info;
        if (key.name[0] != '$') {
          entry.idx = static_cast<int32_t>(_data_ctx->RegisterData(key));
        }
      }
    }
  }

  for (auto& pair : _vertex_context_table) {
    std::shared_ptr<VertexContext>& ctx = pair.second;
    if (ctx->Ready()) {
      _start_ctxs.emplace_back(ctx.get());
    }
  }

  Reset();
  return 0;
}

VertexContext* GraphContext::FindVertexContext(Vertex* v) {
  auto found = _vertex_context_table.find(v);
  if (found == _vertex_context_table.end()) {
    return nullptr;
  }
  return found->second.get();
}
void GraphContext::Reset() {
  _join_vertex_num = _vertex_context_table.size();
  for (auto& pair : _vertex_context_table) {
    std::shared_ptr<VertexContext>& ctx = pair.second;
    ctx->Reset();
  }
  _data_ctx->Reset();
}
void GraphContext::ExecuteReadyVertexs(std::vector<VertexContext*>& ready_vertexs) {
  DIDAGLE_DEBUG("ExecuteReadyVertexs with {} vertexs.", ready_vertexs.size());
  if (ready_vertexs.empty()) {
    return;
  }
  if (ready_vertexs.size() == 1) {
    // inplace run
    ready_vertexs[0]->Execute();
  } else {
    VertexContext* local_execute = nullptr;
    for (size_t i = 0; i < ready_vertexs.size(); i++) {
      // select first non IO op to run directly
      if (nullptr == ready_vertexs[i]->GetProcessor()) {
        // use subgraph as local executor
        local_execute = ready_vertexs[i];
        break;
      }
      if (!ready_vertexs[i]->GetProcessor()->isIOProcessor()) {
        local_execute = ready_vertexs[i];
        break;
      }
    }
    // select first op run directly if there is no non IO op
    if (nullptr == local_execute) {
      local_execute = ready_vertexs[0];
    }
    uint64_t sched_start_ustime = ustime();
    DAGEventTracker* tracker = _data_ctx->GetEventTracker();
    for (VertexContext* ctx : ready_vertexs) {
      if (ctx == local_execute) {
        continue;
      }
      VertexContext* next = ctx;
      _cluster->GetGraphExecuteOptions()->async_executor([tracker, next, sched_start_ustime]() {
        if (nullptr != tracker) {
          auto event = std::make_unique<DAGEvent>();
          event->start_ustime = sched_start_ustime;
          event->end_ustime = ustime();
          event->phase = PhaseType::DAG_PHASE_CONCURRENT_SCHED;
          tracker->Add(std::move(event));
        }
        next->Execute();
      });
    }
    local_execute->Execute();
  }
}
void GraphContext::OnVertexDone(VertexContext* vertex) {
  DIDAGLE_DEBUG("[{}]OnVertexDone while _join_vertex_num:{}.", vertex->GetVertex()->id, _join_vertex_num.load());
  if (1 == _join_vertex_num.fetch_sub(1)) {  // last vertex
    if (_done) {
      _done(0);
    }
    return;
  }
  std::vector<VertexContext*> ready_successors;
  size_t successor_num = vertex->_successor_ctxs.size();
  for (size_t i = 0; i < successor_num; i++) {
    VertexContext* successor_ctx = vertex->_successor_ctxs[i];
    uint32_t wait_num = successor_ctx->SetDependencyResult(vertex->_successor_dep_idxs[i], vertex->GetResult());
    // DIDAGLE_DEBUG("[{}]Successor:{} wait_num:{}.", vertex->GetVertex()->id, successor->id, wait_num);
    //  last dependency
    if (1 == wait_num) {
      if (i == successor_num - 1 && ready_successors.empty()) {
        ExecuteReadyVertex(successor_ctx);
        return;
      } else {
        ready_successors.emplace_back(successor_ctx);
      }
    }
  }
  ExecuteReadyVertexs(ready_successors);
}

int GraphContext::Execute(DoneClosure&& done) {
  _done = std::move(done);
  ExecuteReadyVertexs(_start_ctxs);
  return 0;
}
}  // namespace didagle
