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
 * File:   ToolKit.cpp
 * Author: Tomas Kratochvila <tomas.kratochvila at honeywell.com>
 * Author: Petr Bauch <petr.bauch at honeywell.com>
 */

#include <algorithm>

#include "bbb.h"
#include "subprocess.hpp"

#include "ToolKit.h"

namespace ToolKit {
  Tool::Tool() : busy(false) {}
  Tool::Tool(const String& name, const String& path, const String& outputParser, bool singleInstance) : 
      busy(false),
      singleInstance(singleInstance),
      name(name),
      path(path),
      outputParser(outputParser) {
    update_version();
  }

  Tool::Tool(Tool&& other) noexcept {
    *this = std::move(other);
  }
  Tool& Tool::operator=(Tool&& other) noexcept {
    std::lock_guard<decltype(mutex)> lockGuard(mutex);
    std::lock_guard<decltype(other.mutex)> lockGuardOther(other.mutex);
    this->busy = other.busy;
    other.busy = false;
    this->singleInstance = other.singleInstance;
    this->name = std::move(other.name);
    this->path = std::move(other.path);
    this->outputParser = std::move(other.outputParser);
    this->version = std::move(other.version);
    this->parameterSets = std::move(other.parameterSets);
    this->capabilities = std::move(other.capabilities);
    return *this;
  }
    
  bool Tool::operator==(const Tool& other) {
    std::lock_guard<decltype(mutex)> lockGuard(mutex);
    std::lock_guard<decltype(other.mutex)> lockGuardOther(other.mutex);
    return name == other.name && version == other.version;
  }
    
  Hash Tool::hash() const {
    std::lock_guard<decltype(mutex)> lockGuard(mutex);
    return std::hash<String>{}(name) + std::hash<String>{}(version) + std::hash<String>{}(path);
  }

  void Tool::add_category(const String& c) {
    std::lock_guard<decltype(mutex)> lockGuard(mutex);
    capabilities.insert(c);
  }

  void Tool::set_capabilities(Capabilities& c) {
    std::lock_guard<decltype(mutex)> lockGuard(mutex);
    capabilities = c;
  }

  void Tool::set_capabilities(Capabilities&& c) {
    std::lock_guard<decltype(mutex)> lockGuard(mutex);
    capabilities = std::move(c);
  }

  void Tool::to_string(String& out)
  {
    std::lock_guard<decltype(mutex)> lockGuard(mutex);
    out = "name = " + name;
    out += ", path = " + path;
    out += ", version = " + version;
    return;
  }
  
  /**
   * Attempt to atomically acquire the tool (set its busy flag)
   * @return True if the tool was not busy and was successfully acquired. 
   *         False otherwise.
   */
  bool Tool::acquire() {
    std::lock_guard<decltype(mutex)> lockGuard(mutex);
    if (busy)
      return false;
    if (singleInstance)
        busy = true;  
    return true;
  }
  
  /**
   * Release the tool if it was acquired (busy)
   */
  void Tool::set_free() {
    std::lock_guard<decltype(mutex)> lockGuard(mutex);
    busy = false; 
  }
  
  bool Tool::is_free() const {
      std::lock_guard<decltype(mutex)> lockGuard(mutex);
      return !busy;  
  }
  
  bool Tool::has_category(const String& c) const {
    std::lock_guard<decltype(mutex)> lockGuard(mutex);
    return capabilities.find(c) != capabilities.end(); 
  }
  
  void Tool::update_version() {
    std::lock_guard<decltype(mutex)> lockGuard(mutex);
    try {
      auto process = subprocess::Popen(
              {this->path.c_str(), "--version"},
              subprocess::output{subprocess::PIPE},
              subprocess::error{subprocess::STDOUT},
              subprocess::shell{false});
      auto outputBuffer = process.communicate().first;
      process.wait();
      std::string data(outputBuffer.buf.data());
      std::transform(data.begin(), data.end(), data.begin(), [] (const char c) -> char {return std::tolower(c);});
      size_t versionPos;
      size_t newlinePos;
      versionPos = data.find("version");
      if (versionPos == std::string::npos)
      {
        versionPos = data.find("v");
        if (versionPos == std::string::npos)
          versionPos = 0;
      }
      newlinePos = data.find("\n", versionPos);
      if (newlinePos == std::string::npos)
        newlinePos = data.length();
      version = std::string(&(outputBuffer.buf.data()[versionPos]), &(outputBuffer.buf.data()[newlinePos]));
    }
    catch (const subprocess::OSError& err) {
      version = "ERROR";
      busy = true; // Block the tool
      DEB("Error running tool: " + path);
    }
  }
  
  
  // class ToolKit
  ToolKit::ToolKit() {}
  
  ToolKit::ToolKit(ToolKit&& other) noexcept {
    *this = std::move(other);
  }
    
