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
 * File:   vacuityChecker.cpp
 * Author: Tomas Kratochvila <tomas.kratochvila at honeywell.com>
 * Author: Petr Bauch <petr.bauch at honeywell.com>
 */

#include <iostream>
#include <fstream>
#include <string>

#include "bbb.h"
#include "subprocess.hpp"

using String = bbb::String;
using Strings = bbb::Strings;
using Indices = std::vector<size_t>;
using namespace subprocess;

//--argv[1] .. file containing all requirements separated by new lines
//--argv[2] .. file containing variable partitioning
//--argv[3] .. comma separated list of selected requirement indices

// void filter_unused(const Strings& ltls, const Strings& inputVars,
//                    const Strings& outputVars, String& newPartFile) {
//   newPartFile.clear();
//   newPartFile += ".inputs ";
//   for (auto iv : inputVars)
//     if (is_var_used(ltls, iv))
//       newPartFile += iv + " ";
//   newPartFile += "\n.outputs ";
//   for (auto ov : outputVars)
//     if (is_var_used(ltls, ov))
//       newPartFile += ov + " ";
// }

void prune(const Strings& in, const Indices& indices, Strings& out) {
  std::vector<bool> field(in.size(), false);
  for (auto i : indices)
    if (i < in.size())
      field[i] = true;
  for (size_t i = 0; i < field.size(); i++)
    if (field[i])
      out.push_back(in[i]);
}

bool is_var_used(const Strings& ltls, const String& var) {
  for (auto ltl : ltls)
    if (ltl.find(var) != String::npos)
      return true;
  return false;
}

bool no_inputs(const String& name) {
  Strings partitioningLines;
  if (!bbb::read_file(name, partitioningLines)) return false;

  for (auto &l : partitioningLines) {
    if (l.find(".inputs") != String::npos)
      return l.size() < 9;
  }
  return true;
}

bool is_consistent(const String& name) {
  String conFile;
  bbb::read_file(name, conFile);
  return conFile.find("is consistent") != String::npos;
}

int main(int argc, char** argv) {
  if (argc != 4) {
    std::cerr << "Wrong number of parameters.\n";
    for (int i = 0; i < argc; i++) {
      std::cerr << argv[i] << " | ";
    }
    std::cerr << std::endl;
  }
  Strings allRequirements;
  assert(bbb::read_file(argv[1], allRequirements));
  Strings nonemptyReqs;
  //std::cout << "Number of all lines: " << allRequirements.size() << std::endl;
  bbb::filter(allRequirements,
              nonemptyReqs,
              [&] (auto r)
              {
                return bbb::exists(r, [&] (auto c) {return !isspace(c);});
              }
             );
  //std::cout << "Number of all requirements: " << nonemptyReqs.size() << std::endl;
  Strings allIndices;
  bbb::split_by(argv[3], allIndices, ",");
  //std::cout << "Read indices: " << argv[2] << std::endl;
  Indices indices;
  bbb::for_each(allIndices,
                indices,
                [&] (auto s)
                {
                  auto maybeInt = bbb::s2u(s);
                  assert(maybeInt);
                  return maybeInt.value();
                }
               );
  //std::cout << "Number of requirements: " << indices.size() << std::endl;
  Strings selectedRequirements;
  prune(nonemptyReqs, indices, selectedRequirements);
  std::ofstream tempFile;
  String selReqFile = "tmp.ltl";
  tempFile.open(selReqFile);
  // for (auto &r : selectedRequirements) {
  //   if (r.back() != ';')
  //     r.push_back(';');
  // }
  bbb::write_separated(tempFile, selectedRequirements, "\n");
  tempFile << "\n";
  tempFile.close();

  String vacOutFile = "vacwitOutput.ltl";
  if (bbb::file_exists(vacOutFile)) std::remove(vacOutFile.c_str());
  auto p1 = Popen({"/var/www/vacwit.sh", selReqFile.c_str()}, input{PIPE},
                  output{vacOutFile.c_str()});
  p1.wait();

  String resFile = "partVerResult.txt";
  if (bbb::file_exists(resFile)) std::remove(resFile.c_str());
  auto p2 = Popen({"/var/www/looney", vacOutFile.c_str()}, input{PIPE},
                  output{resFile.c_str()});
  p2.wait();

  tempFile.open(selReqFile);
  bbb::write_separated(tempFile, selectedRequirements, ";\n");
  tempFile << ";\n";
  tempFile.close();

  Strings varPartitioning;
  assert(bbb::read_file(argv[2], varPartitioning) && varPartitioning.size() >= 2);
  // Strings inputVars;
  // bbb::tokenise(varPartitioning[0], inputVars);
  // inputVars.pop_front();
  // Strings outputVars;
  // bbb::tokenise(varPartitioning[1], outputVars);
  // outputVars.pop_front();
  // String partFileContent;
  // filter_unused(selectedRequirements, inputVars, outputVars, partFileContent);
  // std::ofstream partFile;
  String partFileName = argv[2];
  // partFile.open(partFileName);
  // bbb::write_separated(partFile, partFileContent, "\n");
  // partFile.close();
  
  if (no_inputs(partFileName)) {
    std::ofstream resultFile;
    resultFile.open(resFile.c_str(), std::ios_base::app);
    resultFile << "No input signals -> realisability equals consistency.\n";
    resultFile << "Checking realisability took: 0 s.\n";
    resultFile << "This set of requirements is "
               << (is_consistent(resFile.c_str()) ? "" : "not ")
               << "realisable.\n";
    resultFile << "--------------------------------------------------\n";
    resultFile.close();
    return 0;
  }

  // String reaFile = "realisabilityResult.txt";
  // if (bbb::file_exists(reaFile)) std::remove(reaFile.c_str());
  String dotFileName = "tmp.dot";
  String txtFileName = "tmp.txt";
  if (bbb::file_exists(dotFileName)) std::remove(dotFileName.c_str());
  if (bbb::file_exists(txtFileName)) std::remove(txtFileName.c_str());
  Basics::TP start = Basics::SClock::now();
  auto p3 = Popen({  "/home/kratochvila/code/VerificationServer/acaciaRun.sh",
                     selReqFile.c_str(),
                     partFileName.c_str()
                  },
                  input{PIPE},
                  output{PIPE});
  p3.wait();
  Basics::TP end = Basics::SClock::now();
  std::chrono::duration<double> reaTime = end - start;

  std::ofstream resultFile;
  resultFile.open(resFile.c_str(), std::ios_base::app);
  resultFile << "Checking realisability took: " << reaTime.count() << " s.\n";
  String obuf = p3.communicate().first.buf.data();
  resultFile << "This set of requirements is "
             << (obuf.find("is realizable") != String::npos ? "" : "not ")
             << "realisable.\n";
  resultFile << "--------------------------------------------------\n";
  resultFile << "Evidence:\n";
  String dotFileContent;
  // if (bbb::file_exists(dotFileName))
  //   bbb::read_file(dotFileName, dotFileContent);
  if (bbb::file_exists(txtFileName))
    bbb::read_file(txtFileName, dotFileContent);
  resultFile << dotFileContent;
  resultFile << "--------------------------------------------------\n";
  resultFile.close();
  
  return 0;
}
