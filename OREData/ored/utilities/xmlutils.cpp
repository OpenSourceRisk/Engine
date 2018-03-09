/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of ORE, a free-software/open-source library
 for transparent pricing and risk analysis - http://opensourcerisk.org

 ORE is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program.
 The license is also available online at <http://opensourcerisk.org>

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

/*! \file ored/utilities/xmlutils.cpp
    \brief
    \ingroup utilities
*/

#include <boost/algorithm/string/join.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/xmlutils.hpp>

// we only want to include these here.
#include <rapidxml.hpp>
#include <rapidxml_print.hpp>

#include <algorithm>
#include <fstream>

using namespace std;
using namespace rapidxml;
using QuantLib::Size;

namespace ore {
namespace data {

XMLDocument::XMLDocument() : _doc(new rapidxml::xml_document<char>()), _buffer(NULL) {}

XMLDocument::XMLDocument(const string& fileName) : _doc(new rapidxml::xml_document<char>()), _buffer(NULL) {
    // Need to load the entire file into memory to pass to doc.parse().
    ifstream t(fileName.c_str());
    QL_REQUIRE(t.is_open(), "Failed to open file " << fileName);
    t.seekg(0, std::ios::end);                  // go to the end
    Size length = static_cast<Size>(t.tellg()); // report location (this is the length)
    QL_REQUIRE(length > 0, "File " << fileName << " is empty.");
    t.seekg(0, std::ios::beg);      // go back to the beginning
    _buffer = new char[length + 1]; // allocate memory for a buffer of appropriate dimension
    memset(_buffer, 0, length + 1); // Wipe the buffer (caused problems on windows release build)
    t.read(_buffer, length);        // read the whole file into the buffer
    t.close();                      // close file handle
    _buffer[length] = '\0';
    try {
        _doc->parse<0>(_buffer);
    } catch (rapidxml::parse_error& pe) {
        string where(pe.where<char>(), 30); // limit to first 30 chars.
        QL_FAIL("RapidXML Parse Error : " << pe.what() << ". where=" << where);
    }
}

XMLDocument::~XMLDocument() {
    if (_buffer != NULL)
        delete[] _buffer;
    if (_doc != NULL)
        delete _doc;
}

void XMLDocument::fromXMLString(const string& xmlString) {
    QL_REQUIRE(!_buffer, "XML Document is already loaded");
    Size length = xmlString.size();
    _buffer = new char[length + 1];
    strcpy(_buffer, xmlString.c_str());
    _buffer[length] = '\0';
    try {
        _doc->parse<0>(_buffer);
    } catch (rapidxml::parse_error& pe) {
        string where(pe.where<char>(), 30); // limit to first 30 chars.
        QL_FAIL("RapidXML Parse Error : " << pe.what() << ". where=" << where);
    }
}

XMLNode* XMLDocument::getFirstNode(const string& name) { return _doc->first_node(name == "" ? NULL : name.c_str()); }

void XMLDocument::appendNode(XMLNode* node) { _doc->append_node(node); }

void XMLDocument::toFile(const string& fileName) {
    std::ofstream ofs(fileName.c_str());
    ofs << *_doc;
    ofs.close();
}

XMLNode* XMLDocument::allocNode(const string& nodeName) {
    XMLNode* n = _doc->allocate_node(node_element, allocString(nodeName));
    QL_REQUIRE(n, "Failed to allocate XMLNode for " << nodeName);
    return n;
}

XMLNode* XMLDocument::allocNode(const string& nodeName, const string& nodeValue) {
    XMLNode* n = _doc->allocate_node(node_element, allocString(nodeName), allocString(nodeValue));
    QL_REQUIRE(n, "Failed to allocate XMLNode for " << nodeName);
    return n;
}

char* XMLDocument::allocString(const string& str) {
    char* s = _doc->allocate_string(str.c_str());
    QL_REQUIRE(s, "Failed to allocate string for " << str);
    return s;
}

void XMLSerializable::fromFile(const string& filename) {
    XMLDocument doc(filename);
    fromXML(doc.getFirstNode(""));
}

void XMLSerializable::toFile(const string& filename) {
    XMLDocument doc;
    toXML(doc);
    doc.toFile(filename);
}

void XMLUtils::checkNode(XMLNode* node, const string& expectedName) {
    QL_REQUIRE(node, "XML Node is NULL (expected " << expectedName << ")");
    QL_REQUIRE(node->name() == expectedName,
               "XML Node name " << node->name() << " does not match expected name " << expectedName);
}

XMLNode* XMLUtils::addChild(XMLDocument& doc, XMLNode* parent, const string& name) {
    XMLNode* node = doc.allocNode(name);
    parent->insert_node(0, node);
    return node;
}

void XMLUtils::addChild(XMLDocument& doc, XMLNode* n, const string& name, const char* value) {
    addChild(doc, n, name, string(value));
}

void XMLUtils::addChild(XMLDocument& doc, XMLNode* n, const string& name, const string& value) {
    if (value.size() == 0) {
        addChild(doc, n, name);
    } else {
        XMLNode* node = doc.allocNode(name, value);
        n->insert_node(0, node);
    }
}

void XMLUtils::addChild(XMLDocument& doc, XMLNode* n, const string& name, Real value) {
    addChild(doc, n, name, std::to_string(value));
}

void XMLUtils::addChild(XMLDocument& doc, XMLNode* n, const string& name, int value) {
    addChild(doc, n, name, std::to_string(value));
}

void XMLUtils::addChild(XMLDocument& doc, XMLNode* n, const string& name, bool value) {
    string s = value ? "true" : "false";
    addChild(doc, n, name, s);
}

void XMLUtils::addChildren(XMLDocument& doc, XMLNode* parent, const string& names, const string& name,
                           const vector<string>& values) {
    XMLNode* node = addChild(doc, parent, names);
    for (Size i = 0; i < values.size(); i++)
        addChild(doc, node, name, values[i]);
}

void XMLUtils::addChildren(XMLDocument& doc, XMLNode* parent, const string& names, const string& name,
                           const vector<Real>& values) {
    vector<string> strings(values.size());
    std::transform(values.begin(), values.end(), strings.begin(), [](Real x) { return std::to_string(x); });
    addChildren(doc, parent, names, name, strings);
}

void XMLUtils::addChild(XMLDocument& doc, XMLNode* parent, const string& name, const vector<Real>& values) {
    vector<string> strings(values.size());
    std::transform(values.begin(), values.end(), strings.begin(), [](Real x) { return std::to_string(x); });
    addChild(doc, parent, name, boost::algorithm::join(strings, ","));
}

void XMLUtils::addChildrenWithAttributes(XMLDocument& doc, XMLNode* parent, const string& names, const string& name,
                                         const vector<Real>& values, const string& attrName,
                                         const vector<string>& attrs) {
    if (values.size() > 0) {
        QL_REQUIRE(values.size() == attrs.size(), "Values / Attribute vector size mismatch");
        XMLNode* node = addChild(doc, parent, names);
        for (Size i = 0; i < values.size(); i++) {
            XMLNode* c = doc.allocNode(name, std::to_string(values[i]));
            node->insert_node(0, c);
            if (attrs[i] != "")
                addAttribute(doc, c, attrName, attrs[i]);
        }
    }
}

void XMLUtils::addChildren(XMLDocument& doc, XMLNode* parent, const string& names, const string& name,
                           const string& firstName, const string& secondName, const map<string, string>& values) {
    XMLNode* node = addChild(doc, parent, names);
    map<string, string>::const_iterator it;
    for (it = values.begin(); it != values.end(); ++it) {
        XMLNode* n = addChild(doc, node, name);
        addChild(doc, n, firstName, it->first);
        addChild(doc, n, secondName, it->second);
    }
}

string XMLUtils::getChildValue(XMLNode* node, const string& name, bool mandatory) {
    QL_REQUIRE(node, "XMLNode is NULL (was looking for child " << name << ")");
    xml_node<>* child = node->first_node(name.c_str());
    if (mandatory) {
        QL_REQUIRE(child, "Error: No XML Child Node " << name << " found.");
    }
    return child ? child->value() : "";
}

Real XMLUtils::getChildValueAsDouble(XMLNode* node, const string& name, bool mandatory) {
    string s = getChildValue(node, name, mandatory);
    return s == "" ? 0.0 : parseReal(s);
}

int XMLUtils::getChildValueAsInt(XMLNode* node, const string& name, bool mandatory) {
    string s = getChildValue(node, name, mandatory);
    return s == "" ? 0 : parseInteger(s);
}

bool XMLUtils::getChildValueAsBool(XMLNode* node, const string& name, bool mandatory) {
    string s = getChildValue(node, name, mandatory);
    return s == "" ? true : parseBool(s);
}

vector<string> XMLUtils::getChildrenValues(XMLNode* parent, const string& names, const string& name, bool mandatory) {
    vector<string> vec;
    xml_node<>* node = parent->first_node(names.c_str());
    if (mandatory) {
        QL_REQUIRE(node, "Error: No XML Node " << names << " found.");
    }
    if (node) {
        for (xml_node<>* child = node->first_node(name.c_str()); child; child = child->next_sibling(name.c_str()))
            vec.push_back(child->value());
    }
    return vec;
}

vector<string> XMLUtils::getChildrenValues(XMLNode* parent, const string& name, bool mandatory) {
    vector<string> vec;
    if (mandatory) {
        QL_REQUIRE(parent, "Error: No XML Node " << name << " found.");
    }
    if (parent) {
        for (xml_node<>* child = parent->first_node(name.c_str()); child; child = child->next_sibling(name.c_str()))
            vec.push_back(child->value());
    }
    return vec;
}

vector<Real> XMLUtils::getChildrenValuesAsDoubles(XMLNode* node, const string& names, const string& name,
                                                  bool mandatory) {
    vector<string> vecS = getChildrenValues(node, names, name, mandatory);
    vector<Real> vecD(vecS.size());
    std::transform(vecS.begin(), vecS.end(), vecD.begin(), parseReal);
    return vecD;
}

vector<Real> XMLUtils::getChildrenValuesAsDoublesCompact(XMLNode* node, const string& name, bool mandatory) {
    string s = getChildValue(node, name, mandatory);
    return parseListOfValues(s, std::function<Real(string)>(&parseReal));
}

vector<Real> XMLUtils::getChildrenValuesAsDoublesWithAttributes(XMLNode* parent, const string& names,
                                                                const string& name, const string& attrName,
                                                                vector<string>& attrs, bool mandatory) {
    vector<Real> vec;
    xml_node<>* node = parent->first_node(names.c_str());
    if (mandatory) {
        QL_REQUIRE(node, "Error: No XML Node " << names << " found.");
    }
    if (node) {
        for (xml_node<>* child = node->first_node(name.c_str()); child; child = child->next_sibling(name.c_str())) {

            vec.push_back(parseReal(child->value()));

            xml_attribute<>* attr = child->first_attribute(attrName.c_str());
            if (attr && attr->value())
                attrs.push_back(attr->value());
            else
                attrs.push_back("");
        }
    }
    return vec;
}

vector<Period> XMLUtils::getChildrenValuesAsPeriods(XMLNode* node, const string& name, bool mandatory) {
    string s = getChildValue(node, name, mandatory);
    return parseListOfValues(s, std::function<Period(string)>(&parsePeriod));
}

vector<string> XMLUtils::getChildrenValuesAsStrings(XMLNode* node, const string& name, bool mandatory) {
    string s = getChildValue(node, name, mandatory);
    return parseListOfValues(s);
}

map<string, string> XMLUtils::getChildrenValues(XMLNode* parent, const string& names, const string& name,
                                                const string& firstName, const string& secondName, bool mandatory) {
    map<string, string> res;
    xml_node<>* node = parent->first_node(names.c_str());
    if (mandatory) {
        QL_REQUIRE(node, "Error: No XML Node " << names << " found.");
    }
    if (node) {
        for (xml_node<>* child = node->first_node(name.c_str()); child; child = child->next_sibling(name.c_str())) {
            string first = getChildValue(child, firstName, mandatory);
            string second = getChildValue(child, secondName, mandatory);
            // res[first] = second;
            res.insert(pair<string, string>(first, second));
        }
    }
    return res;
}

map<string, string> XMLUtils::getChildrenAttributesAndValues(XMLNode* parent, const string& names,
                                                             const string& attributeName, bool mandatory) {
    map<string, string> res;
    for (XMLNode* child = getChildNode(parent, names.c_str()); child;
         child = XMLUtils::getNextSibling(child, names.c_str())) {
        string first = getAttribute(child, attributeName);
        string second = child->value();
        if (mandatory) {
            QL_REQUIRE(first != "", "empty attribute for " << names);
        }
        res.insert(pair<string, string>(first, second));
    }
    if (mandatory) {
        QL_REQUIRE(res.size() > 0, "Error: No XML Node " << names << " found.");
    }
    return res;
}

// returns first child node
XMLNode* XMLUtils::getChildNode(XMLNode* n, const string& name) {
    return n->first_node(name == "" ? nullptr : name.c_str());
}

// return first node in the hierarchy of n that matches name, maybe n itself
XMLNode* XMLUtils::locateNode(XMLNode* n, const string& name) {
    QL_REQUIRE(n, "XML Node is NULL");
    if (n->name() == name)
        return n;
    else {
        const char* p = name.size() == 0 ? nullptr : name.c_str();
        XMLNode* node = n->first_node(p);
        QL_REQUIRE(node, "XML node with name " << name << " not found");
        return node;
    }
}

// append child to parent
void XMLUtils::appendNode(XMLNode* parent, XMLNode* child) {
    QL_REQUIRE(parent, "XMLUtils::appendNode() parent is NULL");
    QL_REQUIRE(child, "XMLUtils::appendNode() child is NULL");
    parent->append_node(child);
}

void XMLUtils::addAttribute(XMLDocument& doc, XMLNode* node, const string& attrName, const string& attrValue) {
    QL_REQUIRE(node, "XMLUtils::appendAttribute() node is NULL");

    char* name = doc.allocString(attrName.c_str());
    char* value = doc.allocString(attrValue.c_str());
    node->append_attribute(doc.doc()->allocate_attribute(name, value));
}

string XMLUtils::getAttribute(XMLNode* node, const string& attrName) {
    QL_REQUIRE(node, "XMLUtils::getAttribute() node is NULL");
    xml_attribute<>* attr = node->first_attribute(attrName.c_str());
    if (attr && attr->value())
        return string(attr->value());
    else
        return "";
}

vector<XMLNode*> XMLUtils::getChildrenNodes(XMLNode* node, const string& name) {
    QL_REQUIRE(node, "XMLUtils::getAttribute() node is NULL");
    vector<XMLNode*> res;
    const char* p = name.size() == 0 ? nullptr : name.c_str();
    for (xml_node<>* c = node->first_node(p); c; c = c->next_sibling(p))
        res.push_back(c);
    return res;
}

string XMLUtils::getNodeName(XMLNode* node) { return node->name(); }

void XMLUtils::setNodeName(XMLDocument& doc, XMLNode* node, const string& name) {
    char* nodeName = doc.allocString(name);
    node->name(nodeName);
}

XMLNode* XMLUtils::getNextSibling(XMLNode* node, const string& name) {
    return node->next_sibling(name == "" ? nullptr : name.c_str());
}

string XMLUtils::getNodeValue(XMLNode* node) { return node->value(); }
} // namespace data
} // namespace ore
