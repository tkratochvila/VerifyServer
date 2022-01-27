
//+*****************************************************************************
//                         Honeywell Proprietary
// This document and all information and expression contained herein are the
// property of Honeywell International Inc., are loaned in confidence, and
// may not, in whole or in part, be used, duplicated or disclosed for any
// purpose without prior written permission of Honeywell International Inc.
//               This document is an unpublished work.
//
// Copyright (C) 2019 Honeywell International Inc. All rights reserved.
// *****************************************************************************
// Contributors:
//      MiD   Michal Dobes
//
//+*****************************************************************************

/* 
 * File:   Workspace.cpp
 * Author: E879277
 * 
 * Created on March 21, 2019, 2:23 PM
 */

#include "Workspace.h"

#include <algorithm>
#include <limits>
#include <random>
#include <mutex>
#include <set>

#include "bbb.h" // logging only please

namespace Workspace {

  Workspace::Workspace(const filesystem::path& webPath, const filesystem::path& canonicalPath, ToolKit::ToolReservation&& reservation) : 
          webPath(webPath),
          canonicalPath(canonicalPath),
          toolReservation(std::move(reservation))
  {
    filesystem::create_directory(canonicalPath);
  }

  Workspace::~Workspace() {
    std::lock_guard<decltype(mutex)> lockGuard(mutex);
    filesystem::remove_all(canonicalPath);
  }
  
  void Workspace::addReport(Archive::ReportID id) {
    std::lock_guard<decltype(mutex)> lockGuard(mutex);
    reports.insert(id);
  }

  void Workspace::checkinFile(const Archive::Archive& archive, Archive::FileID fileID, const std::string& workspaceRelativePath) {
    std::lock_guard<decltype(mutex)> lockGuard(mutex);
    if (!isRelativePathWithinWorkspace(workspaceRelativePath))
      throw std::runtime_error("Attempted escape from workspace.");
    // TODO: will the path get created if it does not exist?
    filesystem::copy_file(archive.get_file_path(fileID), canonicalPath/workspaceRelativePath, filesystem::copy_options::overwrite_existing); // TODO: create symlinks to save space?
    files[fileID] = workspaceRelativePath;
  }

  std::string Workspace::getWorkspaceRelativeFilePath(Archive::FileID id) const {
    std::lock_guard<decltype(mutex)> lockGuard(mutex);
    return files.at(id);
  }

  std::string Workspace::getCanonicalFilePath(Archive::FileID id) const {
    return canonicalPath.string() + "/" + getWorkspaceRelativeFilePath(id);
  }

  const std::string Workspace::getCanonicalPath() const {
    return canonicalPath;
  }

  
  ExpirationMap::Duration Workspace::getMaxIdleTimeout() const {
    return std::chrono::seconds{60}; // TODO: make parameterizable/dependent on tools and reports held by the workspace
  }

  const std::string Workspace::getWebPath() const {
    return webPath;
  }

  const ToolKit::Tool& Workspace::getTool() const {
    return toolReservation.getTool();
  }

  bool Workspace::isRelativePathWithinWorkspace(const filesystem::path& p) {
    for (const std::string& element : p)
      if (element.find("..") != std::string::npos || element.find_first_of("~$`") != std::string::npos) // TODO: is this enough?
        return false;
    return true;
  }

  bool Workspace::hasFile(Archive::FileID id) const {
    std::lock_guard<decltype(mutex)> lockGuard(mutex);
    return files.find(id) != files.end();
  }

  bool Workspace::isReportAllowed(Archive::ReportID id) const {
    std::lock_guard<decltype(mutex)> lockGuard(mutex);
    return reports.find(id) != reports.end();
  }

  
  
  WorkspaceManager::WorkspaceManager(const std::string& relativeWorkspaceRootPath) : 
          wwwWorkspaceRootPath(relativeWorkspaceRootPath),
          canonicalWorkspaceRootPath(filesystem::absolute(relativeWorkspaceRootPath)),
          workspacesExpirationMap(std::make_shared<WorkspacesExpirationMap>()),
          periodicExpirator(workspacesExpirationMap, std::chrono::seconds{5}, std::bind(&WorkspaceManager::onWorkspacesExpired, this, std::placeholders::_1))
  {
    if (!(is_directory(this->wwwWorkspaceRootPath) || create_directories(this->wwwWorkspaceRootPath))) {
      DEB("Unable to create workspaces dir");
      throw filesystem::filesystem_error("Unable to create workspaces dir", this->wwwWorkspaceRootPath, {});
    }
    for (const filesystem::path& item : filesystem::directory_iterator(this->wwwWorkspaceRootPath)) {
      if (is_directory(item) && item.filename().string().substr(0,9) == "workspace") {
        remove_all(item); // Clean up in case application crashed
      }
    }
  }

  std::pair<WorkspaceID, SharedWorkspace> WorkspaceManager::create(ToolKit::ToolReservation&& toolReservation) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<size_t> dis(0, std::numeric_limits<unsigned long>::max());
    
    WorkspaceID id;
    do {
      std::stringstream ss;
      ss << dis(gen);
      id = ss.str();
    } while (workspacesExpirationMap->get(id)); // ensure id not used already
    SharedWorkspace sw = std::make_shared<Workspace>(wwwWorkspaceRootPath/("workspace"+id), canonicalWorkspaceRootPath/("workspace"+id), std::move(toolReservation));
        workspacesExpirationMap->insert(id, sw, sw->getMaxIdleTimeout());
    return {id, sw};
  }

  void WorkspaceManager::destroy(const WorkspaceID& id) {
    workspacesExpirationMap->erase(id);
  }

  std::shared_ptr<Workspace> WorkspaceManager::get(const WorkspaceID& id) {
    std::experimental::optional<SharedWorkspace> optionalPtr = workspacesExpirationMap->get(id);
    if (!optionalPtr) {
      throw WorkspaceNotFoundError("Workspace does not exist: " + id);
    }
    workspacesExpirationMap->keepAlive(id, (*optionalPtr)->getMaxIdleTimeout());
    return *optionalPtr;
  }
  
  void WorkspaceManager::onWorkspacesExpired(WorkspacesExpirationMap::ExpiredValues&& expiredWorkspacesIDs) {
    for(const auto& workspacePair : expiredWorkspacesIDs) {
      DEB("Expiring workspace with ID " << workspacePair.first);
    }
  }

}
