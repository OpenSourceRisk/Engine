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

#include <orea/scenario/scenariogeneratorbuilder.hpp>
#include <orea/scenario/simplescenariofactory.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>

#include <map>

using namespace ore::data;
using namespace std;

namespace ore {
namespace analytics {

void ScenarioGeneratorData::clear() {
    if (grid_)
        grid_->truncate(0);
}

CrossAssetStateProcess::discretization parseDiscretization(const string& s) {
    static map<string, QuantExt::CrossAssetStateProcess::discretization> m = {
        {"Exact", QuantExt::CrossAssetStateProcess::exact}, {"Euler", QuantExt::CrossAssetStateProcess::euler}};

    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("Cannot convert " << s << " to QuantExt::CrossAssetStateProcess::discretization");
    }
}

std::ostream& operator<<(std::ostream& out, const QuantExt::CrossAssetStateProcess::discretization& dis) {
    switch (dis) {
    case QuantExt::CrossAssetStateProcess::exact:
        return out << "Exact";
    case QuantExt::CrossAssetStateProcess::euler:
        return out << "Euler";
    default:
        return out << "?";
    }
}

void ScenarioGeneratorData::fromXML(XMLNode* root) {
    XMLNode* sim = XMLUtils::locateNode(root, "Simulation");
    XMLNode* node = XMLUtils::getChildNode(sim, "Parameters");
    XMLUtils::checkNode(node, "Parameters");

    std::string discString = XMLUtils::getChildValue(node, "Discretization", true); // mandatory
    discretization_ = parseDiscretization(discString);
    LOG("ScenarioGeneratorData discretization = " << discString);

    std::string calString = XMLUtils::getChildValue(node, "Calendar", true);
    Calendar cal = parseCalendar(calString);

    std::string gridString = XMLUtils::getChildValue(node, "Grid", true);
    std::vector<std::string> tokens;
    boost::split(tokens, gridString, boost::is_any_of(","));
    if (tokens.size() <= 2) {
        grid_ = boost::make_shared<ore::analytics::DateGrid>(gridString, cal);
    } else {
        std::vector<Period> gridTenors = XMLUtils::getChildrenValuesAsPeriods(node, "Grid", true);
        grid_ = boost::make_shared<ore::analytics::DateGrid>(gridTenors, cal);
    }
    LOG("ScenarioGeneratorData grid points size = " << grid_->size());

    std::string sequenceTypeString = XMLUtils::getChildValue(node, "Sequence", true);
    sequenceType_ = parseSequenceType(sequenceTypeString);
    LOG("ScenarioGeneratorData sequence type = " << sequenceTypeString);

    seed_ = XMLUtils::getChildValueAsInt(node, "Seed", true);
    LOG("ScenarioGeneratorData seed = " << seed_);

    samples_ = XMLUtils::getChildValueAsInt(node, "Samples", true);
    LOG("ScenarioGeneratorData samples = " << samples_);

    LOG("ScenarioGeneratorData done.");
}

XMLNode* ScenarioGeneratorData::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("Simlation");
    return node;
}
} // namespace analytics
} // namespace ore
