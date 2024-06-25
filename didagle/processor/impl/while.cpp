// -*- coding: utf-8-unix; -*-
// Copyright (c) 2023 Tencent, Inc.
//     All rights reserved.
//
// Author: faithzeng@tencent.com
// Date: 2023/6/25 15:54
// Description:
//

#include <gflags/gflags.h>
#include <memory>

// #include "trpc/coroutine/fiber.h"
// #include "trpc/coroutine/fiber_latch.h"

#include "didagle/log/log.h"
#include "didagle/processor/processor.h"
#include "folly/String.h"

GRAPH_OP_BEGIN(didagle_while)

int OnExecute(const didagle::Params& args) override { return 0; }
GRAPH_OP_END
