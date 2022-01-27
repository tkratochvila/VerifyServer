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
 * File:   VerificationService.h
 * Author: Tomas Kratochvila <tomas.kratochvila at honeywell.com>
 * Author: Petr Bauch <petr.bauch at honeywell.com>
 */

#pragma once

#include <atomic>
#include <chrono>
#include <fstream>
#include <string>
#include <thread>

#include "Archive.h"
#include "bbb.h"
#include "ExecutionEngine.h"
#include "ToolKit.h"
#include "RequestResponse.h"
#include "Workspace.h"

using namespace std::chrono_literals;
using namespace Basics;

class VerificationService {
public:
  ExecutionEngine::ExecutionWindow executionWindow;
  Archive::Archive archive;
  ToolKit::ToolKit toolKit;
  Workspace::WorkspaceManager workspaceManager;

  Dur tick; ///time between collecting resource cons. statistics
  //Duration lifeSpan; //time for which stats are kept
  std::atomic<bool> doObserve;
  uint previousNumberOfTasks;
  std::thread observer;

  VerificationService();
  VerificationService(ToolKit::ToolKit&& toolkit);
  ~VerificationService();

  void observe();

  /**
   * Creates a new workspace on the server.
   * Throws std::runtime_error on failure.
   * @param toolName The tool to reserve for the workspace.
   * @return First: ID of the workspace.<br/> Second: web URL root-relative path to the workspace
   */
  std::pair<Workspace::WorkspaceID, std::string> createWorkspace(const std::string& toolName);
  
  /**
   * Destroys the specified workspace
   * @param workspaceID
   */
  void destroyWorkspace(const Workspace::WorkspaceID& workspaceID);
  
  /**
   * Adds a file to the archive and makes it available from given workspace.
   * Throws std::runtime_error if workspace does not exist or if filePath points outside workspace
   * @param workspaceID
   * @param fileName Name of the file, including workspace-relative path (e.g. "easy/easy.c")
   * @param fileContent Content of the file to store
   * @return First: true if a new entry was created in the archive, false if an identical file was already stored in the archive.<br/>
   * Second: ID of the file in the archive
   */
  std::pair<bool, Archive::FileID> addFile(const Workspace::WorkspaceID& workspaceID, const std::string& fileName, const std::string& fileContent);
  
  /**
   * Starts verification based on the request. If there already is a <it>valid</it> report for the same verification request, no verification is started.
   * Throws std::runtime_error if launching of the verification fails.
   * @param verificationRequest
   * @return first: true if a new verification report was created and verification started. False if there already was a report for the verification request. <br/>
   *         second: ID of the report for the request, be it a freshly created report or an existing one. Guaranteed to reference a report in the service's Archive.
   */
  std::pair<bool, Archive::ReportID> verify(const RequestResponse::Request& verificationRequest);
  
  /**
   * Queries a report for information and returns OSLC monitoring response. On error, throws std::runtime_error on error.
   * @param workspaceID ID of workspace relevant to the report (for access control)
   * @param reportID ID of the report to access
   * @return 
   */
  std::string getMonitoringOSLC(const Workspace::WorkspaceID& workspaceID, const Archive::ReportID& reportID);
  
  /**
   * Kills a given report's running task. Throws std::runtime_error on error.
   * @param workspaceID
   * @param reportID
   */
  void killTask(const Workspace::WorkspaceID& workspaceID, const Archive::ReportID& reportID);
  
  /**
   * Returns string with information on which categories of verification are supported by the server and which tools are available
   * @return 
   */
  std::string getAvailabilityString() const;
private:
  static String getLocalAddress();
};

