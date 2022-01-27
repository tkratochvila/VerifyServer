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
 * File:   VerifyRequestHandler.cpp
 * Author: Tomas Kratochvila <tomas.kratochvila at honeywell.com>
 * Author: Petr Bauch <petr.bauch at honeywell.com>
 */

#include "VerifyRequestHandler.h"

#include <iostream>
#include <string>
#include <proxygen/httpserver/RequestHandler.h>
#include <proxygen/httpserver/ResponseBuilder.h>
//#include <proxygen/lib/http/HTTPHeaders.h>
#include <folly/FileUtil.h>
#include <regex>

#include "bbb.h"
#include "Archive.h"

using namespace proxygen;

namespace VerifyService {

  VerifyRequestHandler::VerifyRequestHandler(VerificationService* VerificationService) : verificationService(VerificationService) {
  }

  /** Copy the headers from the HTTP message to the request/response headers information.
  */
  void VerifyRequestHandler::onRequest(std::unique_ptr<HTTPMessage> uniqueMessage) noexcept {
    HTTPHeaders headers = uniqueMessage->getHeaders();
    DEB("Received request:");
    headers.forEach([&] (const std::string& header, const std::string& value) {
        DEB("Header = " + header + " , value = " + value);
        requestResponse.push_header(header, value);
      });
      setupBodyCodec(headers);
    DEB("End of request.");
  }

  /** Insert (part of) the body to the body of the request.
  */
  void VerifyRequestHandler::onBody(std::unique_ptr<folly::IOBuf> body) noexcept {
    if (body_) {
      std::string str = (reinterpret_cast<const char*>( body->data()));
      DEB("Prepended part of body:\n" + str + "\nEnd of Prepended part of body");
      body_->prependChain(std::move(body));
    } else {
      std::string str = (reinterpret_cast<const char*>( body->data()));
      DEB("First part of body:\n" + str + "\nEnd of First part of body");
      body_ = std::move(body);
    }
    if (bodyCodec) {
      body_ = bodyCodec->onIngress(std::move(body_));
    }
  }

  /** Invoked when we finish receiving the body. (End Of Message) */
  void VerifyRequestHandler::onEOM() noexcept {
    if (bodyCodec)
      bodyCodec->onIngressEOM();
    std::string head, value, body;
    respond(head, value, body);
    DEB("Responding.\nThe message reads:\n" + head + " :- " + value
      + "\nBody on EOM:\n" + body + "\nEnd of Body on EOM.");
    ResponseBuilder(downstream_)
      .status(200, "OK")
      .header(head, value)
      .body(body)
      .sendWithEOM();
    DEB("Response sent.");
  }

  void VerifyRequestHandler::onUpgrade(UpgradeProtocol protocol) noexcept {
    // handler doesn't support upgrades
  }

  void VerifyRequestHandler::requestComplete() noexcept {
    delete this;
  }

  void VerifyRequestHandler::onError(ProxygenError err) noexcept {
    delete this;
  }


  /** Based on request type decide what action should be taken and call it. */
  void VerifyRequestHandler::respond(String& head, String& value, String& body) {
    // TODO: refactor this into a request factory with polymorphic requests. Handle using double dispatch / visitor pattern
    using RequestType = RequestResponse::Request::RequestType;
    head = "Status";
    value = "OK";
    const auto& request = requestResponse.get_last();
    switch (request.requestType) {
    case RequestType::verify :  
      DEB("handle_verify( head := " + head + " , value := " + value + " , ...)");
      handle_verify(request, body); break;
    case RequestType::monitor :
      DEB("handle_monitor( head := " + head + " , value := " + value + " , ...)");
      handle_monitor(request, body); break;
    case RequestType::upload :
      DEB("handle_upload( head := " + head + " , value := " + value + " , ...)");
      handle_upload(request, value, body); break;
    case RequestType::query :
      DEB("handle_query( head := " + head + " , value := " + value + " , ...)");
      handle_query(request, value, body); break;
    case RequestType::workspace :
      DEB("handle_workspace( head := " + head + " , value := " + value + " , ...)");
      handle_workspace(request, value, body); break;
    default :
      std::cerr << "Fail." << std::endl;
      value = "NOK";
      body = "Request unrecognised.";
      break;
    }
  }
  
