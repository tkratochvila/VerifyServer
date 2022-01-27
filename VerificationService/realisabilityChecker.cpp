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
 * File:   RealisabilityChecker.cpp
 * Author: Petr Bauch <petr.bauch at honeywell.com>
 */

#include "bbb.h"

#include "sanity_checker.h"
#include "minimal_subset.h"

int resolve_fails(m_sanity_checker& sc) {
  if (sc.realisability_timed_out) {
    sc.append_time_out_report();
    return 1;
  }
  if (sc.all_processes_failed) {
    sc.append_failed_realisability();
    return 2;
  }
  return 0;
}

//--argv[1] .. file containing all requirements separated by new lines
//--argv[2] .. file containing variable partitioning
//--argv[3] .. comma separated list of selected requirement indices
//--argv[4] .. timeout (in seconds)
int main(int argc, char** argv) {
  m_par_parser pp(argc, argv);
  if (!pp.valid())
    return 1;

  m_rolling_reporter rr();
  m_consistency_checker cc(pp.requirements);
  auto mises = cc.get_mises();
  if (mises.empty())
    rr.append_consistent();
  else
    rr.append_inconsistent(mises);

  m_vacuity_checker vc(pp.requirements);
  auto mvses = vc.get_mvses();
  if (mvses.empty())
    rr.append_nonvacuous();
  else
    rr.append_vacuous(mvses);

  m_realisability_checker rc(pp.requirements);
  if (rc.is_realisable()) {
    rr.append_realisable();
    return 0;
  }

  auto muses = rc.get_muses();
  
  m_sanity_checker mm(argc, argv);

  mm.check_arguments();
  mm.select_requirements();
  mm.generate_vacuity_witnesses();
  mm.check_consistency();
  mm.check_vacuity();
  if (!mm.check_io_variables()) {
    mm.append_trivial_real_report();
    return 0;
  }
  //check the whole set
  mm.call_real_checkers();
  if (mm.realisable()) {
    mm.append_successful_realisability();
    return 0;
  }
  //resolve time-out and failed execution
  int ret_val = resolve_fails(mm);
  if (ret_val != 0) return ret_val;

  //full set is unrealisable -> find one minimal unrealisable subset
  m_set<String> requirements(mm.selected_requirements);
  auto p = requirements.find_minimal(
    [&] (const Strings& full_set,
         const typename m_set<String>::t_set& subset_ids) {
      DEB("Calling subset real check.");
      Strings cur_subset;
      mm.prune(full_set, subset_ids, cur_subset);
      if (!mm.update_req_file(cur_subset)) {
        return bbb::YesNoFail(mm.check_consistency());
      }
      mm.call_real_checkers();
      if (resolve_fails(mm) != 0)
        return bbb::YesNoFail();
      else
        return bbb::YesNoFail(mm.realisable());
    });

  if (!p) return 1;
  mm.append_successful_realisability(p.value());
  return 0;
}
