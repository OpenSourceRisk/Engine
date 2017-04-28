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

/*! \file ored/utilities/xmlutils.hpp
    \brief XML utility functions
    \ingroup utilities
*/

#pragma once

#include <vector>
#include <map>
#include <string>
#include <sstream> // std::ostringstream
#include <ql/types.hpp>
#include <ql/time/period.hpp>

using std::string;
using std::vector;
using std::map;
using std::pair;
using QuantLib::Real;
using QuantLib::Size;
using QuantLib::Period;

// Forward declarations and typedefs
// so we don't need to #include rapidxml everywhere.
namespace rapidxml {
//! XML Node
/*! \ingroup utilities
*/
template <class Ch> class xml_node;
//! XML Document
/*! \ingroup utilities
*/
template <class Ch> class xml_document;
}

namespace ore {
namespace data {

typedef rapidxml::xml_node<char> XMLNode;

//! Small XML Document wrapper class.
/*! \ingroup utilities
*/
class XMLDocument {
public:
    //! create an empty doc.
    XMLDocument();
    //! load an xml doc from the given file
    XMLDocument(const string& filename);
    //! destructor
    ~XMLDocument();

    //! load a document from a hard-coded string
    void fromXMLString(const string& xmlString);

    //! save the XML Document to the given file.
    void toFile(const string& filename);

    XMLNode* getFirstNode(const string& name);
    void appendNode(XMLNode*);

    // TODO: take these inside cpp, not exposed to clients
    //! util functions that wrap rapidxml
    XMLNode* allocNode(const string& nodeName);
    XMLNode* allocNode(const string& nodeName, const string& nodeValue);
    char* allocString(const string& str);
    rapidxml::xml_document<char>* doc() { return _doc; }

private:
    rapidxml::xml_document<char>* _doc;
    char* _buffer;
};

//! Base class for all serializable classes
/*! \ingroup utilities
*/
class XMLSerializable {
public:
    virtual ~XMLSerializable() {}
    virtual void fromXML(XMLNode* node) = 0;
    virtual XMLNode* toXML(XMLDocument& doc) = 0;

    void fromFile(const std::string& filename);
    void toFile(const std::string& filename);
};

//! XML Utilities Class
/*! \ingroup utilities
*/
class XMLUtils {
public:
    static void checkNode(XMLNode* n, const string& expectedName);

    static XMLNode* addChild(XMLDocument& doc, XMLNode* n, const string& name);
    static void addChild(XMLDocument& doc, XMLNode* n, const string& name, const string& value);
    static void addChild(XMLDocument& doc, XMLNode* n, const string& name, Real value);
    static void addChild(XMLDocument& doc, XMLNode* n, const string& name, int value);
    static void addChild(XMLDocument& doc, XMLNode* n, const string& name, bool value);

    //! Adds <code>\<Name>p1,p2,p3\</Name></code>
    template <class T> static void addGenericChild(XMLDocument& doc, XMLNode* n, const char* name, const T& value) {
        std::ostringstream oss;
        oss << value;
        addChild(doc, n, name, oss.str());
    }

    template <class T>
    static void addGenericChildAsList(XMLDocument& doc, XMLNode* n, const string& name, const vector<T>& values) {
        if (values.size() == 0) {
            addChild(doc, n, name);
        } else {
            std::ostringstream oss;
            oss << values[0];
            for (Size i = 1; i < values.size(); i++) {
                oss << ", " << values[i];
            }
            addChild(doc, n, name, oss.str());
        }
    }

    static void addChildren(XMLDocument& doc, XMLNode* n, const string& names, const string& name,
                            const vector<string>& values);
    static void addChildren(XMLDocument& doc, XMLNode* n, const string& names, const string& name,
                            const vector<Real>& values);
    //! Adds <code>\<Name>v1,v2,v3\</Name></code> - the inverse of getChildrenValuesAsDoublesCompact
    static void addChild(XMLDocument& doc, XMLNode* n, const string& name, const vector<Real>& values);
    static void addChildrenWithAttributes(XMLDocument& doc, XMLNode* n, const string& names, const string& name,
                                          const vector<Real>& values, const string& attrName,
                                          const vector<string>& attrs);

    static void addChildren(XMLDocument& doc, XMLNode* n, const string& names, const string& name,
                            const string& firstName, const string& secondName, const map<string, string>& values);

    // If mandatory == true, we throw if the node is not present, otherwise we return a default vale.
    static string getChildValue(XMLNode* node, const string& name, bool mandatory = false);
    static Real getChildValueAsDouble(XMLNode* node, const string& name, bool mandatory = false);
    static int getChildValueAsInt(XMLNode* node, const string& name, bool mandatory = false);
    static bool getChildValueAsBool(XMLNode* node, const string& name, bool mandatory = false); // default is true
    static vector<string> getChildrenValues(XMLNode* node, const string& names, const string& name,
                                            bool mandatory = false);

    static vector<Real> getChildrenValuesAsDoubles(XMLNode* node, const string& names, const string& name,
                                                   bool mandatory = false);
    static vector<Real> getChildrenValuesAsDoublesCompact(XMLNode* node, const string& name, bool mandatory = false);
    static vector<Real> getChildrenValuesAsDoublesWithAttributes(XMLNode* node, const string& names, const string& name,
                                                                 const string& attrName, vector<string>& attrs,
                                                                 bool mandatory = false);

    static vector<Period> getChildrenValuesAsPeriods(XMLNode* node, const string& name, bool mandatory = false);
    static vector<string> getChildrenValuesAsStrings(XMLNode* node, const string& name, bool mandatory = false);

    static map<string, string> getChildrenValues(XMLNode* node, const string& names, const string& name,
                                                 const string& firstName, const string& secondName,
                                                 bool mandatory = false);

    static map<string, string> getChildrenAttributesAndValues(XMLNode* parent, const string& names,
                                                              const string& attributeName, bool mandatory = false);

    // returns first child node
    static XMLNode* getChildNode(XMLNode* n, const string& name = "");
    // return first node in the hierarchy of n that matches name, maybe n itself
    static XMLNode* locateNode(XMLNode* n, const string& name = "");
    // append child to parent
    static void appendNode(XMLNode* parent, XMLNode* child);

    static void addAttribute(XMLDocument& doc, XMLNode* node, const string& attrName, const string& attrValue);
    static string getAttribute(XMLNode* node, const string& attrName);

    //! Returns all the children with a given name
    // To get all children, set name equal to Null or ""
    static vector<XMLNode*> getChildrenNodes(XMLNode* node, const string& name);

    //! Get and set a node's name
    static string getNodeName(XMLNode* n);
    static void setNodeName(XMLDocument& doc, XMLNode* node, const string& name);

    //! Get a node's next sibling node
    static XMLNode* getNextSibling(XMLNode* node, const string& name = "");

    //! Get a node's value
    static string getNodeValue(XMLNode* node);
};
}
}
