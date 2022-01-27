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
 * File:   DataStore.h
 * Author: Tomas Kratochvila <tomas.kratochvila at honeywell.com>
 * Author: Petr Bauch <petr.bauch at honeywell.com>
 */

#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <map>

#include "bbb.h"
//#include "FileSupport.h"
#include "ToolKit.h"

namespace Archive {

using namespace Basics;
// using File = FS::File;
// using Files = std::vector<File>;

//CPU Usage user time (%) -- double precision
//CPU Usage system time (%) -- double precision  
//Memory VSize (kB) -- 32bit unsigned
//Memory RSS (kB) -- 32bit unsigned
using Resources = std::vector<std::pair<TP, Strings>>;

struct m_hashable_string {
  String s;
  m_hashable_string(const String& s) : s(s) {}
  operator String&() { return s; }
  Hash hash() const {
    std::hash<String> fu;
    return fu(s);
  }
  bool operator==(const m_hashable_string& other) const {
    return s == other.s;
  }
};

const uint8_t subSize = 10;
const uint8_t secSize = 20;

struct m_verification_instance {
  String tool_name;
  Strings input_files;
  String call_parameters;

  
  String to_string() {
    return "";
  }

  void from_string(const String& in) {

  }
};

struct m_verification_result {
  String to_string() {
    return "";
  }
  void from_string(const String& in) {

  }
};
  
struct Report {
  //permanent
  TK::Tool& tool;
  Strings parameters;
  Strings inputFiles;
  Strings outputNames;
  String filePath;
  String automation_plan;

  //once finished
  String callCommand;
  Dur runTime;
  long peakMemory;
  TP date;
  Hash id;
  String partVerResult;

  //while running
  String stOutput;
  String erOutput;
  String runningResult;
  //Files outputFiles;
  Resources rs;
  bool running;
  String pid;
  TP lastMonitor;
  bool valid;

  std::hash<String> hash_fn;

  Report() = delete;
  Report(TK::Tool& t, const Strings& params,
         const Strings& inputs,
         const String& ap, Nat oCount) : tool(t),
                                         parameters(params),
                                         inputFiles(inputs),
                                         automation_plan(ap),
                                         runningResult("Not started."),
                                         valid(false) {
    assert(stOutput.empty());
    for (Nat i = 0; i < oCount; i++) {
      outputNames.push_back(bbb::get_random_fname());
    }
    rs.push_back(
      {SClock::now(),
        { "0", "0", "0", "0", "0", "0" }
      });
    _hash();
    update_monitor();
  }
  
  bool read(std::ifstream& f);
  bool from_string(const String& in);
  void to_string(String& out);
  void write(std::iostream& f);

  void _hash() {
    //id = bbb:chash(inputFiles);
    id = tool.hash();
    for (auto &f : inputFiles)
      id ^= hash_fn(f);
    id ^= hash_fn(automation_plan);
    //id ^= hash_fn(callCommand);// + std::hash<String>{}(version);
  }

  Hash hash() const {
    return id;
  }

  bool operator==(const Report& other) const {
    DEB("Calling eq on reports.");
    if (inputFiles.empty()) {
      return id == other.id;
    }
    if (inputFiles.size() != other.inputFiles.size())
      return false;
    for (Nat i = 0; i < inputFiles.size(); i++)
      if (inputFiles[i] != other.inputFiles[i])
        return false;
    // if (callCommand != other.callCommand)
    //   return false;
    for (Nat i = 0; i < parameters.size(); i++)
      if (parameters[i] != other.parameters[i])
        return false;
    if (tool.hash() != other.tool.hash())
      return false;
    if (automation_plan != other.automation_plan)
      return false;
    return true;
  }

  void update_monitor() {
    lastMonitor = SClock::now();
  }
  // void finalise_output() {
  //   for (auto& s : outputNames)
  //     outputFiles.push_back(File(s));
  //   outputNames.clear();
  // }
  void validate() {
    valid = true;
  }
  bool is_valid() const {
    return valid;
  }

  void log() const {
                    std::stringstream s;
                    std::time_t now_c = SClock::to_time_t(SClock::now());
                    s << "--------------------start time: "
                      << std::put_time(std::localtime(&now_c), "%F %T")
                      << "--------------------\n";

                    s << "Call command: ";
                    s << "Input files: ";
                    for (auto &file : input_files) {
                      file.printout(s);
                    }

                    now_c = SClock::to_time_t(SClock::now());
                    s << "--------------------end time: "
                    << std::put_time(std::localtime(&now_c), "%F %T")
                    << "--------------------\n";

  }
};

using Reports = std::vector<Report>;

struct Archive {
  bbb::UHash<Report> reportStore;
  String reportPath;

  bbb::UHash<m_hashable_string> fileStore;
  String filePath;

  Archive() {}
  Archive(const String& rp, const String& fp) : reportPath(rp), filePath(fp) {}
  ~Archive() {
    /*    for (size_t i = 0; i < fileStore.store.size(); i++) {
      String file_name = "tmp_" + std::to_string(i);
      if (bbb::file_exists(file_name))
        std::remove(file_name.c_str());
        }*/
  }

  template<class... Args>
  std::pair<bool, Nat> checkin_report(Args&&... args) {
    Report r(args...);
    r._hash();
    r.filePath = filePath;
    return reportStore.insert(r);
  }

  Report& get_report(Nat id) {
    return reportStore.decode_mod(id);
  }

  bool has_report(Nat id) {
    return reportStore.has_id(id);
  }

//  void print_report_store(String& out)
  std::pair<bool, Nat> checkin_file(const String& content) {
    m_hashable_string hs(content);
    auto answer = fileStore.insert(hs);
    if (!answer.first)
      return answer;
    std::ofstream s(filePath +  "tmp_" + std::to_string(answer.second));
    // if (!s.is_open())
    //   return false;
    s << content;
    s.close();
    return answer;
  }
};

}
