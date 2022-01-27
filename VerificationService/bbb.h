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
 * File:   bbb.h
 * Author: Petr Bauch <petr.bauch at honeywell.com>
 */

#pragma once

#include <string>
#include <vector>
#include <cassert>
#include <fstream>
#include <sstream>
#include <chrono>
#include <random>
#include <iomanip>
#include <functional>
#include <algorithm>
#include <iostream>
#include <unordered_set>
#include <unordered_map>
#include <cstdlib>
#include <sys/stat.h>
#include <cstdio>
#include <unistd.h>	// for Windows this would be <direct.h>

// static inline void DEB(const std::string& message) {
//   std::cout << "--------------------------------------------------\n";
//   std::cout << message << std::endl;
//   std::cout << __func__ << "(" << __FILE__ << ":" << __LINE__ << ")\n";
//   std::cout << "--------------------------------------------------\n";
// }

#define DEB(message)   std::cout << "--------------------------------------------------\n" << message << std::endl << __func__ << "(" << __FILE__ << ":" << __LINE__ << ")\n" << "--------------------------------------------------\n";

//#define DEB(a, args...) printf("%s(%s:%d)\n" a,  __func__,__FILE__, __LINE__, ##args)

#define GetCurrentDir getcwd	// for Windows this would be "_getcwd"

/** Type aliases */
namespace Basics {
  using Hash = uint64_t;
  using Dur = std::chrono::duration<long, std::ratio<1,1000>>;
  using SClock = std::chrono::system_clock;
  using TimePoint = SClock::time_point;
  using Nat = uint32_t;
  using String = std::string;
  using Strings = std::vector<String>;
  using SSize = std::string::size_type;
}

namespace bbb {
  
using namespace Basics;
  
template<typename T>
struct UHash {
  using Pool = std::unordered_set<Hash>;
  
  std::vector<T> store;
  std::unordered_map<Hash, Pool> index;

  Hash insert_unchecked(const T& value, Hash h) {
    Hash id = store.size();
    index[h].insert(id);
    store.push_back(value);
    return id;
  }

  template<class... Args>
  Hash emplace(Hash h, Args&&... args) {
    Hash id = store.size();
    index[h].insert(id);
    store.emplace_back(args...);
    return id;
  }

  std::pair<bool, Hash>
  insert(const T& value) {
    Hash h = value.hash();
    if (index.count(h) != 0) {
      Pool& p = index[h];
      for (auto &id : p) {
        if (value == store[id])
          return {false, id};
      }
    }
    return {true, insert_unchecked(value, h)};
  }

  template<class... Args>
  std::pair<bool, Hash>
  try_emplace(Hash h, Args&&... args) {
    if (index.count(h) != 0) {
      Pool& p = index[h];
      Hash id = store.size();
      store.emplace_back(args...);
      for (auto &id : p) {
        if (store.back() == store[id]) {
          store.pop_back();
          return {false, id};
        }
      }
      p.insert(id);
      return {true, id};
    }
    return {true, emplace(h, args...)};
  }

  bool has(const T& value) {
    Hash h = value.hash();
    if (index.count(h) == 0)
      return false;
    Pool p = index[h];
    for (auto &id : p) {
      if (value == store[id])
        return true;
      else
        return false;
    }
  }

  bool has_id(Hash id) const {
    return id < store.size();
  }

  const T& decode(Hash id) {
    assert(id < store.size());
    return store[id];
  }

  T& decode_mod(Hash id) {
    assert(id < store.size());
    return store[id];
  }

  template<typename Fun>
  void for_each(Fun f) {
    for (auto &e : store) {
      f(e);
    }
  }

  void clean() {
    store.clear(); index.clear();
  }
};

using String = std::string;
using Strings = std::vector<String>;
using SClock = std::chrono::system_clock;
using TimePoint = SClock::time_point;
using SSize = std::string::size_type;

template<typename T>
struct Maybe {
  T* potValue;

