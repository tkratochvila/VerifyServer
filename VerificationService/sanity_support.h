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
 * File:   sanity_support.h
 * Author: Petr Bauch <petr.bauch at honeywell.com>
 */

#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <set>

#include "bbb.h"

using String = bbb::String;
using Strings = bbb::Strings;

struct m_ltl_parser {
  String original;
  size_t pos;

  Strings aps;
  
  m_ltl_parser(const String& o) : original(o), pos(0) {
    //std::cout << "read: " << o << std::endl;
    while (pos < original.size()) {
      if (std::islower(original[pos]))
        read_ap();
      else
        pos++;
    }
  }

  bool is_ap_char(char c) { return (c != ')' && (std::isalnum(c) || c == '_')); }
  
  void read_ap() {
    size_t start = pos;
    while (is_ap_char(original[++pos]));
    aps.push_back(bbb::interval(original, start, pos));
    //std::cout << "pushed: " << bbb::interval(original, start, pos) << std::endl;
  }
};

struct m_io_aps {
  //permanent
  std::map<String, bool> part_map;

  //temporary
  Strings input_aps;
  Strings output_aps;

  m_io_aps() = delete;
  
  m_io_aps(const String& pfn);
  bool prune_and_rewrite(const std::set<String>& known, m_file& tfn);
};

struct m_tool_configuration {
  // String folder_name;
  // String command;
  // Strings parameters;
  m_file script;
  m_file evidence;
  m_file output;
  String unique_id;
  char this_folder[1024];
    
  std::size_t my_hash(const String& str) {
    return std::hash<String>{}(str);
  }

  std::size_t my_hash(const Strings& strs) {
    std::size_t ret_value = 0;
    for (auto &s : strs)
      ret_value ^= my_hash(s);
    return ret_value;
  }
  
  m_tool_configuration(const String& folder_name,
                       const String& ltl_file_content,
                       const String& part_file_content,
                       const String& call_command,
                       const Strings& supplementary_parameters);
};

struct m_file {
  String name;
  String path;
  String content;
  Strings content_lines;
  bool persistent;

  String current_path() {
    char temp_path[1024];
    getcwd(temp_path, sizeof(temp_path));
    return temp_path;
  }
  
  m_file() : persistent(false) {}
  m_file(const String& name) : name(name),
                               path(current_path()),
                               persistent(false) {}
  m_file(const String& name, const String& path) : name(name),
                                                   path(path),
                                                   persistent(false) {}
  m_file(const String& name, const Strings& cl) : name(name),
                                                  path(current_path()),
                                                  persistent(false) {
    content_lines = cl;
    store();
  }
  ~m_file() { remove_if_exists(); }
  
  void proclaim(const String& n, const String& p) {
    name = n;
    path = p;
  }
  bool empty() { return content.empty(); }
  String full_name() { return path + name; }
  bool exists() {
    if (name.empty()) return false;
    return bbb::file_exists(full_name());
  }
  char* c_name() { return (path + name).c_str(); }
  void remove_if_exists() {
    if (!persistent && exists()) std::remove(c_name());
  }
  bool load() {
    if (!exists())
      return false;
    bbb::read_file(full_name(), content);
    bbb::split_to_lines(content, content_lines);
    return true;
  }
  bool store() {
    if (content.empty())
      return false;
    if (content_lines.empty())
      bbb::split_to_lines(content, content_lines);
    remove_if_exists();
    bbb::write_file(full_name(), content_lines);
    return true;
  }
  void store_as(const String& n, const String& p) {
    proclaim(n, p);
    store();
  }
  void store_append() {
    std::ofstream temp;
    temp.open(c_name(), std::fstream::app);
    temp << content;
    temp.close();
  }
  void fuse_content() {
    content.clear();
    bbb::write_separated(content, content_lines, "\n");
  }
};
