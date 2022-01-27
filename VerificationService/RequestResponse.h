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
 * File:   RequestResponse.h
 * Author: Tomas Kratochvila <tomas.kratochvila at honeywell.com>
 * Author: Petr Bauch <petr.bauch at honeywell.com>
 */

#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <map>
#include <cassert>
#include <proxygen/lib/http/experimental/RFC1867.h>
#include <sstream>

#include "bbb.h"
#include "XMLSupport.h"
#include "Workspace.h"

namespace RequestResponse {

using namespace Basics;

class MultipartBodyCallback : public proxygen::RFC1867Codec::Callback {
  // TODO: this way it will only store a single file. Extend (use a callback or something).
   public:
     const std::string& getFieldName() const {return fieldName;} 
     const std::string& getFileName() const {return fileName;} 
     const std::string& getFileDataAsString() const {return fieldString;} 
     
    // return < 0 to skip remainder of field callbacks?
    virtual int onFieldStart(const std::string& name,
                             folly::Optional<std::string> filename,
                             std::unique_ptr<proxygen::HTTPMessage> msg,
                             uint64_t postBytesProcessed) override {
      this->state = INCOMPLETE;
      this->fieldName = name;
      this->fileName = filename.value_or("");
      return 0;
    }
    
    virtual int onFieldData(std::unique_ptr<folly::IOBuf> data,
                           uint64_t postBytesProcessed) override {
      if (fieldData)
        fieldData->prependChain(std::move(data));
      else
        fieldData = std::move(data);
      return 0;
    }
    
    /** On reading to end of a part indicated by boundary
     * @param endedOnBoundary indicate successful part end
     */
    virtual void onFieldEnd(bool endedOnBoundary,
                            uint64_t postBytesProcessed) override {
      if (fieldData)
        this->fieldString = fieldData->moveToFbString().toStdString(); 
      this->state = FINISHED;
    }
    
    virtual void onError() override {
      this->state = ERROR;
    }
    
  enum State { INCOMPLETE, FINISHED, ERROR};
  
  State state = INCOMPLETE;
  std::string fieldName;
  std::string fileName;
  std::string fieldString;
  std::unique_ptr<folly::IOBuf> fieldData;
};


struct Request : public MultipartBodyCallback {
  // TODO: refactor this into a request factory with polymorphic requests
  enum struct RequestType { verify, monitor, upload, query, workspace, malformed };

  RequestType requestType;
  std::map<String, String> headers;
  XMLSupport::Xml xml;
  String query;
  Workspace::WorkspaceID workspaceID;
  String workspace_cmd;
  String workspace_tool;

  Request() {}

  void clear();
  
  void finalise();

  void finalise_verify();
  void finalise_upload();
  void finalise_monitor();
  void finalise_query();
  void finalise_workspace();

  /**
   * Retrieves tool name from the Verify Request's OSLC plan's usesExecutionEnvironment element.
   * Throws std::out of range if the usesExecutionEnvironment element is not present
   * @return 
   */
  String get_tool_name() const;
  Strings get_parameters() const;
  Strings get_input_names() const;
  String get_call_schema() const;
  String get_auto_plan_name() const;
  bbb::Maybe<Nat> get_id() const;
  bbb::Maybe<String> get_query_cmd() const;

  String get_bound() {
    assert(headers.count("Content-Type") != 0);
    assert(headers["Content-Type"].find("boundary") != String::npos);
    return bbb::dequote(bbb::from_to(headers["Content-Type"], "boundary=", "\n"));
  }
};

struct RequestResponse {

  Request request;

  void push_header(const String& h, const String& v) {
    request.headers[h] = v;
  }
  
  void clear_request() { request.clear(); }

  const Request& get_last() {
    request.finalise();
    return request;
  }

  void embody(Nat id, String& s) { s = std::to_string(id); }
};

}
