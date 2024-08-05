// Copyright (c) 2021, Tencent Inc.
// All rights reserved.
// Created on 2021/04/16
// Authors: qiyingwang (qiyingwang@tencent.com)
#pragma once

#include <stdint.h>

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "folly/concurrency/AtomicSharedPtr.h"
#include "folly/container/F14Map.h"

#include "didagle/graph/vertex.h"
#include "kcfg_toml.h"

namespace didagle {

struct GraphCluster;
struct Graph {
  std::string name;
  std::vector<Vertex> vertex;
  bool vertex_skip_as_error = true;
  bool gen_while_subgraph = false;
  bool early_exit_graph_if_failed = false;
  int priority = -1;

  typedef std::unordered_map<std::string, Vertex*> VertexTable;
  std::vector<std::shared_ptr<Vertex>> _gen_vertex;
  VertexTable _nodes;
  VertexTable _data_mapping_table;
  int64_t _idx = 0;
  GraphCluster* _cluster = nullptr;
  bool _is_gen_while_graph = false;

  KCFG_TOML_DEFINE_FIELDS(name, vertex, priority, vertex_skip_as_error, gen_while_subgraph, early_exit_graph_if_failed)
  std::string generateNodeId();
  Vertex* geneatedCondVertex(const std::string& cond);
  Vertex* FindVertexByData(const std::string& data);
  Vertex* FindVertexById(const std::string& id);

  int Build();
  int DumpDot(std::string& s);
  bool TestCircle();
  ~Graph();
};

class GraphManager;
// class GraphClusterContext;
struct GraphCluster {
  std::string name;
  std::string desc;
  bool strict_dsl = true;
  std::string default_expr_processor;
  int64_t default_context_pool_size = 64;
  std::vector<Graph> graph;
  std::vector<ConfigSetting> config_setting;

  std::string _name;
  typedef std::map<std::string, Graph*> GraphTable;
  GraphTable _graphs;
  bool _builded = false;

  KCFG_TOML_DEFINE_FIELDS(name, desc, strict_dsl, default_expr_processor, default_context_pool_size, graph,
                          config_setting)

  int Build();
  bool ContainsConfigSetting(const std::string& name);
  int DumpDot(std::string& s);
  Graph* FindGraphByName(const std::string& name);
  bool Exists(const std::string& graph);
  ~GraphCluster();
};

}  // namespace didagle