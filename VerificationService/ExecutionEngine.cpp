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
 * File:   ExecutionEngine.cpp
 * Author: Tomas Kratochvila <tomas.kratochvila at honeywell.com>
 * Author: Petr Bauch <petr.bauch at honeywell.com>
 */

#include <functional>
#include <iostream>

#include "ExecutionEngine.h"
#include "Workspace.h"
//#include "DataStore.h"

namespace ExecutionEngine {

/// Initialize the "table" map.
/// in .. the input to the initilializer; e.g. "i1,p22,o333" sets the map to {0 |-> 1, 1 |-> 333, 2 |-> 22}
ParMap::ParMap(const String& in) {
  DEB("ParMap initializer input: " << in);
  Strings list;
  bbb::split_by(in, list, ",");
  for (auto& s : list) {
    if (s.size() <= 1)
      continue;

    auto mid = bbb::s2u(s.data() + 1, s.size() - 1);
    if (!mid)
      continue;
    Nat id = mid.value();
    DEB("id = " << id);
    switch (s[0]) {
    case 'i' : table.push_back({0, id}); break;		// inputs
    case 'o' : table.push_back({1, id}); break;		// outputs
    case 'p' : table.push_back({2, id}); break; 	// ?parameters
    default : break;
    }
  }
  
  String tmp;
  bbb::vector_print(table, "|", tmp, [&] (auto p) { return "<" + std::to_string(p.first) + "," + std::to_string(p.second) + ">"; });
  DEB("Loaded schema \"" + tmp + "\"");
}

/// Get the pair of <parameter, function value> for the given index according to the mapping defined in "table".
/// index .. 0-based order of filling in the mapping
bbb::Maybe<ParMap::IdPair> ParMap::mapping(Nat index) {
  if (table.size() > index)
  {
    DEB("Selected mapping(" + std::to_string(index) + "): <" + std::to_string(table[index].first) + "," + std::to_string(table[index].second) + ">");
    return table[index];
  }
  return {};
}

Run::Run(Archive::ReportID reportID, Archive::Archive& archive, Workspace::SharedWorkspace& workspace) : 
      Run(archive.borrow_report(reportID), reportID, archive, workspace) { }

Run::Run(Archive::BorrowedReport&& borrowedReport, Archive::ReportID reportID, Archive::Archive& archive, Workspace::SharedWorkspace& workspace):
    archive(archive),
    startTime(SClock::now()),
    outFileName(workspace->getCanonicalPath() + "/" + "out"),
    errFileName(workspace->getCanonicalPath() + "/" + "err"),
//    outFileName(workspace->getCanonicalPath() + "/" + bbb::time_to_string(startTime,"%F_%T") + "-out"),
//    errFileName(workspace->getCanonicalPath() + "/" + bbb::time_to_string(startTime,"%F_%T") + "-err"),
    procHandle(borrowedReport->callCommand,
                  subprocess::cwd{workspace->getCanonicalPath()},
                  subprocess::output{outFileName.c_str()},
                  subprocess::error{errFileName.c_str()},
                  subprocess::preexec_func{std::bind(&Run::prepareStdOutputFiles, this)}), // Make sure we are appending output to an empty file (and not last run's output)
    pid(procHandle.pid()),
    reportID(reportID),
    workspace(workspace),
    prevUTime(0),
    prevSTime(0) {
  DEB("Starting process: " + borrowedReport->callCommand + " , PID: " + std::to_string(pid) + " , erFileName = " + errFileName);
  borrowedReport->running = true;
  borrowedReport->pid = pid;
  borrowedReport->runningResult = "Started.";
  }

