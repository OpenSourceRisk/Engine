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

#include <ql/errors.hpp>
#include <ql/time/period.hpp>
#include <ql/time/calendar.hpp>
#include <ql/time/businessdayconvention.hpp>
#include <ql/types.hpp>

#include <map>
#include <sstream> // std::ostringstream
#include <string>
#include <vector>

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
} // namespace rapidxml

namespace ore {
namespace data {
using QuantLib::Period;
using QuantLib::Real;
using QuantLib::Size;
using std::map;
using std::pair;
using std::string;
using std::vector;

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
    void toFile(const string& filename) const;

    //! return the XML Document as a string.
    std::string toString() const;

    XMLNode* getFirstNode(const string& name) const;
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
    virtual XMLNode* toXML(XMLDocument& doc) const = 0;

    void fromFile(const std::string& filename);
    void toFile(const std::string& filename) const;

    //! Parse from XML string
    void fromXMLString(const std::string& xml);
    //! Parse from XML string
    std::string toXMLString() const;
};

//! XML Utilities Class
/*! \ingroup utilities
 */
class XMLUtils {
public:
    static void checkNode(XMLNode* n, const string& expectedName);

    static XMLNode* addChild(XMLDocument& doc, XMLNode* n, const string& name);
    static void addChild(XMLDocument& doc, XMLNode* n, const string& name, const string& value);
    static void addChildAsCdata(XMLDocument& doc, XMLNode* n, const string& name, const string& value);
    static void addChild(XMLDocument& doc, XMLNode* n, const string& name, const string& value, const string& attrName,
                         const string& attr);
    static void addChild(XMLDocument& doc, XMLNode* n, const string& name, const string& value,
                         const vector<string>& attrNames, const vector<string>& attrs);
    static void addChild(XMLDocument& doc, XMLNode* n, const string& name, const char* value);
    static void addChild(XMLDocument& doc, XMLNode* n, const string& name, Real value);
    static void addChild(XMLDocument& doc, XMLNode* n, const string& name, int value);
    static void addChild(XMLDocument& doc, XMLNode* n, const string& name, bool value);
    static void addChild(XMLDocument& doc, XMLNode* n, const string& name, const Period& value);

    //! Adds <code>\<Name>p1,p2,p3\</Name></code>
    template <class T> static void addGenericChild(XMLDocument& doc, XMLNode* n, const char* name, const T& value) {
        std::ostringstream oss;
        oss << value;
        addChild(doc, n, name, oss.str());
    }

    template <class T>
    static void addGenericChildAsList(XMLDocument& doc, XMLNode* n, const string& name, const vector<T>& values,
                                      const string& attrName = "", const string& attr = "") {
        std::ostringstream oss;
        if (values.size() == 0) {
            oss << "";
        } else {
            oss << values[0];
            for (Size i = 1; i < values.size(); i++) {
                oss << ", " << values[i];
            }
        }
        addChild(doc, n, name, oss.str(), attrName, attr);
    }

    template <class T = string>
    static void addChildren(XMLDocument& doc, XMLNode* n, const string& names, const string& name,
                            const vector<T>& values);
    //! Adds <code>\<Name>v1,v2,v3\</Name></code> - the inverse of getChildrenValuesAsDoublesCompact
    static void addChild(XMLDocument& doc, XMLNode* n, const string& name, const vector<Real>& values);
    template <class T = string>

    static void addChildrenWithAttributes(XMLDocument& doc, XMLNode* n, const string& names, const string& name,
                                          const vector<T>& values, const string& attrName,
                                          const vector<string>& attrs); // one attribute (convenience function)
    template <class T = string>
    static void addChildrenWithAttributes(XMLDocument& doc, XMLNode* n, const string& names, const string& name,
                                          const vector<T>& values, const vector<string>& attrNames,
                                          const vector<vector<string>>& attrs); // n attributes
    template <class T = string>
    static void addChildrenWithOptionalAttributes(XMLDocument& doc, XMLNode* n, const string& names, const string& name,
                                                  const vector<T>& values, const string& attrName,
                                                  const vector<string>& attrs); // one attribute (convenience function)
    template <class T = string>
    static void addChildrenWithOptionalAttributes(XMLDocument& doc, XMLNode* n, const string& names, const string& name,
                                                  const vector<T>& values, const vector<string>& attrNames,
                                                  const vector<vector<string>>& attrs); // n attributes

