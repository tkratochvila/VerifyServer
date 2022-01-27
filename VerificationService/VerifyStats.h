//+*****************************************************************************
//                         Honeywell Proprietary
// This document and all information and expression contained herein are the
// property of Honeywell International Inc., are loaned in confidence, and
// may not, in whole or in part, be used, duplicated or disclosed for any
// purpose without prior written permission of Honeywell International Inc.
//               This document is an unpublished work.
//
// Copyright (C) 2021 Honeywell International Inc. All rights reserved.
//+*****************************************************************************
/* 
 * File:   VerifyStats.h
 * Author: Tomas Kratochvila <tomas.kratochvila at honeywell.com>
 * Author: Petr Bauch <petr.bauch at honeywell.com>
 */

#pragma once

#include <string>
#include <vector>

namespace VerifyService {

class VerifyStats {
 public:
  virtual ~VerifyStats() {
  }

  // NOTE: We make the following methods `virtual` so that we can
  //       mock them using Gmock for our C++ unit-tests. PushStats
  //       is an external dependency to handler and we should be
  //       able to mock it.

  virtual void recordRequest(std::string req) {
    record.push_back(req);
  }

  virtual size_t getRequestCount() {
    return record.size();
  }

private:
  std::vector<std::string> record;
};

}