  void Run::prepareStdOutputFiles() {
    std::ofstream emptyFile1(outFileName, std::ios_base::trunc);
    std::ofstream emptyFile2(errFileName, std::ios_base::trunc);
  }

                  
/// Determine whether there is a child process to wait for, whether the child process statistics are readable,
/// and if the child process is not a zombie.
bool Run::is_running() {
  std::lock_guard<decltype(mutex)> lockGuard(mutex);
  auto poll_status = procHandle.poll();
  if (poll_status != -2) {// && poll_status != 0) {	// ; -2 .. child process still running.
    kill("Killed (poll_status " + std::to_string(poll_status) + ").");
    return false;
  }
  if (!get_stats(pid)) {
    kill("Failed to read process stats.");
    return false;
  }
  if (values[2] == "Z") {
    kill("Process is a zombie.");
    return false;
  }
  return true;
}
void Run::print_stats() {
  auto report = borrowReport();
  std::cout << "At " << bbb::time_to_string(report->resources.back().first)
            << " the process " << pid
            << " consumed " << report->resources.back().second.utime
            << " user time.\n";
}

void Run::print_output() {
  auto report = borrowReport();
  std::cout
    << " the process " << pid
    << " produced st. output " << report->stdOutput
    << " and er. output " << report->errOutput << std::endl;
}

double Run::cpu_usage(Nat l1, Nat l2, Nat g1, Nat g2) {
  double locDif = l2 - l1;
  double globDif = g2 - g1;
  return (100.0 * (locDif / globDif));
}

/// Read process statistics from /proc/[pid]/stat;
/// file structure is explained in http://man7.org/linux/man-pages/man5/proc.5.html
/// Returns whether the statistics could be read.
bool Run::get_stats(int pid) {
  std::lock_guard<decltype(mutex)> lockGuard(mutex);
  Strings lines;
  if (!bbb::read_file("/proc/" + std::to_string(pid) + "/stat", lines))
    return false;
  assert(!lines.empty());
  bbb::remove_ws(lines.back(), values);
  return true;
}

std::pair<String, String> Run::get_free_mem() {
  struct sysinfo info;
  sysinfo(&info);
  auto fRam = info.freeram * info.mem_unit;
  auto tRam = info.totalram * info.mem_unit;
  DEB("Free RAM: " + std::to_string(bbb::b2mb(fRam)) + " Total RAM: " + std::to_string(bbb::b2mb(tRam)));
  return { std::to_string(bbb::b2mb(fRam)),
           std::to_string(fRam / (tRam / 100)) };
}

void Run::try_update_stats(TimePoint time, uint32_t prevTTime, uint32_t curTTime) {
  std::lock_guard<decltype(mutex)> lockGuard(mutex);
  get_stats(pid);
  if (values.size() > 25) {
    curUTime = std::stol(values[15]);
    curSTime = std::stol(values[16]);
    auto mem = get_free_mem();
    borrowReport()->resources.push_back(
      {time,
        {   cpu_usage(prevUTime, curUTime, prevTTime, curTTime),
            cpu_usage(prevUTime, curUTime, prevTTime, curTTime),
            values[24],
            values[25],
            mem.first,
            mem.second
        }
      });
    print_stats();
    prevUTime = curUTime;
    prevSTime = curSTime;
  }
  values.clear();
}

void Run::update_report() {
  bbb::read_file(workspace->getCanonicalPath()+"/"+"partVerResult.txt", borrowReport()->partVerResult); // TODO: should be a suitable tmp file but wrappers generate this one
}
  
void Run::finalise_report() {
  std::lock_guard<decltype(mutex)> lockGuard(mutex);
  auto report = borrowReport();
  report->returnCode = procHandle.retcode();
  DEB("Finalising report.");
  try {
    // Read output files. If any of them do not exist, create empty file. (subprocess does not create file if there is no output)
    if (!bbb::read_file(outFileName, report->stdOutput)) {
      DEB("Out file " << outFileName << " does not exist. Creating empty file.");
      std::ofstream emptyFile(outFileName);
    }
    if (!bbb::read_file(errFileName, report->errOutput)) {
      DEB("Err file " << outFileName << " does not exist. Creating empty file.");
      std::ofstream emptyFile(errFileName);
    }
    // TODO: escape possible spaces in the following (quoting did not work):
    report->parsedOutput = subprocess::check_output("./" + report->tool.get_output_parser() + " " + outFileName + " " + errFileName + " " + std::to_string(report->returnCode)).buf.data();
  } catch (const subprocess::OSError& e) {
    report->parsedOutput = "ERROR";
  }
  DEB("After output.");
  report->runTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
  report->peakMemory = 0;
  for (auto& vs : report->resources) {
    auto answer = bbb::s2u(vs.second.vsize);
    assert(answer);
    if (answer.value() > report->peakMemory)
      report->peakMemory = answer.value();
  }
  report->date = startTime;
  update_report();
  report->running = false;
  report->runningResult = "Verification finished.";
  DEB("Finalised report: " + report->callCommand + "\n");
  report->validate();
}

void Run::kill(const String& debug_message) {
  std::lock_guard<decltype(mutex)> lockGuard(mutex);
  DEB(debug_message);
  procHandle.kill(9);
  endTime = SClock::now();
}

/// Get the sum of numbers in the first "cpu" line in the /pros/stat file.
/// These numbers are user/nice/system/idle/... CPU times in USER_HZ/Jiffies units (usually 1/100 s).
void ExecutionWindow::update_ttime() {
  std::lock_guard<decltype(mutex)> lockGuard(mutex);
  Strings lines;
  bbb::read_file("/proc/stat", lines);
  Strings gVals;
  assert(!lines.empty());
  bbb::remove_ws(lines[0], gVals);
  assert(!gVals.empty() && gVals[0] == "cpu");
  previousTtime = currentTtime;
  currentTtime = 0;
  for (size_t i = 1; i < gVals.size(); i++) {
    currentTtime += std::stol(gVals[i]);
  }
}

void ExecutionWindow::start_new_run(Archive::ReportID reportID, Archive::Archive& archive, Workspace::SharedWorkspace workspace, const String& call_schema) {
  try {
    std::lock_guard<decltype(mutex)> lockGuard(mutex);
    auto report = archive.borrow_report(reportID);
    // print debugging info
    String reportStr;
    report->to_string(reportStr);
    DEB("Report:\n" + reportStr);

    String com = report->tool.get_path() + " ";
    Strings iFiles;
    for (const Archive::FileID fid : report->inputFiles) {
      iFiles.push_back(workspace->getWorkspaceRelativeFilePath(fid));
    }
    ParMap pm(call_schema);
    // TODO: Verify that lengths of iFiles and parameters are sufficient, otherwise risking segfault
    bbb::zip_schema(
      {   iFiles,
          report->outputNames,
          report->parameters
      },
      " ",
      com,
      [&] (size_t index) { return pm.mapping(index); });
    bbb::replace_all(com, "  ", " ");
    while (isspace(com.back()))
      com.pop_back();
    report->callCommand = com;
    DEB("Starting verification process \"" + com + "\"");
    running.emplace_back(reportID, archive, workspace);
  }
  catch (const subprocess::OSError& e) {
    throw std::runtime_error("Launching verification process failed: " + std::string(e.what()));
  }
}

/// For each verification task update statistics, update report.
/// Kill the tasks that reached the timeout.
/// Finalize reports for not running tasks.
void ExecutionWindow::update_stats() {
  std::lock_guard<decltype(mutex)> lockGuard(mutex);
  DEB("Updating stats.");
  TimePoint now = SClock::now();
  update_ttime();
  std::for_each(running.begin(), running.end(),
                [&] (Run& run)
                {
                  run.try_update_stats(now, previousTtime, currentTtime);
                  run.update_report();
                  if (now - run.getLastMonitored() > monitorTimeout) {
                    run.kill();
                  }
                }
               );
  DEB("Stats updated.");

  running.remove_if([&] (Run& run)
                    {
                      if (run.is_running())
                        return false;
                      else {
                        run.finalise_report();
                        return true;
                      }
                    }
                   );
  DEB("Zombies removed.");
}

bool ExecutionWindow::kill_process(Nat pid) {
  std::lock_guard<decltype(mutex)> lockGuard(mutex);
  for (auto &run : running)
    if (run.pid == pid) {
      run.kill();
      return true;
    }
  return false;
}
}
