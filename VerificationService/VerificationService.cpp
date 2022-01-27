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
 * File:   VerificationService.cpp
 * Author: Tomas Kratochvila <tomas.kratochvila at honeywell.com>
 * Author: Petr Bauch <petr.bauch at honeywell.com>
 */

#include <ifaddrs.h>
#include <netdb.h>
#include <string>
#include <sys/stat.h>

#include "VerificationService.h"

using namespace std::literals::chrono_literals;

VerificationService::VerificationService(ToolKit::ToolKit&& toolkit) : VerificationService() {
  toolKit = std::move(toolkit);
}

VerificationService::VerificationService() : archive("./archiveReports", "./archiveFiles"),
                                             toolKit{},
                                             workspaceManager("./workspaces/"),
                                             tick(1s),
                                             doObserve(true),
                                             previousNumberOfTasks(-1),
                                             observer(&VerificationService::observe,
                                             this) {
/*  std::ifstream aStream("archive.dat");
  if (aStream.is_open()) {
    //archive.read(aStream);//TODO
    aStream.close();
    }*/
}

VerificationService::~VerificationService() {
  doObserve = false;
  observer.join();
/*  std::fstream aStream;
  aStream.open("archive.dat", std::fstream::out | std::fstream::app);
  archive.write(aStream);
  aStream.close();*/
}

/** Keep observing the number of the running tasks. Update statistics, if there is any running task. */
void VerificationService::observe() {
  //std::cout << "Thread started.\n";
  //int tInLS = 60;
  //int i = 0;
  while (doObserve) {
    //std::cout << "Doing loop.\n";
    std::this_thread::sleep_for(tick);
    if (executionWindow.size() != previousNumberOfTasks) {
      previousNumberOfTasks = executionWindow.size();
      DEB(std::to_string(previousNumberOfTasks) + " running tasks.");
    }
    if (executionWindow.empty())
      continue;
    //Archive::Reports resources;
    //TimePoint present = SClock::now();
    DEB("Updating stats.");
    executionWindow.update_stats();//report_finished(present);//, resources);
    // if (resources.empty())
    //   continue;

    //archive.store_reports(resources);
  }
}

std::pair<Workspace::WorkspaceID, std::string> VerificationService::createWorkspace(const std::string& toolName) {
  ToolKit::ToolReservation toolReservation = toolKit.reserveTool(toolName);
  std::pair<Workspace::WorkspaceID, Workspace::SharedWorkspace> answer(workspaceManager.create(std::move(toolReservation)));
  return {answer.first, answer.second->getWebPath()};
}

void VerificationService::destroyWorkspace(const Workspace::WorkspaceID& workspaceID) {
  workspaceManager.destroy(workspaceID);
}

std::pair<bool, Archive::FileID> VerificationService::addFile(const Workspace::WorkspaceID& workspaceID, const std::string& fileName, const std::string& fileContent) {
  // Get relevant workspace:
  Workspace::SharedWorkspace workspace(workspaceManager.get(workspaceID));
  std::pair<bool, Archive::FileID> answer = archive.checkin_file(fileContent);
  // Make the file available from the current workspace:
  workspace->checkinFile(archive, answer.second, fileName);
  return answer;
}


