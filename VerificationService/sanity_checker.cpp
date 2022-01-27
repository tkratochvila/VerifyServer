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
 * File:   sanity_checker.cpp
 * Author: Petr Bauch <petr.bauch at honeywell.com>
 */

#include <unistd.h>

#include "sanity_checker.h"

using namespace std::chrono_literals;

bool m_sanity_checker::update_req_file(const Strings& content) {
//  std::ofstream tempFile;
  // if (bbb::file_exists(req_file_name_realisability))
  //   std::remove(req_file_name_realisability.c_str());
//  tempFile.open(req_file_name_realisability);
  requirements_realisability.content_lines = content;
  bbb::for_each(requirements_realisability.content_lines,
                [&] (auto& s) { s += ";\n"; } );
  // std::stringstream ss;
  // bbb::write_separated(ss, content, ";\n");
  // ss << ";\n";
  // ltl_file_content = ss.str();
//  tempFile.close();

  std::set<String> cur_aps;
  for (auto& s : content) {
    m_ltl_parser ap_getter(s);
    for (auto& ap : ap_getter.aps) {
      cur_aps.insert(ap);
    }
  }
  return io_partitioning.prune_and_rewrite(cur_aps, variable_partitioning);
}

void m_sanity_checker::check_arguments() {
  if (arguments_count < 4) {
    std::cerr << "Wrong number of parameters.\n";
    for (int i = 0; i < arguments_count; i++) {
      std::cerr << arguments_view[i] << " | ";
    }
    std::cerr << std::endl;
  }
  if (arguments_count == 4) {
    no_time_out = true;
    return;
  }
  no_time_out = false;
  auto maybe_timeout = bbb::s2u(arguments_view[4]);
  assert(maybe_timeout);
  time_out = maybe_timeout.value();
}

void m_sanity_checker::select_requirements() {
  Strings allRequirements;
  assert(bbb::read_file(arguments_view[1], allRequirements));
  Strings nonemptyReqs;

  bbb::filter(allRequirements,
              nonemptyReqs,
              [&] (auto r)
              {
                return bbb::exists(r, [&] (auto c) {return !isspace(c);});
              }
    );

  Strings allIndices;
  bbb::split_by(arguments_view[3], allIndices, ",");

  Indices indices(nonemptyReqs.size(), false);
  std::for_each(allIndices.begin(),
                allIndices.end(),
                [&] (const String& s) {
                  auto maybeInt = bbb::s2u(s);
                  assert(maybeInt);
                  indices[maybeInt.value()] = true;
                });

  prune(nonemptyReqs, indices, selected_requirements);
  std::ofstream tempFile;
  req_file_name_vacuity = "tmp_v.ltl";

  tempFile.open(req_file_name_vacuity);
  bbb::write_separated(tempFile, selected_requirements, "\n");
  tempFile << "\n";
  tempFile.close();

//  req_file_name_realisability = "tmp_r.ltl";
  std::stringstream ss;
//  tempFile.open(req_file_name_realisability);
  bbb::write_separated(ss, selected_requirements, ";\n");
  ss << ";\n";
  ltl_file_content = ss.str();
//  bbb::read_file(arguments_view[2], part_file_content);
  // tempFile << ";\n";
  // tempFile.close();
}

void m_sanity_checker::call_vacuity_checker() {
  std::vector<std::pair<String, Strings>> witness_map;
  vacuity_output.load();
  Strings& lines = vacuity_output.content_lines;
  for (auto lit = lines.begin(); lit != lines.end();) {
    assert(lit->find("Formula:") != String::npos);
    String from = lit->substr(lit->find("Formula") + 9);
    lit++;
    Strings to;
    while (lit->find("Vacuity") != String::npos) {
      to.push_back(lit->substr(lit->find("witness") + 9));
      lit++;
    }
    witness_map.push_back({from, to});
  }
  std::vector<std::pair<String, Strings>> vacuity_map;
  for (auto &p : witness_map) {
    Strings other_requirements;
    bbb::extract_if(witness_map,
                    outher_requirements,
                    [&] (auto &pair) { return pair.first != p.first; },
                    [&] (auto &pair) { return pair.first; }
      );
    for (auto &w: p.second) {
      vacuity_map.push_back({w, other_requirements});
    }
  }
  for (auto &pp : vacuity_map) {
    String negated_formula;
    bbb::negate_ltl_formula(pp.first, negated_formula);
    pp.second.push_back(negated_formula);
    bbb::write_file(req_file_name_consistency, pp.second);
  }
}


void m_sanity_checker::start_processes(const std::vector<m_tool_configuration>& cs,
                                             std::map<size_t, Popen>&           ps) {
  std::stringstream ss;
  for (size_t i = 0; i < cs.size(); i++) {
    DEB("Starting: bash " + String(cs[i].script_name.c_str()));
    ps.try_emplace(i, Popen({ "bash",
                    cs[i].script_name.c_str()
                  },
                  input{PIPE},
        output{PIPE})
      );
  }
}

void m_sanity_checker::fill_configurations(std::vector<m_tool_configuration>& cs) {
  cs.emplace_back(
    "/home/kratochvila/AcaciaPlus-v2.3/",
    ltl_file_content,
    part_file_content,
    "python2",
    Strings({ 
      "acacia_plus.py",
      // "--ltl=out" + cs.unique_id + ".ltl",
      // "--part=tmp_io.part",
      "--player=1",
      "--check=BOTH"//,
//      "--tool=SPOT"
    })
  );
}

bool m_sanity_checker::check_io_variables() {
  Strings varPartitioning;
  return bbb::read_file(arguments_view[2], part_file_content)
    && part_file_content.find(".inputs") != String::npos;
  // assert(   bbb::read_file(arguments_view[2], varPartitioning)
  //        && varPartitioning.size() >= 2);

  // for (auto &l : varPartitioning) {
  //   if (l.find(".inputs") != String::npos)
  //     return l.size() > 8;
  // }
  // return true;
}

void m_sanity_checker::call_real_checkers() {
  realisability_timed_out = false;
  all_processes_failed = false;

  std::vector<m_tool_configuration> tool_configurations;
  fill_configurations(tool_configurations);
  // for (auto& tc : tool_configurations)
  //   prepare_script(tc);

  auto start = Basics::SClock::now();

  std::map<size_t, Popen> ps;
  start_processes(tool_configurations, ps);

  double time_left = time_out * 1000;
  while (no_time_out || time_left > 0.0) {
    DEB("Running realizability.");
    if (ps.empty()) {
      all_processes_failed = true;
      return;
    }
    for (auto it = ps.begin(); it != ps.end(); ) {
      DEB("Iterating over running processes.");
      switch (check_process(it->second,
                            tool_configurations[it->first].output_file_name)) {
      case process_result::running :
        it++;
        break;
      case process_result::failed :
        it = ps.erase(it);
        break;
      case process_result::success :
        auto end = Basics::SClock::now();
        realisability_time = end - start;
        txt_file_evidence = tool_configurations[it->first].evidence_file_name;
        return;
      }
    }
    std::this_thread::sleep_for(100ms);
    time_left -= 100;
  }
  realisability_timed_out = true;
}

void m_sanity_checker::prune(const Strings& in, const Indices& indices, Strings& out) {
  assert(in.size() == indices.size());
  for (size_t i = 0; i < indices.size(); i++)
    if (indices[i])
      out.push_back(in[i]);
}
