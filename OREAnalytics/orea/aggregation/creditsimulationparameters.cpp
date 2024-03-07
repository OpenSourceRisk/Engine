/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <orea/aggregation/creditsimulationparameters.hpp>

#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/xmlutils.hpp>

#include <boost/lexical_cast.hpp>

using namespace QuantLib;

namespace ore {
namespace analytics {

void CreditSimulationParameters::fromXML(XMLNode* root) {
    XMLNode* sim = XMLUtils::locateNode(root, "CreditSimulation");
    XMLNode* node = XMLUtils::getChildNode(sim, "TransitionMatrices");
    QL_REQUIRE(node != nullptr, "CreditSimulationParameters: Node TransitionMatrices not found.");

    node = XMLUtils::getChildNode(node, "TransitionMatrix");
    while (node != nullptr) {
        string name = XMLUtils::getChildValue(node, "Name", true);
        std::vector<Real> data = XMLUtils::getChildrenValuesAsDoublesCompact(node, "Data", true);
        Size n = static_cast<Size>(std::sqrt(data.size()));
        QL_REQUIRE(data.size() == n * n, "CreditSimulationParameters: Square matrix required, found " << data.size()
                                                                                                      << " elements");
        Matrix m(n, n, 0.0);
        for (Size i = 0; i < n; ++i) {
            for (Size j = 0; j < n; ++j) {
                m[i][j] = data[i * n + j];
            }
        }
        transitionMatrix_[name] = m;
        node = XMLUtils::getNextSibling(node, "TransitionMatrix");
    }

    node = XMLUtils::getChildNode(sim, "Entities");
    QL_REQUIRE(node != nullptr, "CreditSimulationparameters: Node Entities not found.");
    node = XMLUtils::getChildNode(node, "Entity");
    while (node != nullptr) {
        string name = XMLUtils::getChildValue(node, "Name", true);
        std::vector<Real> tmp = XMLUtils::getChildrenValuesAsDoublesCompact(node, "FactorLoadings", true);
        Array loadings(tmp.begin(), tmp.end());
        string transitionMatrix = XMLUtils::getChildValue(node, "TransitionMatrix", true);
        Size initialState = XMLUtils::getChildValueAsInt(node, "InitialState", true);
        entities_.push_back(name);
        factorLoadings_.push_back(loadings);
        transitionMatrices_.push_back(transitionMatrix);
        initialStates_.push_back(initialState);
        node = XMLUtils::getNextSibling(node, "Entity");
    }

    node = XMLUtils::getChildNode(sim, "Risk");
    QL_REQUIRE(node != nullptr, "CreditSimulationParameters: Node Risk not found.");
    marketRisk_ = XMLUtils::getChildValueAsBool(node, "Market", true);
    creditRisk_ = XMLUtils::getChildValueAsBool(node, "Credit", true);
    zeroMarketPnl_ = XMLUtils::getChildValueAsBool(node, "ZeroMarketPnl", true);
    evaluation_ = XMLUtils::getChildValue(node, "Evaluation", true);
    doubleDefault_ = XMLUtils::getChildValueAsBool(node, "DoubleDefault", true);
    seed_ = XMLUtils::getChildValueAsInt(node, "Seed", true);
    paths_ = XMLUtils::getChildValueAsInt(node, "Paths", true);
    creditMode_ = XMLUtils::getChildValue(node, "CreditMode", true);
    loanExposureMode_ = XMLUtils::getChildValue(node, "LoanExposureMode", true);

    nettingSetIds_ = parseListOfValues(XMLUtils::getChildValue(sim, "NettingSetIds", true));
}

XMLNode* CreditSimulationParameters::toXML(XMLDocument& doc) const {
    QL_FAIL("CreditSimulationParameters::toXML() not implemented");
}

} // namespace analytics
} // namespace ore
