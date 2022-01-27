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
 * File:   ToolKit.h
 * Author: Tomas Kratochvila <tomas.kratochvila at honeywell.com>
 * Author: Petr Bauch <petr.bauch at honeywell.com>
 */

#pragma once

#include <experimental/optional>
#include <fstream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>

#include "bbb.h"

namespace ToolKit {
  // Forward declarations:
  class Tool;
  
  using String = std::string;
  using Strings = std::vector<String>;
  using Hash = bbb::Hash;
  using Capabilities = std::set<String>;

  class ReservationError : public std::runtime_error {
  public:
    ReservationError(const std::string& __arg) : runtime_error(__arg) { }
  };
  
  class ToolReservation {
  public:
    ToolReservation();
    /**
     * Attempts to create and hold reservation for the tool or throws a ReservationError.
     * @param tool
     */
    ToolReservation(Tool& tool);
    ToolReservation(const ToolReservation& other) = delete;
    ToolReservation& operator=(const ToolReservation& right) = delete;
    ToolReservation(ToolReservation&& other) noexcept;
    ToolReservation& operator=(ToolReservation&& right) noexcept;
    
    virtual ~ToolReservation();
    
    operator bool() const;
    
    Tool& getTool() const;
    
  protected:
    bool hasReservedTool_nomutex() const;
    mutable std::mutex mutex;
    Tool* reservedTool; // TODO: Tool should be kept as a shared pointer. It is not safe to rely on the reference only (in case runtime changes are made to the toolkit).
  };
  
  class Tool {
  protected:
    mutable std::recursive_mutex mutex;
    bool busy;
    bool singleInstance;
    String name;
    String path;
    String outputParser;
    String version;
    std::map<String, Strings> parameterSets;
    Capabilities capabilities;
  public:
    Tool();
    Tool(const String& name, const String& path, const String& outputParser, bool singleInstance = false);
    Tool(const Tool& other) = delete;
    Tool(Tool&& other) noexcept;
    
    virtual ~Tool() {}

    Tool& operator=(const Tool& other) = delete;
    Tool& operator=(Tool&& other) noexcept;
    bool operator==(const Tool& other);

    bool acquire();
    void add_category(const String& c);
    
    Capabilities get_capabilities() const {std::lock_guard<decltype(mutex)> lockGuard(mutex); return capabilities; }
    String get_name() const               {std::lock_guard<decltype(mutex)> lockGuard(mutex); return name; }
    String get_path() const               {std::lock_guard<decltype(mutex)> lockGuard(mutex); return path; }
    String get_version() const            {std::lock_guard<decltype(mutex)> lockGuard(mutex); return version; }
    String get_output_parser() const      {std::lock_guard<decltype(mutex)> lockGuard(mutex); return outputParser; }

    
    bool has_category(const String& c) const;
    Hash hash() const;
    bool is_free() const;
    
    void set_capabilities(Capabilities& c);
    void set_capabilities(Capabilities&& c);
    void set_free();
    void to_string(String& out);
    void write(std::fstream& f) {}
    
  protected:
    /**
     * Attempts to run the tool with --version, sets the version member accordingly.
     */
    void update_version();
  };


  class ToolKit {
  public:
    ToolKit();
    ToolKit(const ToolKit& other) = delete;
    ToolKit(ToolKit&& other) noexcept;

    ~ToolKit() {}
    
    ToolKit& operator=(const ToolKit& other) = delete;
    ToolKit& operator=(ToolKit&& other) noexcept;

    bbb::MaybeRef<Tool> get(const String& name);
    bbb::MaybeRef<const Tool> get(const String& name) const;
    bool is_tool_free(const String& name) const;
    void insert(Tool&& tool);
    String category_available(const String& c) const;
    Capabilities get_capabilities() const;
    std::set<String> get_tools(const String& category) const;
    ToolReservation reserveTool(const std::string& name);
    
  protected:
    static std::string normalizeName(const std::string& name);
    
    mutable std::recursive_mutex mutex;
    std::map<String, Tool> toolStore;
    Capabilities capabilities;
    using ToolStoreIterator = decltype(toolStore)::iterator;
    std::map<String, std::list<ToolStoreIterator>> toolsByCategoryMap;
  };
}
