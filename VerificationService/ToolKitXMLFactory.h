
//+*****************************************************************************
//                         Honeywell Proprietary
// This document and all information and expression contained herein are the
// property of Honeywell International Inc., are loaned in confidence, and
// may not, in whole or in part, be used, duplicated or disclosed for any
// purpose without prior written permission of Honeywell International Inc.
//               This document is an unpublished work.
//
// Copyright (C) 2019 Honeywell International Inc. All rights reserved.
// *****************************************************************************
// Contributors:
//      MiD   Michal Dobes
//
//+*****************************************************************************

#pragma once

#include "ToolKit.h"
#include "XMLSupport.h"

namespace ToolKit {
  class ToolKitXMLFactory {
  public:
    static ToolKit create(const String& xmlFilePath);
    static ToolKit create(const XMLSupport::Xml& xml);
  protected:
    static Tool createToolFromItem(const XMLSupport::Xml& xml, XMLSupport::Index toolItemId);
    static void getToolProperties(const XMLSupport::Xml& xml, const XMLSupport::Index toolItemId, String & name, String & path, String& outputParser, bool & singleInstance);
    static void getToolCapabilities(const XMLSupport::Xml& xml, const XMLSupport::Index toolItemId, std::set<String>& capabilities);
  };
}