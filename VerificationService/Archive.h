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
 * File:   Archive.h
 * Author: Tomas Kratochvila <tomas.kratochvila at honeywell.com>
 * Author: Petr Bauch <petr.bauch at honeywell.com>
 */

#pragma once

#include <experimental/filesystem>
#include <fstream>
#include <map>
#include <new>
#include <string>
#include <vector>
#include <utility>

#include "bbb.h"
#include "ToolKit.h"
#include "XMLSupport.h" // For OSLCReporter

namespace Archive {

using namespace Basics;
namespace filesystem = std::experimental::filesystem;

using ReportID = uint32_t;
using FileID = Hash;

struct m_hashable_string {
  String str;
  m_hashable_string(const String& s) : str(s) {}
  operator String&() { return str; }
  Hash hash() const {
    std::hash<String> fu;
    return fu(str);
  }
  bool operator==(const m_hashable_string& other) const {
    return str == other.str;
  }
};

const uint8_t subSize = 10;
const uint8_t secSize = 20;


/**
 * Resource usage information
 */
struct ResourceInformation {
  double utime = 0.0;
  double stime = 0.0;
  std::string vsize;
  std::string rss;
  std::string memFree;
  std::string memPerc;
};
//CPU Usage user time (%) -- double precision
//CPU Usage system time (%) -- double precision
//Memory VSize (kB) -- 32bit unsigned
//Memory RSS (kB) -- 32bit unsigned
using Resources = std::vector<std::pair<TimePoint, ResourceInformation>>;

/**
* Full monitoring information of a report
*/
struct ReportInformation : public ResourceInformation {
  ReportInformation(const ResourceInformation& resources) : ResourceInformation(resources) {};
  virtual ~ReportInformation() {}

  unsigned int pid;
  std::string stdOut;
  std::string errOut;
  std::string verResult;
  int retCode = 0;
  std::string parsedOutput;
  std::string planName;
  std::string runningResult;
};

/**
 * Holds XML structure of the report's status for OSLC monitoring request
 */
class OSLCReporter { // TODO: Ideally, this should be in a separate module in a directory together with the Archive module
public:
  /**
   * Constructs a new reporter based on the plan's name and public address of the server
   * @param automationPlanName
   * @param localAddress
   */
  OSLCReporter(const std::string& automationPlanName, const std::string& localAddress);
  
  /**
   * Generates an OSLC response to the monitor request
   * @param stats
   * @param message
   * @param result
   */
  std::string getOSLC(const ReportInformation& info);
private:
  /**
  * Creates a xml structure based on the automation plan's name and local address
  * @param automationPlan
  * @param localAddress Address the server is available at (e.g. "138.90.228.243")
  */
  void build_oslc_perf(const String& automationPlan, const std::string& localAddress);
  
  XMLSupport::Xml xml;
  XMLSupport::Index pidNode, utimeNode, stimeNode, vsizeNode, rssNode,
    bResultNode, freeNode, percNode;
  XMLSupport::Index stdOutNode, errOutNode, verResultNode, retCodeNode, parsedOutputNode;
};

/**
 * Report is a summary of a single verification task
 * it is created with a verification request
 * updated during verification execution
 * and returned with a monitoring request
 *
 * It contains reference to the verification tool
 * to mark is as busy/free
 */
class Report {
public:
  //permanent
  mutable std::recursive_mutex mutex;
  ToolKit::Tool& tool;
  Strings parameters;
  std::vector<FileID> inputFiles;
  Strings outputNames;
  String automationPlanName;
  OSLCReporter oslcReporter;

  //once finished
  String callCommand;
  Dur runTime;
  long peakMemory;
  TimePoint date;
  Hash id;
  String partVerResult;
  int returnCode;
  String parsedOutput;

  //while running
  String stdOutput;
  String errOutput;
  String runningResult;
  //Files outputFiles;
  Resources resources;
  bool running;
  int pid;
  TimePoint lastMonitored;
  bool valid;

  std::hash<String> hash_fn_string;
  std::hash<FileID> hash_fn_fileID;
  std::hash<size_t> hash_fn_size_t;

  Report() = delete;
  
