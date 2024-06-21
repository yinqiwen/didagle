// Copyright (c) 2020, Tencent Inc.
// All rights reserved.
#include "didagle/store/cluster_context.h"

namespace didagle {

GraphContext* GraphClusterContext::GetRunGraph(const std::string& name) {
  if (nullptr != _running_graph) {
    return _running_graph;
  }
  if (nullptr != _last_runnin_graph && _last_runnin_graph->GetGraph()->name == name) {
    _running_graph = _last_runnin_graph;
  } else {
    auto found = _graph_context_table.find(name);
    if (found == _graph_context_table.end()) {
      DIDAGLE_ERROR("No graph:{} found in cluster:{}", name, _cluster->_name);
      return nullptr;
    }
    _running_graph = found->second.get();
    _last_runnin_graph = _running_graph;
  }
  _running_graph->SetGraphDataContext(_extern_data_ctx);
  _extern_data_ctx->SetChild(_running_graph->GetGraphDataContext(), 0);
  return _running_graph;
}
void GraphClusterContext::Reset() {
  _end_ustime = 0;
  _exec_params = nullptr;
  if (nullptr != _running_graph) {
    _running_graph->Reset();
    _running_graph = nullptr;
  }
  for (auto& item : _config_settings) {
    item.eval_proc->Reset();
    item.result = 0;
  }
  _extern_data_ctx = nullptr;
  _running_cluster.reset();
}
int GraphClusterContext::Setup(GraphCluster* c) {
  _cluster = c;
  if (!_cluster) {
    return -1;
  }

  for (const auto& cfg : c->config_setting) {
    Processor* p = ProcessorFactory::GetProcessor(cfg.processor);
    if (nullptr == p && c->strict_dsl) {
      DIDAGLE_ERROR("No processor found for {}", cfg.processor);
      return -1;
    }
    if (nullptr != p) {
      Params args;
      args.SetString(cfg.cond);
      if (0 != p->Setup(args)) {
        DIDAGLE_ERROR("Failed to setup expr processor", cfg.processor);
        return -1;
      }
      ConfigSettingContext item;
      item.eval_proc = p;
      _config_settings.push_back(item);
    }
  }
  for (auto& pair : _cluster->_graphs) {
    std::shared_ptr<GraphContext> g(new GraphContext);
    if (0 != g->Setup(this, pair.second)) {
      DIDAGLE_ERROR("Failed to setup graph:{}", pair.second->name);
      return -1;
    }
    _graph_context_table[pair.second->name] = g;
  }
  return 0;
}
int GraphClusterContext::Execute(const std::string& graph, DoneClosure&& done, GraphContext*& graph_ctx) {
  uint64_t start_exec_ustime = ustime();
  graph_ctx = nullptr;
  GraphContext* g = GetRunGraph(graph);
  if (nullptr == g) {
    if (done) {
      done(-1);
    }
    return 0;
  }
  graph_ctx = g;
  GraphDataContext& data_ctx = g->GetGraphDataContextRef();
  DIDAGLE_DEBUG("config setting size = {}", _config_settings.size());
  for (size_t i = 0; i < _config_settings.size(); i++) {
    Processor* p = _config_settings[i].eval_proc;
    p->SetDataContext(g->GetGraphDataContext());

    int eval_rc = 0;
    if (nullptr != _exec_params) {
      eval_rc = p->Execute(*_exec_params);
    } else {
      Params args;
      eval_rc = p->Execute(args);
    }
    if (0 != eval_rc) {
      _config_settings[i].result = 0;
    } else {
      _config_settings[i].result = 1;
    }
    bool* v = reinterpret_cast<bool*>(&_config_settings[i].result);
    data_ctx.Set(_cluster->config_setting[i].name, v);
  }

  /**
   * @brief diable data entry creation since the graph is executing while the
   * creation is not thread-safe.
   */
  data_ctx.DisableEntryCreation();
  DAGEventTracker* tracker = data_ctx.GetEventTracker();
  if (nullptr != tracker) {
    auto event = std::make_unique<DAGEvent>();
    event->start_ustime = start_exec_ustime;
    event->end_ustime = ustime();
    event->phase = PhaseType::DAG_GRAPH_GRAPH_PREPARE_EXECUTE;
    tracker->Add(std::move(event));
  }
  return g->Execute(std::move(done));
}

GraphClusterContext::~GraphClusterContext() {
  for (auto& item : _config_settings) {
    delete item.eval_proc;
  }
}

GraphClusterContext* GraphClusterHandle::GetContext(GraphStore* store, GraphExecuteOptionsPtr options) {
  GraphClusterContext* ctx = nullptr;
  if (contexts.try_dequeue(ctx)) {
    return ctx;
  }
  ctx = new GraphClusterContext(store, options);
  ctx->Setup(&cluster);
  return ctx;
}
void GraphClusterHandle::ReleaseContext(GraphClusterContext* p) {
  p->Reset();
  contexts.enqueue(p);
}
int GraphClusterHandle::Build(GraphStore* store, GraphExecuteOptionsPtr options) {
  if (0 != cluster.Build()) {
    DIDAGLE_ERROR("Failed to build toml script:{}", cluster._name);
    return -1;
  }
  if (cluster.strict_dsl) {
    GraphClusterContext ctx(store, options);
    if (0 != ctx.Setup(&cluster)) {
      return -1;
    }
    ctx.Reset();
  }
  for (int i = 0; i < cluster.default_context_pool_size; i++) {
    GraphClusterContext* ctx = new GraphClusterContext(store, options);
    ctx->Setup(&cluster);
    contexts.enqueue(ctx);
  }
  return 0;
}
GraphClusterHandle::~GraphClusterHandle() {
  GraphClusterContext* ctx = nullptr;
  while (contexts.try_dequeue(ctx)) {
    delete ctx;
  }
}

}  // namespace didagle
