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
#include <ored/utilities/to_string.hpp>

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

void ScenarioGeneratorData::setGrid(QuantLib::ext::shared_ptr<DateGrid> grid) { 
    grid_ = grid;
    
    std::ostringstream oss;
    if (grid->tenors().size() == 0) {
        oss << "";
    } else {
        oss << grid->tenors()[0];
        for (Size i = 1; i < grid->tenors().size(); i++) {
            oss << ", " << grid->tenors()[i];
        }
    }

    gridString_ = oss.str();
}

void ScenarioGeneratorData::fromXML(XMLNode* root) {
    XMLNode* sim = XMLUtils::locateNode(root, "Simulation");
    XMLNode* node = XMLUtils::getChildNode(sim, "Parameters");
    XMLUtils::checkNode(node, "Parameters");

    std::string calString = XMLUtils::getChildValue(node, "Calendar", true);
    Calendar cal = parseCalendar(calString);

    std::string dcString = XMLUtils::getChildValue(node, "DayCounter", false);
    DayCounter dc = dcString.empty() ? ActualActual(ActualActual::ISDA) : parseDayCounter(dcString);

    gridString_ = XMLUtils::getChildValue(node, "Grid", true);
    std::vector<std::string> tokens;
    boost::split(tokens, gridString_, boost::is_any_of(","));
    if (tokens.size() <= 2) {
        grid_ = QuantLib::ext::make_shared<DateGrid>(gridString_, cal, dc);
    } else {
        std::vector<Period> gridTenors = XMLUtils::getChildrenValuesAsPeriods(node, "Grid", true);
        grid_ = QuantLib::ext::make_shared<DateGrid>(gridTenors, cal, dc);
    }
    LOG("ScenarioGeneratorData grid points size = " << grid_->size());

    std::string sequenceTypeString = XMLUtils::getChildValue(node, "Sequence", true);
    sequenceType_ = parseSequenceType(sequenceTypeString);
    LOG("ScenarioGeneratorData sequence type = " << sequenceTypeString);

    seed_ = XMLUtils::getChildValueAsInt(node, "Seed", true);
    LOG("ScenarioGeneratorData seed = " << seed_);

    samples_ = XMLUtils::getChildValueAsInt(node, "Samples", true);
    LOG("ScenarioGeneratorData samples = " << samples_);

    // overwrite samples with environment variable OVERWRITE_SCENARIOGENERATOR_SAMPLES
    if (auto c = getenv("OVERWRITE_SCENARIOGENERATOR_SAMPLES")) {
        try {
            samples_ = std::stol(c);
        } catch (const std::exception& e) {
            WLOG("enviroment variable OVERWRITE_SCENARIOGENERATOR_SAMPLES is set ("
                 << c << ") but can not be parsed to a number - ignoring.");
        }
        LOG("Overwrite samples with " << samples_ << " from environment variable OVERWRITE_SCENARIOGENERATOR_SAMPLES")
    }

    if (auto n = XMLUtils::getChildNode(node, "Ordering"))
        ordering_ = parseSobolBrownianGeneratorOrdering(XMLUtils::getNodeValue(n));
    else
        ordering_ = SobolBrownianGenerator::Steps;

    if (auto n = XMLUtils::getChildNode(node, "DirectionIntegers"))
        directionIntegers_ = parseSobolRsgDirectionIntegers(XMLUtils::getNodeValue(n));
    else
        directionIntegers_ = SobolRsg::JoeKuoD7;

    withCloseOutLag_ = false;
    if (XMLUtils::getChildNode(node, "CloseOutLag") != NULL) {
        withCloseOutLag_ = true;
        closeOutLag_ = parsePeriod(XMLUtils::getChildValue(node, "CloseOutLag", true));
        grid_->addCloseOutDates(closeOutLag_);
        LOG("Use lagged close out grid, lag period is " << closeOutLag_);
    }
    withMporStickyDate_ = false;
    if (XMLUtils::getChildNode(node, "MporMode") != NULL) {
        string mporMode = XMLUtils::getChildValue(node, "MporMode", true);
        if (mporMode == "StickyDate") {
            withMporStickyDate_ = true;
            LOG("Use Mpor sticky date mode");
        } else if (mporMode == "ActualDate") {
            withMporStickyDate_ = false;
            LOG("Use Mpor actual date mode");
        } else {
            QL_FAIL("MporMode " << mporMode << " not recognised");
        }
    }

    LOG("ScenarioGeneratorData done.");
}

XMLNode* ScenarioGeneratorData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("Simulation");
    XMLNode* pNode = XMLUtils::addChild(doc, node, "Parameters");

    if (grid_) {
        XMLUtils::addChild(doc, pNode, "Calendar", grid_->calendar().name());
        XMLUtils::addChild(doc, pNode, "DayCounter", grid_->dayCounter().name());
        if (!gridString_.empty()) {
            XMLUtils::addChild(doc, pNode, "Grid", gridString_);
        } else {
            XMLUtils::addGenericChildAsList(doc, pNode, "Grid", grid_->tenors());
        }
    }

    XMLUtils::addChild(doc, pNode, "Sequence", ore::data::to_string( sequenceType_));
    XMLUtils::addChild(doc, pNode, "Seed", to_string(seed_));
    XMLUtils::addChild(doc, pNode, "Samples", to_string(samples_));

    XMLUtils::addChild(doc, pNode, "Ordering", ore::data::to_string((SobolBrownianGenerator::Ordering) ordering_) );
    XMLUtils::addChild(doc, pNode, "DirectionIntegers", ore::data::to_string(directionIntegers_));

    if (withCloseOutLag_) {
        XMLUtils::addChild(doc, pNode, "CloseOutLag", closeOutLag_);
    }
    if (withMporStickyDate_) {
        XMLUtils::addChild(doc, pNode, "MporMode", "StickyDate");
    } else {
        XMLUtils::addChild(doc, pNode, "MporMode", "ActualDate");
    }

    return node;
}

} // namespace analytics
} // namespace ore
