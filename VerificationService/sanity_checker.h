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
 * File:   sanity_checker.h
 * Author: Tomas Kratochvila <tomas.kratochvila at honeywell.com>
 * Author: Petr Bauch <petr.bauch at honeywell.com>
 */

#pragma once

#include <thread>

#include "bbb.h"
#include "sanity_support.h"
#include "subprocess.hpp"

using namespace subprocess;

struct m_consistency_checker {
public:
  using t_indices = std::vector<size_t>;
  using t_muses = std::vector<t_indices>;
  
private:
  //one requirement per line, one line per string
  Strings requirements;

  t_mus reqs_to_mus(const Strings& reqs) {
    t_mus ret_val;
    for (size_t i = 0; i < requirements.size(); i++)
      for (auto &l : reqs)
        if (requirements[i] == l)
          ret_val.push_back(i);
    return ret_val;
  }

  void conjunct_two(const String& left,
                    const String& right,
                    String& out) {
    out = "(" + left + " & " + right + ")";
  }
  
  void conjunct_some(const t_indices& which, String& out) {
    if (which.empty())
      return;
    assert(which[0] < requirements.size());
    out = requirements[which[0]];
    if (which.size() == 1)
      return;
    for (size_t i = 1; i < which.size(); i++) {
      assert(which[i] < requirements.size());
      conjunct_two(out, requirements[which[i]], out);
    }
  }
  
  void conjunct_all(String& out) {
    t_indices all(requirements.size());
    std::iota(all.begin(), all.end(), 0);
    conjunct_some(all, out);
  }
  
public:
  m_consistency_checker() delete;
  m_consistency_checker(const Strings& r) : requirements(r) {}
  
  bool is_consistent() {
    String conjunction;
    conjunct_all(conjunction);
    m_file req_temp("tmp_hon_reqs.ltl", {conjunction});
    m_file mvc_input("mvc_input.ltl");
    m_file con_output("mvc_output.txt");

    auto p = Popen(
      {   "python3",
          "/home/xkratochvila/bin/remus2/hon_to_nuxmv.py",
          req_temp.c_name()
      },
      input{PIPE},
      output{mvc_input.c_name()}
    );
    p.wait();
    p = Popen(
      {   "/home/xkratochvila/bin/remus2/mvc",
          mvc_input.c_name()
      },
      input{PIPE},
      output{mvc_output.c_name()});
    p.wait();
    mvc_output.load();
    if (mvc_output.content.find("is consistent") != String::npos)
      return true;
    else
      return false;
  }
  
  t_muses get_mises() {
    m_file req_temp("tmp_hon_reqs.ltl", requirements);
    m_file mvc_input("mvc_input.ltl");
    m_file con_output("mvc_output.txt");
    m_file list_of_muses("muses.txt");

    auto p = Popen(
      {   "python3",
          "/home/xkratochvila/bin/remus2/hon_to_nuxmv.py",
          req_temp.c_name()
      },
      input{PIPE},
      output{mvc_input.c_name()}
    );
    p.wait();
    p = Popen(
      {   "/home/xkratochvila/bin/remus2/mvc",
          "-o",
          list_of_muses.c_name(),
          mvc_input.c_name()
      },
      input{PIPE},
      output{mvc_output.c_name()});
    p.wait();
    mvc_output.load();

    t_muses all_muses;
    if (mvc_output.content.find("is consistent") != String::npos)
      return all_muses;

    list_of_muses.load();
    Strings reqs;
    for (auto &l : list_of_muses.content_lines) {
      if (l.empty() && !reqs.empty()) {
        all_muses.push_back(reqs_to_mus(reqs));
        reqs.clear();
      }
      else
        reqs.push_back(l);
    }
    if (!reqs.empty())
      all_muses.push_back(reqs_to_mus(reqs));
    return all_muses;
  }
};

struct m_vacuity_checker : public m_consistency_checker {
  m_vacuity_checker() delete;
  m_vacuity_checker(const Strings &r) : m_consistency_checker(r) {}

  String negate(const String& in) {
    assert(in.size() > 0);
    return out = "!(" + in + ")";
  }
  
