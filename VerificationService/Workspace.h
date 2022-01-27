
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
 * File:   Workspace.h
 * Author: E879277
 *
 * Created on March 21, 2019, 2:23 PM
 */

#pragma once

#include <experimental/filesystem>
#include <map>
#include <mutex>
#include <string>

#include "ExpirationMap.hpp"
#include "ToolKit.h"
#include "Archive.h"

namespace Workspace {
  class Workspace;

  namespace filesystem = std::experimental::filesystem;

  using WorkspaceID = std::string;
  using SharedWorkspace = std::shared_ptr<Workspace>;

  class WorkspaceNotFoundError : public std::runtime_error {
  public:
    WorkspaceNotFoundError(const std::string& msg) : runtime_error(msg) { }
  };
  
  class Workspace : public std::enable_shared_from_this<Workspace> {
  public:
    /**
     * Creates workspace in the specified canonical path. WebPath will be used to access the workspace from other machines.
     * @param webPath
     * @param canonicalPath
     * @param reservation
     */
    Workspace(const filesystem::path& webPath, const filesystem::path& canonicalPath, ToolKit::ToolReservation&& reservation);
    Workspace(const Workspace& orig) = delete;
    Workspace(Workspace&& orig) = delete;
    
    Workspace& operator=(const Workspace& right) = delete;
    Workspace& operator=(Workspace&& right) = delete;

    virtual ~Workspace();
    
    void addReport(Archive::ReportID id);
    
    /**
     * Adds a file into the workspace, referenced under archiveID 
     * @param archiveID
     * @param workspacePath
     * @param fileContents
     */
    void checkinFile(const Archive::Archive& archive, Archive::FileID fileID, const std::string& workspacePath);
    
    /**
     * Gets the relative path to the file with given ID. If file does not exist, throws std::out_of_range
     * @param id
     * @return 
     */
    std::string getWorkspaceRelativeFilePath(Archive::FileID id) const;
    
    /**
     * Gets the full canonical path to the file with given ID. If file does not exist, throws std::out_of_range
     * @param id
     * @return 
     */
    std::string getCanonicalFilePath(Archive::FileID id) const;
    
    /**
     * Returns the full canonical filesystem path of the workspace directory
     * @return 
     */
    const std::string getCanonicalPath() const;
    
    /**
     * 
     * @return Duration after which the workspace shall be destroyed if not accessed
     */
    ExpirationMap::Duration getMaxIdleTimeout() const;
    
    /**
     * Returns the web-accessible path to the workspace
     * @return 
     */
    const std::string getWebPath() const;
    
    /**
     * Returns reserved tool or throws ToolKit::ReservationError
     * @return 
     */
    const ToolKit::Tool& getTool() const;
    
    /**
     * Checks if file with given ID is available in the workspace
     * @param id Archive ID of the file
     * @return 
     */
    bool hasFile(Archive::FileID id) const;
    
    /**
     * Determines if a report is allowed to be accessed from this workspace
     * @param id The report's ID
     * @return true if the report is allowed to be accessed. False if not.
     */
    bool isReportAllowed(Archive::ReportID id) const;
    
  private:
    /**
     * Checks if a given path is / would be within the workspace directory (possibly after creating it)
     * @param p
     * @return 
     */
    bool isRelativePathWithinWorkspace(const filesystem::path& p);
    
    mutable std::mutex mutex;
    const filesystem::path webPath; // root of the workspace relative to the www root
    const filesystem::path canonicalPath; // root of the workspace - absolute canonical path
    ToolKit::ToolReservation toolReservation; // Reservation of the workspace's tool
    std::set<Archive::ReportID> reports; // List of accessible report IDs
    std::map<Archive::FileID, filesystem::path> files; // Map of ArchiveIDs to filepaths within this workspace
  };

  
  
  /**
   * Class managing (allocating, destroying, ...) workspaces, including their filesystem directories.
   * @return 
   */
  class WorkspaceManager {
  public:
    /**
    * 
    * @param workspaceRootPath Has to be a directory path. If it is not, or cannot be created, 
    * a std::experimantal::filesystem_exception will be thrown.
    * If the path does not exist, it will be created.
    */
    WorkspaceManager(const std::string& workspaceRootPath);
    WorkspaceManager(const WorkspaceManager& other) = delete;
    WorkspaceManager(WorkspaceManager&& other) = delete;
    
    /**
     * Creates a new workspace with the specified tool reserved. 
     * @param toolReservation
     * @return 
     */
    std::pair<WorkspaceID, SharedWorkspace> create(ToolKit::ToolReservation&& toolReservation);
    
    /**
     * Destroys workspace (removes from manager, workspace will release resources when all shared_ptrs are destroyed)
     * @param id
     */
    void destroy(const WorkspaceID& id);
    
    /**
    * Get poiter to workspace if the workspace has not expired. If it has, a WorkspaceNotFoundError will be thrown
    * @param id
    * @return 
    */
    std::shared_ptr<Workspace> get(const WorkspaceID& id);

  private:
    using WorkspacesExpirationMap = ExpirationMap::ExpirationMap<WorkspaceID, SharedWorkspace>;
    using SharedWorkspacesExpirationMap = std::shared_ptr<WorkspacesExpirationMap>;
    
    void onWorkspacesExpired(WorkspacesExpirationMap::ExpiredValues&& expiredWorkspacesIDs);
    
    filesystem::path wwwWorkspaceRootPath; // TODO: path to the workspaces dir when accessed from other machines - make sure it is relative to www root
    filesystem::path canonicalWorkspaceRootPath; // canonical path to the root of workspaces
    SharedWorkspacesExpirationMap workspacesExpirationMap;
    ExpirationMap::PeriodicExpirator<WorkspaceID, SharedWorkspace> periodicExpirator;
  };

} // namespace