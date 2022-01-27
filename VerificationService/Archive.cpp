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
 * File:   Archive.cpp
 * Author: Tomas Kratochvila <tomas.kratochvila at honeywell.com>
 * Author: Petr Bauch <petr.bauch at honeywell.com>
 */

#include <regex>

#include "Archive.h"

namespace Archive {

OSLCReporter::OSLCReporter(const std::string& automationPlanName, const std::string& localAddress) {
  build_oslc_perf(automationPlanName, localAddress);
}

void OSLCReporter::build_oslc_perf(const String& automationPlan, const std::string& localAddress) {
  DEB("Local address: " + localAddress);
  xml.top =
  xml.ii("rdf", "RDF",
       {   xml.ip("xmlns", "rdf",    "http://www.w3.org/1999/02/22-rdf-syntax-ns#"),
           xml.ip("xmlns", "rdfs",    "http://www.w3.org/2000/01/rdf-schema#"),
           xml.ip("xmlns", "owl",     "http://www.w3.org/2002/07/owl"),
           xml.ip("xmlns", "dcterms", "http://purl.org/dc/terms/"),
           xml.ip("xmlns", "pm",      "http://open-services.net/ns/perfmon#"), 
           xml.ip("xmlns", "ems",     "http://open-services.net/ns/ems#"),
           xml.ip("xmlns", "oslc_auto", "http://open-services.net/ns/auto#")
       },
       xml.isi({ xml.perf_item(localAddress, "Process ID", "pm:MemoryMetrics",
                           "http://open-services.net/ns/ems/unit#Bytes",
                           "http://www.w3.org/2001/XMLSchema#integer", pidNode),
               xml.perf_item(localAddress, automationPlan, "http://open-services.net/ns/auto#AutomationResult",
                           "http://open-services.net/ns/ems/unit#Char",
                           "http://www.w3.org/2001/XMLSchema#String", bResultNode),
               xml.perf_item(localAddress, "Free Memory in Absolute Value", "pm:MemoryMetrics",
                           "http://open-services.net/ns/ems/unit#Bytes",
                           "http://www.w3.org/2001/XMLSchema#integer", freeNode),
               xml.perf_item(localAddress, "Free Memory in Percentage", "pm:MemoryMetrics",
                           "http://open-services.net/ns/ems/unit#Bytes",
                           "http://www.w3.org/2001/XMLSchema#integer", percNode),
               xml.perf_item(localAddress, "CPU Usage (user)", "pm:CPUMetrics",
                           "dbp:Percentage",
                           "http://www.w3.org/2001/XMLSchema#float", utimeNode),
               xml.perf_item(localAddress, "CPU Usage (system)", "pm:CPUMetrics",
                           "dbp:Percentage",
                           "http://www.w3.org/2001/XMLSchema#float", stimeNode),
               xml.perf_item(localAddress, "Consumed Memory Usage (vsize)", "pm:MemoryMetrics",
                           "http://open-services.net/ns/ems/unit#Bytes",
                           "http://www.w3.org/2001/XMLSchema#integer", vsizeNode),
               xml.perf_item(localAddress, "Memory Usage (rss)", "pm:MemoryMetrics",
                           "http://open-services.net/ns/ems/unit#Bytes",
                           "http://www.w3.org/2001/XMLSchema#integer", rssNode),
               xml.ii("oslc_auto", "AutomationResult",
                    {   xml.ip("rdf", "about", "http://example.org/autoresults/3456")
                    },
                    xml.isi(
                    {
                      xml.perf_item(localAddress, "Standard Output", "foo", "string",
                                  "http://www.w3.org/2001/XMLSchema#string",
                                  stdOutNode),
                      xml.perf_item(localAddress, "Error Output", "foo", "string", "characters",
                                  errOutNode),
                      xml.perf_item(localAddress, "partVerResult", "foo", "string",
                                  "http://www.w3.org/2001/XMLSchema#string",
                                  verResultNode),
                      xml.perf_item(localAddress, "retCode", "foo", "string",
                                  "http://www.w3.org/2001/XMLSchema#string",
                                  retCodeNode),
                      xml.perf_item(localAddress, "parsedOutput", "foo", "string",
                                  "http://www.w3.org/2001/XMLSchema#string",
                                  parsedOutputNode),
                    })),
               xml.ii("dcterms", "creator",
                    { xml.ip("rdf", "resource", "Honeywell International") })
             }));
}

std::string OSLCReporter::getOSLC(const ReportInformation& info) {
  std::string result;
  xml.subs[pidNode].value = xml.insert_token(std::to_string(info.pid));
  xml.subs[bResultNode].value = xml.insert_token(info.runningResult);
  xml.subs[utimeNode].value = xml.insert_token(std::to_string(info.utime));
  xml.subs[stimeNode].value = xml.insert_token(std::to_string(info.stime));
  xml.subs[vsizeNode].value = xml.insert_token(info.vsize);
  xml.subs[rssNode].value = xml.insert_token(info.rss);
  xml.subs[freeNode].value = xml.insert_token(info.memFree);
  xml.subs[percNode].value = xml.insert_token(info.memPerc);
  xml.subs[stdOutNode].value = xml.insert_token(info.stdOut);
  xml.subs[errOutNode].value = xml.insert_token(info.errOut);
  xml.subs[verResultNode].value = xml.insert_token(info.verResult);
  xml.subs[retCodeNode].value = xml.insert_token(std::to_string(info.retCode));
  xml.subs[parsedOutputNode].value = xml.insert_token(info.parsedOutput);

  std::stringstream ss;
  xml.print(ss);
  result = ss.str();
  std::regex reg("(<dcterms:title>\\s+Error\\s+Output\\s+</dcterms:title>\\s+<ems:metric rdf:resource=\"foo\"/>\\s+<ems:unitOfMeasure rdf:resource=\"string\"/>\\s+<ems:numericValue\\s+rdf:datatype=\"characters\">)\\s+compiling\\s+[/\\w\\.]+\\s+a report was written to [\\w\\.]+\\s+");
  //DEB("regex =\n" + std::regex_replace(result, reg, "$1"));
  result = std::regex_replace(result, reg, "$1");
  return result;
}

  
Report::Report(ToolKit::Tool& t, const Strings& params,
               const std::vector<FileID>& inputs,
               const String& automationPlanName,
               const String& localAddress,
               size_t oCount) :
    tool(t),
    parameters(params),
    inputFiles(inputs),
    automationPlanName(automationPlanName),
    oslcReporter(automationPlanName, localAddress),
    returnCode(-9999), // Dummy value for debugging
    runningResult("Not started."),
    pid(-9999), // Dummy value for debugging
    valid(false) {
  assert(stdOutput.empty());
  for (size_t i = 0; i < oCount; i++) {
    outputNames.push_back(bbb::get_random_fname());
  }
  resources.push_back(
               {SClock::now(),
                { 0.0, 0.0, "0", "0", "0", "0" }
               });
  update_hash();
  updateLastMonitored();
  }

