/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <ored/portfolio/counterpartycorrelationmatrix.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/utilities/parsers.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/assign.hpp>
#include <boost/bimap.hpp>
#include <boost/lexical_cast.hpp>

using namespace QuantLib;
using std::string;

namespace ore {
namespace data {

void CounterpartyCorrelationMatrix::addCorrelation(const std::string& cpty1, const std::string& cpty2,
                                              QuantLib::Real correlation) {
    key k = buildkey(cpty1, cpty2);
    QL_REQUIRE(data_.find(k) == data_.end(), "Correlation for key " << k.first << "," << k.second << " already set");
    QL_REQUIRE(correlation >= -1.0 && correlation <= 1.0,
               "Invalid correlation " << correlation);
    data_[k] = correlation;
}

CounterpartyCorrelationMatrix::key CounterpartyCorrelationMatrix::buildkey(const string& f1In, const string& f2In) {
    string f1(f1In), f2(f2In); // we need local copies
    QL_REQUIRE(f1 != f2, "Correlation factors must be unique (" << f1 << ")");
    if (f1 < f2)
        return make_pair(f1, f2);
    else
        return make_pair(f2, f1);
}

Real CounterpartyCorrelationMatrix::lookup(const string& f1, const string& f2) {
    if (f1 == f2)
        return 1.0;
    key k = buildkey(f1, f2);
    if (data_.find(k) != data_.end())
        return data_[k];
    else
        QL_FAIL("correlation not found for " << f1 << f2);
}

CounterpartyCorrelationMatrix::CounterpartyCorrelationMatrix(ore::data::XMLNode* node) {
    fromXML(node);
}

//! loads NettingSetDefinition object from XML
void CounterpartyCorrelationMatrix::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Correlations"); 
    
    vector<XMLNode*> nodes = XMLUtils::getChildrenNodes(node, "Correlation");
    for (Size i = 0; i < nodes.size(); ++i) {
        string c1 = XMLUtils::getAttribute(nodes[i], "cpty1");
        string c2 = XMLUtils::getAttribute(nodes[i], "cpty2");
        string v = XMLUtils::getNodeValue(nodes[i]);
        if (c1 != "" && c2 != "" && v != "") {
            addCorrelation(c1, c2, boost::lexical_cast<Real>(v));
        }
    }
    

}

//! writes object to XML
XMLNode* CounterpartyCorrelationMatrix::toXML(XMLDocument& doc) const {
    // Allocate a node.
    XMLNode* corrNode = doc.allocNode("Correlations");

    for (auto correlationIterator = data_.begin(); correlationIterator != data_.end();
         correlationIterator++) {
        XMLNode* node = doc.allocNode("Correlation", std::to_string(correlationIterator->second));
        XMLUtils::appendNode(corrNode, node);
        std::vector<std::string> cptys = pairToStrings(correlationIterator->first);
        XMLUtils::addAttribute(doc, node, "cpty1", cptys[0]);
        XMLUtils::addAttribute(doc, node, "cpty2", cptys[1]);
    }
    return corrNode;
}

} // data
} // ore
