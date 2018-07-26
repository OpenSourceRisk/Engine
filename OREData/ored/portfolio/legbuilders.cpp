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


namespace ore {
namespace data {

Leg FixedLegBuilder::buildLeg(const LegData& data, const boost::shared_ptr<EngineFactory>& engineFactory,
                              const string& configuration) const {
    return makeFixedLeg(data);
}

Leg FloatingLegBuilder::buildLeg(const LegData& data, const boost::shared_ptr<EngineFactory>& engineFactory,
                                 const string& configuration) const {
    auto floatData = boost::dynamic_pointer_cast<FloatingLegData>(data.concreteLegData());
    QL_REQUIRE(floatData, "Wrong LegType, expected Floating");
    string indexName = floatData->index();
    auto index = *engineFactory->market()->iborIndex(indexName, configuration);
    auto ois = boost::dynamic_pointer_cast<OvernightIndex>(index);
    if (ois != nullptr)
        return makeOISLeg(data, ois);
    else {
        auto bma = boost::dynamic_pointer_cast<QuantExt::BMAIndexWrapper>(index);
        if (bma != nullptr)
            return makeBMALeg(data, bma);
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
    return makeCPILeg(data, index);
}

Leg YYLegBuilder::buildLeg(const LegData& data, const boost::shared_ptr<EngineFactory>& engineFactory,
                           const string& configuration) const {
    auto yyData = boost::dynamic_pointer_cast<YoYLegData>(data.concreteLegData());
    QL_REQUIRE(yyData, "Wrong LegType, expected YY");
    string inflationIndexName = yyData->index();
    auto index = *engineFactory->market()->yoyInflationIndex(inflationIndexName, configuration);
    return makeYoYLeg(data, index);
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

Leg EquityLegBuilder::buildLeg(const LegData& data, const boost::shared_ptr<EngineFactory>& engineFactory,
    const string& configuration) const {
    auto eqData = boost::dynamic_pointer_cast<EquityLegData>(data.concreteLegData());
    QL_REQUIRE(eqData, "Wrong LegType, expected Equity");
    string eqName = eqData->eqName();
    auto eqCurve = *engineFactory->market()->equityCurve(eqName, configuration);
    return makeEquityLeg(data, eqCurve);
}

} // namespace data
} // namespace ore
