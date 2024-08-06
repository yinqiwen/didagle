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
#include "didagle/store/graph_store.h"
#include <fmt/core.h>
#include <unistd.h>
#include <functional>
#include <memory>

namespace didagle {
static std::string get_basename(const std::string& filename) {
#if defined(_WIN32)
  char dir_sep('\\');
#else
  char dir_sep('/');
#endif

  std::string::size_type pos = filename.rfind(dir_sep);
  if (pos != std::string::npos)
    return filename.substr(pos + 1);
  else
    return filename;
}

GraphStore::GraphStore(const GraphExecuteOptions& options) {
  _exec_options = std::make_shared<GraphExecuteOptions>(options);
  // _async_reset_worker = std::make_unique<AsyncResetWorker>(options.async_reset_worker_num);
  _graph_exec_func_ =
      std::bind(&GraphStore::Execute, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
                std::placeholders::_4, std::placeholders::_5, std::placeholders::_6);
}

std::shared_ptr<GraphClusterHandle> GraphStore::LoadString(const std::string& content) {
  std::shared_ptr<GraphClusterHandle> g = std::make_shared<GraphClusterHandle>();
  bool v = kcfg::ParseFromTomlString(content, g->cluster);
  if (!v) {
    DIDAGLE_ERROR("Failed to parse didagle toml string:{}", content);
    return nullptr;
  }
  if (g->cluster.name.empty()) {
    DIDAGLE_ERROR("Missing 'name' in toml script", content);
    return nullptr;
  }
  g->cluster._name = g->cluster.name;
  if (0 != g->Build(this, _exec_options)) {
    return nullptr;
  }
  {
    std::lock_guard<std::mutex> guard(_graphs_mutex);
    _graphs[std::move(g->cluster.name)].store(g);
  }

  return g;
}
std::shared_ptr<GraphClusterHandle> GraphStore::Load(const std::string& file) {
  std::shared_ptr<GraphClusterHandle> g = std::make_shared<GraphClusterHandle>();
  bool v = kcfg::ParseFromTomlFile(file, g->cluster);
  if (!v) {
    DIDAGLE_ERROR("Failed to parse didagle toml script:{}", file);
    return nullptr;
  }
  std::string name = get_basename(file);
  g->cluster._name = name;
  // g->_graph_manager = this;
  if (0 != g->Build(this, _exec_options)) {
    return nullptr;
  }
  {
    std::lock_guard<std::mutex> guard(_graphs_mutex);
    _graphs[std::move(name)].store(g);
  }

  return g;
}
std::shared_ptr<GraphClusterHandle> GraphStore::FindGraphClusterByName(const std::string& name) {
  std::lock_guard<std::mutex> guard(_graphs_mutex);
  auto found = _graphs.find(name);
  if (found != _graphs.end()) {
    return found->second.load();
  }
  return nullptr;
}
GraphClusterContext* GraphStore::GetGraphClusterContext(const std::string& cluster) {
  std::shared_ptr<GraphClusterHandle> c = FindGraphClusterByName(cluster);
  if (!c) {
    return nullptr;
  }
  GraphClusterContext* ctx = c->GetContext(this, _exec_options);
  ctx->SetRunningCluster(c);
  return ctx;
}

Graph GraphStore::BuildGraphByTaskGroup(TaskGroupPtr group) {
  Graph group_g;
  group_g.name = group->Id();
  for (auto task : group->Tasks()) {
    Vertex vertex;
    vertex.id = task->Id();
    vertex.processor = task->processor_;
    if (vertex.processor.empty()) {
      vertex.cluster = group->ClusterId();
      vertex.graph = task->group_id_;
    }
    vertex.successor = task->succeed_;
    vertex.deps = task->precede_;
    vertex.consequent = task->consequent_;
    vertex.alternative = task->alternative_;
    // if (!vertex.consequent.empty() || !vertex.alternative.empty()) {
    //   vertex.ignore_processor_execute_error = false;
    // }
    group_g.vertex.emplace_back(vertex);
  }
  return group_g;
}

std::shared_ptr<GraphClusterHandle> GraphStore::LoadTaskGroup(TaskGroupPtr graph) {
  std::shared_ptr<GraphClusterHandle> c = FindGraphClusterByName(graph->ClusterId());
  if (c) {
    return c;
  }
  std::shared_ptr<GraphClusterHandle> g = std::make_shared<GraphClusterHandle>();
  g->cluster._name = graph->ClusterId();

  for (auto group : graph->SubTaskGroups()) {
    Graph group_g = BuildGraphByTaskGroup(group);
    g->cluster.graph.emplace_back(group_g);
  }
  g->cluster.graph.emplace_back(BuildGraphByTaskGroup(graph));

  if (0 != g->Build(this, _exec_options)) {
    return nullptr;
  }
  {
    std::lock_guard<std::mutex> guard(_graphs_mutex);
    _graphs[g->cluster._name].store(g);
  }

  return g;
}

bool GraphStore::Exists(const std::string& cluster, const std::string& graph) {
  std::shared_ptr<GraphClusterHandle> c = FindGraphClusterByName(cluster);
  if (!c) {
    return false;
  }
  return c->cluster.Exists(graph);
}

int GraphStore::Execute(GraphDataContextPtr data_ctx, const std::string& cluster, const std::string& graph,
                        ParamsPtr params, DoneClosure&& done, uint64_t time_out_ms) {
  if (!_exec_options->async_executor) {
    DIDAGLE_ERROR("Empty async_executor ");
    done(-1);
    return -1;
  }
  if (!data_ctx) {
    DIDAGLE_ERROR("Empty 'GraphDataContext'");
    done(-1);
    return -1;
  }
  // printf("00 data_ctx ref:%d\n", data_ctx.use_count());
  data_ctx->ReserveChildCapacity(1);
  GraphClusterContext* ctx = GetGraphClusterContext(cluster);
  if (!ctx) {
    DIDAGLE_ERROR("Find graph cluster {} failed.", cluster);
    done(-1);
    return -1;
  }
  ctx->SetExternGraphDataContext(data_ctx.get());
  ctx->SetExecuteParams(params.get());
  if (time_out_ms != 0) {
    ctx->SetEndTime(ustime() + time_out_ms * 1000);
  }
  running_graphs_.fetch_add(1);
  auto release_func = [this, ctx]() mutable {
    uint64_t start_exec_ustime = ustime();
    {
      std::shared_ptr<GraphClusterHandle> runing_cluster = ctx->GetRunningCluster();
      runing_cluster->ReleaseContext(ctx);
    }
    if (_exec_options->event_reporter) {
      DAGEvent event;
      event.start_ustime = start_exec_ustime;
      event.end_ustime = ustime();
      event.phase = PhaseType::DAG_PHASE_GRAPH_ASYNC_RESET;
      _exec_options->event_reporter(std::move(event));
    }
    running_graphs_.fetch_sub(1);
  };
  auto release_closure = [release_func](int rc) mutable { AsyncResetWorker::GetInstance()->Post(release_func); };
  data_ctx->SetReleaseClosure(std::move(release_closure));

  auto graph_done = [params, data_ctx, done](int code) mutable {
    data_ctx.reset();
    done(code);
    params.reset();

    // AsyncResetWorker::GetInstance()->Post([this, ctx]() mutable {
    //   uint64_t start_exec_ustime = ustime();
    //   {
    //     std::shared_ptr<GraphClusterHandle> runing_cluster = ctx->GetRunningCluster();
    //     runing_cluster->ReleaseContext(ctx);
    //   }
    //   if (_exec_options->event_reporter) {
    //     DAGEvent event;
    //     event.start_ustime = start_exec_ustime;
    //     event.end_ustime = ustime();
    //     event.phase = PhaseType::DAG_PHASE_GRAPH_ASYNC_RESET;
    //     _exec_options->event_reporter(std::move(event));
    //   }
    // });
  };
  // printf("2 data_ctx ref:%d\n", data_ctx.use_count());
  GraphContext* graph_ctx = nullptr;
  int rc = ctx->Execute(graph, std::move(graph_done), graph_ctx);
  // printf("3 data_ctx ref:%d\n", data_ctx.use_count());
  if (nullptr != graph_ctx) {
    data_ctx->SetChild(graph_ctx->GetGraphDataContext(), 0);
  }
  return rc;
}
int GraphStore::SyncExecute(GraphDataContextPtr data_ctx, const std::string& cluster, const std::string& graph,
                            ParamsPtr params, uint64_t time_out_ms) {
  if (!_exec_options->latch_creator) {
    DIDAGLE_ERROR("Empty 'latch_creator'");
    return -1;
  }
  auto latch = _exec_options->latch_creator(1);
  int code = 0;
  int rc = Execute(
      data_ctx, cluster, graph, params,
      [&](int rc) {
        code = rc;
        latch->CountDown();
      },
      time_out_ms);
  latch->Wait();
  if (0 != rc) {
    return rc;
  }
  return code;
}

int GraphStore::AsyncExecute(TaskGroupPtr graph, DoneClosure&& done, uint64_t time_out_ms) {
  std::shared_ptr<GraphClusterHandle> c = LoadTaskGroup(graph);
  if (!c) {
    done(-1);
    return -1;
  }
  GraphDataContextPtr data_ctx = GraphDataContext::New();
  return Execute(data_ctx, graph->ClusterId(), graph->Id(), nullptr, std::move(done), time_out_ms);
}

int GraphStore::SyncExecute(TaskGroupPtr graph, uint64_t time_out_ms) {
  if (!_exec_options->latch_creator) {
    DIDAGLE_ERROR("Empty 'latch_creator'");
    return -1;
  }
  auto latch = _exec_options->latch_creator(1);
  int code = 0;
  int rc = AsyncExecute(
      graph,
      [&](int rc) {
        code = rc;
        latch->CountDown();
      },
      time_out_ms);
  latch->Wait();
  if (0 != rc) {
    return rc;
  }
  return code;
}

GraphStore::~GraphStore() {
  while (running_graphs_.load() > 0) {
    usleep(kWaitRunningGraphCompleteTimeUs);
  }
}

}  // namespace didagle