std::pair<bool, Archive::ReportID> VerificationService::verify(const RequestResponse::Request& verificationRequest) {
  // Get relevant workspace:
  Workspace::SharedWorkspace workspace(workspaceManager.get(verificationRequest.workspaceID));
 
  // Check availability of the needed tool.
  String toolName = verificationRequest.get_tool_name();
  DEB("\nTool name: " + toolName);
  auto tool = toolKit.get(toolName);
  if (!tool)
   throw std::runtime_error("Cannot verify: Unknown tool. (" + toolName + ")");

  // Check if the requested tool is reserved for this workspace:
  if (workspace->getTool().get_name() != tool.value().get_name())
   throw ToolKit::ReservationError("Invalid tool requested. Requested " + tool.value().get_name() + " but reserved " + workspace->getTool().get_name());

  // Identify input filenames and make sure the files are available.
  std::vector<Archive::FileID> inputFileIDs;
  {
    Strings inputs = verificationRequest.get_input_names();
    for (const std::string& input: inputs) {
      try {
        Archive::FileID id = std::stoul(input);
        if (!workspace->hasFile(id)) // Verify that the workspace is allowed to access the file
          throw std::logic_error("");
        inputFileIDs.emplace_back(id);
      }
      catch (const std::logic_error& e) {
        throw std::runtime_error("Invalid input file ID specified: " + input);
      }
    }
  }    

  DEB("archive.filePath = " << archive.filePath);	// e.g. ""

  String schema = verificationRequest.get_call_schema();	// e.g. "i0"
  DEB("Read schema \"" + schema + "\"");
  String automationPlan = verificationRequest.get_auto_plan_name();	// e.g. "http://honeywell.com/autoplans/MuxDemuxMuxDemux"
  DEB("Automation plan \n" + automationPlan + "\nEnd Automation plan");
  auto answer = archive.checkin_report(tool.value(),
                                verificationRequest.get_parameters(),
                                inputFileIDs,
                                automationPlan,
                                getLocalAddress(),
                                std::count(schema.begin(), schema.end(), 'o'));
  
  // Report was either known or created. Add its id to the workspace's allowed reports:
  workspace->addReport(answer.second);
  
  if (answer.first || !archive.borrow_report(answer.second)->is_valid()) {
    executionWindow.start_new_run(answer.second, archive, workspace, schema);
    return {true, answer.second};
  } 
  else {
    return {false, answer.second};
  }
}

std::string VerificationService::getMonitoringOSLC(const Workspace::WorkspaceID& workspaceID, const Archive::ReportID& reportID) {
  Workspace::SharedWorkspace workspace = workspaceManager.get(workspaceID);
      if (!workspace->isReportAllowed(reportID) || !archive.has_report(reportID))
        throw std::runtime_error("Error: Cannot access report.");
    //  executionWindow.update_stats();
      DEB("Accessing report " + std::to_string(reportID) + "\n");
      return archive.borrow_report(reportID)->getMonitoringOSLC();
}

void VerificationService::killTask(const Workspace::WorkspaceID& workspaceID, const Archive::ReportID& reportID) {
  Workspace::SharedWorkspace workspace = workspaceManager.get(workspaceID);
  // for example server was restarted and client still remembers the id and wants to kill it
  if (!workspace->isReportAllowed(reportID) || !archive.has_report(reportID))
    throw std::runtime_error("Error: The report id that should be killed cannot be accessed: " + std::to_string(reportID));
  Nat pid = archive.borrow_report(reportID)->pid;
  DEB("Killing process number \"" + std::to_string(pid) + "\"");
  executionWindow.kill_process(pid); // TODO: report.pid should be cleared to a SAFE value when the report finished running for WHATEVER reason
}

std::string VerificationService::getAvailabilityString() const {
  std::string result;
  for (const std::string& category : toolKit.get_capabilities()) {
    result += category + " " + toolKit.category_available(category) + "\n";
    for (const String& tool : toolKit.get_tools(category)) {
      result += " - " + tool + " " + (toolKit.get(tool).value().is_free() ? "yes" : "busy") + "\n";
    }
  }
  return result;
}

String VerificationService::getLocalAddress() {
  struct ifaddrs *ifaddr, *ifa;
  int family, s, n;
  char host[1025];
  int succ = getifaddrs(&ifaddr);
  assert(succ != -1);
  for (ifa = ifaddr, n = 0; ifa != NULL; ifa = ifa->ifa_next, n++) {
    if (ifa->ifa_addr == NULL)
      continue;
    family = ifa->ifa_addr->sa_family;
    if (family == AF_INET && ifa->ifa_name[0] != 'l') {
      s = getnameinfo(ifa->ifa_addr, 16,
                      host, 1025,
                      NULL, 0, 1);
      assert(s == 0);
      return String(host);
    }
  }
  assert(false);
  return String("");
}