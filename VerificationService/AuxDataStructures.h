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
 * File:   AuxDataStructures.h
 * Author: Petr Bauch <petr.bauch at honeywell.com>
 */

#pragma once

#include <random>
#include <cstdio>
#include "bbb.h"

namespace bbb {

template<typename Container>
struct IOWrapped : Container {
  virtual void read() = 0;
  virtual void write() = 0;
};

template<typename Stream>  
static void bodies_to_stream(const Strings& bs, const String& bound, Stream& vf) {
  Strings lines;
  split_to_lines(bs[1], lines);
  assert(lines.size() > 3);
  int i;
  for (i = 0; lines[i].empty() || lines[i].find("Content-") != String::npos; i++) ;
  for (; i < lines.size() - 1; i++)
    vf << lines[i] << "\n";
  vf << lines.back();
  for (i = 2; i < bs.size() - 1; i++)
    vf << bs[i];
  
  split_to_lines(bs.back(), lines);
//  int i;
  for (i = lines.size() - 1; lines[i].empty() || lines[i].find(bound) != String::npos; i--) ;
  for (int j = 0; j < i; j++)
    vf << lines[j] + "\n";
}
  
struct PreFile {
  String path, orName;
  String ext, name;
  //std::ostream stream;
  uint64_t id;
  
  PreFile(String p, String o) : path(p), orName(o) {
    ext = orName.substr(orName.rfind(".") + 1);
    std::random_device rd;
    std::default_random_engine dre(rd());
    std::uniform_int_distribution<int> uid(0,1000);
    name = "temp_" + std::to_string(uid(dre));
  }

  String full_path() { return path + name + "." + ext; }
  String new_path(const String& nn) {
    name = nn;
    return full_path();
  }

  void fill(const Strings& con, const String& bound, bool hash = true) {
    std::ofstream stream(full_path());
    assert(stream.is_open());
    bodies_to_stream(con, bound, stream);
    stream.close();
    if (!hash)
      return;
    String fp = full_path();
    id = BMK_hash(fp.c_str());
    assert(std::rename(full_path().c_str(), new_path(std::to_string(id)).c_str()) == 0);
  }
};

struct File {
  enum struct Type {ltl, c, cpp, smv, part, other};

  uint64_t localId;
  String path;
  String name;
  String extension;

  String address;
  bool local;
  Type t;

  File(const PreFile& pf) : localId(pf.id), path(pf.path), name(pf.name), extension(pf.ext) {
    address = get_local_address();
    local = true;
    switch (extension[0]) {
    case 'l' : t = Type::ltl; break;
    case 'c' : t = Type::cpp; break;
    case 'p' : t = Type::part; break;
    case 's' : t = Type::smv; break;
    default : t = Type::other;
    }
  }
  
  bool operator==(const File& other) const { return localId == other.localId; }
  bool operator<(const File& other ) const { return localId < other.localId; }
};

//using Files = std::set<File>;
struct Files {
  std::set<File> base;
  String toolPath;
  using iterator = std::set<File>::iterator;

  Files(const String toolPath) : toolPath(toolPath) {}
  std::pair<std::set<File>::iterator, bool>
  emplace(const PreFile& pf) { return base.emplace(pf); }
};
//using Files = std::vector<File>;

struct VerificationRequest {
  
};

struct HttpRequest {
  enum struct Type { verify, monitor, upload, malformed };

  bool finalised;
  Type t;
  std::map<String, String> headers;
  Strings bodies;
  std::pair<std::set<File>::iterator, bool> answer;

  HttpRequest() : finalised(false) {}

  void finalise() {
    t = Type::malformed;
    if (headers.count("type") == 0 || headers["type"].empty())
      return;
    switch (headers["type"][0]) {
    case 'v' : t = Type::verify; break;
    case 'm' : t = Type::monitor; break;
    case 'u' : t = Type::upload; break;
    default :  break;
    }
  }
  

  bool prep_file() {
    if (headers.count("Content-Type") == 0)
      return false;
    if (headers["Content-Type"].find("form-data") == String::npos)
      return false;

    //Strings firstBodyLines;
    if (bodies.empty())
      return false;
    // bbb::split_to_lines(bodies[0], firstBodyLines);
    // if (firstBodyLines.size() <= 1)
    //   return false;
    // if (firstBodyLines[0] != boundary)
    //   return false;
    return true;
  }

  void to_file(Files& fs) {
    assert(prep_file());
    String idLine = bbb::get_line(bodies[1], 1);
    std::cout << "idline: " << idLine << std::endl;
    String name = bbb::from_to(idLine, "name=\"", "\"");
    String orFileName = bbb::from_to(idLine, "filename=\"", "\"");
    //std::cout << "Foo: " << orFileName << std::endl;
    String boundary = bbb::from_to(headers["Content-Type"], "boundary=", "\n");
    size_t length = std::stol(headers["Content-Length"]);
    PreFile pf(fs.toolPath, orFileName);
    pf.fill(bodies, boundary);
    answer = fs.emplace(pf);
  }

  void stringify_plan(String& plan) {
    std::stringstream ss;
    String boundary = bbb::from_to(headers["Content-Type"], "boundary=", "\n");
    bodies_to_stream(bodies, boundary, ss);
    plan = ss.str();
  }

  void stream_plan(std::stringstream& ss) {
    String boundary = bbb::from_to(headers["Content-Type"], "boundary=", "\n");
    bodies_to_stream(bodies, boundary, ss);
  }

  bool known() {
    return !answer.second;
  }

  String get_file_id() {
    return std::to_string(answer.first->localId);
  }
};

}
