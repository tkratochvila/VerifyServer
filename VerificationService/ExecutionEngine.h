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
 * File:   ExecutionEngine.h
 * Author: Tomas Kratochvila <tomas.kratochvila at honeywell.com>
 * Author: Petr Bauch <petr.bauch at honeywell.com>
 */

#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <list>
#include <cstdio>//remove
#include <sys/sysinfo.h>//sysconf

#include "bbb.h"
#include "subprocess.hpp"
#include "Archive.h"
#include "Workspace.h"

namespace ExecutionEngine {

using namespace Basics;
using IOT = subprocess::IOTYPE;
using namespace std::chrono_literals;

struct ParMap {
  using IdPair = std::pair<Nat, Nat>;
  std::vector<IdPair> table;

  ParMap(const String& in);
  bbb::Maybe<IdPair> mapping(Nat index);
};

class Run {
public:
  mutable std::recursive_mutex mutex;
  Archive::Archive& archive;
  TimePoint startTime;
  String outFileName;
  String errFileName;
  subprocess::Popen procHandle;
  Nat pid;
  TimePoint endTime;

  Archive::ReportID reportID;
  Workspace::SharedWorkspace workspace;
  Strings values;	// temporary statistics, read by get_stats() from /proc/[pid]/stat
  Nat prevUTime, curUTime, prevSTime, curSTime;

  Run() = delete;
  Run(Archive::ReportID reportID, Archive::Archive& archive, Workspace::SharedWorkspace& workspace);
  Run(const Run& other) = delete;
  Run(Run&& other) = delete;
  ~Run() { }

  Run& operator=(const Run& other) = delete;
  Run& operator=(Run&& other) = delete;
  
  bool is_running();
  void print_stats();
  void print_output();

  static double cpu_usage(Nat l1, Nat l2, Nat g1, Nat g2);

  bool get_stats(int pid);
  static std::pair<String, String> get_free_mem();
  void try_update_stats(TimePoint time, Nat prevTTime, Nat curTTime);
  void update_report();
  TimePoint getLastMonitored() {return borrowReport()->lastMonitored;}
  void finalise_report();

  void kill(const String& debug_message = "");
private:
  Run(Archive::BorrowedReport&& borrowedReport, Archive::ReportID reportID, Archive::Archive& archive, Workspace::SharedWorkspace& workspace);
  Archive::BorrowedReport borrowReport() {return archive.borrow_report(reportID);}
  void prepareStdOutputFiles();
};

struct ExecutionWindow {
  mutable std::recursive_mutex mutex;
  std::list<Run> running;
  Nat currentTtime, previousTtime;
  Dur monitorTimeout;

  ExecutionWindow() : currentTtime(0), monitorTimeout(1min) { update_ttime(); }

  void update_ttime();
  void start_new_run(Archive::ReportID report, Archive::Archive& archive, Workspace::SharedWorkspace workspace,
                    const String& call_schema);
  void update_stats();
  bool empty() const  {std::lock_guard<decltype(mutex)> lockGuard(mutex); return running.empty(); }
  size_t size() const {std::lock_guard<decltype(mutex)> lockGuard(mutex); return running.size(); }
  bool kill_process(Nat pid);
};

}
