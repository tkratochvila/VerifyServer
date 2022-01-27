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
 * File:   VerifyRequestHandler.h
 * Author: Tomas Kratochvila <tomas.kratochvila at honeywell.com>
 * Author: Petr Bauch <petr.bauch at honeywell.com>
 */

#pragma once

#include <folly/Memory.h>
#include <proxygen/httpserver/RequestHandler.h>

//#include "RFC1867.h"
#include "VerificationService.h"

namespace proxygen {
class ResponseHandler;
}

namespace VerifyService {

class VerifyRequestHandler : public proxygen::RequestHandler {
 public:
  explicit VerifyRequestHandler(VerificationService* verificationService);

  void onRequest(std::unique_ptr<proxygen::HTTPMessage> headers)
      noexcept override;

  void onBody(std::unique_ptr<folly::IOBuf> body) noexcept override;

  void onEOM() noexcept override;

  void onUpgrade(proxygen::UpgradeProtocol proto) noexcept override;

  void requestComplete() noexcept override;

  void onError(proxygen::ProxygenError err) noexcept override;

 private:
  VerificationService* const verificationService{nullptr};
  RequestResponse::RequestResponse requestResponse; 
  std::unique_ptr<folly::IOBuf> body_;
  std::unique_ptr<proxygen::RFC1867Codec> bodyCodec;

  void respond(String& head, String& value, String& body);
  void handle_verify(const RequestResponse::Request& request, String& result);
  void handle_monitor(const RequestResponse::Request& request, String& result);
  void handle_upload(const RequestResponse::Request& request, String& statusValue, String& result);
  void handle_query(const RequestResponse::Request& request, String& statusValue, String& result);
  void handle_workspace(const RequestResponse::Request& request, String& statusValue, String& result);
  
  void setupBodyCodec(const proxygen::HTTPHeaders& headers);
};

}