  Maybe() : potValue(nullptr) {}
  //Maybe(Maybe& m) : potValue(m.potValue) { m.potValue = nullptr; }
  Maybe(const Maybe& m) {
    //std::cout << "--------------------copy-ref--------------------\n";
    potValue = (m ? new T(*m.potValue) : nullptr);
  }
  Maybe(Maybe& m) {
    //std::cout << "--------------------copy--------------------\n";
    potValue = (m ? new T(*m.potValue) : nullptr);
  }
  Maybe& operator=(const Maybe& m) {
    //std::cout << "--------------------eq--------------------\n";
    potValue = (m ? new T(*m.potValue) : nullptr);
    return *this;
  }
  // template<class... Args>
  // Maybe(Args&&... args) {
  //   std::cout << "--------------------build--------------------\n";
  //   potValue = new T(args...);
  //   std::cout << "value is " << *potValue << std::endl;
  // }
  Maybe(const T& val) : potValue(new T(val)) {}
  
  ~Maybe() {
    if (potValue != nullptr)
      delete potValue;
  }

  template<class... Args>
  void validate(Args&&... args) {
    if (potValue != nullptr)
      delete potValue;
    potValue = new T(args...);
  }
  operator bool() const {
    return potValue != nullptr;
  }
  operator T() const {
    return *potValue;
  }

  T& value() const {
    return *potValue;
  }
};

template<typename T>
struct MaybeRef {
  T* potValue;

  MaybeRef() : potValue(nullptr) {}
  MaybeRef(T& value) {
    potValue = &value;
  }

  operator bool() const {
    return potValue != nullptr;
  }

  T& value() const {
    return *potValue;
  }
};

struct YesNoFail {
  bool yes_no;
  bool fail;

