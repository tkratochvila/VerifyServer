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
 * File:   XMLSupport.cpp
 * Author: Tomas Kratochvila <tomas.kratochvila at honeywell.com>
 * Author: Petr Bauch <petr.bauch at honeywell.com>
 */

#include <algorithm>
#include <iostream>

#include "XMLSupport.h"

namespace XMLSupport {

  void Xml::clear() {
    knownTokens.clear();
    tokens.clear();
    iNames.clear();
    parameters.clear();
    items.clear();
    subs.clear();
    top = 0;
  }
  
/** If the first substring in the string (in) at the position (pos) (or after skipping possible white space)
    is equal to the char (c), return true, else return false.
*/
bool Xml::read_single_char(const String& in, SSize& pos, char c) {
  skip_whites(in, pos);
  if (in[pos] == c) {
    pos++;
    return true;
  }
  else {
    return false;
  }
}

/** If the first substring in the string (in) at the position (pos) (or after skipping possible white space)
    is equal to the string (s), return true, else return false.
*/
bool Xml::read_exact_string(const String& in, SSize& pos, const String& s) {
  skip_whites(in, pos);
  if (String(in, pos, s.size()) == s) {
    pos += s.size();
    return true;
  }
  else
    return false;
}

bool Xml::read_value_as_item(const String& in, SSize& pos, Index& value) {
  skip_whites(in, pos);
  if (in[pos] == '<')
    return false;
  if (in[pos] == '\"')
    return read_value(in, pos, value);

  auto last = in.find("</", pos);
  if (last == String::npos) {
    return false;
  }
  while (!std::isgraph(in[--last])) ;
  last++;
  value = tokenise(String(in, pos, last - pos));
  pos = last;
  return true;
}

bool Xml::read_value(const String& in, SSize& pos, Index& value) {
  skip_whites(in, pos);
  if (in[pos] != '\"')
    return read_string_if(in, pos, is_domain_char, value);

  auto last = find_first_if(in, pos + 1,
                            [] (const String& s, SSize loc)
                            { return s[loc] == '"' && s[loc - 1] != '\\'; }
    );
  if (last == in.size())
    return false;
  last++;
  value = tokenise(String(in, pos, last - pos));
  pos = last;
  return true;
}

/** In the string (in) from the position (pos) read the qualified XML element name
    in the form <domain>:<name>
    Sets the index (iName) to the index of the found/inserted name.
    Returns true if the name exists.
    Returns false otherwise.
*/
bool Xml::read_iname(const String& in, SSize& pos, Index& iName) {
  SSize start = pos;
  Index dom, name;
  if (   read_string_if(  in, pos, is_domain_char, dom )
      && read_single_char(in, pos, ':'                 )
      && read_string_if(  in, pos, is_domain_char, name)
     ) {
    iName = iNames.size();
    iNames.emplace_back(dom, name);
    return true;
  }
  pos = start;
  if (read_string_if(in, pos, is_domain_char, name) ) {
    iName = iNames.size();
    iNames.emplace_back(-1, name);
    return true;
  }
  pos = start;
  return false;
}

/** From the string (in) from the position (pos) read an XML parameter (attribute and its value)
    of the form
    domain:name="value"
    or
    name="value"
    or
    name=Value
    Return true if parameter was successfully read and return also the index of the parameter
    to the vector of parameters.
*/
bool Xml::read_param(const String& in, SSize& pos, Index& param) {
  SSize start = pos;
  Index dom, name, value;
  if (   read_string_if(  in, pos, is_domain_char, dom )
      && read_single_char(in, pos, ':'                 )
      && read_string_if(  in, pos, is_domain_char, name)
      && read_single_char(in, pos, '='                 )
      && read_value(      in, pos,                 value)
    ) {
    param = parameters.size();
    parameters.emplace_back(dom, name, value);
    return true;
  }
  pos = start;
  if (   read_string_if(  in, pos, is_domain_char, name)
      && read_single_char(in, pos, '='                 )
      && read_value(      in, pos,                 value)
    ) {
    param = parameters.size();
    parameters.emplace_back(-1, name, value);
    return true;
  }
  pos = start;
  return false;
}

/** From the string (in) from the position (pos) read a set of consecutive XML parameters.
    Return true, and the indices of the read parameters.
*/
bool Xml::read_params(const String& in, SSize& pos, Indices& params) {
  Index param;
  while (read_param(in, pos, param)) {
    params.push_back(param);
  }
  return true;
}

bool Xml::read_items(const String& in, SSize& pos, Indices& sItems) {
  Index item;
  if (read_item(in, pos, item))
    sItems.push_back(item);
  else
    return false;

  while (read_item(in, pos, item)) {
    sItems.push_back(item);
  }
  return true;
}

bool Xml::read_sub(const String& in, SSize& pos, Index& sub) {
  Indices subItems;
  Index value;
  if (read_items(in, pos, subItems)) {
    sub = subs.size();
    subs.emplace_back(subItems);
    return true;
  }
  if (read_value_as_item(in, pos, value)) {
    sub = subs.size();
    subs.emplace_back(value);
    return true;
  }
  sub = -1; // TODO: added to prevent use of unitialized variable. It it safe?
  return true;
}

bool Xml::read_item(const String& in, SSize& pos, Index& item) {
  Index openName, closeName;
  Indices params;
  Index sub;
  SSize start = pos;
  //std::cout << "String is: " << String(in, pos, 40) << std::endl;
 
  if (   read_single_char( in, pos, '<')
      && read_iname(       in, pos, openName)
      && read_params(      in, pos, params)
    ) {
    SSize mid = pos;
    if (read_exact_string(in, pos, "/>")) {
      item = items.size();
      items.emplace_back(openName, params);
      return true;
    }
    pos = mid;
    if (   read_single_char( in, pos, '>')
        && read_sub(         in, pos, sub)
        && read_exact_string(in, pos, "</")
        && read_iname(       in, pos, closeName)
        && read_single_char( in, pos, '>')
      ) {
      item = items.size();
      items.emplace_back(openName, params, sub);
      return true;
    }
    //std::cout << "Row 171" << std::endl;
  }
  pos = start;
  //std::cout << "String is: " << String(in, pos, 50) << std::endl;
  return false;
}

/** Read the first line "<?...?>" of the XML code (in), starting from the position (pos).
    Return true if such line is found, and Update (pos) to the end of this line.
*/
bool Xml::read_xml_line(const String& in, SSize& pos) {
  if (read_exact_string(in, pos, "<?")) {
    auto last = in.find("?>", pos);
    if (last == String::npos)
      return false;
    pos = last + 2;
  }
  return true;
}

void Xml::find_items(const String& iName, Indices& finds) const {
  for (Nat i = 0; i < items.size(); i++)
    if (item_name(i).find(iName) != String::npos)
      finds.push_back(i);
}

Index Xml::find_item(const String& iName) const {
  Indices temp;
  find_items(iName, temp);
  return (temp.empty() ? -1 : temp[0]);
}

/**
 * 
 * @param params
 * @param pName Substring to search for in the params' names
 * @return index into xml.parameters or -1 if no match
 */
Index Xml::find_param(const Indices& params, const String& pName) const {
  for (Nat i = 0; i < params.size(); i++) {
    if (parameter_name(params[i]).find(pName) != String::npos)
      return params[i];
  }
  return -1;
}

String Xml::find_param_value_string(const Indices& params, const String& pName) const {
  Index param = find_param(params, pName);
  assert_xml(param > -1);
  return bbb::dequote(get_param_value(param));
}

bool Xml::find_param_value_bool(const Indices& params, const String& pName) const {
  Index param = find_param(params, pName);
  assert_xml(param > -1);
  std::string value = bbb::dequote(get_param_value(param));
  if (value == "true")
      return true;
  else if (value == "false")
      return false;
  assert_xml(false); // Invalid value for the bool parameter in XML
}

void Xml::find_params(const Indices& params, const String& pName, Indices& res) const {
  for (Nat i = 0; i < params.size(); i++) {
    if (parameter_name(params[i]).find(pName) != String::npos)
      res.push_back(params[i]);
  }
}

void Xml::get_params(const Index itemId, Indices& res) const {
  //std::transform(items[itemId].params.cbegin(), items[itemId].params.cend(), std::back_inserter(res), [this](Index paramIdx) -> Parameter {return this->parameters[paramIdx]; });
  res = items[itemId].params;
}

void Xml::get_param_item(const String& iName, const String& pName, Strings& res) const {
  Indices itemIds;
  find_items(iName, itemIds);
  for (auto& i : itemIds) {
    Indices paramIds;
    find_params(items[i].params, pName, paramIds);
    for (auto& p : paramIds)
      res.push_back(bbb::dequote(get_param_value(p)));
  }
}

void Xml::get_subitems(const Index itemId, Indices & res) const
{
  Index subId = items[itemId].sub;
  if (subId > -1) {
    res = subs[subId].sItems;
  }
  else {
    // The item does not have subitems
    res.clear();
  }
}

void Xml::get_value_after(Index presub_id, Strings& res) const {
  assert_xml(presub_id > -1);

  Index preitem_id = -1;
  for (Nat i = 0; i < items.size(); i++)
    if (items[i].sub == presub_id)
      preitem_id = i;
  assert_xml(presub_id > -1);

  Index sub_id = -1;;
  for (Nat i = 0; i < subs.size(); i++)
    if (subs[i].sItems.size() > 1 && subs[i].sItems[0] == preitem_id)
      sub_id = i;
  assert_xml(sub_id > -1);

  Index item_id = subs[sub_id].sItems[1];
  assert_xml(item_id > -1);
  assert_xml(items[item_id].sub > -1);
  Index id = subs[items[item_id].sub].value;
  assert_xml(id > -1);
  res.push_back( tokens[id]);
}

void Xml::get_values_after(const String& value_name, Strings& res) const {
  if (knownTokens.count(value_name) == 0)
    return;
  //  assert_xml(knownTokens.count(value_name) > 0);
  const Index& vn_id = knownTokens.at(value_name);
  Indices presub_ids;
  for (Nat i = 0; i < subs.size(); i++)
    if (subs[i].value == vn_id)
      presub_ids.push_back(i);
  assert_xml(!presub_ids.empty());
  for (auto i : presub_ids)
    get_value_after(i, res);
}

/** (?) If the string (s) is successfuly inserted with a unique identifier into the knownTokens map,
    the string (s) is added to the vector of strings (tokens).
    The identifier of the token is returned.
*/
Index Xml::tokenise(String s) {
  auto resp = knownTokens.insert({s, tokens.size()});
  if (resp.second)
    tokens.push_back(s);
  return resp.first->second;
}

}
