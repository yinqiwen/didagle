/*
** BSD 3-Clause License
**
** Copyright (c) 2024, qiyingwang <qiyingwang@tencent.com>, the respective contributors, as shown by the AUTHORS file.
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

#include <fmt/core.h>
#include <gflags/gflags.h>
#include <stdlib.h>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "boost/asio/post.hpp"
#include "boost/asio/thread_pool.hpp"
#include "folly/Singleton.h"
#include "folly/synchronization/HazptrThreadPoolExecutor.h"
#include "folly/synchronization/Latch.h"
#include "spdlog/spdlog.h"

#include "didagle/log/log.h"
#include "didagle/store/graph_store.h"
#include "didagle/trace/event.h"

DEFINE_string(script, "", "script name");
DEFINE_string(graph, "", "graph name");
DEFINE_int32(test_count, 100, "test count");
DEFINE_int32(concurrent, 4, "concurrent test worker");
static int64_t gettimeofday_us() {
  struct timeval tv;
  uint64_t ust;
  gettimeofday(&tv, nullptr);
  ust = ((int64_t)tv.tv_sec) * 1000000;
  ust += tv.tv_usec;
  return ust;
}

using namespace didagle;
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
int main(int argc, char** argv) {
  google::ParseCommandLineFlags(&argc, &argv, false);
  folly::SingletonVault::singleton()->registrationComplete();
  // Set the default hazard pointer domain to use a thread pool executor
  // for asynchronous reclamation
  folly::enable_hazptr_thread_pool_executor();

  spdlog::set_level(spdlog::level::warn);

  boost::asio::thread_pool pool(8);
  GraphExecuteOptions exec_opt;
  exec_opt.async_executor = [&pool](AnyClosure&& r) {
    boost::asio::post(pool, r);
    // r();
  };
  exec_opt.async_reset_worker_num = FLAGS_concurrent;

  exec_opt.event_reporter = [&](DAGEvent event) {
    // fmt::print("##phase:{}\n", static_cast<int>(event.phase));
    // if (event.phase == PhaseType::DAG_PHASE_UNKNOWN) {
    //   proc_run_total_us += (event.end_ustime - event.start_ustime);
    // }
  };
  {
    GraphStore graphs(exec_opt);
    std::string config = FLAGS_script;
    std::string graph = FLAGS_graph;

    // std::string png_file = config + ".png";
    auto cluster = graphs.Load(config);
    if (!cluster) {
      printf("Failed to parse toml\n");
      return -1;
    }
    std::string cluster_name = get_basename(config);

    exec_opt.params.reset(new Params);

    Params paras;
    paras["x"].SetInt(1);
    paras["y"].SetInt(1);
    paras["A"].SetBool(true);
    paras["myid"].SetString("xyz");
    paras["myid0"].SetString("output");
    paras["expid"].SetInt(1000);
    paras["EXP"]["field1"].SetInt(1221);

    auto params_ptr = Params::New(std::move(paras));
    // fmt::print("##Start run graph!\n");

    auto test_func = [&]() {
      int64_t proc_run_total_us = 0;
      int64_t graph_run_us = 0;
      int64_t framework_run_us = 0;
      int64_t prpare_run_us = 0;
      int64_t post_run_us = 0;
      int64_t graph_prepare_run_us = 0;
      for (int i = 0; i < FLAGS_test_count; i++) {
        auto root = GraphDataContext::New();
        root->EnableEventTracker();
        // fmt::print("0 data_ctx ref {}\n", root.use_count());
        folly::Latch latch(1);
        int64_t exec_start_us = gettimeofday_us();
        graphs.Execute(root, cluster_name, graph, params_ptr, [&, root, exec_start_us](int c) mutable {
          // DIDAGLE_ERROR("Graph done with {}", c);

          graph_run_us += (gettimeofday_us() - exec_start_us);
          auto event_tracker = root->GetEventTracker();
          event_tracker->Sweep([&](DAGEvent* event) -> EventReportStatus {
            if (!event->processor.empty()) {
              proc_run_total_us += (event->end_ustime - event->start_ustime);
            } else {
              switch (event->phase) {
                case didagle::PhaseType::DAG_PHASE_OP_PREPARE_EXECUTE: {
                  framework_run_us += (event->end_ustime - event->start_ustime);
                  prpare_run_us += (event->end_ustime - event->start_ustime);
                  break;
                }
                case didagle::PhaseType::DAG_PHASE_OP_POST_EXECUTE: {
                  framework_run_us += (event->end_ustime - event->start_ustime);
                  post_run_us += (event->end_ustime - event->start_ustime);
                  break;
                }
                case didagle::PhaseType::DAG_GRAPH_GRAPH_PREPARE_EXECUTE: {
                  framework_run_us += (event->end_ustime - event->start_ustime);
                  graph_prepare_run_us += (event->end_ustime - event->start_ustime);
                  break;
                }
                default: {
                  fmt::print("#####{}\n", static_cast<int>(event->phase));
                  break;
                }
              }
            }

            return EventReportStatus::STATUS_NORMAL;
          });
          root.reset();
          // fmt::print("x data_ctx ref {}\n", root.use_count());
          latch.count_down();
        });
        // fmt::print("x ref {}\n", root.use_count());
        latch.wait();
        // fmt::print("y ref {}\n", root.use_count());
      }
      fmt::print("graph run cost {}us\n", graph_run_us);
      fmt::print("proc run cost {}us\n", proc_run_total_us);
      fmt::print("framework run cost {}us\n", framework_run_us);
      fmt::print("prepare run cost {}us\n", prpare_run_us);
      fmt::print("post run cost {}us\n", post_run_us);
      fmt::print("graph prepare run cost {}us\n", graph_prepare_run_us);
      fmt::print("gap {}us\n", graph_run_us - proc_run_total_us);
    };

    std::vector<std::thread> workers;
    for (int i = 0; i < FLAGS_concurrent; i++) {
      std::thread t(test_func);
      workers.emplace_back(std::move(t));
    }
    for (auto& t : workers) {
      t.join();
    }
  }
  pool.join();
  return 0;
}
