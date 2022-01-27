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
 * File:   FileSupport.h
 * Author: Petr Bauch <petr.bauch at honeywell.com>
 */

#pragma once

#include <sys/stat.h> //file exists
#include <string>
#include <functional>//hash

#include "bbb.h"

//namespace FS {

using namespace Basics;  
  
// struct FileNFO {
//   Hash id;
//   String path, name, ext;
//   Strings userAliases;

//   FileNFO() { assert(false);}
//   FileNFO(Hash id) : id(id) {}

//   bool read(std::ifstream& f) {
//     return true;
//   }
//   void write(std::fstream& f) const {
//     f << id;
//   }
// };

struct m_file_header {
  String original_name, extension;
  String local_path;
  Nat unique_id;

  String get_local_name() const {
    return local_path + std::to_string(unique_id);
  }

  m_file_header(const String& lp, Nat ui) : local_path(lp), unique_id(ui) {}
};

struct m_prefile {
  String content;
  Hash id;

  Hash _hash() {
    SSize from = content.size() / 4;
    SSize to = std::min(from + 1000, content.size());
    return std::hash<String>{}(bbb::interval(content, from, to));
  }
  
  m_prefile(const String& c) : content(c) {
    id = _hash();
  }
};

struct m_file {
  bbb::Maybe<m_file_header> h;
  bbb::Maybe<m_prefile> p;

  m_file(const &m_prefile p) : p(p) {}
};

struct File {
  bbb::Maybe<String> _file;
  bbb::Maybe<Hash> id;
  bbb::Maybe<std::pair<String, String>> loc;

  File() {}//_file({}), nfo({}) {}
  //File(const String& _f) : _file(_f), nfo(_hash()) {}
  File(const Hash& id) : id(id) {}
  File(const File& f) : id(f.id) {
    if (f._file)
      _file.validate(f._file.value());
    loc = f.loc;
  }
  File(const String& n) {
    String f;
    assert(bbb::read_file(n, f));
    _file.validate(f);
    id.validate(_hash());
    std::rename(n.c_str(), name().c_str());
  }

  // void add_alias(const String& a) {
  //   assert(nfo);
  //   nfo.value().userAliases.push_back(a);
  // }
  void build(const String& content) {
    _file.validate(content);
    id.validate(_hash());

//DEB("Read file \"" + _file.value() + "\"");
//DEB("Assigned hash \"" + std::to_string(id.value()) + "\"");
  }
  Hash _hash() {
    assert(_file);
    auto content = _file.value();
    SSize from = content.size() / 4;
    SSize to = std::min(from + 1000, content.size());
//DEB("Hash using \"" + bbb::interval(content, from, to) + "\"");
    return std::hash<String>{}(bbb::interval(content, from, to));
  }
  Hash hash() const {
    assert(id);
    return id.value();
  }
  bool operator==(const File& other) const {
    assert(_file && other._file);
    return _file.value() == other._file.value();
  }
  bool store(const String& path, const String& ln) {
    loc.validate(path, ln);
    assert(_file && id);
    std::ofstream s(path + ln);
    if (!s.is_open())
      return false;
    s << _file.value();
    s.close();
    // id.value().path = path;
    // id.value().name = name;
    return true;
  }
  String name() const {
    assert(loc);
    return loc.value().second;
  }
  String full_name() const {
    assert(loc);
    return loc.value().first + loc.value().second;
  }
  bool load(const String& path, const String& ln) {
    assert(!loc);
    loc.validate(path, ln);
    String f;

    if (!bbb::read_file(full_name(), f))
      return false;
    _file.validate(f);
    return true;
  }
  bool read(std::ifstream& f) {
    String idLine;
    if (!std::getline(f, idLine).good())
      return false;
    auto tempId = bbb::s2u(idLine);
    if (!tempId)
      return false;
    id.validate(tempId.value());
    return true;
  }
  void write(std::fstream& f) const {
    assert(id);
    f << id.value();
  }

  template<typename Stream>
  friend Stream& operator<<(Stream& s, const File& fr) {
    fr.write(s);
    return s;
  }

  bool file_exists() {
    return loc && bbb::file_exists(full_name());
}
  // String full_name() {
  //   assert(id);
  //   return id.value().path + id.value().name;
  // }
};

//}
