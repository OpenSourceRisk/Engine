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
#include <ored/portfolio/referencedata.hpp>

using namespace QuantExt;

namespace ore {
namespace data {

Leg FixedLegBuilder::buildLeg(const LegData& data, const boost::shared_ptr<EngineFactory>& engineFactory,
                              RequiredFixings& requiredFixings, const string& configuration) const {
    Leg leg = makeFixedLeg(data);
    std::map<std::string, std::string> qlToOREIndexNames;
    applyIndexing(leg, data, engineFactory, qlToOREIndexNames, requiredFixings);
    addToRequiredFixings(leg, boost::make_shared<FixingDateGetter>(requiredFixings, qlToOREIndexNames));
    return leg;
}

Leg ZeroCouponFixedLegBuilder::buildLeg(const LegData& data, const boost::shared_ptr<EngineFactory>& engineFactory,
                                        RequiredFixings& requiredFixings, const string& configuration) const {
    return makeZCFixedLeg(data);
}

Leg FloatingLegBuilder::buildLeg(const LegData& data, const boost::shared_ptr<EngineFactory>& engineFactory,
                                 RequiredFixings& requiredFixings, const string& configuration) const {
    auto floatData = boost::dynamic_pointer_cast<FloatingLegData>(data.concreteLegData());
    QL_REQUIRE(floatData, "Wrong LegType, expected Floating");
    string indexName = floatData->index();
    auto index = *engineFactory->market()->iborIndex(indexName, configuration);
    auto ois = boost::dynamic_pointer_cast<OvernightIndex>(index);
    Leg result;
    if (ois != nullptr)
        result = makeOISLeg(data, ois, engineFactory);
    else {
        auto bma = boost::dynamic_pointer_cast<QuantExt::BMAIndexWrapper>(index);
        if (bma != nullptr)
            result = makeBMALeg(data, bma);
        else
            result = makeIborLeg(data, index, engineFactory);
    }
    std::map<std::string, std::string> qlToOREIndexNames;
    applyIndexing(result, data, engineFactory, qlToOREIndexNames, requiredFixings);
    qlToOREIndexNames[index->name()] = indexName;
    addToRequiredFixings(result, boost::make_shared<FixingDateGetter>(requiredFixings, qlToOREIndexNames));
    return result;
}

Leg CashflowLegBuilder::buildLeg(const LegData& data, const boost::shared_ptr<EngineFactory>& engineFactory,
                                 RequiredFixings& requiredFixings, const string& configuration) const {
    return makeSimpleLeg(data);
}

Leg CPILegBuilder::buildLeg(const LegData& data, const boost::shared_ptr<EngineFactory>& engineFactory,
                            RequiredFixings& requiredFixings, const string& configuration) const {
    auto cpiData = boost::dynamic_pointer_cast<CPILegData>(data.concreteLegData());
    QL_REQUIRE(cpiData, "Wrong LegType, expected CPI");
    string inflationIndexName = cpiData->index();
    auto index = *engineFactory->market()->zeroInflationIndex(inflationIndexName, configuration);
    Leg result = makeCPILeg(data, index, engineFactory);
    addToRequiredFixings(result, boost::make_shared<FixingDateGetter>(
                                     requiredFixings, std::map<string, string>{{index->name(), inflationIndexName}}));
    return result;
}

Leg YYLegBuilder::buildLeg(const LegData& data, const boost::shared_ptr<EngineFactory>& engineFactory,
                           RequiredFixings& requiredFixings, const string& configuration) const {
    auto yyData = boost::dynamic_pointer_cast<YoYLegData>(data.concreteLegData());
    QL_REQUIRE(yyData, "Wrong LegType, expected YY");
    string inflationIndexName = yyData->index();
    auto index = *engineFactory->market()->yoyInflationIndex(inflationIndexName, configuration);
    Leg result = makeYoYLeg(data, index, engineFactory);
    addToRequiredFixings(result, boost::make_shared<FixingDateGetter>(
                                     requiredFixings, std::map<string, string>{{index->name(), inflationIndexName}}));
    return result;
}

Leg CMSLegBuilder::buildLeg(const LegData& data, const boost::shared_ptr<EngineFactory>& engineFactory,
                            RequiredFixings& requiredFixings, const string& configuration) const {
    auto cmsData = boost::dynamic_pointer_cast<CMSLegData>(data.concreteLegData());
    QL_REQUIRE(cmsData, "Wrong LegType, expected CMS");
    string swapIndexName = cmsData->swapIndex();
    auto index = *engineFactory->market()->swapIndex(swapIndexName, configuration);
    Leg result = makeCMSLeg(data, index, engineFactory);
    std::map<std::string, std::string> qlToOREIndexNames;
    applyIndexing(result, data, engineFactory, qlToOREIndexNames, requiredFixings);
    qlToOREIndexNames[index->name()] = swapIndexName;
    addToRequiredFixings(result, boost::make_shared<FixingDateGetter>(requiredFixings, qlToOREIndexNames));
    return result;
}

Leg CMSSpreadLegBuilder::buildLeg(const LegData& data, const boost::shared_ptr<EngineFactory>& engineFactory,
                                  RequiredFixings& requiredFixings, const string& configuration) const {
    auto cmsSpreadData = boost::dynamic_pointer_cast<CMSSpreadLegData>(data.concreteLegData());
    QL_REQUIRE(cmsSpreadData, "Wrong LegType, expected CMSSpread");
    auto index1 = *engineFactory->market()->swapIndex(cmsSpreadData->swapIndex1(), configuration);
    auto index2 = *engineFactory->market()->swapIndex(cmsSpreadData->swapIndex2(), configuration);
    Leg result = makeCMSSpreadLeg(data,
                                  boost::make_shared<QuantLib::SwapSpreadIndex>(
                                      "CMSSpread_" + index1->familyName() + "_" + index2->familyName(), index1, index2),
                                  engineFactory);
    std::map<std::string, std::string> qlToOREIndexNames;
    applyIndexing(result, data, engineFactory, qlToOREIndexNames, requiredFixings);
    qlToOREIndexNames[index1->name()] = cmsSpreadData->swapIndex1();
    qlToOREIndexNames[index2->name()] = cmsSpreadData->swapIndex2();
    addToRequiredFixings(result, boost::make_shared<FixingDateGetter>(requiredFixings, qlToOREIndexNames));
    return result;
}

Leg DigitalCMSSpreadLegBuilder::buildLeg(const LegData& data, const boost::shared_ptr<EngineFactory>& engineFactory,
                                         RequiredFixings& requiredFixings, const string& configuration) const {
    auto digitalCmsSpreadData = boost::dynamic_pointer_cast<DigitalCMSSpreadLegData>(data.concreteLegData());
    QL_REQUIRE(digitalCmsSpreadData, "Wrong LegType, expected DigitalCMSSpread");

    auto cmsSpreadData = boost::dynamic_pointer_cast<CMSSpreadLegData>(digitalCmsSpreadData->underlying());
    QL_REQUIRE(cmsSpreadData, "Incomplete DigitalCmsSpread Leg, expected CMSSpread data");

    auto index1 = *engineFactory->market()->swapIndex(cmsSpreadData->swapIndex1(), configuration);
    auto index2 = *engineFactory->market()->swapIndex(cmsSpreadData->swapIndex2(), configuration);

    Leg result =
        makeDigitalCMSSpreadLeg(data,
                                boost::make_shared<QuantLib::SwapSpreadIndex>(
                                    "CMSSpread_" + index1->familyName() + "_" + index2->familyName(), index1, index2),
                                engineFactory);
    std::map<std::string, std::string> qlToOREIndexNames;
    applyIndexing(result, data, engineFactory, qlToOREIndexNames, requiredFixings);
    qlToOREIndexNames[index1->name()] = cmsSpreadData->swapIndex1();
    qlToOREIndexNames[index2->name()] = cmsSpreadData->swapIndex2();
    addToRequiredFixings(result,
                         boost::make_shared<FixingDateGetter>(
                             requiredFixings, std::map<string, string>{{index1->name(), cmsSpreadData->swapIndex1()},
                                                                       {index2->name(), cmsSpreadData->swapIndex2()}}));
    return result;
}

Leg EquityLegBuilder::buildLeg(const LegData& data, const boost::shared_ptr<EngineFactory>& engineFactory,
                               RequiredFixings& requiredFixings, const string& configuration) const {
    auto eqData = boost::dynamic_pointer_cast<EquityLegData>(data.concreteLegData());
    QL_REQUIRE(eqData, "Wrong LegType, expected Equity");
    string eqName = eqData->eqName();

    auto eqCurve = *engineFactory->market()->equityCurve(eqName, configuration);

    boost::shared_ptr<QuantExt::FxIndex> fxIndex = nullptr;
    // if equity currency differs from the leg currency we need an FxIndex
    if ((eqData->eqCurrency() != "" && eqData->eqCurrency() != data.currency()) ||
        (data.currency() != eqCurve->currency().code())) {
        QL_REQUIRE(eqData->fxIndex() != "",
                   "No FxIndex - if equity currency differs from leg currency an FxIndex must be provided");

        fxIndex = buildFxIndex(eqData->fxIndex(), data.currency(), eqData->eqCurrency(), engineFactory->market(),
                               configuration, eqData->fxIndexCalendar(), eqData->fxIndexFixingDays());
    }

    Leg result = makeEquityLeg(data, eqCurve, fxIndex);
    addToRequiredFixings(
        result, boost::make_shared<FixingDateGetter>(
                    requiredFixings,
                    std::map<string, string>{{eqCurve->name(), "EQ-" + eqName},
                                             {fxIndex != nullptr ? fxIndex->name() : "na", eqData->fxIndex()}}));
    return result;
}

} // namespace data
} // namespace ore
