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
#include "didagle/log/log.h"
#include "didagle/processor/api.h"

GRAPH_OP_BEGIN(recall_3)
GRAPH_OP_IN_OUT((std::string), abc)
GRAPH_OP_MAP_INPUT((std::string), ms)
int OnSetup(const didagle::Params& args) override { return 0; }
int OnExecute(const didagle::Params& args) override {
  if (nullptr == abc) {
    DIDAGLE_DEBUG("empty abc");
  } else {
    DIDAGLE_DEBUG("abc = {}", *abc);
  }

  DIDAGLE_DEBUG("map size={}", ms.size());
  for (const auto& kv : ms) {
    DIDAGLE_DEBUG("map key={}", kv.first);
  }
  return 0;
}
GRAPH_OP_END

GRAPH_OP_BEGIN(recall_4)
GRAPH_OP_IN_OUT((std::string), abc)
GRAPH_OP_MAP_INPUT((std::string), ms)
int OnSetup(const didagle::Params& args) override { return 0; }
int OnExecute(const didagle::Params& args) override {
  if (nullptr == abc) {
    DIDAGLE_DEBUG("empty abc");
  } else {
    DIDAGLE_DEBUG("abc = {}", *abc);
  }

  DIDAGLE_DEBUG("map size={}", ms.size());
  for (const auto& kv : ms) {
    DIDAGLE_DEBUG("map key={}", kv.first);
  }
  return 0;
}
GRAPH_OP_END