  /// Handle verification request.
  void VerifyRequestHandler::handle_verify(const RequestResponse::Request& request, String& result) {
    try {
      auto answer = verificationService->verify(request);
      if (answer.first)
        result = "Verification successfully started.\nMonitor or request report n. ";
      else
        result = "Verification result already known.\nRequest report n. ";
      result += std::to_string(answer.second);
    }
    catch (const std::runtime_error& e) {
      result = "Error: ";
      result += e.what();
    }
    DEB(result);
  }

  void VerifyRequestHandler::handle_monitor(const RequestResponse::Request& request, String& result) {
    try {
      auto maybeReportID = request.get_id();
      if (!maybeReportID)
        throw std::runtime_error("Error: Cannot access report.");
      result = verificationService->getMonitoringOSLC(request.workspaceID, maybeReportID);
    }
    catch (const std::exception& e) {
      result = "Error: ";
      result += e.what();
    }
  }

  void VerifyRequestHandler::handle_upload(const RequestResponse::Request& request, String& statusValue,
                                          String& result) {
    try {
      DEB("get_file_content:\n" << request.getFileDataAsString() << "\nEnd of get_file_content().");
      for (std::map<String, String>::const_iterator it = request.headers.cbegin(); it != request.headers.cend(); it++) {
        DEB("Key: " + it->first + "\t\tValue: " + it->second);
      }
      auto answer = verificationService->addFile(request.workspaceID, request.getFileName(), request.getFileDataAsString());
      if (answer.first)
        result = "File successfully uploaded under id:" + std::to_string(answer.second);
      else {
        statusValue = "NOK";
        result = "File already stored under id:" + std::to_string(answer.second);
      }
    }
    catch (const std::exception& e) {
      statusValue = "NOK";
      result = "Error: ";
      result += e.what();
    }
  }

  void VerifyRequestHandler::handle_query(const RequestResponse::Request& request, String& statusValue,
                                         String& result) {
    auto mCmd = request.get_query_cmd();
    if (!mCmd) {
      statusValue = "NOK";
      result = "No query specified.";
      return;
    }
    String cmd = mCmd.value();
    if (bbb::contains(cmd, "kill")) {
      try {
        auto index = cmd.rfind(' ') + 1;
        if (index == String::npos)
          throw std::runtime_error("No report to kill specified.");
        auto reportNum = bbb::s2u(cmd.data() + index,
                                  cmd.size() - index);
        if (!reportNum)
          throw std::runtime_error( "Could not read the report number.");
        verificationService->killTask(request.workspaceID, reportNum);
      }
      catch (const std::runtime_error& e) {
        result = "Error: ";
        result += e.what();
      }
    }
    else if (bbb::contains(cmd, "availability")) {
      result = verificationService->getAvailabilityString();
    }
  }

  void VerifyRequestHandler::handle_workspace(const RequestResponse::Request& request, String& statusValue, String& result) {
    if (request.workspace_cmd == "new") {
      try {
        auto answer = verificationService->createWorkspace(request.workspace_tool);
        result = "Workspace successfully created.\n   id:" + answer.first + "\n   path:\"" + answer.second + "\"";
        // TODO: path should be relative to web root (check)
      }
      catch (const std::exception& err) {
        DEB("Workspace creation failed: " << err.what());
        result = "Workspace creation failed:";
        result += err.what(); // TODO: possible info leak
      }
    }
    else if (request.workspace_cmd == "destroy") {      
      verificationService->destroyWorkspace(request.workspaceID);
      result = "Workspace " + request.workspaceID + " destroyed.";
    }
  }  

  void VerifyRequestHandler::setupBodyCodec(const proxygen::HTTPHeaders& headers) {
    // TODO: convert header to lowercase?
    const std::regex multipartRegex(R"REGEX(multipart\/form-data\s*;\s*boundary=(([^ "]+)|"([^ "]+)"))REGEX");
    std::smatch matchResults;
    std::regex_match(headers.getSingleOrEmpty(proxygen::HTTPHeaderCode::HTTP_HEADER_CONTENT_TYPE), matchResults, multipartRegex);
    if (!matchResults.empty()) {
      std::string boundary;
      if (matchResults[2].matched)
        boundary = matchResults[2].str();
      else
        boundary = matchResults[3].str();
      bodyCodec.reset(new proxygen::RFC1867Codec(boundary));
      bodyCodec->setCallback(&(requestResponse.request));
//      bodyCodec->onHeaders(headers);
    }
    else {
      bodyCodec.reset();
    }
  }
}
