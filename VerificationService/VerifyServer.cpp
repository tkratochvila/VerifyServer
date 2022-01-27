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
 * File:   VerifyServer.cpp
 * Author: Tomas Kratochvila <tomas.kratochvila at honeywell.com>
 * Author: Petr Bauch <petr.bauch at honeywell.com>
 */

#include <gflags/gflags.h>
#include <folly/Memory.h>
#include <folly/Portability.h>
#include <folly/io/async/EventBaseManager.h>
#include <proxygen/httpserver/HTTPServer.h>
#include <proxygen/httpserver/RequestHandlerFactory.h>
#include <unistd.h>

#include "ToolKit.h"
#include "ToolKitXMLFactory.h"
#include "VerifyRequestHandler.h"
#include "VerificationService.h"

using namespace VerifyService;
using namespace proxygen;

using folly::EventBase;
using folly::EventBaseManager;
using folly::SocketAddress;

using Protocol = HTTPServer::Protocol;

DEFINE_int32(http_port, 6080, "Port to listen on with HTTP protocol");
DEFINE_int32(spdy_port, 6001, "Port to listen on with SPDY protocol");
DEFINE_int32(h2_port, 6002, "Port to listen on with HTTP/2 protocol");
DEFINE_string(ip, "0.0.0.0", "IP/Hostname to bind to");
DEFINE_int32(threads, 1, "Number of threads to listen on. Numbers <= 0 "
             "will use the number of cores on this machine.");
DEFINE_string(toolkit_file, "toolkit.xml", "Configuration file with available verification tools");

class VerifyRequestHandlerFactory : public RequestHandlerFactory {
 public:
  void onServerStart(folly::EventBase* evb) noexcept override {
    ToolKit::ToolKit toolkit = ToolKit::ToolKitXMLFactory::create(FLAGS_toolkit_file);
    verificationService.reset(new VerificationService(std::move(toolkit)));
  }

  void onServerStop() noexcept override {
    verificationService.reset();
  }

  RequestHandler* onRequest(RequestHandler*, HTTPMessage*) noexcept override {
    return new VerifyRequestHandler(verificationService.get());
  }

 private:
  folly::ThreadLocalPtr<VerificationService> verificationService;
};

int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();

  std::vector<HTTPServer::IPConfig> IPs = {
    {SocketAddress(FLAGS_ip, FLAGS_http_port, true), Protocol::HTTP},
    {SocketAddress(FLAGS_ip, FLAGS_spdy_port, true), Protocol::SPDY},
    {SocketAddress(FLAGS_ip, FLAGS_h2_port, true), Protocol::HTTP2},
  };

  if (FLAGS_threads <= 0) {
    FLAGS_threads = sysconf(_SC_NPROCESSORS_ONLN);
    CHECK(FLAGS_threads > 0);
  }

  HTTPServerOptions options;
  options.threads = static_cast<size_t>(FLAGS_threads);
  options.idleTimeout = std::chrono::milliseconds(60000);
  options.shutdownOn = {SIGINT, SIGTERM};
  options.enableContentCompression = true;
  options.handlerFactories = RequestHandlerChain()
      .addThen<VerifyRequestHandlerFactory>()
      .build();

  HTTPServer server(std::move(options));
  server.bind(IPs);

  // Start HTTPServer mainloop in a separate thread
  std::thread t([&] () {
    server.start();
  });

  t.join();
  return 0;
}