  bool is_vacuous(size_t req_id) {
    String curr_req = requirements[req_id];
    auto witnesses = get_witnesses(req_id);
    for (auto &w : witnesses) {
      requirements[req_id] = negate(w);
      if (!is_consistent()) {
        requirements[req_id] = curr_req;
        return true;
      }
    }
    requirements[req_id] = curr_req;
    return false;
  }

  t_indices get_mvses() {
    t_indices ret_val;
    for (size_t i = 0; i < requirements.size(); i++)
      if (is_vacuous(i))
        ret_val.push_back(i);
    return ret_val;
  }
};

struct m_realisability_checker : public m_consistency_checker {
  m_io_apps& ios;
  
  m_realisability_checker() delete;
  m_realisability_checker(const Strings &r,
                          const m_io_apps& ios) : m_consistency_checker(r),
                                                  ios(ios) {}

  void fill_configurations(std::vector<m_tool_configuration>& cs) {
    cs.emplace_back(
      "/home/kratochvila/AcaciaPlus-v2.3/",
      ltl_file_content,
      part_file_content,
      "python2",
      Strings({ 
          "acacia_plus.py",
            "--player=1",
            "--check=BOTH"//,
            })
      );
  }
  
  bool is_realisable(const t_set& subset_ids) {
    m_file io_partitioning("temp_real.part");
    if (!ios.prune_and_rewrite(subset_ids, io_partitioning))
      return is_consistent(subset_ids);

    m_file req_subset("temp_real.ltl");//TODO
    bool realisability_timed_out = false;
    bool all_processes_failed = false;

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
};

struct m_sanity_checker {
  //--------------------Types-------------------->
  using Indices = std::vector<bool>;
  enum struct process_result { running, failed, success };
  //<-------------------Types---------------------
  //--------------------Files-------------------->
  m_file requirements_vacuity;
  m_file requirements_consistency;
  m_file vacuity_output;
  m_file collective_result;
  m_file consistency_output;
  m_file requirements_realisability;
  m_file variable_partitioning;
  //<-------------------Files---------------------
  int arguments_count;
  char** arguments_view;
  std::stringstream result_stream;
  String req_file_name_realisability;
  m_io_aps io_partitioning;
  int time_out;
  bool no_time_out;
  String ltl_file_content;
  String part_file_content;

  bool realisability_timed_out;
  String realisability_output;
  std::chrono::duration<double> realisability_time;
  bool all_processes_failed;
  String txt_file_evidence;
  Strings selected_requirements;
  
  m_sanity_checker(int c, char** v) : arguments_count(c),
                                      arguments_view(v),
                                      collective_result("partVerResult.txt"),
                                      io_partitioning(arguments_view[2]) {
    collective_result.remove_if_exists();
  }

  ~m_sanity_checker() {
    collective_result.content = result_stream.str();
    collective_result.store_append();
  }

  //--------------------IO Functions--------------------
  bool update_req_file(const Strings& content);
  void check_arguments();
  void select_requirements();
  void generate_vacuity_witnesses() {
    vacOutFile.name = "vacwitOutput.ltl";
    vacOutFile.remove_if_exists();
    auto p = Popen({"/var/www/vacwit.sh", requirements_vacuity.c_name()},
                   input{PIPE},
                   output{vacOutFile.c_name()});
    
    p.wait();
  }
  void call_looney() {
    auto p = Popen({"/var/www/looney", vacOutFile.c_name()}, input{PIPE},
                   output{collective_result.c_name()});
    p.wait();
  }
  void call_consistency_checker() {
    m_file mvc_input("mvc_input.ltl");
    auto p = Popen(
      {   "python3",
          "/home/xkratochvila/bin/remus2/hon_to_nuxmv.py",
          requirements_consistency.c_name()
      },
      input{PIPE},
      output{mvc_input.c_name()});
    p.wait();
    p = Popen(
      {   "/home/xkratochvila/bin/remus2/mvc",
          requirements_consistency.c_name()
      },
      input{PIPE},
      output{consistency_output.c_name()});
    p.wait();
  }
  void call_vacuity_checker();
  bool check_io_variables();
  void call_real_checkers();
  bool realisable() {
    return !realisability_timed_out
        && !all_processes_failed
        && realisability_output.find("is realizable") != String::npos;
  }
  //--------------------IO Functions--------------------
  //--------------------Reporting Functions-------------
  void append_trivial_real_report() {
    result_stream << "No input signals -> realisability equals consistency.\n";
    result_stream << "Checking realisability took: 0 s.\n";
    result_stream << "This set of requirements is "
                  << (check_consistency() ? "" : "not ")
                  << "realisable.\n";
    result_stream << "--------------------------------------------------\n";
  }

