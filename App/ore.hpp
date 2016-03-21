/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2011 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! \file App/ore.hpp
    \brief Open Risk Engine setup and analytics choice
*/

#pragma once

#include <vector>
#include <map>

#include <qlw/utilities/xmlutils.hpp>

using namespace std;
using namespace openxva::utilities;

class Parameters : public XMLSerializable {
public:
    Parameters() {}
    
    void clear();
    void fromFile(const string&);
    virtual void fromXML(XMLNode *node);
    virtual XMLNode* toXML(XMLDocument& doc);
    
    bool hasGroup(const string& groupName) const;
    bool has(const string& groupName, const string& paramName) const;
    string get(const string& groupName, const string& paramName) const; 
    
    void log();
    
private:
    map<string, map<string,string>> data_;
};
