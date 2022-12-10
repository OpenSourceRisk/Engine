/*
 Copyright (C) 2016-2022 Quaternion Risk Management Ltd
 Copyright (C) 2021 Skandinaviska Enskilda Banken AB (publ)
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

#include <boost/make_shared.hpp>
#include <ored/portfolio/builders/bond.hpp>
#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/builders/capfloor.hpp>
#include <ored/portfolio/builders/capflooredaverageonindexedcouponleg.hpp>
#include <ored/portfolio/builders/capflooredcpileg.hpp>
#include <ored/portfolio/builders/capfloorediborleg.hpp>
#include <ored/portfolio/builders/capfloorednonstandardyoyleg.hpp>
#include <ored/portfolio/builders/capflooredovernightindexedcouponleg.hpp>
#include <ored/portfolio/builders/capflooredyoyleg.hpp>
#include <ored/portfolio/builders/cms.hpp>
#include <ored/portfolio/builders/cmsspread.hpp>
#include <ored/portfolio/builders/commodityasianoption.hpp>
#include <ored/portfolio/builders/commodityforward.hpp>
#include <ored/portfolio/builders/commodityoption.hpp>
#include <ored/portfolio/builders/commodityapo.hpp>
#include <ored/portfolio/builders/commodityapomodelbuilder.hpp>
#include <ored/portfolio/builders/commodityspreadoption.hpp>
#include <ored/portfolio/builders/commodityswap.hpp>
#include <ored/portfolio/builders/commodityswaption.hpp>
#include <ored/portfolio/builders/cpicapfloor.hpp>
#include <ored/portfolio/builders/creditdefaultswap.hpp>
#include <ored/portfolio/builders/creditdefaultswapoption.hpp>
#include <ored/portfolio/builders/durationadjustedcms.hpp>
#include <ored/portfolio/builders/equityasianoption.hpp>
#include <ored/portfolio/builders/equitybarrieroption.hpp>
#include <ored/portfolio/builders/equitycompositeoption.hpp>
#include <ored/portfolio/builders/equitydigitaloption.hpp>
#include <ored/portfolio/builders/equitydoublebarrieroption.hpp>
#include <ored/portfolio/builders/equitydoubletouchoption.hpp>
#include <ored/portfolio/builders/equityforward.hpp>
#include <ored/portfolio/builders/equityfuturesoption.hpp>
#include <ored/portfolio/builders/equityoption.hpp>
#include <ored/portfolio/builders/equitytouchoption.hpp>
#include <ored/portfolio/builders/forwardbond.hpp>
#include <ored/portfolio/builders/fxasianoption.hpp>
#include <ored/portfolio/builders/fxbarrieroption.hpp>
#include <ored/portfolio/builders/fxdigitalbarrieroption.hpp>
#include <ored/portfolio/builders/fxdigitaloption.hpp>
#include <ored/portfolio/builders/fxdoublebarrieroption.hpp>
#include <ored/portfolio/builders/fxdoubletouchoption.hpp>
#include <ored/portfolio/builders/fxforward.hpp>
#include <ored/portfolio/builders/fxoption.hpp>
#include <ored/portfolio/builders/fxtouchoption.hpp>
#include <ored/portfolio/builders/quantoequityoption.hpp>
#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/builders/swaption.hpp>
#include <ored/portfolio/builders/yoycapfloor.hpp>
#include <ored/portfolio/builders/varianceswap.hpp>
#include <ored/portfolio/durationadjustedcmslegbuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/legbuilders.hpp>
#include <ored/portfolio/commoditylegbuilder.hpp>
#include <ored/portfolio/equityfxlegbuilder.hpp>
#include <ored/utilities/log.hpp>

#include <boost/algorithm/string/join.hpp>

namespace ore {
namespace data {

namespace {
std::string getParameter(const std::map<std::string, std::string>& m, const std::string& p,
                         const std::vector<std::string>& qs, const bool mandatory, const std::string& defaultValue) {
    // first look for p_q if one or several qualifiers are given
    for (auto const& q : qs) {
        if (!q.empty()) {
            auto r = m.find(p + "_" + q);
            if (r != m.end())
                return r->second;
        }
    }
    // no qualifier given, or fall back on p because p_q was not found
    auto r = m.find(p);
    if (r != m.end()) {
        return r->second;
    }
    // if parameter is mandatory throw, otherwise return the default value
    if (mandatory) {
        QL_FAIL("parameter " << p << " not found (qualifier list was \"" << boost::algorithm::join(qs, ", ") << "\")");
    }
    return defaultValue;
}
} // namespace

std::string EngineBuilder::engineParameter(const std::string& p, const std::vector<std::string>& qualifiers,
                                           const bool mandatory, const std::string& defaultValue) const {
    return getParameter(engineParameters_, p, qualifiers, mandatory, defaultValue);
}

std::string EngineBuilder::modelParameter(const std::string& p, const std::vector<std::string>& qualifiers,
                                          const bool mandatory, const std::string& defaultValue) const {
    return getParameter(modelParameters_, p, qualifiers, mandatory, defaultValue);
}

EngineFactory::EngineFactory(const boost::shared_ptr<EngineData>& engineData, const boost::shared_ptr<Market>& market,
                             const map<MarketContext, string>& configurations,
                             const std::vector<boost::shared_ptr<EngineBuilder>> extraEngineBuilders,
                             const std::vector<boost::shared_ptr<LegBuilder>> extraLegBuilders,
                             const boost::shared_ptr<ReferenceDataManager>& referenceData,
                             const IborFallbackConfig& iborFallbackConfig)
    : market_(market), engineData_(engineData), configurations_(configurations), referenceData_(referenceData),
      iborFallbackConfig_(iborFallbackConfig) {
    LOG("Building EngineFactory");

    addDefaultBuilders();
    addExtraBuilders(extraEngineBuilders, extraLegBuilders);
}

void EngineFactory::registerBuilder(const boost::shared_ptr<EngineBuilder>& builder) {
    const string& modelName = builder->model();
    const string& engineName = builder->engine();
    builders_[make_tuple(modelName, engineName, builder->tradeTypes())] = builder;
}

boost::shared_ptr<EngineBuilder> EngineFactory::builder(const string& tradeType) {
    // Check that we have a model/engine for tradetype
    QL_REQUIRE(engineData_->hasProduct(tradeType),
               "No Pricing Engine configuration was provided for trade type " << tradeType);

    // Find a builder for the model/engine/tradeType
    const string& model = engineData_->model(tradeType);
    const string& engine = engineData_->engine(tradeType);
    typedef pair<tuple<string, string, set<string>>, boost::shared_ptr<EngineBuilder>> map_type;
    auto it =
        std::find_if(builders_.begin(), builders_.end(), [&model, &engine, &tradeType](const map_type& v) -> bool {
            const set<string>& types = std::get<2>(v.first);
            return std::get<0>(v.first) == model && std::get<1>(v.first) == engine &&
                   std::find(types.begin(), types.end(), tradeType) != types.end();
        });
    QL_REQUIRE(it != builders_.end(), "No EngineBuilder for " << model << "/" << engine << "/" << tradeType);

    boost::shared_ptr<EngineBuilder> builder = it->second;
    string effectiveTradeType = tradeType;
    if(auto db = boost::dynamic_pointer_cast<DelegatingEngineBuilder>(builder))
	effectiveTradeType = db->effectiveTradeType();

    builder->init(market_, configurations_, engineData_->modelParameters(effectiveTradeType),
                  engineData_->engineParameters(effectiveTradeType), engineData_->globalParameters());

    return builder;
}

void EngineFactory::registerLegBuilder(const boost::shared_ptr<LegBuilder>& legBuilder) {
    legBuilders_[legBuilder->legType()] = legBuilder;
}

boost::shared_ptr<LegBuilder> EngineFactory::legBuilder(const string& legType) {
    auto it = legBuilders_.find(legType);
    QL_REQUIRE(it != legBuilders_.end(), "No LegBuilder for " << legType);
    return it->second;
}

set<std::pair<string, boost::shared_ptr<ModelBuilder>>> EngineFactory::modelBuilders() const {
    set<std::pair<string, boost::shared_ptr<ModelBuilder>>> res;
    for (auto const& b : builders_) {
        res.insert(b.second->modelBuilders().begin(), b.second->modelBuilders().end());
    }
    return res;
}

void EngineFactory::addDefaultBuilders() {
    registerBuilder(boost::make_shared<SwapEngineBuilder>());
    registerBuilder(boost::make_shared<SwapEngineBuilderOptimised>());
    registerBuilder(boost::make_shared<CrossCurrencySwapEngineBuilder>());

    registerBuilder(boost::make_shared<EuropeanSwaptionEngineBuilder>());
    registerBuilder(boost::make_shared<LGMGridBermudanSwaptionEngineBuilder>());

    registerBuilder(boost::make_shared<FxForwardEngineBuilder>());
    registerBuilder(boost::make_shared<FxEuropeanOptionEngineBuilder>());
    registerBuilder(boost::make_shared<FxEuropeanCSOptionEngineBuilder>());
    registerBuilder(boost::make_shared<FxAmericanOptionFDEngineBuilder>());
    registerBuilder(boost::make_shared<FxAmericanOptionBAWEngineBuilder>());
    registerBuilder(boost::make_shared<FxEuropeanAsianOptionMCDAAPEngineBuilder>());
    registerBuilder(boost::make_shared<FxEuropeanAsianOptionMCDAASEngineBuilder>());
    registerBuilder(boost::make_shared<FxEuropeanAsianOptionMCDGAPEngineBuilder>());
    registerBuilder(boost::make_shared<FxEuropeanAsianOptionACGAPEngineBuilder>());
    registerBuilder(boost::make_shared<FxEuropeanAsianOptionADGAPEngineBuilder>());
    registerBuilder(boost::make_shared<FxEuropeanAsianOptionADGASEngineBuilder>());
    registerBuilder(boost::make_shared<FxEuropeanAsianOptionTWEngineBuilder>());
    registerBuilder(boost::make_shared<FxBarrierOptionAnalyticEngineBuilder>());
    registerBuilder(boost::make_shared<FxBarrierOptionFDEngineBuilder>());
    registerBuilder(boost::make_shared<FxDoubleBarrierOptionAnalyticEngineBuilder>());
    registerBuilder(boost::make_shared<FxTouchOptionEngineBuilder>());
    registerBuilder(boost::make_shared<FxDoubleTouchOptionAnalyticEngineBuilder>());
    registerBuilder(boost::make_shared<FxDigitalOptionEngineBuilder>());
    registerBuilder(boost::make_shared<FxDigitalCSOptionEngineBuilder>());
    registerBuilder(boost::make_shared<FxDigitalBarrierOptionEngineBuilder>());

    registerBuilder(boost::make_shared<CapFloorEngineBuilder>());
    registerBuilder(boost::make_shared<CapFlooredIborLegEngineBuilder>());
    registerBuilder(boost::make_shared<CapFlooredOvernightIndexedCouponLegEngineBuilder>());
    registerBuilder(boost::make_shared<CapFlooredAverageONIndexedCouponLegEngineBuilder>());
    registerBuilder(boost::make_shared<CapFlooredYoYLegEngineBuilder>());
    registerBuilder(boost::make_shared<CapFlooredNonStandardYoYLegEngineBuilder>());
    registerBuilder(boost::make_shared<CapFlooredCpiLegCouponEngineBuilder>());
    registerBuilder(boost::make_shared<CapFlooredCpiLegCashFlowEngineBuilder>());
    registerBuilder(boost::make_shared<CmsSpreadCouponPricerBuilder>());

    registerBuilder(boost::make_shared<CpiCapFloorEngineBuilder>());
    registerBuilder(boost::make_shared<YoYCapFloorEngineBuilder>());

    registerBuilder(boost::make_shared<EquityForwardEngineBuilder>());
    registerBuilder(boost::make_shared<EquityEuropeanOptionEngineBuilder>());
    registerBuilder(boost::make_shared<EquityEuropeanCompositeEngineBuilder>());
    registerBuilder(boost::make_shared<EquityEuropeanCSOptionEngineBuilder>());
    registerBuilder(boost::make_shared<EquityAmericanOptionFDEngineBuilder>());
    registerBuilder(boost::make_shared<EquityAmericanOptionBAWEngineBuilder>());
    registerBuilder(boost::make_shared<EquityEuropeanAsianOptionMCDAAPEngineBuilder>());
    registerBuilder(boost::make_shared<EquityEuropeanAsianOptionMCDAASEngineBuilder>());
    registerBuilder(boost::make_shared<EquityEuropeanAsianOptionMCDGAPEngineBuilder>());
    registerBuilder(boost::make_shared<EquityEuropeanAsianOptionACGAPEngineBuilder>());
    registerBuilder(boost::make_shared<EquityEuropeanAsianOptionADGAPEngineBuilder>());
    registerBuilder(boost::make_shared<EquityEuropeanAsianOptionADGASEngineBuilder>());
    registerBuilder(boost::make_shared<EquityEuropeanAsianOptionTWEngineBuilder>());
    registerBuilder(boost::make_shared<EquityFutureEuropeanOptionEngineBuilder>());
    registerBuilder(boost::make_shared<EquityBarrierOptionAnalyticEngineBuilder>());
    registerBuilder(boost::make_shared<EquityBarrierOptionFDEngineBuilder>());
    registerBuilder(boost::make_shared<EquityDoubleBarrierOptionAnalyticEngineBuilder>());
    registerBuilder(boost::make_shared<EquityDigitalOptionEngineBuilder>());
    registerBuilder(boost::make_shared<EquityDoubleTouchOptionAnalyticEngineBuilder>());
    registerBuilder(boost::make_shared<EquityTouchOptionEngineBuilder>());
    registerBuilder(boost::make_shared<VarSwapEngineBuilder>());
    

    registerBuilder(boost::make_shared<BondDiscountingEngineBuilder>());
    registerBuilder(boost::make_shared<DiscountingForwardBondEngineBuilder>());

    registerBuilder(boost::make_shared<AnalyticHaganCmsCouponPricerBuilder>());
    registerBuilder(boost::make_shared<NumericalHaganCmsCouponPricerBuilder>());
    registerBuilder(boost::make_shared<LinearTSRCmsCouponPricerBuilder>());
    registerBuilder(boost::make_shared<data::LinearTsrDurationAdjustedCmsCouponPricerBuilder>());

    registerBuilder(boost::make_shared<MidPointCdsEngineBuilder>());
    registerBuilder(boost::make_shared<BlackCdsOptionEngineBuilder>());
    registerBuilder(boost::make_shared<CommodityForwardEngineBuilder>());
    registerBuilder(boost::make_shared<CommodityEuropeanOptionEngineBuilder>());
    registerBuilder(boost::make_shared<CommodityEuropeanForwardOptionEngineBuilder>());
    registerBuilder(boost::make_shared<CommodityEuropeanCSOptionEngineBuilder>());
    registerBuilder(boost::make_shared<CommodityAmericanOptionFDEngineBuilder>());
    registerBuilder(boost::make_shared<CommodityAmericanOptionBAWEngineBuilder>());
    registerBuilder(boost::make_shared<CommodityEuropeanAsianOptionMCDAAPEngineBuilder>());
    registerBuilder(boost::make_shared<CommodityEuropeanAsianOptionMCDAASEngineBuilder>());
    registerBuilder(boost::make_shared<CommodityEuropeanAsianOptionMCDGAPEngineBuilder>());
    registerBuilder(boost::make_shared<CommodityEuropeanAsianOptionACGAPEngineBuilder>());
    registerBuilder(boost::make_shared<CommodityEuropeanAsianOptionADGAPEngineBuilder>());
    registerBuilder(boost::make_shared<CommodityEuropeanAsianOptionADGASEngineBuilder>());
    registerBuilder(boost::make_shared<CommodityEuropeanAsianOptionTWEngineBuilder>());
    registerBuilder(boost::make_shared<CommoditySwapEngineBuilder>());
    registerBuilder(boost::make_shared<CommoditySwaptionAnalyticalEngineBuilder>());
    registerBuilder(boost::make_shared<CommoditySwaptionMonteCarloEngineBuilder>());
    registerBuilder(boost::make_shared<CommodityApoAnalyticalEngineBuilder>());
    registerBuilder(boost::make_shared<CommodityApoMonteCarloEngineBuilder>());
    registerBuilder(boost::make_shared<QuantoEquityEuropeanOptionEngineBuilder>());
    registerBuilder(boost::make_shared<CommoditySpreadOptionEngineBuilder>());

    registerLegBuilder(boost::make_shared<DurationAdjustedCmsLegBuilder>());
    registerLegBuilder(boost::make_shared<FixedLegBuilder>());
    registerLegBuilder(boost::make_shared<ZeroCouponFixedLegBuilder>());
    registerLegBuilder(boost::make_shared<FloatingLegBuilder>());
    registerLegBuilder(boost::make_shared<CashflowLegBuilder>());
    registerLegBuilder(boost::make_shared<CPILegBuilder>());
    registerLegBuilder(boost::make_shared<YYLegBuilder>());
    registerLegBuilder(boost::make_shared<CMSLegBuilder>());
    registerLegBuilder(boost::make_shared<CMBLegBuilder>());
    registerLegBuilder(boost::make_shared<DigitalCMSLegBuilder>());
    registerLegBuilder(boost::make_shared<CMSSpreadLegBuilder>());
    registerLegBuilder(boost::make_shared<DigitalCMSSpreadLegBuilder>());
    registerLegBuilder(boost::make_shared<EquityLegBuilder>());
    registerLegBuilder(boost::make_shared<CommodityFixedLegBuilder>());
    registerLegBuilder(boost::make_shared<CommodityFloatingLegBuilder>());
    registerLegBuilder(boost::make_shared<EquityMarginLegBuilder>());
}

void EngineFactory::addExtraBuilders(const std::vector<boost::shared_ptr<EngineBuilder>> extraEngineBuilders,
                                     const std::vector<boost::shared_ptr<LegBuilder>> extraLegBuilders) {

    if (extraEngineBuilders.size() > 0) {
        DLOG("adding " << extraEngineBuilders.size() << " extra engine builders");
        for (auto eb : extraEngineBuilders)
            registerBuilder(eb);
    }
    if (extraLegBuilders.size() > 0) {
        DLOG("adding " << extraLegBuilders.size() << " extra leg builders");
        for (auto elb : extraLegBuilders)
            registerLegBuilder(elb);
    }
}

} // namespace data
} // namespace ore