  Report::Report(const Report& other) : Report(other, std::lock_guard<decltype(mutex)>(other.mutex)) { }
  Report::Report(const Report& other, const std::lock_guard<decltype(mutex)>& lockGuardOther) :
    tool(other.tool),
    parameters(other.parameters),
    inputFiles(other.inputFiles),
    outputNames(other.outputNames),
    automationPlanName(other.automationPlanName),
    oslcReporter(other.oslcReporter),
    callCommand(other.callCommand),
    runTime(other.runTime),
    peakMemory(other.peakMemory),
    date(other.date),
    id(other.id),
    partVerResult(other.partVerResult),
    returnCode(other.returnCode),
    parsedOutput(other.parsedOutput),
    stdOutput(other.stdOutput),
    errOutput(other.errOutput),
    runningResult(other.runningResult),
    resources(other.resources),
    running(other.running),
    pid(other.pid),
    lastMonitored(other.lastMonitored),
    valid(other.valid),
    hash_fn_string(other.hash_fn_string),
    hash_fn_fileID(other.hash_fn_fileID),
    hash_fn_size_t(other.hash_fn_size_t) 
    { }


bool Report::from_string(const String& in) {
  return false;
}

void Report::to_string(String& out) {
  String strTmp;
  out += "\ncallCommand = " + callCommand;
  out += "\npartVerResult = " + partVerResult;
  out += "\nstOutput = " + stdOutput;
  out += "\nerOutput = " + errOutput;
  out += "\nrunningResult = " + runningResult;
  out += "\npid = " + std::to_string(pid);
  out += "\nautomation_plan = " + automationPlanName;
  out += "\nparameters = " + std::accumulate(parameters.begin(), parameters.end(), std::string(","));
  out += "\ninputFiles = " + std::accumulate(inputFiles.begin(), inputFiles.end(), std::string(","), [] (std::string& s, FileID fid) -> std::string {return s + std::to_string(fid);});
  out += "\noutputNames = " + std::accumulate(outputNames.begin(), outputNames.end(), std::string(","));
  tool.to_string(strTmp);
  out += "\ntool :  " + strTmp;
  return;
}

void Report::write(std::iostream& f) {
  bbb::write_enclosed(f, subSize, '-', "ToolName");
  f << "\n" + tool.get_name() + "\n";
  bbb::write_enclosed(f, subSize, '-', "Parameters");
  f << "\n";
  bbb::write_separated(f, parameters, "\n");
  f << "\n";
  bbb::write_enclosed(f, subSize, '-', "InputFiles");
  f << "\n";
  bbb::write_separated(f, inputFiles, "\n");
  f << "\n";
  bbb::write_enclosed(f, subSize, '-', "OutputFiles");
  f << "\n";
  bbb::write_separated(f, outputNames, "\n");
  f << "\n";
}

void Report::update_hash() {
  id = tool.hash();
  for (size_t i=0; i<inputFiles.size(); i++)
    id ^= hash_fn_fileID(inputFiles[i]) ^ hash_fn_size_t(i);
  for (size_t i=0; i<parameters.size(); i++)
    id ^= hash_fn_string(parameters[i]) ^ hash_fn_size_t(i);
  id ^= hash_fn_string(automationPlanName);
}

bool Report::operator==(const Report& other) const {
  DEB("Calling eq on reports.");
  if (inputFiles.empty()) {
    return id == other.id;
  }
  if (inputFiles != other.inputFiles)
    return false;
  if (parameters != other.parameters)
    return false;
  if (tool.hash() != other.tool.hash())
    return false;
  if (automationPlanName != other.automationPlanName)
    return false;
  return true;
  }

