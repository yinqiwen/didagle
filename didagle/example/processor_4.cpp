// Copyright (c) 2020, Tencent Inc.
// All rights reserved.

#include "didagle/log/log.h"
#include "didagle/processor/api.h"

GRAPH_OP_BEGIN(phase4)
GRAPH_OP_MAP_INPUT(std::string, v100)
int OnSetup(const didagle::Params& args) override { return 0; }
int OnExecute(const didagle::Params& args) override {
  DIDAGLE_DEBUG("Run {} v100 size = {}", Name(), v100.size());
  for (auto& pair : v100) {
    DIDAGLE_DEBUG("{}->{}", pair.first, *(pair.second));
  }
  return 0;
}
GRAPH_OP_END
