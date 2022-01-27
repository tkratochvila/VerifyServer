
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


#include <fstream>
#include<mutex>
#include <set>

#include "bbb.h"

#include "ToolKitXMLFactory.h"

namespace ToolKit {
  ToolKit ToolKitXMLFactory::create(const String& xmlFilePath) {
    String xmlString;
    bbb::read_file(xmlFilePath, xmlString);
    auto xml = XMLSupport::Xml(xmlString);
    return create(xml);
  }

  ToolKit ToolKitXMLFactory::create(const XMLSupport::Xml& xml) {
    ToolKit toolkit;
    XMLSupport::Indices toolItems;
    xml.find_items("tool", toolItems);
    for (const XMLSupport::Index toolItem : toolItems) {
      toolkit.insert(createToolFromItem(xml, toolItem));
    }
    return toolkit;
  }

  Tool ToolKitXMLFactory::createToolFromItem(const XMLSupport::Xml& xml, const XMLSupport::Index toolItemId)
  {
    String name, path, outputParser;
    bool singleInstance;
    std::set<String> capabilities;
    getToolProperties(xml, toolItemId, name, path, outputParser, singleInstance);
    getToolCapabilities(xml, toolItemId, capabilities);
    Tool tool(name, path, outputParser, singleInstance);
    tool.set_capabilities(std::move(capabilities));
    return tool;
  }

  void ToolKitXMLFactory::getToolProperties(const XMLSupport::Xml& xml, const XMLSupport::Index toolItemId, String& name, String& path, String& outputParser, bool& singleInstance) {
    XMLSupport::Indices parameters;
    xml.get_params(toolItemId, parameters);
    name = xml.find_param_value_string(parameters, "name");
    path = xml.find_param_value_string(parameters, "path");
    outputParser = xml.find_param_value_string(parameters, "output_parser");
    singleInstance = xml.find_param_value_bool(parameters, "single_instance");
  }

  void ToolKitXMLFactory::getToolCapabilities(const XMLSupport::Xml& xml, const XMLSupport::Index toolItemId, std::set<String>& capabilities) {
    XMLSupport::Indices subItems;
    xml.get_subitems(toolItemId, subItems);
    for (const XMLSupport::Index subItemId : subItems) {
      if (xml.item_name(subItemId) == "category") {
        XMLSupport::Indices parameters;
        xml.get_params(subItemId, parameters);
        capabilities.insert(xml.find_param_value_string(parameters, "name"));
      }
    }
  }
}