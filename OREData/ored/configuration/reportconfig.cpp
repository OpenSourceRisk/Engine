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

    if (auto tmp = XMLUtils::getChildNode(node, "Expiries")) {
        expiries_ = parseListOfValues<Period>(XMLUtils::getNodeValue(tmp), &parsePeriod);
    } else {
        expiries_ = boost::none;
    }
}

XMLNode* ReportConfig::toXML(XMLDocument& doc) {
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
    return node;
}

ReportConfig effectiveReportConfig(const ReportConfig& globalConfig, const ReportConfig& localConfig) {
    bool reportOnDeltaGrid = false;
    bool reportOnMoneynessGrid = false;
    std::vector<Real> moneyness;
    std::vector<std::string> deltas;
    std::vector<Period> expiries;

    if (localConfig.reportOnDeltaGrid())
        reportOnDeltaGrid = *localConfig.reportOnDeltaGrid();
    else if (globalConfig.reportOnDeltaGrid())
        reportOnDeltaGrid = *globalConfig.reportOnDeltaGrid();

    if (localConfig.reportOnMoneynessGrid())
        reportOnMoneynessGrid = *localConfig.reportOnMoneynessGrid();
    else if (globalConfig.reportOnMoneynessGrid())
        reportOnMoneynessGrid = *globalConfig.reportOnMoneynessGrid();

    if (localConfig.moneyness())
        moneyness = *localConfig.moneyness();
    else if (globalConfig.moneyness())
        moneyness = *globalConfig.moneyness();

    if (localConfig.deltas())
        deltas = *localConfig.deltas();
    else if (globalConfig.deltas())
        deltas = *globalConfig.deltas();

    if (localConfig.expiries())
        expiries = *localConfig.expiries();
    else if (globalConfig.expiries())
        expiries = *globalConfig.expiries();

    return ReportConfig(reportOnDeltaGrid, reportOnMoneynessGrid, deltas, moneyness, expiries);
}

} // namespace data
} // namespace ore
