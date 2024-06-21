// Copyright (c) 2020, Tencent Inc.
// All rights reserved.
// Created on 2021/07/06
// Authors: qiyingwang (qiyingwang@tencent.com)

#include "didagle/log/log.h"
#include "didagle/processor/api.h"
#include "folly/String.h"
#include "gflags/gflags.h"

#include "ssexpr/spirit_expression.h"

DEFINE_int32(miss_expr_param_log_every_n, 1000, "txt");

GRAPH_OP_BEGIN(didagle_expr)
std::string _cond;  // NOLINT
ssexpr::SpiritExpression _expr;

std::string_view GetString(didagle::Processor::GetStringMode mode) const override { return _cond; }

int OnSetup(const Params& args) override {
  ssexpr::ExprOptions opt;
  opt.dynamic_var_access = [this](const void* root, const std::vector<std::string>& args) -> ssexpr::Value {
    if (args.size() == 1) {
      const bool* bv = GetDataContext().Get<bool>(args[0]);
      if (nullptr != bv) {
        bool rv = *bv;
        return rv;
      }
      const std::string* s = GetDataContext().Get<std::string>(args[0]);
      if (nullptr != s) {
        std::string_view sv = *s;
        return sv;
      }
      const int64_t* iv = GetDataContext().Get<int64_t>(args[0]);
      if (nullptr != iv) {
        int64_t rv = *iv;
        return rv;
      }
      const double* dv = GetDataContext().Get<double>(args[0]);
      if (nullptr != dv) {
        double rv = *dv;
        return rv;
      }
    }

    const didagle::Params* dynamic_vars = static_cast<const didagle::Params*>(root);
    for (const std::string& name : args) {
      dynamic_vars = &((*dynamic_vars)[name]);
    }
    ssexpr::Value r;
    if (dynamic_vars->IsInt()) {
      r = dynamic_vars->Int();
    } else if (dynamic_vars->IsDouble()) {
      r = dynamic_vars->Double();
    } else if (dynamic_vars->IsBool()) {
      r = dynamic_vars->Bool();
    } else if (dynamic_vars->IsString()) {
      r = dynamic_vars->String();
    } else {
      if (dynamic_vars->Valid()) {
        r = reinterpret_cast<const void*>(dynamic_vars);
      } else {
        bool rv = false;
        r = rv;
        std::string full_args;
        folly::join(".", args, full_args);
        DIDAGLE_WARN_EVERY_N(FLAGS_miss_expr_param_log_every_n, "Param:{} is not exist, use 'false' as param value.",
                             full_args);
      }
    }
    return r;
  };

  opt.functions["has_param"] = [](const std::vector<ssexpr::Value>& args) {
    ssexpr::Value r;
    if (args.size() < 2) {
      r = false;
      return r;
    }
    try {
      const void* v = std::get<const void*>(args[0]);
      if (nullptr == v) {
        r = false;
        return r;
      }
      const didagle::Params* params = reinterpret_cast<const didagle::Params*>(v);
      std::string_view param_key = std::get<std::string_view>(args[1]);
      folly::fbstring param_key_str(param_key.data(), param_key.size());
      r = params->Contains(param_key_str);
    } catch (std::exception& e) {
      r = false;
    }
    return r;
  };

  opt.functions["has_value"] = [](const std::vector<ssexpr::Value>& args) {
    ssexpr::Value r;
    if (args.size() != 2) {
      r = false;
      return r;
    }
    try {
      const void* v = std::get<const void*>(args[0]);
      if (nullptr == v) {
        r = false;
        return r;
      }
      const didagle::Params* params = reinterpret_cast<const didagle::Params*>(v);
      const int64_t* iv = std::get_if<int64_t>(&args[1]);
      const std::string_view* sv = nullptr;
      const double* dv = nullptr;
      const bool* bv = nullptr;
      if (nullptr == iv) {
        sv = std::get_if<std::string_view>(&args[1]);
        if (nullptr == sv) {
          dv = std::get_if<double>(&args[1]);
          if (nullptr == dv) {
            bv = std::get_if<bool>(&args[1]);
          }
        }
      }
      bool exist = false;
      for (size_t i = 0; i < params->Size(); i++) {
        const auto& p = (*params)[i];
        if (nullptr != iv) {
          exist = (p.Int() == *iv);
        } else if (nullptr != sv) {
          exist = (p.String() == *sv);
        } else if (nullptr != dv) {
          exist = (p.Double() == *dv);
        } else if (nullptr != bv) {
          exist = (p.Bool() == *bv);
        }
        if (exist) {
          break;
        }
      }
      r = exist;
    } catch (std::exception& e) {
      r = false;
    }
    return r;
  };

  _cond = args.String();
  int rc = _expr.Init(_cond, opt);
  if (0 != rc) {
    DIDAGLE_ERROR("expression:{}, init failed with rc:{}", _cond, rc);
  } else {
    DIDAGLE_DEBUG("expression:{}, init rc:{}", _cond, rc);
  }
  return rc;
}
int OnExecute(const didagle::Params& args) override {
  auto eval_val = _expr.EvalDynamic(args);
  try {
    bool r = std::get<bool>(eval_val);
    DIDAGLE_DEBUG("cond:{} eval result:{}", _cond, r);
    return r ? 0 : -1;
  } catch (std::exception& e) {
    ssexpr::Error* err = std::get_if<ssexpr::Error>(&eval_val);
    if (nullptr != err) {
      DIDAGLE_WARN_EVERY_N(FLAGS_miss_expr_param_log_every_n, "cond:{} eval exception:{}/{}", _cond, err->code,
                           err->reason);
    } else {
      DIDAGLE_WARN_EVERY_N(FLAGS_miss_expr_param_log_every_n, "cond:{} eval exception:{}", _cond, e.what());
    }
    return -1;
  }
}
GRAPH_OP_END