  ReportInformation Report::getMonitoringInformation() {
    std::lock_guard<decltype(mutex)> lockGuard(mutex);
    updateLastMonitored();
    ReportInformation info(resources.back().second);
    info.pid = pid;
    info.stdOut = stdOutput;
    info.errOut = errOutput;
    info.verResult = partVerResult;
    info.retCode = returnCode;
    info.parsedOutput = parsedOutput;
    info.planName = automationPlanName;
    info.runningResult = runningResult;
    return info;
  }
    
  std::string Report::getMonitoringOSLC() {
    return oslcReporter.getOSLC(getMonitoringInformation());
  }


  Archive::Archive(const String& reportPath, const String& filePath) : reportPath(reportPath), filePath(filePath) {
    // Ensure that archive dirs exist:
    filesystem::create_directories(this->reportPath);
    filesystem::create_directories(this->filePath);
    // TODO: instead of deleting, the tmp files and reports could be loaded back into archive again
    // Ensure clean state on startup:
    for (const filesystem::path& p : filesystem::directory_iterator(this->reportPath))
      filesystem::remove_all(p);
    for (const filesystem::path& p : filesystem::directory_iterator(this->filePath))
      filesystem::remove_all(p);
    DEB("Report path = " + reportPath + "\nFile path = " + filePath);
  }


std::pair<bool, FileID> Archive::checkin_file(const String& content) {
  std::lock_guard<decltype(mutex)> lockGuard(mutex);
  DEB("checkin_file(\n" + content + "\n)");
  m_hashable_string hs(content);
  auto answer = fileStore.insert(hs);
  if (!answer.first)
    return answer;
  String path = filePath / formatArchivedFileName(answer.second);
  DEB("Storing the content into: " + path);
  std::ofstream s(path);
  s << content;
  s.close();
  return answer;
  }

  std::string Archive::get_file_path(FileID id) const {
    std::lock_guard<decltype(mutex)> lockGuard(mutex);
    if (has_file(id))
      return filePath / formatArchivedFileName(id);
    return "FILE_UNAVAILABLE";
  }

  BorrowedReport Archive::borrow_report(ReportID id) {
    std::unique_lock<decltype(mutex)> archiveLock(mutex);
    return {get_report_nomutex(id), std::move(archiveLock)};
  }

  std::string Archive::formatArchivedFileName(FileID id) {
    return "tmp_" + std::to_string(id);
  }
}
