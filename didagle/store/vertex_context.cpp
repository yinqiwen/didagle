// Copyright (c) 2020, Tencent Inc.
// All rights reserved.
#include "didagle/store/vertex_context.h"

#include "didagle/graph/vertex.h"
#include "folly/executors/InlineExecutor.h"

#include "didagle/store/cluster_context.h"
#include "didagle/store/graph_context.h"
#include "didagle/store/graph_store.h"

namespace didagle {

VertexContext::~VertexContext() {
  delete _processor;
  delete _processor_di;
  for (auto select : _select_contexts) {
    if (select.p != nullptr) {
      delete select.p;
    }
  }
  // delete _subgraph;
}

int VertexContext::Setup(GraphContext* g, Vertex* v) {
  _graph_ctx = g;
  _vertex = v;
  // todo get processor
  if (!_vertex->graph.empty()) {
    if (!_vertex->while_cond.empty()) {
      _processor = ProcessorFactory::GetProcessor(std::string(kDefaultWhileOperatorName));
    } else {
      // get subgraph at runtime
      // remove suffix
      size_t lastindex = _vertex->cluster.find_last_of(".");
      if (lastindex != std::string::npos) {
        _full_graph_name = _vertex->cluster.substr(0, lastindex);
      } else {
        _full_graph_name = _vertex->cluster;
      }
      _full_graph_name.append("_").append(_vertex->graph);
    }
  } else {
    _processor = ProcessorFactory::GetProcessor(_vertex->processor);
    if (_processor) {
      _processor->id_ = _vertex->id;
    }
  }
  if (_graph_ctx->GetGraphCluster()->strict_dsl) {
    if (nullptr == _processor && !_vertex->processor.empty()) {
      DIDAGLE_ERROR("No processor found for {}", _vertex->processor);
      return -1;
    }
  }
  _params = _vertex->args;

  for (const auto& select : _vertex->select_args) {
    SelectCondParamsContext select_ctx;
    select_ctx.param = select;
    if (select.IsCondExpr()) {
      select_ctx.p = ProcessorFactory::GetProcessor(_graph_ctx->GetGraphCluster()->default_expr_processor);
      if (select_ctx.p == nullptr && _graph_ctx->GetGraphCluster()->strict_dsl) {
        DIDAGLE_ERROR("No processor found for {}", _graph_ctx->GetGraphCluster()->default_expr_processor);
        return -1;
      }
      if (nullptr != select_ctx.p) {
        didagle::Params param;
        param.SetString(select.match);
        if (0 != select_ctx.p->Setup(param)) {
          DIDAGLE_ERROR("Failed to setup expr processor", _graph_ctx->GetGraphCluster()->default_expr_processor);
          return -1;
        }
      }
    }
    _select_contexts.emplace_back(select_ctx);
  }

  if (nullptr != _processor) {
    _processor_di = new ProcessorDI(_processor, _graph_ctx->GetGraphCluster()->strict_dsl);
    if (0 != _processor_di->PrepareInputs(_vertex->input)) {
      return -1;
    }
    if (0 != _processor_di->PrepareOutputs(_vertex->output)) {
      return -1;
    }
  }
  Reset();
  if (nullptr != _processor) {
    if (!_vertex->cond.empty()) {
      _params.SetString(_vertex->cond);
    }
    if (!_vertex->while_cond.empty()) {
      _params[std::string(kExprParamKey)].SetString(_vertex->while_cond);
      _params[std::string(kWhileExecCluterParamKey)].SetString(_vertex->cluster);
      _params[std::string(kWhileExecGraphParamKey)].SetString(_vertex->graph);
      _params[std::string(kWhileAsyncExecParamKey)].SetBool(_vertex->while_async);
    }
    return _processor->Setup(_params);
  }
  return 0;
}

void VertexContext::Reset() {
  _exec_start_ustime = 0;
  _result = V_RESULT_INVALID;
  _code = V_CODE_INVALID;
  _exec_rc = INT_MAX;
  _deps_results.assign(_vertex->_deps_idx.size(), V_RESULT_INVALID);
  _waiting_num = _vertex->_deps_idx.size();
  if (nullptr != _processor) {
    _processor->Reset();
  }
  _subgraph_ctx = nullptr;
  if (nullptr != _subgraph_cluster) {
    std::shared_ptr<GraphClusterHandle> running_cluster = _subgraph_cluster->GetRunningCluster();
    _subgraph_cluster->Reset();
    running_cluster->ReleaseContext(_subgraph_cluster);
    _subgraph_cluster = nullptr;
  }

  for (auto& select : _select_contexts) {
    select.param.args.SetParent(nullptr);
  }
  _params.SetParent(nullptr);
  _exec_params = nullptr;
  _exec_matched_cond = "";
}

VertexContext::VertexContext() { _waiting_num = 0; }

void VertexContext::SetupSuccessors() {
  for (Vertex* successor : GetVertex()->_successor_vertex) {
    VertexContext* successor_ctx = _graph_ctx->FindVertexContext(successor);
    if (nullptr == successor_ctx) {
      // error
      abort();
      continue;
    }
    _successor_ctxs.emplace_back(successor_ctx);
    int idx = successor->GetDependencyIndex(_vertex);
    _successor_dep_idxs.emplace_back(idx);
  }
}

void VertexContext::FinishVertexProcess(int code) {
  _code = (VertexErrCode)code;
  DAGEventTracker* tracker = _graph_ctx->GetGraphDataContextRef().GetEventTracker();
  if (_exec_rc == INT_MAX) {
    _exec_rc = _code;
  }
  auto exec_end_ustime = ustime();
  if (0 != _code) {
    if (0 == _result) {
      _result = V_RESULT_ERR;
    }

    if (nullptr != _processor_di) {
      _processor_di->MoveDataWhenSkipped(_graph_ctx->GetGraphDataContextRef());
    }
  } else {
    _result = V_RESULT_OK;
    if (nullptr != _processor_di) {
      uint64_t post_exec_start_ustime = ustime();
      _processor_di->CollectOutputs(_graph_ctx->GetGraphDataContextRef(), _exec_params);
      if (nullptr != tracker) {
        auto event = std::make_unique<DAGEvent>();
        event->start_ustime = post_exec_start_ustime;
        event->end_ustime = ustime();
        event->phase = PhaseType::DAG_PHASE_OP_POST_EXECUTE;
        tracker->Add(std::move(event));
      }
    }
  }
  if (nullptr != tracker) {
    auto event = std::make_unique<DAGEvent>();
    event->start_ustime = _exec_start_ustime;
    event->end_ustime = exec_end_ustime;
    if (nullptr != _processor) {
      event->processor = _processor->Name();
    } else {
      event->graph = _vertex->graph;
      event->cluster = _vertex->_graph->_cluster->_name;
      event->full_graph_name = _full_graph_name;
    }
    event->matched_cond = _exec_matched_cond;
    event->rc = _exec_rc;
    tracker->Add(std::move(event));
  }
  if (nullptr != _subgraph_ctx) {
    _graph_ctx->GetGraphDataContextRef().SetChild(_subgraph_ctx->GetGraphDataContext(), _child_idx);
  }
  _graph_ctx->OnVertexDone(this);
}
const Params* VertexContext::GetExecParams(std::string_view* matched_cond) {
  Params* exec_params = nullptr;
  const Params* cluster_exec_params = _graph_ctx->GetGraphClusterContext()->GetExecuteParams();
  // std::string_view matched_cond;
  if (!_select_contexts.empty()) {
    for (auto& select : _select_contexts) {
      auto& args = select.param;
      if (args.IsCondExpr() && nullptr != select.p) {
        select.p->SetDataContext(_graph_ctx->GetGraphDataContext());
        int eval_rc;
        if (cluster_exec_params != nullptr) {
          eval_rc = select.p->Execute(*cluster_exec_params);
        } else {
          Params params;
          eval_rc = select.p->Execute(params);
        }
        if (eval_rc == 0) {
          exec_params = &(args.args);
          *matched_cond = select.p->GetString(Processor::GetStringMode::kDefault);
          break;
        }
      } else {
        const bool* v = _graph_ctx->GetGraphDataContextRef().Get<bool>(args.match);
        if (nullptr == v) {
          DIDAGLE_DEBUG("0 Vertex:{} match {} null", _vertex->GetDotLable(), args.match);
          continue;
        }
        DIDAGLE_DEBUG("0 Vertex:{} match {} args {}", _vertex->GetDotLable(), args.match, *v);
        if (!(*v)) {
          continue;
        }
        DIDAGLE_DEBUG("Vertex:{} match {} args", _vertex->GetDotLable(), args.match);
        exec_params = &(args.args);
        *matched_cond = args.match;
        break;
      }
    }
  }
  if (nullptr == exec_params) {
    exec_params = &_params;
  }

  if (nullptr != cluster_exec_params) {
    exec_params->SetParent(cluster_exec_params);
  }
  return exec_params;
}

int VertexContext::ExecuteProcessor() {
  DIDAGLE_DEBUG("Vertex:{} begin execute", _vertex->GetDotLable());
  auto prepare_start_us = ustime();
  _processor->SetDataContext(_graph_ctx->GetGraphDataContext());
  _exec_params = GetExecParams(&_exec_matched_cond);
  _processor->Prepare(*_exec_params);
  if (0 != _processor_di->InjectInputs(_graph_ctx->GetGraphDataContextRef(), _exec_params)) {
    DIDAGLE_DEBUG("Vertex:{} inject inputs failed", _vertex->GetDotLable());
    FinishVertexProcess(V_CODE_SKIP);
    return 0;
  }
  auto prepare_end_ustime = ustime();
  DAGEventTracker* tracker = _graph_ctx->GetGraphDataContextRef().GetEventTracker();
  if (nullptr != tracker) {
    auto event = std::make_unique<DAGEvent>();
    event->start_ustime = prepare_start_us;
    event->end_ustime = prepare_end_ustime;
    event->phase = PhaseType::DAG_PHASE_OP_PREPARE_EXECUTE;
    tracker->Add(std::move(event));
  }
  _exec_start_ustime = ustime();
  switch (_processor->GetExecMode()) {
    case Processor::ExecMode::EXEC_ASYNC_FUTURE: {
      try {
        _processor->FutureExecute(*_exec_params).thenValue([this](int rc) {
          _exec_rc = rc;
          FinishVertexProcess((_vertex->ignore_processor_execute_error && !_vertex->IsCondVertex()) ? 0 : _exec_rc);
        });
      } catch (std::exception& ex) {
        DIDAGLE_ERROR("Vertex:{} execute with caught excetion:{} ", _vertex->GetDotLable(), ex.what());
        _exec_rc = V_CODE_ERR;
        FinishVertexProcess((_vertex->ignore_processor_execute_error && !_vertex->IsCondVertex()) ? 0 : _exec_rc);
      } catch (...) {
        DIDAGLE_ERROR("Vertex:{} execute with caught unknown excetion.", _vertex->GetDotLable());
        _exec_rc = V_CODE_ERR;
        FinishVertexProcess((_vertex->ignore_processor_execute_error && !_vertex->IsCondVertex()) ? 0 : _exec_rc);
      }
      break;
    }
#if ISPINE_HAS_COROUTINES
    case Processor::ExecMode::EXEC_STD_COROUTINE: {
      folly::QueuedImmediateExecutor* ex = &(folly::QueuedImmediateExecutor::instance());
      ispine::coro_spawn(ex, _processor->CoroExecute(*_exec_params)).via(ex).thenValue([this](int rc) {
        _exec_rc = rc;
        FinishVertexProcess((_vertex->ignore_processor_execute_error && !_vertex->IsCondVertex()) ? 0 : _exec_rc);
      });
      break;
    }
#endif
    case Processor::ExecMode::EXEC_ADAPTIVE: {
#if ISPINE_HAS_COROUTINES
      folly::QueuedImmediateExecutor* ex = &(folly::QueuedImmediateExecutor::instance());
      ispine::coro_spawn(ex, _processor->AExecute(*_exec_params)).via(ex).thenValue([this](int rc) {
        _exec_rc = rc;
        FinishVertexProcess((_vertex->ignore_processor_execute_error && !_vertex->IsCondVertex()) ? 0 : _exec_rc);
      });
#else
      try {
        _exec_rc = _processor->AExecute(*_exec_params);
      } catch (std::exception& ex) {
        DIDAGLE_ERROR("Vertex:{} execute with caught excetion:{} ", _vertex->GetDotLable(), ex.what());
        _exec_rc = V_CODE_ERR;
      } catch (...) {
        DIDAGLE_ERROR("Vertex:{} execute with caught unknown excetion.", _vertex->GetDotLable());
        _exec_rc = V_CODE_ERR;
      }
      FinishVertexProcess((_vertex->ignore_processor_execute_error && !_vertex->IsCondVertex()) ? 0 : _exec_rc);
#endif
      break;
    }
    default: {
      try {
        _exec_rc = _processor->Execute(*_exec_params);
      } catch (std::exception& ex) {
        DIDAGLE_ERROR("Vertex:{} execute with caught excetion:{} ", _vertex->GetDotLable(), ex.what());
        _exec_rc = V_CODE_ERR;
      } catch (...) {
        DIDAGLE_ERROR("Vertex:{} execute with caught unknown excetion.", _vertex->GetDotLable());
        _exec_rc = V_CODE_ERR;
      }
      FinishVertexProcess((_vertex->ignore_processor_execute_error && !_vertex->IsCondVertex()) ? 0 : _exec_rc);
      break;
    }
  }
  return 0;
}
int VertexContext::ExecuteSubGraph() {
  if (!_subgraph_cluster) {
    DIDAGLE_ERROR("No subgraph cluster found for {}", _vertex->cluster);
    return -1;
  }
  _exec_start_ustime = ustime();
  _exec_params = GetExecParams(&_exec_matched_cond);
  _subgraph_cluster->SetExternGraphDataContext(_graph_ctx->GetGraphDataContext());
  _subgraph_cluster->SetExecuteParams(_exec_params);
  // succeed end time
  _subgraph_cluster->SetEndTime(_graph_ctx->GetGraphClusterContext()->GetEndTime());
  _subgraph_cluster->Execute(
      _vertex->graph, [this](int code) { FinishVertexProcess(code); }, _subgraph_ctx);
  return 0;
}
int VertexContext::Execute() {
  bool match_dep_expected_result = true;
  if (!_vertex->cluster.empty() && nullptr != _graph_ctx->GetGraphClusterContext()->GetStore() &&
      nullptr == _subgraph_cluster) {
    _subgraph_cluster = _graph_ctx->GetGraphClusterContext()->GetStore()->GetGraphClusterContext(_vertex->cluster);
  }
  if (nullptr == _processor && nullptr == _subgraph_cluster) {
    match_dep_expected_result = false;
    DIDAGLE_DEBUG("Vertex:{} has empty processor and empty subgraph context.", _vertex->GetDotLable());
  } else {
    for (size_t i = 0; i < _deps_results.size(); i++) {
      if (V_RESULT_INVALID == _deps_results[i] || (0 == (_deps_results[i] & _vertex->_deps_expected_results[i]))) {
        match_dep_expected_result = false;
        break;
      }
    }
  }
  DIDAGLE_DEBUG("Vertex:{} match deps result:{}.", _vertex->GetDotLable(), match_dep_expected_result);
  if (match_dep_expected_result) {
    if (!_vertex->expect_config.empty()) {
      std::string_view var_name = _vertex->expect_config;
      bool not_cond = false;
      if (var_name[0] == '!') {
        var_name = var_name.substr(1);
        not_cond = true;
      }
      const bool* v = _graph_ctx->GetGraphDataContextRef().Get<bool>(var_name);
      bool match_result = false;
      if (nullptr != v) {
        match_result = *v;
      }
      if (not_cond) {
        match_result = !match_result;
      }
      if (!match_result) {
        match_dep_expected_result = false;
        DIDAGLE_DEBUG("Vertex:{} match expect_config:{} failed.", _vertex->GetDotLable(), _vertex->expect_config);
      } else {
        DIDAGLE_DEBUG("Vertex:{} match expect_config:{} success.", _vertex->GetDotLable(), _vertex->expect_config);
      }
    }
  }

  if (!match_dep_expected_result || (_graph_ctx->GetGraphClusterContext()->GetEndTime() != 0 &&
                                     ustime() >= _graph_ctx->GetGraphClusterContext()->GetEndTime())) {
    if (_vertex->_graph->vertex_skip_as_error) {
      _result = V_RESULT_ERR;
    } else {
      _result = V_RESULT_SKIP;
    }

    _code = V_CODE_SKIP;
    // no need to exec this
    FinishVertexProcess(_code);
  } else {
    if (nullptr != _processor) {
      ExecuteProcessor();
    } else {
      ExecuteSubGraph();
    }
  }
  return 0;
}

}  // namespace didagle
