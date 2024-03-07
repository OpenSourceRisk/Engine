/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <ored/configuration/reportconfig.hpp>

#include <ored/utilities/parsers.hpp>

namespace ore {
namespace data {

using namespace QuantLib;

void ReportConfig::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Report");

    if (auto tmp = XMLUtils::getChildNode(node, "ReportOnDeltaGrid")) {
        reportOnDeltaGrid_ = parseBool(XMLUtils::getNodeValue(tmp));
    } else {
        reportOnDeltaGrid_ = boost::none;
    }

    if (auto tmp = XMLUtils::getChildNode(node, "ReportOnMoneynessGrid")) {
        reportOnMoneynessGrid_ = parseBool(XMLUtils::getNodeValue(tmp));
    } else {
        reportOnMoneynessGrid_ = boost::none;
    }

    if (auto tmp = XMLUtils::getChildNode(node, "ReportOnStrikeGrid")) {
        reportOnStrikeGrid_ = parseBool(XMLUtils::getNodeValue(tmp));
    } else {
        reportOnStrikeGrid_ = boost::none;
    }

    if (auto tmp = XMLUtils::getChildNode(node, "ReportOnStrikeSpreadGrid")) {
        reportOnStrikeSpreadGrid_ = parseBool(XMLUtils::getNodeValue(tmp));
    } else {
        reportOnStrikeGrid_ = boost::none;
    }

    if (auto tmp = XMLUtils::getChildNode(node, "Deltas")) {
        deltas_ = parseListOfValues(XMLUtils::getNodeValue(tmp));
    } else {
        deltas_ = boost::none;
    }

    if (auto tmp = XMLUtils::getChildNode(node, "Moneyness")) {
        moneyness_ = parseListOfValues<Real>(XMLUtils::getNodeValue(tmp), &parseReal);
    } else {
        moneyness_ = boost::none;
    }

    if (auto tmp = XMLUtils::getChildNode(node, "Strikes")) {
        strikes_ = parseListOfValues<Real>(XMLUtils::getNodeValue(tmp), &parseReal);
    } else {
        strikes_ = boost::none;
    }

    if (auto tmp = XMLUtils::getChildNode(node, "StrikeSpreads")) {
        strikeSpreads_ = parseListOfValues<Real>(XMLUtils::getNodeValue(tmp), &parseReal);
    } else {
        strikeSpreads_ = boost::none;
    }

    if (auto tmp = XMLUtils::getChildNode(node, "Expiries")) {
        expiries_ = parseListOfValues<Period>(XMLUtils::getNodeValue(tmp), &parsePeriod);
    } else {
        expiries_ = boost::none;
    }

    if (auto tmp = XMLUtils::getChildNode(node, "UnderlyingTenors")) {
        underlyingTenors_ = parseListOfValues<Period>(XMLUtils::getNodeValue(tmp), &parsePeriod);
    } else {
        underlyingTenors_ = boost::none;
    }
}

XMLNode* ReportConfig::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("Report");
    if (reportOnDeltaGrid_)
        XMLUtils::addChild(doc, node, "ReportOnDeltaGrid", *reportOnDeltaGrid_);
    if (reportOnMoneynessGrid_)
        XMLUtils::addChild(doc, node, "ReportOnMoneynessGrid", *reportOnMoneynessGrid_);
    if (deltas_)
        XMLUtils::addGenericChildAsList(doc, node, "Deltas", *deltas_);
    if (moneyness_)
        XMLUtils::addGenericChildAsList(doc, node, "Moneyness", *moneyness_);
    if (expiries_)
        XMLUtils::addGenericChildAsList(doc, node, "Expiries", *expiries_);
    if (underlyingTenors_)
        XMLUtils::addGenericChildAsList(doc, node, "UnderlyingTenors", *underlyingTenors_);
    return node;
}

ReportConfig effectiveReportConfig(const ReportConfig& globalConfig, const ReportConfig& localConfig) {
    bool reportOnDeltaGrid = false;
    bool reportOnMoneynessGrid = false;
    bool reportOnStrikeGrid = false;
    bool reportOnStrikeSpreadGrid = false;
    std::vector<Real> moneyness;
    std::vector<std::string> deltas;
    std::vector<Real> strikes;
    std::vector<Real> strikeSpreads;
    std::vector<Period> expiries;
    std::vector<Period> underlyingTenors;

    if (localConfig.reportOnDeltaGrid())
        reportOnDeltaGrid = *localConfig.reportOnDeltaGrid();
    else if (globalConfig.reportOnDeltaGrid())
        reportOnDeltaGrid = *globalConfig.reportOnDeltaGrid();

    if (localConfig.reportOnMoneynessGrid())
        reportOnMoneynessGrid = *localConfig.reportOnMoneynessGrid();
    else if (globalConfig.reportOnMoneynessGrid())
        reportOnMoneynessGrid = *globalConfig.reportOnMoneynessGrid();

    if (localConfig.reportOnStrikeGrid())
        reportOnStrikeGrid = *localConfig.reportOnStrikeGrid();
    else if (globalConfig.reportOnStrikeGrid())
        reportOnStrikeGrid = *globalConfig.reportOnStrikeGrid();

    if (localConfig.reportOnStrikeSpreadGrid())
        reportOnStrikeSpreadGrid = *localConfig.reportOnStrikeSpreadGrid();
    else if (globalConfig.reportOnStrikeSpreadGrid())
        reportOnStrikeSpreadGrid = *globalConfig.reportOnStrikeSpreadGrid();

    if (localConfig.moneyness())
        moneyness = *localConfig.moneyness();
    else if (globalConfig.moneyness())
        moneyness = *globalConfig.moneyness();

    if (localConfig.deltas())
        deltas = *localConfig.deltas();
    else if (globalConfig.deltas())
        deltas = *globalConfig.deltas();

    if (localConfig.strikes())
        strikes = *localConfig.strikes();
    else if (globalConfig.strikes())
        strikes = *globalConfig.strikes();

    if (localConfig.strikeSpreads())
        strikeSpreads = *localConfig.strikeSpreads();
    else if (globalConfig.strikeSpreads())
        strikeSpreads = *globalConfig.strikeSpreads();

    if (localConfig.expiries())
        expiries = *localConfig.expiries();
    else if (globalConfig.expiries())
        expiries = *globalConfig.expiries();

    if (localConfig.underlyingTenors())
        underlyingTenors = *localConfig.underlyingTenors();
    else if (globalConfig.underlyingTenors())
        underlyingTenors = *globalConfig.underlyingTenors();

    return ReportConfig(reportOnDeltaGrid, reportOnMoneynessGrid, reportOnStrikeGrid, reportOnStrikeSpreadGrid, deltas,
                        moneyness, strikes, strikeSpreads, expiries, underlyingTenors);
}

} // namespace data
} // namespace ore
