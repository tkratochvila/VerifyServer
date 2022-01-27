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
 * File:   Minimal_subset.h
 * Author: Petr Bauch <petr.bauch at honeywell.com>
 */

#pragma once

#include <set>
#include <vector>
#include <random>
#include <algorithm>

#include "bbb.h"

using namespace Basics;

template<typename T>
struct m_set {
  using t_set = std::vector<bool>;
  using t_result = bbb::Maybe<std::pair<t_set, size_t>>;
  
  const std::vector<T>& original;
  size_t n;

  //std::random_device rd;
  //std::mt19937 g;
  size_t random_seed;
  
  m_set(const std::vector<T>& o) :
    original(o), n(original.size()),
    random_seed(SClock::now().time_since_epoch().count()) {}

  String to_string(const t_set& s) {
    String ret_val;
    for (const auto &v : s)
      ret_val += (v ? '1' : '0');
    ret_val += '\n';
    return ret_val;
  }
  
  size_t size(const t_set& s) {
    size_t count = 0;
    for (const auto &v : s)
      if (v)
        count++;
    return count;
  }

  void update_candidate(t_set& candidate, size_t depth) {
    int c_size = size(candidate);
    int diff = (int)depth - c_size;
    if (diff == 0)
      return;
    bool positive = diff > 0;
    t_set indices(positive ? diff              :        - diff,  true);
    t_set zeroes( positive ? n - c_size - diff : c_size + diff, false);
    indices.insert(indices.end(),
                   zeroes.begin(),
                   zeroes.end());
    std::shuffle(indices.begin(),
                 indices.end(),
                 std::default_random_engine(random_seed));
    assert(size(indices) == size_t(positive ? diff : -diff));
    for (size_t i = 0, j = 0; i < n; i++)
      if (candidate[i] != positive && indices[j++])
        candidate[i] = !candidate[i];
    assert(size(candidate) == depth);
  }

  size_t get_dif_index(const t_set& s1,
                       const t_set& s2) {
    size_t i;
    for (i = 0; i < s1.size(); i++)
      if (s1[i] != s2[i])
        break;
    return i;
  }

  //Predicate :: Set a -> Set Bool -> Bool
  //precondition: p(\emptyset) /\ \neg p(original)
  //postcondition: strict_subset(ret2,ret1) /\ p(ret1) /\ \neg p(ret2)
  //complexity: O(log n) calls of p
  template<typename Predicate>
  t_result find_minimal(const Predicate& p) {
    size_t lower = 0;
    //size_t range = n; Invariant: upper=lower + range
    t_set cur_candidate(n, false);
    t_set pre_candidate(n, false);

    std::cout << "known inconsistent: " << n << std::endl;
    std::cout << "known consistent: " << 0 << std::endl;
    for (size_t range = n/2; range > 0; range = range/2) {
      std::cout << "lower: " << lower << std::endl;
      std::cout << "range:" << range << std::endl;
      size_t current_depth = lower + range;
      pre_candidate = cur_candidate;
      update_candidate(cur_candidate, current_depth);
      auto answer = p(original, cur_candidate);
      if (answer.fail)
        return t_result();
      if (answer.yes_no)
        std::cout << "known consistent: " << current_depth << ": " << to_string(cur_candidate) << std::endl;
      else
        std::cout << "known inconsistent: " << current_depth << ": " << to_string(cur_candidate) << std::endl;
      lower = answer.yes_no ? current_depth : lower;
    }
    auto answer = p(original, cur_candidate);
    if (answer.fail)
      return t_result();
    
    if (answer.yes_no)
      return std::make_pair(cur_candidate,
                            get_dif_index(cur_candidate, pre_candidate));
    else
      return std::make_pair(pre_candidate,
                            get_dif_index(cur_candidate, pre_candidate));
  }
};
