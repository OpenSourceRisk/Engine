/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <ored/portfolio/legbuilders.hpp>
#include <ored/portfolio/legdata.hpp>
#include <qle/indexes/fxindex.hpp>

using namespace QuantExt;

namespace ore {
namespace data {

Leg FixedLegBuilder::buildLeg(const LegData& data, const boost::shared_ptr<EngineFactory>& engineFactory,
                              const string& configuration) const {

    return makeFixedLeg(data, engineFactory);
}

Leg ZeroCouponFixedLegBuilder::buildLeg(const LegData& data, const boost::shared_ptr<EngineFactory>& engineFactory,
                                        const string& configuration) const {

    return makeZCFixedLeg(data);
}

Leg FloatingLegBuilder::buildLeg(const LegData& data, const boost::shared_ptr<EngineFactory>& engineFactory,
                                 const string& configuration) const {
    auto floatData = boost::dynamic_pointer_cast<FloatingLegData>(data.concreteLegData());
    QL_REQUIRE(floatData, "Wrong LegType, expected Floating");
    string indexName = floatData->index();
    auto index = *engineFactory->market()->iborIndex(indexName, configuration);
    auto ois = boost::dynamic_pointer_cast<OvernightIndex>(index);
    if (ois != nullptr)
        return makeOISLeg(data, ois, engineFactory);
    else {
        auto bma = boost::dynamic_pointer_cast<QuantExt::BMAIndexWrapper>(index);
        if (bma != nullptr)
            return makeBMALeg(data, bma, engineFactory);
        else
            return makeIborLeg(data, index, engineFactory);
    }
}

Leg CashflowLegBuilder::buildLeg(const LegData& data, const boost::shared_ptr<EngineFactory>& engineFactory,
                                 const string& configuration) const {
    return makeSimpleLeg(data);
}

Leg CPILegBuilder::buildLeg(const LegData& data, const boost::shared_ptr<EngineFactory>& engineFactory,
                            const string& configuration) const {
    auto cpiData = boost::dynamic_pointer_cast<CPILegData>(data.concreteLegData());
    QL_REQUIRE(cpiData, "Wrong LegType, expected CPI");
    string inflationIndexName = cpiData->index();
    auto index = *engineFactory->market()->zeroInflationIndex(inflationIndexName, configuration);
    return makeCPILeg(data, index, engineFactory);
}

Leg YYLegBuilder::buildLeg(const LegData& data, const boost::shared_ptr<EngineFactory>& engineFactory,
                           const string& configuration) const {
    auto yyData = boost::dynamic_pointer_cast<YoYLegData>(data.concreteLegData());
    QL_REQUIRE(yyData, "Wrong LegType, expected YY");
    string inflationIndexName = yyData->index();
    auto index = *engineFactory->market()->yoyInflationIndex(inflationIndexName, configuration);
    return makeYoYLeg(data, index, engineFactory);
}

Leg CMSLegBuilder::buildLeg(const LegData& data, const boost::shared_ptr<EngineFactory>& engineFactory,
                            const string& configuration) const {
    auto cmsData = boost::dynamic_pointer_cast<CMSLegData>(data.concreteLegData());
    QL_REQUIRE(cmsData, "Wrong LegType, expected CMS");
    string swapIndexName = cmsData->swapIndex();
    auto index = *engineFactory->market()->swapIndex(swapIndexName, configuration);
    return makeCMSLeg(data, index, engineFactory);
}

Leg CMSSpreadLegBuilder::buildLeg(const LegData& data, const boost::shared_ptr<EngineFactory>& engineFactory,
                                  const string& configuration) const {
    auto cmsSpreadData = boost::dynamic_pointer_cast<CMSSpreadLegData>(data.concreteLegData());
    QL_REQUIRE(cmsSpreadData, "Wrong LegType, expected CMSSpread");
    auto index1 = *engineFactory->market()->swapIndex(cmsSpreadData->swapIndex1(), configuration);
    auto index2 = *engineFactory->market()->swapIndex(cmsSpreadData->swapIndex2(), configuration);
    return makeCMSSpreadLeg(data,
                            boost::make_shared<QuantLib::SwapSpreadIndex>(
                                "CMSSpread_" + index1->familyName() + "_" + index2->familyName(), index1, index2),
                            engineFactory);
}

Leg DigitalCMSSpreadLegBuilder::buildLeg(const LegData& data, const boost::shared_ptr<EngineFactory>& engineFactory,
                                         const string& configuration) const {
    auto digitalCmsSpreadData = boost::dynamic_pointer_cast<DigitalCMSSpreadLegData>(data.concreteLegData());
    QL_REQUIRE(digitalCmsSpreadData, "Wrong LegType, expected DigitalCMSSpread");

    auto cmsSpreadData = boost::dynamic_pointer_cast<CMSSpreadLegData>(digitalCmsSpreadData->underlying());
    QL_REQUIRE(cmsSpreadData, "Incomplete DigitalCmsSpread Leg, expected CMSSpread data");

    auto index1 = *engineFactory->market()->swapIndex(cmsSpreadData->swapIndex1(), configuration);
    auto index2 = *engineFactory->market()->swapIndex(cmsSpreadData->swapIndex2(), configuration);

    return makeDigitalCMSSpreadLeg(
        data,
        boost::make_shared<QuantLib::SwapSpreadIndex>("CMSSpread_" + index1->familyName() + "_" + index2->familyName(),
                                                      index1, index2),
        engineFactory);
}

Leg EquityLegBuilder::buildLeg(const LegData& data, const boost::shared_ptr<EngineFactory>& engineFactory,
                               const string& configuration) const {
    auto eqData = boost::dynamic_pointer_cast<EquityLegData>(data.concreteLegData());
    QL_REQUIRE(eqData, "Wrong LegType, expected Equity");
    string eqName = eqData->eqName();
    auto eqCurve = *engineFactory->market()->equityCurve(eqName, configuration);

    boost::shared_ptr<QuantExt::FxIndex> fxIndex = nullptr;
    // if equity currency differs from the leg currency we need an FxIndex
    if (eqData->eqCurrency() != "" && eqData->eqCurrency() != data.currency()) {
        QL_REQUIRE(eqData->fxIndex() != "", 
            "No FxIndex - if equity currency differs from leg currency an FxIndex must be provided");

        fxIndex = buildFxIndex(eqData->fxIndex(), data.currency(), eqData->eqCurrency(), engineFactory->market(),
            configuration, eqData->fxIndexCalendar(), eqData->fxIndexFixingDays());
    }

    return makeEquityLeg(data, eqCurve, fxIndex);
}


boost::shared_ptr<QuantExt::FxIndex> buildFxIndex(const string& fxIndex, const string& domestic, 
    const string& foreign, const boost::shared_ptr<Market>& market, const string& configuration, 
    const string& calendar, Size fixingDays) {
    // 1. Parse the index we have with no term structures
    boost::shared_ptr<QuantExt::FxIndex> fxIndexBase = parseFxIndex(fxIndex);

    // get market data objects - we set up the index using source/target, fixing days
    // and calendar from legData_[i].fxIndex()
    string source = fxIndexBase->sourceCurrency().code();
    string target = fxIndexBase->targetCurrency().code();
    Handle<YieldTermStructure> sorTS = market->discountCurve(source, configuration);
    Handle<YieldTermStructure> tarTS = market->discountCurve(target, configuration);
    Handle<Quote> spot = market->fxSpot(source + target);
    Calendar cal = parseCalendar(calendar);
    
    // Now check the ccy and foreignCcy from the legdata, work out if we need to invert or not
    bool invertFxIndex = false;
    if (domestic == target && foreign == source) {
        invertFxIndex = false;
    } else if (domestic == source && foreign == target) {
        invertFxIndex = true;
    } else {
        QL_FAIL("Cannot combine FX Index " << fxIndex << " with reset ccy " << domestic
            << " and reset foreignCurrency " << foreign);
    }

    auto fxi = boost::make_shared<FxIndex>(fxIndexBase->familyName(), fixingDays,
        fxIndexBase->sourceCurrency(), fxIndexBase->targetCurrency(), cal, spot, sorTS, 
        tarTS, invertFxIndex);

    QL_REQUIRE(fxi, "Failed to build FXIndex " << fxIndex);
    
    return fxi;
}

} // namespace data
} // namespace ore
