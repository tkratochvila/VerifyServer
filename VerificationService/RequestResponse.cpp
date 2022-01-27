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
 * File:   RequestResponse.cpp
 * Author: Tomas Kratochvila <tomas.kratochvila at honeywell.com>
 * Author: Petr Bauch <petr.bauch at honeywell.com>
 */

#include <algorithm>
#include <fstream>

#include "RequestResponse.h"

namespace RequestResponse {


void Request::finalise_verify() {
  try {
    requestType = RequestType::verify;
    workspaceID = headers.at("workspace");
    if (this->state != MultipartBodyCallback::State::FINISHED)
      throw std::runtime_error("Unfinished or missing file upload.");
    xml.fill(getFileDataAsString());
  }
  catch (const std::out_of_range& e) {
    requestType = RequestType::malformed;
  }
}

/// \image html copyingOfFiles.png
void Request::finalise_upload() {
  // Archive the file and also copy to workspace. Use full path relative to workspace.
  // here in finalise, only set data structures. Handle processing in the handler fn
  try {
    requestType = RequestType::upload;
    workspaceID = headers.at("workspace");
    if (this->state != MultipartBodyCallback::State::FINISHED)
      throw std::runtime_error("Unfinished or missing file upload.");
  }
  catch (const std::exception& e) {
    requestType = RequestType::malformed;
  }
}

void Request::finalise_monitor() {
  try {
    requestType = RequestType::monitor;
    workspaceID = headers.at("workspace");
    headers.at("id"); // Verify that id is present
  }
  catch (const std::out_of_range& e) {
    requestType = RequestType::malformed;
  }
}

void Request::finalise_workspace() {
  try {
    requestType = RequestType::workspace;
    workspace_cmd = headers.at("cmd");
    if (workspace_cmd == "new")
      workspace_tool = headers.at("tool");
    if (workspace_cmd == "destroy")
      workspaceID = headers.at("workspace");
  }
  catch (const std::out_of_range& e) {
    requestType = RequestType::malformed;
  }
}

/// Finalize the query request. Set the query field of the request to the value of the "cmd" header.
void Request::finalise_query() {
  try {
    requestType = RequestType::query;
    query = headers.at("cmd");
    if (query.find("kill") != std::string::npos) {
      workspaceID = headers.at("workspace");
    }
  }
  catch (const std::out_of_range& e) {
    requestType = RequestType::malformed;
  }
}

bbb::Maybe<String> Request::get_query_cmd() const {
  if (query.empty())
    return {};
  else
    return query;
}

/**
 * Reverts the Request to a clean state (as if just constructed)
 */
void Request::clear() {
  requestType = RequestType::malformed;
  headers.clear();
  xml.clear();
  query.clear();
  workspaceID.clear();
  workspace_cmd.clear();
  workspace_tool.clear();
}

/**
  Based on the "type" header of the request decide, which finalization has to be performed.
*/
void Request::finalise() {
  requestType = RequestType::malformed;
  if (headers.count("type") == 0 || headers["type"].empty())
    return;
  String type = headers["type"];
  if (type == "query") finalise_query();
  else if (type == "upload") finalise_upload();
  else if (type == "verify") finalise_verify();
  else if (type == "monitor") finalise_monitor();
  else if (type == "workspace") finalise_workspace();
}

String Request::get_tool_name() const {
  Strings all;
  xml.get_param_item("usesExecutionEnvironment", "resource", all);
  if (all.size() != 1)
    throw std::out_of_range("The OSLC request does not contain a usesExecutionEnvironment element.");
  String& name = all[0];
  if (name.find("://") != String::npos) // Check if name is a URL
  {
    size_t lastSlashPos = name.rfind('/');
    if (lastSlashPos +1 >= name.length())
      return ""; // Slash is the last character
    return name.substr(lastSlashPos + 1 ); // If name is a URI, return the part after the last /
  }
  return name;
}

Strings Request::get_parameters() const {
  Strings res;
  xml.get_values_after("CallParameters", res);
  return res;
}

Strings Request::get_input_names() const {
  Strings res;
  xml.get_values_after("InputFiles", res);
  return res;
}

String Request::get_call_schema() const {
  Strings res;
  xml.get_values_after("CallSchemaSignature", res);
  assert(res.size() == 1);
  return res[0];
}

String Request::get_auto_plan_name() const {
  Strings res;
  xml.get_param_item("AutomationPlan", "about", res);
  assert(res.size() == 1);
  return res[0];
}

bbb::Maybe<Nat> Request::get_id() const {
  return bbb::s2u(headers.at("id"));
}

}