  void append_time_out_report() {
    result_stream << "Realisability checking timed out.\n";
    result_stream << "--------------------------------------------------\n";
  }

  void append_failed_realisability() {
    result_stream << "Realisability checking failed.\n";
    result_stream << "--------------------------------------------------\n";
  }

  void append_successful_realisability() {
    result_stream << "Checking realisability took: " << realisability_time.count()
               << " s.\n";
    result_stream << "This set of requirements is "
               << (realisability_output.find("is realizable") != String::npos ?
                   "" : "not ")
               << "realisable.\n";
    result_stream << "--------------------------------------------------\n";
    result_stream << "Evidence:\n";
    String dotFileContent;
    if (bbb::file_exists(txt_file_evidence))
      bbb::read_file(txt_file_evidence, dotFileContent);
    result_stream << dotFileContent;
    result_stream << "--------------------------------------------------\n";
  }
  
  void append_successful_realisability(const std::pair<std::vector<bool>,
                                                       size_t>& unrealising) {
    result_stream << "Checking realisability took: " << realisability_time.count()
               << " s.\n";
    result_stream << "This set of requirements is not realisable.\n";
    result_stream << "--------------------------------------------------\n";
    result_stream << "A realisable subsets consists of: ";
    std::vector<size_t> indices;
    for (size_t i = 0; i < unrealising.first.size(); i++)
      if (unrealising.first[i])
        indices.push_back(i);
    bbb::write_separated(result_stream, indices, " ");
    result_stream << "\n--------------------------------------------------\n";
    result_stream << "A violating requirement is: " << unrealising.second;
    result_stream << "\n--------------------------------------------------\n";
    result_stream << "Evidence:\n";
    String dotFileContent;
    if (bbb::file_exists(txt_file_evidence))
      bbb::read_file(txt_file_evidence, dotFileContent);
    result_stream << dotFileContent;
    result_stream << "--------------------------------------------------\n";
  }
  //--------------------Reporting Functions-------------
  //--------------------Helper Functions----------------
  void remove_exists(const String& file_name) {
    if (bbb::file_exists(file_name))
      std::remove(file_name.c_str());
  }
  bool check_consistency() {
    return result_stream.str().find("not consistent") != String::npos;
  }
  // void write_into(const String& content, const String& file_name) {
  //   std::ofstream file_stream;
  //   file_stream.open(file_name);
  //   file_stream << content;
  //   file_stream.close();
  // }

  template<typename Con>
  void write_separated(const Con& con, String& f, String sep) {
    std::stringstream ss;
    bbb::write_separated(ss, con, sep);
    f = ss.str();
  }

  // void prepare_script(const m_tool_configuration& config) {
  //   String temp = "#!/bin/bash\n";
  //   temp += "cd " + config.folder_name + "\n";
  //   temp += config.command + " ";
  //   String params;
  //   write_separated(config.parameters, params, " ");
  //   temp += params;
  //   temp += "\n";//    temp += " > " + config.evidence_file + "\n";
  //   write_into(temp, config.script_name);
  // }
  String decorate(const String& input) {
    return "";
  }

  void start_processes(const std::vector<m_tool_configuration>& cs,
                             std::map<size_t, Popen>&              ps);

  void fill_configurations(std::vector<m_tool_configuration>& cs);
  void prune(const Strings& in, const Indices& indices, Strings& out);
  process_result check_process(Popen& p, const String& output) {
    DEB("Checking a process.");
    auto res = p.poll();
    if (res == -1)
      return process_result::running;
    if (res > 0)
      return process_result::failed;
    DEB("Process finished successfully.");
    //realisability_output = p.communicate().first.buf.data();
    bbb::read_file(output, realisability_output);
    DEB("Collected realisability output.");
    if (realisability_output.find("error") != String::npos)
      return process_result::failed;
    else
      return process_result::success;
  }
//--------------------Helper Functions----------------
};
