// Copyright (c) 2021, Tencent Inc.
// All rights reserved.
// Created on 2021/04/16
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
#include <utility>
#include <vector>
#include "didagle/graph/params.h"
#include "kcfg_toml.h"
namespace didagle {
enum VertexResult {
  V_RESULT_INVALID = 0,
  V_RESULT_OK = 1,
  V_RESULT_ERR = 2,
  V_RESULT_SKIP = 4,
  V_RESULT_ALL = 7,
};

enum VertexErrCode {
  V_CODE_INVALID = 0,
  V_CODE_OK = 1,
  V_CODE_ERR = 2,
  V_CODE_SKIP = 3,
};

struct Graph;
struct Vertex {
  std::string id;

  // phase node
  std::string processor;
  GraphParams args;
  std::vector<CondParams> select_args;
  std::string cond;
  std::string expect;
  std::set<std::string> expect_deps;  // expect可能有独立的依赖
  std::string expect_config;
  bool is_start = false;

  // sub graph node
  std::string cluster;
  std::string graph;

  // while sub graph node
  std::string while_cond;
  bool while_async = true;

  // dependents or successors
  std::set<std::string> successor;
  std::set<std::string> successor_on_ok;
  std::set<std::string> successor_on_err;
  std::set<std::string> consequent;
  std::set<std::string> alternative;
  std::set<std::string> deps;
  std::set<std::string> deps_on_ok;
  std::set<std::string> deps_on_err;

  std::vector<GraphData> input;
  std::vector<GraphData> output;

  bool ignore_processor_execute_error = true;
  bool early_exit_graph_if_failed = false;

  std::unordered_set<Vertex*> _successor_vertex;
  std::vector<VertexResult> _deps_expected_results;
  std::unordered_map<Vertex*, int> _deps_idx;
  Graph* _graph = nullptr;
  Graph* _vertex_graph = nullptr;
  bool _is_id_generated = false;
  bool _is_cond_processor = false;
  bool _disable = false;

  KCFG_TOML_DEFINE_FIELD_MAPPING(({"consequent", "if"}, {"alternative", "else"}, {"is_start", "start"},
                                  {"while_cond", "while"}, {"while_async", "async"}))

  KCFG_TOML_DEFINE_FIELDS(id, processor, args, cond, expect, expect_deps, expect_config, is_start, select_args, cluster,
                          graph, while_cond, while_async, successor, successor_on_ok, successor_on_err, consequent,
                          alternative, deps, deps_on_ok, deps_on_err, input, output, ignore_processor_execute_error,
                          early_exit_graph_if_failed)
  Vertex();
  bool IsDepsEmpty() const {
    return expect.empty() && expect_config.empty() && deps.empty() && deps_on_ok.empty() && deps_on_err.empty();
  }
  bool IsCondVertex() const;
  void MergeSuccessor();
  bool FindVertexInSuccessors(Vertex* v) const;
  int FillInputOutput();
  void SetGeneratedId(const std::string& id);
  bool IsSuccessorsEmpty();
  bool IsDepsEmpty();
  bool Verify();
  void Depend(Vertex* v, VertexResult expected);
  int BuildDeps(const std::set<std::string>& dependency, VertexResult expected);
  int BuildSuccessors(const std::set<std::string>& sucessor, VertexResult expected);
  int Build();
  std::string GetDotLable() const;
  std::string GetDotId() const;
  int DumpDotDefine(std::string& s);
  int DumpDotEdge(std::string& s);
  int GetDependencyIndex(Vertex* v);
};

}  // namespace didagle
