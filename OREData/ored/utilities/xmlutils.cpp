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

#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/utilities/xmlutils.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/erase.hpp>

// we only want to include these here.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsuggest-override"
#endif
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsuggest-override"
#endif
#include <rapidxml.hpp>
#include <rapidxml_print.hpp>
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <algorithm>
#include <fstream>

using namespace std;
using namespace rapidxml;
using QuantLib::Size;

namespace ore {
namespace data {

namespace {

// handle rapid xml parser errors

void handle_rapidxml_parse_error(const rapidxml::parse_error& e) {
    string where = e.where<char>();
    boost::erase_all(where, "\n");
    boost::erase_all(where, "\r");
    QL_FAIL("RapidXML Parse Error (" << e.what() << ") at '" << where.substr(0, 400) << "'");
}


} // namespace

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
    t.read(_buffer, length);        // read the whole file into the buffer
    _buffer[static_cast<int>(t.gcount())] = '\0';
    t.close(); // close file handle
    try {
        _doc->parse<0>(_buffer);
    } catch (const rapidxml::parse_error& pe) {
        handle_rapidxml_parse_error(pe);
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
    } catch (const rapidxml::parse_error& pe) {
        handle_rapidxml_parse_error(pe);
    }
}

XMLNode* XMLDocument::getFirstNode(const string& name) const { return _doc->first_node(name == "" ? NULL : name.c_str()); }

void XMLDocument::appendNode(XMLNode* node) { _doc->append_node(node); }

void XMLDocument::toFile(const string& fileName) const {
    std::ofstream ofs(fileName.c_str());
    ofs << *_doc;
    ofs.close();
}

string XMLDocument::toString() const {
    ostringstream oss;
    oss << *_doc;
    return oss.str();
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

void XMLSerializable::toFile(const string& filename) const {
    XMLDocument doc;
    XMLNode* node = toXML(doc);
    doc.appendNode(node);
    doc.toFile(filename);
}

void XMLSerializable::fromXMLString(const string& xml) {
    ore::data::XMLDocument doc;
    doc.fromXMLString(xml);
    fromXML(doc.getFirstNode(""));
}

string XMLSerializable::toXMLString() const {
    XMLDocument doc;
    XMLNode* node = toXML(doc);
    doc.appendNode(node);
    return doc.toString();
}

void XMLUtils::checkNode(XMLNode* node, const string& expectedName) {
    QL_REQUIRE(node, "XML Node is NULL (expected " << expectedName << ")");
    QL_REQUIRE(node->name() == expectedName,
               "XML Node name " << node->name() << " does not match expected name " << expectedName);
}

XMLNode* XMLUtils::addChild(XMLDocument& doc, XMLNode* parent, const string& name) {
    QL_REQUIRE(parent, "XML Parent Node is NULL (adding Child " << name << ")");
    XMLNode* node = doc.allocNode(name);
    parent->insert_node(0, node);
    return node;
}

void XMLUtils::addChild(XMLDocument& doc, XMLNode* n, const string& name, const char* value) {
    addChild(doc, n, name, string(value));
}

void XMLUtils::addChild(XMLDocument& doc, XMLNode* n, const string& name, const string& value) {
    if (value.empty()) {
        addChild(doc, n, name);
    } else {
        XMLNode* node = doc.allocNode(name, value);
        QL_REQUIRE(n, "XML Node is NULL (adding " << name << ")");
        n->insert_node(0, node);
    }
}

void XMLUtils::addChildAsCdata(XMLDocument& doc, XMLNode* n, const string& name, const string& value) {
    if (value.empty()) {
        addChild(doc, n, name);
    } else {
        QL_REQUIRE(n, "XML Node is NULL (adding " << name << ")");
        XMLNode* node = doc.allocNode(name);
        n->insert_node(0, node);
        XMLNode* cdata_node = doc.doc()->allocate_node(node_cdata);
        cdata_node->value(doc.allocString(value));
        QL_REQUIRE(cdata_node, "Failed to allocate cdata node for " << name);
        node->insert_node(0, cdata_node);
    }
}

void XMLUtils::addChild(XMLDocument& doc, XMLNode* n, const string& name, const string& value, const string& attrName,
                        const string& attr) {
    if (!attrName.empty() || !attr.empty()) {
        addChild(doc, n, name, value, std::vector<string>{attrName}, std::vector<string>{attr});
    } else {
        addChild(doc, n, name, value, std::vector<string>{}, std::vector<string>{});
    }
}

void XMLUtils::addChild(XMLDocument& doc, XMLNode* n, const string& name, const string& value,
                        const vector<string>& attrNames, const vector<string>& attrs) {
    QL_REQUIRE(attrNames.size() == attrs.size(), "The size of attrNames should be the same as the size of attrs.");
    XMLNode* node;
    if (value.empty()) {
        node = addChild(doc, n, name);
    } else {
        node = doc.allocNode(name, value);
        QL_REQUIRE(n, "XML Node is NULL (adding " << name << ")");
        n->insert_node(0, node);
    }
    for (Size i = 0; i < attrNames.size(); ++i) {
        XMLUtils::addAttribute(doc, node, attrNames[i], attrs[i]);
    }
}

void XMLUtils::addChild(XMLDocument& doc, XMLNode* n, const string& name, Real value) {
    addChild(doc, n, name, convertToString(value));
}

void XMLUtils::addChild(XMLDocument& doc, XMLNode* n, const string& name, int value) {
    addChild(doc, n, name, std::to_string(value));
}

void XMLUtils::addChild(XMLDocument& doc, XMLNode* n, const string& name, bool value) {
    string s = value ? "true" : "false";
    addChild(doc, n, name, s);
}

void XMLUtils::addChild(XMLDocument& doc, XMLNode* n, const string& name, const Period& value) {
    addChild(doc, n, name, ore::data::to_string(value));
}

void XMLUtils::addChild(XMLDocument& doc, XMLNode* parent, const string& name, const vector<Real>& values) {
    vector<string> strings(values.size());
    std::transform(values.begin(), values.end(), strings.begin(), [](Real x) { return convertToString(x); });
    addChild(doc, parent, name, boost::algorithm::join(strings, ","));
}

void XMLUtils::addChildren(XMLDocument& doc, XMLNode* parent, const string& names, const string& name,
                           const string& firstName, const string& secondName, const map<string, string>& values) {
    QL_REQUIRE(parent, "XML Node is null (Adding " << names << ")");
    XMLNode* node = addChild(doc, parent, names);
    map<string, string>::const_iterator it;
    for (it = values.begin(); it != values.end(); ++it) {
        XMLNode* n = addChild(doc, node, name);
        QL_REQUIRE(n, "XML AllocNode failure (" << name << ")");
        addChild(doc, n, firstName, it->first);
        addChild(doc, n, secondName, it->second);
    }
}

string XMLUtils::getChildValue(XMLNode* node, const string& name, bool mandatory, const string& defaultValue) {
    QL_REQUIRE(node, "XMLNode is NULL (was looking for child " << name << ")");
    xml_node<>* child = node->first_node(name.c_str());
    if (mandatory) {
        QL_REQUIRE(child, "Error: No XML Child Node " << name << " found.");
    }
    return child ? getNodeValue(child) : defaultValue;
}

Real XMLUtils::getChildValueAsDouble(XMLNode* node, const string& name, bool mandatory, double defaultValue) {
    string s = getChildValue(node, name, mandatory);
    return s == "" ? defaultValue : parseReal(s);
}

int XMLUtils::getChildValueAsInt(XMLNode* node, const string& name, bool mandatory, int defaultValue) {
    string s = getChildValue(node, name, mandatory);
    return s == "" ? defaultValue : parseInteger(s);
}

bool XMLUtils::getChildValueAsBool(XMLNode* node, const string& name, bool mandatory, bool defaultValue) {
    string s = getChildValue(node, name, mandatory);
    return s == "" ? defaultValue : parseBool(s);
}

Period XMLUtils::getChildValueAsPeriod(XMLNode* node, const string& name, bool mandatory, const Period& defaultValue) {
    string s = getChildValue(node, name, mandatory);
    return s == "" ? defaultValue : parsePeriod(s);
}

vector<string> XMLUtils::getChildrenValues(XMLNode* parent, const string& names, const string& name, bool mandatory) {
    vector<string> vec;
    xml_node<>* node = parent->first_node(names.c_str());
    if (mandatory) {
        QL_REQUIRE(node, "Error: No XML Node " << names << " found.");
    }
    if (node) {
        for (xml_node<>* child = node->first_node(name.c_str()); child; child = child->next_sibling(name.c_str()))
            vec.push_back(getNodeValue(child));
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

vector<Real> XMLUtils::getNodeValueAsDoublesCompact(XMLNode* node) {
    string s = getNodeValue(node);
    return parseListOfValues(s, std::function<Real(string)>(&parseReal));
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
        string second = getNodeValue(child);
        if (first.empty())
            continue;
        auto it = res.find(first);
        if (it != res.end())
            WLOG("XMLUtils::getChildrenAttributesAndValues: Duplicate entry " << first <<
                " in node " << names << ". Overwritting with value " << second << ".");
        res.insert(pair<string, string>(first, second));
    }
    if (mandatory) {
        QL_REQUIRE(!res.empty(), "Error: No XML Node " << names << " found.");
    }
    return res;
}

// returns first child node
XMLNode* XMLUtils::getChildNode(XMLNode* n, const string& name) {
    QL_REQUIRE(n, "XMLUtils::getChildNode(" << name << "): XML Node is NULL");
    return n->first_node(name == "" ? nullptr : name.c_str());
}

// return first node in the hierarchy of n that matches name, maybe n itself
XMLNode* XMLUtils::locateNode(XMLNode* n, const string& name) {
    QL_REQUIRE(n, "XMLUtils::locateNode(" << name << "): XML Node is NULL");
    if (n->name() == name)
        return n;
    else {
        const char* p = name.empty() ? nullptr : name.c_str();
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
    QL_REQUIRE(node, "XMLUtils::appendAttribute(" << attrName << "," << attrName << ") node is NULL");
    char* name = doc.allocString(attrName.c_str());
    char* value = doc.allocString(attrValue.c_str());
    node->append_attribute(doc.doc()->allocate_attribute(name, value));
}

string XMLUtils::getAttribute(XMLNode* node, const string& attrName) {
    QL_REQUIRE(node, "XMLUtils::getAttribute(" << attrName << ") node is NULL");
    xml_attribute<>* attr = node->first_attribute(attrName.c_str());
    if (attr && attr->value())
        return string(attr->value());
    else
        return "";
}

vector<XMLNode*> XMLUtils::getChildrenNodes(XMLNode* node, const string& name) {
    QL_REQUIRE(node, "XMLUtils::getChildrenNodes(" << name << ") node is NULL");
    vector<XMLNode*> res;
    const char* p = name.empty() ? nullptr : name.c_str();
    for (xml_node<>* c = node->first_node(p); c; c = c->next_sibling(p))
        res.push_back(c);
    return res;
}

vector<XMLNode*> XMLUtils::getChildrenNodesWithAttributes(XMLNode* parent, const string& names, const string& name,
                                                          const string& attrName, vector<string>& attrs,
                                                          bool mandatory) {
    std::vector<std::reference_wrapper<vector<string>>> attrs_v;
    attrs_v.push_back(attrs);
    return getChildrenNodesWithAttributes(parent, names, name, {attrName}, attrs_v, mandatory);
}

vector<XMLNode*> XMLUtils::getChildrenNodesWithAttributes(XMLNode* parent, const string& names, const string& name,
                                                          const vector<string>& attrNames,
                                                          const vector<std::reference_wrapper<vector<string>>>& attrs,
                                                          bool mandatory) {
    QL_REQUIRE(attrNames.size() == attrs.size(),
               "attrNames size (" << attrNames.size() << ") must match attrs size (" << attrs.size() << ")");
    vector<XMLNode*> vec;
    // if names is empty, use the parent as the anchor node
    rapidxml::xml_node<>* node = names.empty() ? parent : parent->first_node(names.c_str());
    if (mandatory) {
        QL_REQUIRE(node, "Error: No XML Node " << names << " found.");
    }
    if (node) {
        for (rapidxml::xml_node<>* child = node->first_node(name.c_str()); child;
             child = child->next_sibling(name.c_str())) {
            vec.push_back(child);
            for (Size i = 0; i < attrNames.size(); ++i) {
                xml_attribute<>* attr = child->first_attribute(attrNames[i].c_str());
                if (attr && attr->value())
                    ((vector<string>&)attrs[i]).push_back(attr->value());
                else
                    ((vector<string>&)attrs[i]).push_back("");
            }
        }
    }
    return vec;
}

string XMLUtils::getNodeName(XMLNode* node) {
    QL_REQUIRE(node, "XMLUtils::getNodeName(): XML Node is NULL");
    return node->name();
}

void XMLUtils::setNodeName(XMLDocument& doc, XMLNode* node, const string& name) {
    QL_REQUIRE(node, "XMLUtils::setNodeName(" << name << "): XML Node is NULL");
    char* nodeName = doc.allocString(name);
    node->name(nodeName);
}

XMLNode* XMLUtils::getNextSibling(XMLNode* node, const string& name) {
    QL_REQUIRE(node, "XMLUtils::getNextSibling(" << name << "): XML Node is NULL");
    return node->next_sibling(name == "" ? nullptr : name.c_str());
}

string XMLUtils::getNodeValue(XMLNode* node) {
    QL_REQUIRE(node, "XMLUtils::getNodeValue(): XML Node is NULL");
    // handle CDATA nodes
    XMLNode* n = node->first_node();
    if (n && n->type() == node_cdata)
        return n->value();
    // all other cases
    return node->value();
}

// template implementations

template <class T>
void XMLUtils::addChildren(XMLDocument& doc, XMLNode* parent, const string& names, const string& name,
                           const vector<T>& values) {
    XMLNode* node = addChild(doc, parent, names);
    for (Size i = 0; i < values.size(); i++)
        addChild(doc, node, name, values[i]);
}

template <class T>
void XMLUtils::addChildrenWithAttributes(XMLDocument& doc, XMLNode* parent, const string& names, const string& name,
                                         const vector<T>& values, const string& attrName, const vector<string>& attrs) {
    addChildrenWithAttributes(doc, parent, names, name, values, vector<string>{attrName},
                              vector<vector<string>>{attrs});
}

template <class T>
void XMLUtils::addChildrenWithAttributes(XMLDocument& doc, XMLNode* parent, const string& names, const string& name,
                                         const vector<T>& values, const vector<string>& attrNames,
                                         const vector<vector<string>>& attrs) {
    QL_REQUIRE(attrNames.size() == attrs.size(),
               "attrNames size (" << attrNames.size() << ") must match attrs size (" << attrs.size() << ")");
    if (values.size() > 0) {
        for (auto const& attr : attrs) {
            QL_REQUIRE(values.size() == attr.size(), "Values / Attribute vector size mismatch");
        }
        QL_REQUIRE(parent, "XML Node is null (Adding " << names << ")");
        XMLNode* node = addChild(doc, parent, names);
        for (Size i = 0; i < values.size(); i++) {
            XMLNode* c = doc.allocNode(name, convertToString(values[i]));
            QL_REQUIRE(c, "XML AllocNode failure (" << name << ")");
            QL_REQUIRE(node, "XML Node is NULL (" << name << ")");
            node->insert_node(0, c);
            for (Size j = 0; j < attrs.size(); ++j) {
                if (attrs[j][i] != "")
                    addAttribute(doc, c, attrNames[j], attrs[j][i]);
            }
        }
    }
}

template <class T>
void XMLUtils::addChildrenWithOptionalAttributes(XMLDocument& doc, XMLNode* n, const string& names, const string& name,
                                                 const vector<T>& values, const string& attrName,
                                                 const vector<string>& attrs) {
    addChildrenWithOptionalAttributes(doc, n, names, name, values, vector<string>{attrName},
                                      vector<vector<string>>{attrs});
}

template <class T>
void XMLUtils::addChildrenWithOptionalAttributes(XMLDocument& doc, XMLNode* n, const string& names, const string& name,
                                                 const vector<T>& values, const vector<string>& attrNames,
                                                 const vector<vector<string>>& attrs) {
    QL_REQUIRE(attrNames.size() == attrs.size(),
               "attrNames size (" << attrNames.size() << ") must match attrs size (" << attrs.size() << ")");
    for (auto const& attr : attrs)
        QL_REQUIRE(attr.empty() == attrs.front().empty(), "all attributes must be empty or non-empty at the same time");
    if (attrs.empty() || attrs.front().empty())
        XMLUtils::addChildren(doc, n, names, name, values);
    else
        XMLUtils::addChildrenWithAttributes(doc, n, names, name, values, attrNames, attrs);
}

vector<string> XMLUtils::getChildrenValuesWithAttributes(XMLNode* parent, const string& names, const string& name,
                                                         const string& attrName, vector<string>& attrs,
                                                         bool mandatory) {
    return getChildrenValuesWithAttributes<string>(
        parent, names, name, attrName, attrs, [](const string& x) { return x; }, mandatory);
}

template <class T>
vector<T> XMLUtils::getChildrenValuesWithAttributes(XMLNode* parent, const string& names, const string& name,
                                                    const string& attrName, vector<string>& attrs,
                                                    const std::function<T(string)> parser, bool mandatory) {
    std::vector<std::reference_wrapper<vector<string>>> attrs_v;
    attrs_v.push_back(attrs);
    return getChildrenValuesWithAttributes(parent, names, name, vector<string>{attrName}, attrs_v, parser, mandatory);
}

vector<string> XMLUtils::getChildrenValuesWithAttributes(XMLNode* parent, const string& names, const string& name,
                                                         const vector<string>& attrNames,
                                                         const vector<std::reference_wrapper<vector<string>>>& attrs,
                                                         bool mandatory) {
    return getChildrenValuesWithAttributes<string>(
        parent, names, name, attrNames, attrs, [](const string& x) { return x; }, mandatory);
}

template <class T>
vector<T> XMLUtils::getChildrenValuesWithAttributes(XMLNode* parent, const string& names, const string& name,
                                                    const vector<string>& attrNames,
                                                    const vector<std::reference_wrapper<vector<string>>>& attrs,
                                                    const std::function<T(string)> parser, bool mandatory) {
    QL_REQUIRE(parser, "XMLUtils::getChildrenValuesWithAttributes(): parser is null");
    QL_REQUIRE(attrNames.size() == attrs.size(),
               "attrNames size (" << attrNames.size() << ") must match attrs size (" << attrs.size() << ")");
    vector<T> vec;
    // if 'names' is not given, use the parent node directly
    rapidxml::xml_node<>* node = names.empty() ? parent : parent->first_node(names.c_str());
    if (mandatory) {
        QL_REQUIRE(node, "Error: No XML Node " << names << " found.");
    }
    if (node) {
        for (rapidxml::xml_node<>* child = node->first_node(name.c_str()); child;
             child = child->next_sibling(name.c_str())) {

            std::string vstr = getNodeValue(child);
            vec.push_back(parser(vstr));

            for (Size i = 0; i < attrNames.size(); ++i) {
                xml_attribute<>* attr = child->first_attribute(attrNames[i].c_str());
                if (attr && attr->value())
                    ((vector<string>&)attrs[i]).push_back(attr->value());
                else
                    ((vector<string>&)attrs[i]).push_back("");
            }
        }
    }
    return vec;
}

string XMLUtils::convertToString(const Real value) {
    // We want to write out a double that conforms to xs:double, this means no
    // scientific notation, so we check for really small numbers here and explicitly set
    // to 16 decimal places
    string result;
    if (std::abs(value) < 1.0e-6) {
        std::ostringstream obj1;
        obj1.precision(16);
        obj1 << std::fixed << value;
        result = obj1.str();
    } else {
        // And here we just use boost::lexical_cast which is better
        // at precision than std::to_string()
        result = boost::lexical_cast<std::string>(value);
    }
    return result;
}

template <class T> string XMLUtils::convertToString(const T& value) { return boost::lexical_cast<std::string>(value); }

/* Instantiate some template functions above for types T we want to support. Add instantiations for more types here,
   if required. This has two advantages over putting the templated version of the functions in the header file:
   - faster compile times
   - we do not need to include the rapid xml headers in xmlutils.hpp */

template void XMLUtils::addChildren(XMLDocument& doc, XMLNode* parent, const string& names, const string& name,
                                    const vector<string>& values);
// throws a warning currently, but that is ok, see the FIXME above in template <> void XMLUtils::addChildren(...)
// template void XMLUtils::addChildren(XMLDocument& doc, XMLNode* parent, const string& names, const string& name,
//                                     const vector<double>& values);
template void XMLUtils::addChildren(XMLDocument& doc, XMLNode* parent, const string& names, const string& name,
                                     const vector<Real>& values);
template void XMLUtils::addChildren(XMLDocument& doc, XMLNode* parent, const string& names, const string& name,
                                    const vector<bool>& values);

template void XMLUtils::addChildrenWithAttributes(XMLDocument& doc, XMLNode* parent, const string& names,
                                                  const string& name, const vector<string>& values,
                                                  const string& attrName, const vector<string>& attrs);
template void XMLUtils::addChildrenWithAttributes(XMLDocument& doc, XMLNode* parent, const string& names,
                                                  const string& name, const vector<double>& values,
                                                  const string& attrName, const vector<string>& attrs);
template void XMLUtils::addChildrenWithAttributes(XMLDocument& doc, XMLNode* parent, const string& names,
                                                  const string& name, const vector<bool>& values,
                                                  const string& attrName, const vector<string>& attrs);

template void XMLUtils::addChildrenWithAttributes(XMLDocument& doc, XMLNode* parent, const string& names,
                                                  const string& name, const vector<string>& values,
                                                  const vector<string>& attrNames, const vector<vector<string>>& attrs);
template void XMLUtils::addChildrenWithAttributes(XMLDocument& doc, XMLNode* parent, const string& names,
                                                  const string& name, const vector<double>& values,
                                                  const vector<string>& attrNames, const vector<vector<string>>& attrs);
template void XMLUtils::addChildrenWithAttributes(XMLDocument& doc, XMLNode* parent, const string& names,
                                                  const string& name, const vector<bool>& values,
                                                  const vector<string>& attrNames, const vector<vector<string>>& attrs);

template void XMLUtils::addChildrenWithOptionalAttributes(XMLDocument& doc, XMLNode* n, const string& names,
                                                          const string& name, const vector<string>& values,
                                                          const string& attrName, const vector<string>& attrs);
template void XMLUtils::addChildrenWithOptionalAttributes(XMLDocument& doc, XMLNode* n, const string& names,
                                                          const string& name, const vector<double>& values,
                                                          const string& attrName, const vector<string>& attrs);
template void XMLUtils::addChildrenWithOptionalAttributes(XMLDocument& doc, XMLNode* n, const string& names,
                                                          const string& name, const vector<bool>& values,
                                                          const string& attrName, const vector<string>& attrs);

template void XMLUtils::addChildrenWithOptionalAttributes(XMLDocument& doc, XMLNode* n, const string& names,
                                                          const string& name, const vector<string>& values,
                                                          const vector<string>& attrNames,
                                                          const vector<vector<string>>& attrs);
template void XMLUtils::addChildrenWithOptionalAttributes(XMLDocument& doc, XMLNode* n, const string& names,
                                                          const string& name, const vector<double>& values,
                                                          const vector<string>& attrNames,
                                                          const vector<vector<string>>& attrs);
template void XMLUtils::addChildrenWithOptionalAttributes(XMLDocument& doc, XMLNode* n, const string& names,
                                                          const string& name, const vector<bool>& values,
                                                          const vector<string>& attrNames,
                                                          const vector<vector<string>>& attrs);

template vector<std::string> XMLUtils::getChildrenValuesWithAttributes(XMLNode* parent, const string& names,
                                                                       const string& name, const string& attrName,
                                                                       vector<string>& attrs,
                                                                       std::function<string(string)> parser,
                                                                       bool mandatory);

template vector<double> XMLUtils::getChildrenValuesWithAttributes(XMLNode* parent, const string& names,
                                                                  const string& name, const string& attrName,
                                                                  vector<string>& attrs,
                                                                  std::function<double(string)> parser, bool mandatory);
template vector<bool> XMLUtils::getChildrenValuesWithAttributes(XMLNode* parent, const string& names,
                                                                const string& name, const string& attrName,
                                                                vector<string>& attrs,
                                                                std::function<bool(string)> parser, bool mandatory);


string XMLUtils::toString(XMLNode* node) {
    string xml_as_string;
    rapidxml::print(std::back_inserter(xml_as_string), *node);
    return xml_as_string;
}

} // namespace data
} // namespace ore
