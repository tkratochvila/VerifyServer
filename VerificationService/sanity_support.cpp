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
 * File:   sanity_support.cpp
 * Author: Petr Bauch <petr.bauch at honeywell.com>
 */

#include <unistd.h>

#include "sanity_support.h"

using namespace std::chrono_literals;

m_io_aps::m_io_aps(const String& pfn) {
  Strings lines;
  bbb::read_file(pfn, lines);
  assert(lines.size() > 1);
  bbb::split_by(lines[0], input_aps, " ");
  for (size_t i = 1; i < input_aps.size(); i++) {
    part_map[input_aps[i]] = true;
  }
  bbb::split_by(lines[1], output_aps, " ");
  for (size_t i = 1; i < output_aps.size(); i++) {
    part_map[output_aps[i]] = false;
  }
}

bool m_io_aps::prune_and_rewrite(const std::set<String>& known, m_file& tfn) {
  //bbb::write_separated(std::cout, known, " - ");
  input_aps.clear();
  output_aps.clear();
  for (auto& s : known) {
    if (part_map[s])
      input_aps.push_back(s);
    else
      output_aps.push_back(s);
  }
  std::stringstream ss;
//    os.open(tfn.c_str());
  ss << ".inputs" << (input_aps.empty() ? "" : " ");
  bbb::write_separated(ss, input_aps, " ");
  tfn.content_lines.clear();
  tfn.content_lines.push_back(ss.str());
  ss << ".outputs ";
  bbb::write_separated(ss, output_aps, " ");
  tfn.content_lines.push_back(ss.str());
  tfn.fuse_content();
//    os.close();
  if (input_aps.empty())
    DEB("There are no input signals.");
  return !input_aps.empty();
}
 
m_tool_configuration::m_tool_configuration(const String& folder_name,
                                           m_file& requirements,
                                           m_file& partitioning,
                                           const String& call_command,
                                           const Strings& supplementary_parameters) {
  getcwd(this_folder, sizeof(this_folder));

  assert(!requirements.empty());
  assert(!paritioning.empty());
  unique_id = std::to_string(my_hash(call_command)
                             + my_hash(requirements.content)
                             + my_hash(partitioning.content)
                             + my_hash(supplementary_parameters));
  String prefix = "task";

  requirements.store_as(prefix + unique_id + ".ltl",  this_folder);
  partitioning.store_as(prefix + unique_id + ".part", this_folder);

  output.name = "stdout" + unique_id + ".txt";
  evidence.name = prefix + unique_id + ".txt";
    
  std::stringstream ss;
  ss << "#!/bin/bash\n";
  ss << "cd " + folder_name + "\n";
  ss << call_command + " ";
  //String params;
  bbb::write_separated(ss, supplementary_parameters, " ");
//    ss << params;
  ss << " --ltl=" + requirements.full_name();
  ss << " --part=" + partitioning.full_name();
  ss << " > " + output.full_name();
  ss << "\n";
  script.content = ss.str();
  script.store_as("temp" + unique_id + ".sh", this_folder);
}
