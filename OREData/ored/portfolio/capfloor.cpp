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

#include <ored/portfolio/builders/capfloor.hpp>
#include <ored/portfolio/builders/yoycapfloor.hpp>
#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/capfloor.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/utilities/log.hpp>

#include <qle/indexes/inflationindexwrapper.hpp>
#include <ql/instruments/capfloor.hpp>
#include <ql/instruments/inflationcapfloor.hpp>

#include <boost/make_shared.hpp>

using namespace QuantLib;

namespace ore {
namespace data {
void CapFloor::build(const boost::shared_ptr<EngineFactory>& engineFactory) {

    // Make sure the leg is floating or CMS
    QL_REQUIRE((legData_.legType() == "Floating") || (legData_.legType() == "CMS") || 
               (legData_.legType() == "CPI") || (legData_.legType() == "YY"),
               "CapFloor build error, LegType must be Floating, CMS, CPI or YY");

    // Determine if we have a cap, a floor or a collar
    QL_REQUIRE(caps_.size() > 0 || floors_.size() > 0, "CapFloor build error, no cap rates or floor rates provided");
    QuantLib::CapFloor::Type capFloorType;
    if (floors_.size() == 0) {
        capFloorType = QuantLib::CapFloor::Cap;
    } else if (caps_.size() == 0) {
        capFloorType = QuantLib::CapFloor::Floor;
    } else {
        capFloorType = QuantLib::CapFloor::Collar;
    }

    // Make sure that the floating leg section does not have caps or floors
    boost::shared_ptr<EngineBuilder> builder;

    if (legData_.legType() == "Floating") {
        builder = engineFactory->builder(tradeType_);

        boost::shared_ptr<FloatingLegData> floatData =
            boost::dynamic_pointer_cast<FloatingLegData>(legData_.concreteLegData());
        QL_REQUIRE(floatData, "Wrong LegType, expected Floating, got " << legData_.legType());
        QL_REQUIRE(floatData->caps().empty() && floatData->floors().empty(),
                   "CapFloor build error, Floating leg section must not have caps and floors");

        string indexName = floatData->index();
        Handle<IborIndex> hIndex =
            engineFactory->market()->iborIndex(indexName, builder->configuration(MarketContext::pricing));
        QL_REQUIRE(!hIndex.empty(), "Could not find ibor index " << indexName << " in market.");
        boost::shared_ptr<IborIndex> index = hIndex.currentLink();

        // Do not support caps/floors involving overnight indices
        boost::shared_ptr<OvernightIndex> ois = boost::dynamic_pointer_cast<OvernightIndex>(index);
        QL_REQUIRE(!ois, "CapFloor trade type does not support overnight indices.");
        legs_.push_back(makeIborLeg(legData_, index, engineFactory));

        // If a vector of cap/floor rates are provided, ensure they align with the number of schedule periods
        if (floors_.size() > 1) {
            QL_REQUIRE(floors_.size() == legs_[0].size(),
                       "The number of floor rates provided does not match the number of schedule periods");
        }

        if (caps_.size() > 1) {
            QL_REQUIRE(caps_.size() == legs_[0].size(),
                       "The number of cap rates provided does not match the number of schedule periods");
        }

        // If one cap/floor rate is given, extend the vector to align with the number of schedule periods
        if (floors_.size() == 1)
            floors_.resize(legs_[0].size(), floors_[0]);

        if (caps_.size() == 1)
            caps_.resize(legs_[0].size(), caps_[0]);

        // Create QL CapFloor instrument
        boost::shared_ptr<QuantLib::CapFloor> capFloor =
            boost::make_shared<QuantLib::CapFloor>(capFloorType, legs_[0], caps_, floors_);

        boost::shared_ptr<CapFloorEngineBuilder> capFloorBuilder =
            boost::dynamic_pointer_cast<CapFloorEngineBuilder>(builder);
        capFloor->setPricingEngine(capFloorBuilder->engine(parseCurrency(legData_.currency())));

        // Wrap the QL instrument in a vanilla instrument
        Real multiplier = (parsePositionType(longShort_) == Position::Long ? 1.0 : -1.0);
        instrument_ = boost::make_shared<VanillaInstrument>(capFloor, multiplier);

        maturity_ = capFloor->maturityDate();

    } else if (legData_.legType() == "CMS") {
        builder = engineFactory->builder("Swap");

        boost::shared_ptr<CMSLegData> cmsData = boost::dynamic_pointer_cast<CMSLegData>(legData_.concreteLegData());
        QL_REQUIRE(cmsData, "Wrong LegType, expected CMS");

        string swapIndexName = cmsData->swapIndex();
        Handle<SwapIndex> hIndex =
            engineFactory->market()->swapIndex(swapIndexName, builder->configuration(MarketContext::pricing));
        QL_REQUIRE(!hIndex.empty(), "Could not find swap index " << swapIndexName << " in market.");

        boost::shared_ptr<SwapIndex> index = hIndex.currentLink();

        bool payer = (parsePositionType(longShort_) == Position::Long ? false : true);
        vector<bool> legPayers_;
        legs_.push_back(makeCMSLeg(legData_, index, engineFactory, caps_, floors_));
        legPayers_.push_back(!payer);
        legs_.push_back(makeCMSLeg(legData_, index, engineFactory));
        legPayers_.push_back(payer);

        boost::shared_ptr<QuantLib::Swap> capFloor(new QuantLib::Swap(legs_, legPayers_));
        boost::shared_ptr<SwapEngineBuilderBase> cmsCapFloorBuilder =
            boost::dynamic_pointer_cast<SwapEngineBuilderBase>(builder);
        capFloor->setPricingEngine(cmsCapFloorBuilder->engine(parseCurrency(legData_.currency())));

        instrument_.reset(new VanillaInstrument(capFloor));
        maturity_ = capFloor->maturityDate();

    }
    else if (legData_.legType() == "CPI") {
        // Add CPI Cap/Floors
    }
    else if (legData_.legType() == "YY") {
        builder = engineFactory->builder("YYCapFloor");

        boost::shared_ptr<YoYLegData> yyData = boost::dynamic_pointer_cast<YoYLegData>(legData_.concreteLegData());
        QL_REQUIRE(yyData, "Wrong LegType, expected YY");

        string indexName = yyData->index();
        Handle<YoYInflationIndex> yoyIndex;
        // look for yoy inflation index
        yoyIndex = engineFactory->market()->yoyInflationIndex (indexName, builder->configuration(MarketContext::pricing));
        // we must have either an yoy or a zero inflation index in the market, if no yoy curve, get teh zero
        // and create a yoy index from it
        if (yoyIndex.empty()) {
            Handle<ZeroInflationIndex> zeroIndex =
                engineFactory->market()->zeroInflationIndex(indexName, builder->configuration(MarketContext::pricing));
            QL_REQUIRE(!zeroIndex.empty(), 
                "Could not find inflation index (of type either zero or yoy) " << indexName << " in market.");
            yoyIndex = Handle<YoYInflationIndex>(
                boost::make_shared<QuantExt::YoYInflationIndexWrapper>(zeroIndex.currentLink(), zeroIndex->interpolated()));
        }

        legs_.push_back(makeYoYLeg(legData_, yoyIndex.currentLink()));

        // If a vector of cap/floor rates are provided, ensure they align with the number of schedule periods
        if (floors_.size() > 1) {
            QL_REQUIRE(floors_.size() == legs_[0].size(),
                "The number of floor rates provided does not match the number of schedule periods");
        }

        if (caps_.size() > 1) {
            QL_REQUIRE(caps_.size() == legs_[0].size(),
                "The number of cap rates provided does not match the number of schedule periods");
        }

        // If one cap/floor rate is given, extend the vector to align with the number of schedule periods
        if (floors_.size() == 1)
            floors_.resize(legs_[0].size(), floors_[0]);

        if (caps_.size() == 1)
            caps_.resize(legs_[0].size(), caps_[0]);

        // Create QL YoY Inflation CapFloor instrument
        boost::shared_ptr<QuantLib::YoYInflationCapFloor> yoyCapFloor;
        if (capFloorType == QuantLib::CapFloor::Cap) {
            yoyCapFloor = boost::shared_ptr<YoYInflationCapFloor>(
                new YoYInflationCap(legs_[0], caps_));
        }
        else if (capFloorType == QuantLib::CapFloor::Floor) {
            yoyCapFloor = boost::shared_ptr<YoYInflationCapFloor>(
                new YoYInflationFloor(legs_[0], floors_));
        } 
        else {
            QL_FAIL("unknown YoYInflation cap/floor type");
        }
        
        boost::shared_ptr<YoYCapFloorEngineBuilder> capFloorBuilder =
            boost::dynamic_pointer_cast<YoYCapFloorEngineBuilder>(builder);
        yoyCapFloor->setPricingEngine(capFloorBuilder->engine(indexName));

        // Wrap the QL instrument in a vanilla instrument
        Real multiplier = (parsePositionType(longShort_) == Position::Long ? 1.0 : -1.0);
        instrument_ = boost::make_shared<VanillaInstrument>(yoyCapFloor, multiplier);

        maturity_ = yoyCapFloor->maturityDate();

    }
    else {
        QL_FAIL("Invalid legType for CapFloor");
    }

    // Fill in remaining Trade member data
    legCurrencies_.push_back(legData_.currency());
    legPayers_.push_back(legData_.isPayer());
    npvCurrency_ = legData_.currency();
    notional_ = currentNotional(legs_[0]);
}

void CapFloor::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* capFloorNode = XMLUtils::getChildNode(node, "CapFloorData");
    longShort_ = XMLUtils::getChildValue(capFloorNode, "LongShort", true);
    legData_.fromXML(XMLUtils::getChildNode(capFloorNode, "LegData"));
    caps_ = XMLUtils::getChildrenValuesAsDoubles(capFloorNode, "Caps", "Cap");
    floors_ = XMLUtils::getChildrenValuesAsDoubles(capFloorNode, "Floors", "Floor");
}

XMLNode* CapFloor::toXML(XMLDocument& doc) {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* capFloorNode = doc.allocNode("CapFloorData");
    XMLUtils::appendNode(node, capFloorNode);
    XMLUtils::addChild(doc, capFloorNode, "LongShort", longShort_);
    XMLUtils::appendNode(capFloorNode, legData_.toXML(doc));
    XMLUtils::addChildren(doc, capFloorNode, "Caps", "Cap", caps_);
    XMLUtils::addChildren(doc, capFloorNode, "Floors", "Floor", floors_);
    return node;
}
} // namespace data
} // namespace ore