  /**
   * Creates a new Report
   * @param tool Tool to run
   * @param params Tool's parameters
   * @param inputs Tool's inputs
   * @param automationPlanName Name of the automation plan
   * @param localAddress Local web address of the server
   * @param oCount Number of the tool's output files according to its schema
   */
  Report(ToolKit::Tool& tool, const Strings& params,
         const std::vector<FileID>& inputs,
         const String& automationPlanName,
         const String& localAddress,
         size_t oCount);
  // TODO: Tool should be kept as a shared pointer. It is not safe to rely on the reference.
    
  Report(const Report& other);
  
  bool read(std::ifstream& f);
  bool from_string(const String& in);
  void to_string(String& out);
  void write(std::iostream& f);

  void update_hash();
  Hash hash() const { return id; }
  bool operator==(const Report& other) const;

  ReportInformation getMonitoringInformation();
  std::string getMonitoringOSLC();
  TimePoint getLastMonitored() {std::lock_guard<decltype(mutex)> lockGuard(mutex); return lastMonitored;}
  void updateLastMonitored() {std::lock_guard<decltype(mutex)> lockGuard(mutex); lastMonitored = SClock::now(); }
  void validate() {std::lock_guard<decltype(mutex)> lockGuard(mutex); valid = true; }
  bool is_valid() const {std::lock_guard<decltype(mutex)> lockGuard(mutex); return valid; }
  
private:
  Report(const Report& other, const std::lock_guard<decltype(mutex)>& lockGuardOther);
};

/**
 * This class allows user to safely "borrow" a report from the Archive with exclusive access to both Archive and the report.
 * BorrowedReport holds a lock to Archive's mutex and to the Report's mutex and can only exist on a stack.
 * This ensures that the borrowed Report will not get deallocated/moved in memory
 * while holding a pointer/reference to it.
 * * and -> operators are overloaded, so that report can be accessed easily.
 */
class BorrowedReport {
public:
  BorrowedReport(const BorrowedReport& other) = delete;
  BorrowedReport(BorrowedReport&& other) :
      archiveLock(std::move(other.archiveLock)),
      reportLock(std::move(other.reportLock)),
      report(other.report) { }
  
  Report& operator*() { return report; }
  const Report& operator*() const { return report; }
  Report* operator->() { return &report; }
  const Report* operator->() const { return &report; }
private:
  BorrowedReport(Report& report, std::unique_lock<std::recursive_mutex>&& archiveLock) :
    archiveLock(std::move(archiveLock)),
    reportLock(report.mutex),
    report(report) { }
    
  // This class is not supposed to be allocated on the heap:
  void* operator new  (std::size_t count) = delete;
  void* operator new[](std::size_t count) = delete;
  void* operator new  ( std::size_t count, const std::nothrow_t& tag) = delete;
  void* operator new[]( std::size_t count, const std::nothrow_t& tag) = delete;
  
  std::unique_lock<std::recursive_mutex> archiveLock;
  std::unique_lock<decltype(std::declval<Report>().mutex)> reportLock;
  Report& report;
  
  friend class Archive; // Allow construction from within Archive
};

/**
 * Archive store files and reports
 * Stored object have hashes and are in UHash containers
 * Can be used to identify known verification results
 */
class Archive {
public:
  mutable std::recursive_mutex mutex;
  bbb::UHash<Report> reportStore;
  filesystem::path reportPath;

  bbb::UHash<m_hashable_string> fileStore;
  filesystem::path filePath;

  Archive() {};
  Archive(const String& reportPath, const String& filePath);
  Archive(const Archive& other) = delete;

  template<class... Args>
  std::pair<bool, ReportID> checkin_report(Args&&... args) {
    std::lock_guard<decltype(mutex)> lockGuard(mutex);
    Report report(std::forward<Args>(args)...);
    DEB("File path = " << filePath);
    return reportStore.insert(report); // TODO: Report gets copied here, unnecessarilly. But it would be pain to change it.
  }

  BorrowedReport borrow_report(ReportID id);
  bool has_report(ReportID id) const {std::lock_guard<decltype(mutex)> lockGuard(mutex); return reportStore.has_id(id); }

  std::pair<bool, FileID> checkin_file(const String& content);
  bool has_file(FileID id) const {std::lock_guard<decltype(mutex)> lockGuard(mutex); return fileStore.has_id(id);}
  std::string get_file_path(FileID id) const;
private:
  Report& get_report_nomutex(ReportID id) { return reportStore.decode_mod(id); }
  static std::string formatArchivedFileName(FileID id);
};
}
