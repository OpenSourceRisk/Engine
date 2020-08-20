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

#include <ored/portfolio/additionalresults.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/xmlutils.hpp>
#include <ql/errors.hpp>
#include <ql/time/date.hpp>

using namespace QuantLib;
using namespace std;

namespace ore {
namespace data {

using namespace data;

AdditionalResults::AdditionalResults(const boost::shared_ptr<Portfolio>& portfolio) : portfolio_(portfolio) {

    for (auto t : portfolio->trades())
        additionalResults_.push_back(boost::make_shared<AdditionalResult>(t));
}

AdditionalResult::AdditionalResult(const boost::shared_ptr<Trade>& trade) {

    tradeId_ = trade->id();
    std::map<std::string,boost::any> additionalResults = 
        trade->instrument()->qlInstrument()->additionalResults();
    for (auto p : additionalResults) {
        if (p.second.type() == typeid(int)) {
            intResults_[p.first] = boost::any_cast<int>(p.second);
        } else if (p.second.type() == typeid(double)) {
            doubleResults_[p.first] = boost::any_cast<double>(p.second);
        } else if (p.second.type() == typeid(std::string)) {
            stringResults_[p.first] = boost::any_cast<std::string>(p.second);
        } else if (p.second.type() == typeid(std::vector<double>)) {
            vectorResults_[p.first] = boost::any_cast<std::vector<double>>(p.second);
        } else if (p.second.type() == typeid(QuantLib::Matrix)) {
            matrixResults_[p.first] = boost::any_cast<QuantLib::Matrix>(p.second);
        } else {
            ALOG("Unsupported AdditionalResults type: " << p.first);
        }
    }
}

XMLNode*  AdditionalResult::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("AdditionalResult");
    XMLUtils::addChild(doc, node, "TradeId", tradeId_);

    if (vectorResults_.size() > 0) {
        XMLNode* vectorNode = doc.allocNode("VectorResults");
        for (auto r : vectorResults_) {
            std::ostringstream oss;
            if (r.second.size() == 0) {
                oss << "";
            } else {
                oss << std::fixed << std::setprecision(8) << r.second[0];
                for (Size i = 1; i < r.second.size(); i++) {
                    oss << ", " << r.second[i];
                }
            }
            std::string valueString = oss.str();
            
            XMLNode* v = doc.allocNode("VectorResult", valueString);
            XMLUtils::addAttribute(doc, v, "id", r.first);
            XMLUtils::appendNode(vectorNode, v);
        }
        XMLUtils::appendNode(node, vectorNode);
    }

    if (matrixResults_.size() > 0) {
        XMLNode* matrixNode = doc.allocNode("MatrixResults");
        for (auto r : matrixResults_) {
            ostringstream oss;
            oss << std::fixed << std::setprecision(8) << r.second;
            std::string valueString = oss.str();
            XMLNode* v = doc.allocNode("MatrixResult", valueString);
            XMLUtils::addAttribute(doc, v, "id", r.first);
            XMLUtils::appendNode(matrixNode, v);
        } 
        XMLUtils::appendNode(node, matrixNode);
    }

    if (doubleResults_.size() > 0) {
        XMLNode* doubleNode = doc.allocNode("DoubleResults");
        for (auto r : doubleResults_) {
            ostringstream oss;
            oss << std::fixed << std::setprecision(8) << r.second;
            std::string valueString = oss.str();
            XMLNode* v = doc.allocNode("DoubleResult", valueString);
            XMLUtils::addAttribute(doc, v, "id", r.first);
            XMLUtils::appendNode(doubleNode, v);
        }
        XMLUtils::appendNode(node, doubleNode);
    }

    if (intResults_.size() > 0) {
        XMLNode* intNode = doc.allocNode("IntResults");
        for (auto r : intResults_) {
            ostringstream oss;
            oss << std::fixed << std::setprecision(8) << r.second;
            std::string valueString = oss.str();
            XMLNode* v = doc.allocNode("IntResult", valueString);
            XMLUtils::addAttribute(doc, v, "id", r.first);
            XMLUtils::appendNode(intNode, v);
        }
        XMLUtils::appendNode(node, intNode);
    }

    if (stringResults_.size() > 0) {
        XMLNode* stringNode = doc.allocNode("StringResults");
        for (auto r : stringResults_) {
            ostringstream oss;
            oss << std::fixed << std::setprecision(8) << r.second;
            std::string valueString = oss.str();
            XMLNode* v = doc.allocNode("StringResult", valueString);
            XMLUtils::addAttribute(doc, v, "id", r.first);
            XMLUtils::appendNode(stringNode, v);
        }
        XMLUtils::appendNode(node, stringNode);
    }

    return node;
}

void AdditionalResults::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("AdditionalResults");

    for (auto r : additionalResults_)
        XMLUtils::appendNode(node, r->toXML(doc));

}
void AdditionalResults::save(const string& fileName) const {
    LOG("Saving AdditionalResults to " << fileName);

    XMLDocument doc;
    XMLNode* node = doc.allocNode("AdditionalResults");
    doc.appendNode(node);
    for (auto r : additionalResults_)
        XMLUtils::appendNode(node, r->toXML(doc));
    // Write doc out.
    doc.toFile(fileName);
}


} // namespace data
} // namespace ore