  YesNoFail() : yes_no(true), fail(true) {}
  YesNoFail(bool v) : yes_no(v), fail(false) {}
};
  
static inline Maybe<Nat> s2u(const char* s, size_t size) {
  String strng = s;
  strng = "s = " + strng + " ; size = " + std::to_string(size);
  DEB(strng);
  if (size == 0)
    return {};
  if (size == 1 && s[0] == '0')
    return 0;
  char* c;
  Hash retV = std::strtoul(s, &c, 10);
  if (retV == 0)
    return {};
  return retV;
}

static inline Maybe<Nat> s2u(const String& s) {
  return bbb::s2u(s.c_str(), s.size());
}

// template<typename FwIt, typename E, typename B, typename Pred>
// void iterate_remove(FwIt first, FwIt last, E each, B bad, Pred p) {
//   std::for_each(
//   while (first != last && !p(*first))
//     each(*first++);

//   if (first != last)
//     for(FwIt i = first; ++i != last; ) {
//       each(*first);
//       if (!p(*i))
//         *first++ = std::move(*i);
//       else
//         bad(*i);
//     }
// }

// template<typename T>
// Hash uhash(const T& s) {
//   return std::hash<T>{}(s);
// }

// template<typename Con>
// Hash chash(const Con& c) {
//   Hash h = 0;
//   for (auto& e : c)
//     h ^= 
// }

static inline bool is_white(const String& in) {
  for (auto &c : in)
    if (std::isgraph(c))
      return false;
  return true;
}
  
template<typename Stream>
void write_enclosed(Stream& f, size_t n, char c, String name) {
  f << String(n, c) << name << String(n, c);
}

/** Get the number of chars (c) at the beginning of string (s). */
static inline size_t n_char(const String& s, char c) {
  size_t n;
  for (n = 0; n < s.size() && s[n] == c; n++);
  return n;
}

/** Get the length of the beginning of the string (s) which remains
 * after removal of all consecutive chars (c) at the end of this string. 
 */
static inline size_t n_char_r(const String& s, char c) {
  int n;
  for (n = s.size(); n >= 0 && s[n] == c; n--);
  return s.size() - n;
}

template<typename Stream>
std::tuple<bool, size_t, String> read_enclosed(Stream& f, char encC, char endC) {
  String line;
  std::getline(f, line);
  if (line.empty() || line.back() != endC)
    return {false, 0, ""};
  size_t count = n_char(line, encC);
  line.pop_back();
  if (count != n_char_r(line, encC))
    return {false, 0, ""};
  return {true, count, line.substr(count, line.size() - 2*count)};
}

template<typename Stream, typename Con>
void write_separated(Stream& f, const Con& con, String sep) {
  if (con.empty())
    return;
  f << *con.begin();
  for (auto it = ++con.begin(); it != con.end(); it++)
    f << sep << *it;
}

static inline void write_separated(String& to, const Strings& from, String sep) {
  std::stringstream ss;
  write_separated(ss, from, sep);
  to = ss.str();
}

/// Return substring of the string (in) from position (from) to position (to).
static String interval(const String& in, SSize from, SSize to) {
  assert(to <= in.size());
  assert(to >= from);
  SSize count = to - from;
  return in.substr(from, count);
}

static inline void replace_all(const String& in, String& out,
                               const String& from, const String& to) {
//  assert(to.find(from) == String::npos);
  SSize index = 0;
  SSize start = 0;
  out = in;
  while ((index = out.find(from, start)) != String::npos) {
    out.replace(index, from.size(), to);
    start = index + to.size();
  }
}

static inline void replace_all(String& in, const String& from, const String& to) {
  replace_all(in, in, from, to);
}

static void sanitise(String& in) {
  in.erase(std::remove(in.begin(), in.end(), '\r'), in.end());
}

static inline void split_to_lines(const String& orig, Strings& out) {
  SSize left, right;
  String in = orig;
  replace_all(orig, in, "\r\n", "\n");
  replace_all(in, "\r", "\n");
  for (left = 0; (right = in.find("\n", left)) != String::npos; left = right + 1) {
    out.push_back(interval(in, left, right));
  }
  if (left != in.size())
    out.push_back(interval(in, left, in.size()));
}

static inline bool read_file(const String fileName, String& fileString) {
  std::ifstream t(fileName);
  if (!t.is_open())
    return false;
  t.seekg(0, std::ios::end);   
  fileString.reserve(t.tellg());
  t.seekg(0, std::ios::beg);
  fileString.assign(std::istreambuf_iterator<char>(t),
           std::istreambuf_iterator<char>());
  return true;
}

static inline bool read_file(const String fileName, Strings& lines) {
  std::ifstream fs(fileName);
  if (!fs.is_open()) {
    return false;
  }
  String line;
  while (std::getline(fs, line)) {
    lines.push_back(line);
  }
  fs.close();
  return true;
}

static inline void write_file(const String& fileName, const Strings& lines) {
  std::ofstream t;
  t.open(fileName.c_str());
  for (auto &l : lines) {
    t << l;
    t.flush();
    t << std::endl;
  }
  t.close();
}

static inline void negate_ltl_formula(const String& in, String& out) {
  assert(in.size() > 0);
  out = "!(";
  out.append(in);
  out.push_back(')');
}

template<typename In, typename Out, typename Valid, typename ExtractFun>
static inline void extract_if(const In& in, Out& out, Valid v, ExtractFun ef) {
  out.clear();
  for (auto &e: in) {
    if (!v(e))
      continue;
    out.push_back(ef(e));
  }
}

static inline String dequote(String s) {
  SSize start = (s[0] == '\"' ? 1 : 0);
  SSize count = s.size() - (s.back() == '\"' ? 1 : 0);
  return String(s, start, count - start);
}

/// Return a container (vector) of Strings that in the input "in" are separated by white-space.
static inline void remove_ws(const String& in, Strings& out) {
  SSize from = 0;
  for (SSize i = 0; i < in.size(); i++) {
    if (std::isspace(in[i])) {
      if (i > from) {
        out.push_back(interval(in, from, i));
      }
      from = i + 1;
    }
  }
  if (in.size() > from) {
    out.push_back(interval(in, from, in.size()));
  }
}

/// Returns e.g. "2018-11-19 12:28:26"
static inline String time_to_string(TimePoint time, const String& format = "%F %T") {
  std::stringstream ss;
  std::time_t tt = SClock::to_time_t(time);
  ss << std::put_time(std::localtime(&tt), format.c_str());
  return ss.str();
}

/// Return the substring of (in) which starts immediately after the first occurence of the string (from)
/// and ends before the following occurrence of the string (to),
/// or at the end of the string (in) if (to) is not found.
static inline String from_to(const String& in, const String from, const String to) {
  SSize left = in.find(from);
  assert(left != String::npos);
  left += from.size();
  SSize right = in.find(to, left);
  return interval(in, left, std::min(right, in.size()));
}

  
static inline String get_line(const String& in, int i) {
  SSize left, right;
  String temp = in;
  sanitise(temp);
  for (left = 0; ((right = temp.find("\n", left)) != String::npos) && i > 0; left = right + 1, i--) ;
  return interval(temp, left, std::min(right, temp.size()));
}

static inline void zip(const Strings& one, const Strings& two, const String& sep, String& out) {
  size_t maxSize = std::max(one.size(), two.size());
  for (size_t i = 0; i < maxSize; i++) {
    if (i < one.size()) {
      out += one[i];
      out += sep;
    }
    if (i < two.size()) {
      out += two[i];
      out += sep;
    }
  }
}

template<typename M>
static inline void zip_via(const std::vector<Strings>& in, const String& sep, String& out, M m) {
  for (size_t i = 0; ; i++) {
    auto p = m(i);
    if (!p)
      return;
    size_t f, s;
    std::tie(f, s) = p.value();
    assert(in.size() > f);
    assert(in[f].size() > s);
    out += in[f][s];
  }
}

static inline String find_line(const Strings& in, const String bait) {
  for (auto &f : in) {
    SSize pos = f.find(bait);
    if (pos == String::npos)
      continue;
    SSize left = f.rfind("\n", pos) + 1;
    SSize right = f.find("\n", pos);
    return interval(f, left, std::min(right, f.size()));
  }
  return "";
}

template<typename In, typename Out, typename F>
static inline void for_each(const std::vector<In> &in, std::vector<Out> &out, F f) {
  for (auto& i : in) {
    out.push_back(f(i));
  }
}

template<typename In, typename F>
static inline void for_each(std::vector<In> &in, F f) {
  for (auto& i : in) {
    f(i);
  }
}

static inline void split_by(const String& in, Strings& out, const String by) {
  SSize index = 0;
  SSize start = 0;
  while ((index = in.find(by, start)) != String::npos) {
    out.push_back(interval(in, start, index));
    start = index + by.size();
  }
  if (!in.empty() && start < in.size())
    out.push_back(String(in, start));
}

template<typename In, typename P>
static inline void filter(const std::vector<In> &in, std::vector<In> &out, P p) {
  for (auto &i : in)
    if (p(i))
      out.push_back(i);
}

template<typename In, typename P>
static inline bool exists(const In& in, P p) {
  for (auto &i : in)
    if (p(i))
      return true;
  return false;
}

static inline bool file_exists(const String& name) {
  struct stat buffer;
  return (stat (name.c_str(), &buffer) == 0);
}


template<typename T>
static inline int get_rand(T low = 0, T high = std::numeric_limits<T>::max()) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<size_t> dis(low, high);
  return dis(gen);
}

