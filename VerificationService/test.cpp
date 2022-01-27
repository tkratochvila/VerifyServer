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
 * File:   XMLSupport.h
 * Author: Petr Bauch <petr.bauch at honeywell.com>
 */

#include <fstream>
#include <iostream>

#include "XMLSupport.h"

int main(int argc, char** argv) {
  if (std::ifstream f{argv[1], std::ios::ate}) {
    XMLSupport::Xml xml(f);
    xml.print(std::cout);
    return 0;
  }
  return 1;
}