  ToolKit& ToolKit::operator=(ToolKit&& other) noexcept {
    std::lock_guard<decltype(mutex)> lockGuard(mutex);
    std::lock_guard<decltype(other.mutex)> lockGuardOther(other.mutex);
    this->toolStore = std::move(other.toolStore);
    this->capabilities = std::move(other.capabilities);
    this->toolsByCategoryMap = std::move(other.toolsByCategoryMap);
    return *this;
  }

  bbb::MaybeRef<Tool> ToolKit::get(const String& name) {
    std::lock_guard<decltype(mutex)> lockGuard(mutex);
    try {
      return toolStore.at(normalizeName(name));
    }
    catch (const std::out_of_range& e) {
      return {};
    }
  }
  
  bbb::MaybeRef<const Tool> ToolKit::get(const String& name) const {
    std::lock_guard<decltype(mutex)> lockGuard(mutex);
    try {
      const decltype(toolStore)& constToolStore = toolStore;
      return constToolStore.at(normalizeName(name));
    }
    catch (const std::out_of_range& e) {
      return {};
    }
  }

  bool ToolKit::is_tool_free(const String& name) const {
    std::lock_guard<decltype(mutex)> lockGuard(mutex);
    try {
      return toolStore.at(normalizeName(name)).is_free();
    }
    catch (const std::out_of_range& e) {
      return false;
    }
  }

  void ToolKit::insert(Tool&& tool) {
    std::lock_guard<decltype(mutex)> lockGuard(mutex);
    ToolStoreIterator it = toolStore.emplace(normalizeName(tool.get_name()), std::move(tool)).first;
    for (const std::string& cap : it->second.get_capabilities())
    {
      toolsByCategoryMap[cap].push_back(it); //TODO: set would be more suitable but would require comparator of rbtree iterators
      capabilities.insert(cap);
    }
  }

  String ToolKit::category_available(const String& c) const {
    std::lock_guard<decltype(mutex)> lockGuard(mutex);
    bool at_least_one_capable = false;
    for (auto &tp : toolStore) {
      if (tp.second.has_category(c)) {
        at_least_one_capable = true;
        if (tp.second.is_free())
          return "yes";
      }
    }
    return (at_least_one_capable ? "busy" : "no");
  }
  
  Capabilities ToolKit::get_capabilities() const {
    std::lock_guard<decltype(mutex)> lockGuard(mutex);
    return capabilities;
  }
  
  std::set<String> ToolKit::get_tools(const String& category) const {
    std::lock_guard<decltype(mutex)> lockGuard(mutex);
    std::set<String> toolsWithCategory;
    for (const ToolStoreIterator toolIt : toolsByCategoryMap.at(category)) {
      toolsWithCategory.insert(toolIt->first);
    }
    return toolsWithCategory;
  }

  ToolReservation ToolKit::reserveTool(const std::string& name) {
    auto maybeTool = get(name);
    if (!maybeTool)
      throw ReservationError("Reservation failed: no such tool in toolkit");
    ToolReservation res(maybeTool.value());
    if (!res)
      throw ReservationError("Reservation failed: tool busy");
    return res;
  }

  std::string ToolKit::normalizeName(const std::string& name) {
    std::string normalizedName;
    std::transform(name.begin(), name.end(), std::back_inserter(normalizedName), 
              [] (const char c) -> char {return std::tolower(c);});
    return normalizedName;
  }


  
  ToolReservation::ToolReservation() : reservedTool(nullptr) {}
  
  ToolReservation::ToolReservation(Tool& tool) {
    if (tool.acquire())
      reservedTool = &tool;
    else
      reservedTool = nullptr;
  }
  
  ToolReservation::ToolReservation(ToolReservation&& other) noexcept {
    *this = std::move(other);
  }
  
  ToolReservation& ToolReservation::operator=(ToolReservation&& right) noexcept {
    std::lock_guard<decltype(mutex) > lockGuard(mutex);
    std::lock_guard<decltype(right.mutex) > lockGuardRight(right.mutex);
    reservedTool = right.reservedTool;
    right.reservedTool = nullptr;
    return *this;
  }
  
  ToolReservation::~ToolReservation() {
    if (*this)
      reservedTool->set_free();
  }
  
  ToolReservation::operator bool() const {
    std::lock_guard<decltype(mutex) > lockGuard(mutex);
    return hasReservedTool_nomutex();
  }

  Tool& ToolReservation::getTool() const {
    std::lock_guard<decltype(mutex) > lockGuard(mutex);
    if (hasReservedTool_nomutex())
      return *reservedTool;
    throw ReservationError("Tool accessed from invalid reservation.");
  }

  bool ToolReservation::hasReservedTool_nomutex() const {
    return reservedTool != nullptr;
  }

}