static inline String get_random_fname() {
  return "_" + std::to_string(get_rand<size_t>()) + ".tmp";
}

static inline void copy_file(const String& from, const String& to) {
  std::ifstream  src(from.c_str(), std::ios::binary);
  std::ofstream  dst(to.c_str(),   std::ios::binary);
  dst << src.rdbuf();
}

static inline void write_into(const String& content, const String& file_name) {
  std::ofstream file_stream;
  file_stream.open(file_name);
  file_stream << content;
  file_stream.close();
}

template<typename P, typename T>
static inline void vector_print(const std::vector<T>& con,
                                const String& sep,
                                String& out,
                                P p) {
  std::stringstream f;
  if (con.empty())
    return;
  f << p(*con.begin());
  for (auto it = ++con.begin(); it != con.end(); it++)
    f << sep << p(*it);
  out = f.str();
}

static inline Nat b2mb(Nat in) {
  return (in / 1024) / 1024; 
}

template<typename P>
static inline void zip_schema(const std::vector<Strings>& in,
                              const String& sep,
                              String& out,
                              P p) {
  for (Nat i = 0; ; i++) {
    auto id_pair = p(i);
    if (!id_pair)
      break;
    out += in[id_pair.value().first][id_pair.value().second] + sep; 
  }
}

static inline bool contains(const String& in, const String& subs) {
  return in.find(subs) != String::npos;
}

static inline bool ends_with(const String& value, const String& ending)
{
	if (ending.size() > value.size()) return false;
	return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

static inline std::string GetCurrentWorkingDir() {
  char buff[FILENAME_MAX];
  GetCurrentDir( buff, FILENAME_MAX );
  std::string current_working_dir(buff);
  return current_working_dir;
}
const char pathSeparator =
#ifdef _WIN32
                            '\\';
#else
                            '/';
#endif
}