    static void addChildren(XMLDocument& doc, XMLNode* n, const string& names, const string& name,
                            const string& firstName, const string& secondName, const map<string, string>& values);

    // If mandatory == true, we throw if the node is not present, otherwise we return a default vale.
    static string getChildValue(XMLNode* node, const string& name, bool mandatory = false, const string& defaultValue = string());
    static Real getChildValueAsDouble(XMLNode* node, const string& name, bool mandatory = false, double defaultValue = 0.0);
    static int getChildValueAsInt(XMLNode* node, const string& name, bool mandatory = false, int defaultValue = 0);
    static bool getChildValueAsBool(XMLNode* node, const string& name, bool mandatory = false, bool defaultValue = true);
    static Period getChildValueAsPeriod(XMLNode* node, const string& name, bool mandatory = false,
                                        const QuantLib::Period& defaultValue = 0 * QuantLib::Days);
    static vector<string> getChildrenValues(XMLNode* node, const string& names, const string& name,
                                            bool mandatory = false);
    static vector<string>
    getChildrenValuesWithAttributes(XMLNode* node, const string& names, const string& name, const string& attrName,
                                    vector<string>& attrs,
                                    bool mandatory = false); // one attribute (convenience function)

    static vector<string> getChildrenValuesWithAttributes(XMLNode* node, const string& names, const string& name,
                                                          const vector<string>& attrNames,
                                                          const vector<std::reference_wrapper<vector<string>>>& attrs,
                                                          bool mandatory = false); // n attributes
    template <class T>
    static vector<T> getChildrenValuesWithAttributes(XMLNode* node, const string& names, const string& name,
                                                     const string& attrName, vector<string>& attrs,
                                                     const std::function<T(string)> parser,
                                                     bool mandatory = false); // one attribute (convenience function)

    template <class T>
    static vector<T> getChildrenValuesWithAttributes(XMLNode* node, const string& names, const string& name,
                                                     const vector<string>& attrNames,
                                                     const vector<std::reference_wrapper<vector<string>>>& attrs,
                                                     const std::function<T(string)> parser,
                                                     bool mandatory = false); // n attributes

    static vector<Real> getChildrenValuesAsDoubles(XMLNode* node, const string& names, const string& name,
                                                   bool mandatory = false);
    static vector<Real> getChildrenValuesAsDoublesCompact(XMLNode* node, const string& name, bool mandatory = false);

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
    // To get all children, set name equal to ""
    static vector<XMLNode*> getChildrenNodes(XMLNode* node, const string& name);

    static vector<XMLNode*> getChildrenNodesWithAttributes(XMLNode* node, const string& names, const string& name,
                                                           const string& attrName, vector<string>& attrs,
                                                           bool mandatory = false);
    static vector<XMLNode*> getChildrenNodesWithAttributes(XMLNode* node, const string& names, const string& name,
                                                           const vector<string>& attrNames,
                                                           const vector<std::reference_wrapper<vector<string>>>& attrs,
                                                           bool mandatory = false);

    //! Get and set a node's name
    static string getNodeName(XMLNode* n);
    static void setNodeName(XMLDocument& doc, XMLNode* node, const string& name);

    //! Get a node's next sibling node
    static XMLNode* getNextSibling(XMLNode* node, const string& name = "");

    //! Get a node's value
    static string getNodeValue(XMLNode* node);

    //! Get a node's compact values as vector of doubles
    static vector<Real> getNodeValueAsDoublesCompact(XMLNode* node);

    //! Write a node out as a string
    static string toString(XMLNode* node);

    // helper routine to convert a value of an arbitrary type to string
    static string convertToString(const Real value);

	template <class T> static string convertToString(const T& value);

};

} // namespace data
} // namespace ore
