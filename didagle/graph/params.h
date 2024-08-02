/*
 *Copyright (c) 2021, qiyingwang <qiyingwang@tencent.com>
 *All rights reserved.
 *
 *Redistribution and use in source and binary forms, with or without
 *modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of rimos nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 *THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 *BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 *THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once
#include <stdint.h>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "folly/FBString.h"
#include "folly/FBVector.h"
#include "folly/container/F14Map.h"
#include "kcfg_toml.h"

namespace didagle {
typedef folly::fbstring ParamsString;
class Params;
using ParamsPtr = std::shared_ptr<Params>;
class Params {
 public:
  using ParamValueTable = folly::F14NodeMap<ParamsString, Params>;

 protected:
  enum ParamValueType {
    PARAM_INVALID = 0,
    PARAM_STRING,
    PARAM_INT,
    PARAM_DOUBLE,
    PARAM_BOOL,
    PARAM_OBJECT,
    PARAM_ARRAY,
  };
  ParamValueType _param_type = PARAM_INVALID;
  ParamsString str;
  int64_t iv;
  double dv;
  bool bv;
  bool invalid;

  using ParamValueArray = std::vector<Params>;
  ParamValueTable params;
  ParamValueArray param_array;

  const Params* parent = nullptr;

 public:
  explicit Params(bool invalid_ = false);
  static ParamsPtr New() { return std::make_shared<Params>(); }
  static ParamsPtr New(Params&& params) { return std::make_shared<Params>(std::move(params)); }
  void SetParent(const Params* p);
  bool Valid() const;
  bool IsBool() const;
  bool IsString() const;
  bool IsDouble() const;
  bool IsInt() const;
  bool IsObject() const;
  bool IsArray() const;
  const ParamsString& String() const;
  int64_t Int() const;
  bool Bool() const;
  double Double() const;
  void SetString(const ParamsString& v);
  void SetInt(int64_t v);
  void SetDouble(double d);
  void SetBool(bool v);
  size_t Size() const;
  const ParamValueTable& Members() const;

  const Params& operator[](const ParamsString& name) const;
  Params& operator[](const ParamsString& name);
  const Params& operator[](size_t idx) const;
  Params& operator[](size_t idx);

  Params& Add();
  const Params& Get(const ParamsString& name) const;
  Params& Put(const ParamsString& name, const char* value);
  Params& Put(const ParamsString& name, const ParamsString& value);
  Params& Put(const ParamsString& name, int64_t value);
  Params& Put(const ParamsString& name, double value);
  Params& Put(const ParamsString& name, bool value);
  Params& Put(const ParamsString& name, Params&& p);
  bool Contains(const ParamsString& name) const;
  void Insert(const Params& other);
  void BuildFromString(const std::string& v);
  void ParseFromString(const std::string& v);
  const Params& GetVar(const ParamsString& name) const;
};

struct GraphData {
  std::string id;
  std::string field;
  std::string move_from_when_skipped;
  std::vector<std::string> aggregate;
  bool required = false;
  bool move = false;
  bool is_extern = false;
  bool _is_in_out = false;

  KCFG_TOML_DEFINE_FIELD_MAPPING(({"is_extern", "extern"}))
  KCFG_TOML_DEFINE_FIELDS(id, field, move_from_when_skipped, required, move, is_extern, aggregate)
};

struct ConfigSetting {
  std::string name;
  std::string cond;
  std::string processor;
  KCFG_TOML_DEFINE_FIELDS(name, cond, processor)
};

struct GraphParams : public Params {
  bool ParseFromToml(const kcfg::TomlValue& doc);
};

inline bool isCondString(const std::string& str) {
  if (str.empty()) return false;
  for (const auto& ch : str) {
    if (!isalpha(ch) && !isdigit(ch) && ch != '_') {
      return true;
    }
  }
  return false;
}

struct CondParams {
  std::string match;
  GraphParams args;
  KCFG_TOML_DEFINE_FIELDS(match, args)
  // 是否是条件表达式 包含有$符号算条件表达式
  bool IsCondExpr() const { return isCondString(match); }
};

}  // namespace didagle