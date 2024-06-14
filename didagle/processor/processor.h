// Copyright (c) 2021, Tencent Inc.
// All rights reserved.
// Created on 2021/04/16
// Authors: qiyingwang (qiyingwang@tencent.com)
#pragma once
#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "didagle/processor/api.h"

namespace didagle {

struct ProcessorMeta {
  std::string name;
  std::string desc;
  bool isIOProcessor = false;
  std::vector<FieldInfo> input;
  std::vector<FieldInfo> output;
  std::vector<ParamInfo> params;
  KCFG_DEFINE_FIELDS(name, desc, isIOProcessor, input, output, params)
};

class ProcessorFactory {
 public:
  static bool Register(std::string_view name, const ProcessorCreator& creator);
  template <typename T>
  static bool Register() {
    auto creator = []() -> Processor* { return new T; };
    T tmp;
    auto name = tmp.Name();
    return Register(name, creator);
  }
  static Processor* GetProcessor(const std::string& name);
  static void GetAllMetas(std::vector<ProcessorMeta>& metas);
  static int DumpAllMetas(const std::string& file = "all_processors.json");
};

struct ProcessorRunResult {
  std::shared_ptr<Processor> processor;
  int rc = -1;
  explicit ProcessorRunResult(int v = -1) : rc(v) {}
};

struct ProcessorRunOptions {
  const Params* params = nullptr;
  std::map<std::string, std::vector<std::string>> map_aggregate_ids;
};

class FuncProcessor : public Processor {
 public:
  FuncProcessor(const std::string& name, const ExecFunc& f) : func_(f) {}

 private:
  std::string_view Name() const { return name_; }
  int OnExecute(const Params& args) { return func_(args); }
  std::string name_;
  ExecFunc func_;
};

ProcessorRunResult run_processor(GraphDataContext& ctx, const std::string& proc, const ProcessorRunOptions& opts = {});

bool register_func_processor(std::string_view name, ExecFunc&& f);

}  // namespace didagle