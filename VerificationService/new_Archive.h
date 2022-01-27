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
 * File:   new_Archive.h
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

struct m_verification_result : v_stringable {
  void to_string(String& output) {
    return "";
  }
  void from_string(const String& input) {
    
  }
};

struct m_verification_report : v_stringable {
  m_verification_instance vi;
  m_verification_result vr;

};
  
struct Report {
  //permanent
  TK::Tool tool;
  Strings parameters;
  Strings inputFiles;
  Strings outputNames;
  String filePath;

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

  std::hash<String> hash_fn;

  Report() = delete;
  Report(const String& com) : callCommand(com),
                              id(hash_fn(com)) {
    rs.push_back(
      {SClock::now(),
        { "0", "0", "0", "0", "0", "0" }
      });
  }  
  Report(const TK::Tool& t, const Strings& params,
         const Strings& inputs, Nat oCount) : tool(t),
                                              parameters(params),
                                              inputFiles(inputs),
                                              runningResult("Not started.") {
    assert(stOutput.empty());
    for (Nat i = 0; i < oCount; i++) {
      outputNames.push_back(bbb::get_random_fname());
    }
    rs.push_back(
      {SClock::now(),
        { "0", "0", "0", "0", "0", "0" }
      });
    _hash();
  }
  
  bool read(std::ifstream& f);
  void write(std::iostream& f);

  void _hash() {
    //id = bbb:chash(inputFiles);
    id = tool.hash();
    for (auto &f : inputFiles)
      id ^= hash_fn(f);
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
      if (inputFiles[i] != inputFiles[i])
        return false;
    // if (callCommand != other.callCommand)
    //   return false;
    if (tool.hash() != other.tool.hash())
      return false;
    return true;
  }

  // void finalise_output() {
  //   for (auto& s : outputNames)
  //     outputFiles.push_back(File(s));
  //   outputNames.clear();
  // }
};
using Reports = std::vector<Report>;

//Wrap for facebook RocksDB
struct Archive {
  String database_path;
  rocksdb::DB* db;

  Archive() {
    database_path = "/tmp/rocksdb_database";
    rocksdb::Options options;
    options.IncreaseParallelism();
    options.OptimizeLevelStyleCompaction();
    options.create_if_missing = true;

    rocksdb::Status s = rocksdb::DB::Open(options, database_path, &db);
    assert(s.ok());
  }
  ~Archive() {
    delete db;
  }
  
  template<typename Key, typename Value>
  bool store(const Key& key, const Value& value) {
    rocksdb::Status s = db->Put(rocksdb::WriteOptions(),
                                key.to_string(),
                                value.to_string());
    return s.ok();
  }

  template<typename Key, typename Value>
  bool load(const Key& key, Value& value) {
    rocksdb::Status s = db->Get(rocksdb::ReadOptions(),
                                key.to_string(),
                                value.from_string());
    return s.ok();
  }
};

}
