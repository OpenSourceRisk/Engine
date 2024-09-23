/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file portfolio/builders/forwardbond.hpp
    \brief Engine builder for forward bonds
    \ingroup builders
*/

#pragma once

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/utilities/marketdata.hpp>

#include <qle/instruments/impliedbondspread.hpp>
#include <qle/pricingengines/discountingforwardbondengine.hpp>
#include <qle/pricingengines/discountingriskybondengine.hpp>

#include <ql/termstructures/yield/zerospreadedtermstructure.hpp>

#include <boost/make_shared.hpp>

namespace ore {
namespace data {

class FwdBondEngineBuilder
    : public CachingPricingEngineBuilder<string, const string&, const Currency&, const string&, const string&,
                                         const string&, const string&, const string&, const bool> {
protected:
    FwdBondEngineBuilder(const std::string& model, const std::string& engine)
        : CachingEngineBuilder(model, engine, {"ForwardBond"}) {}

    virtual string keyImpl(const string& id, const Currency& ccy, const std::string& discountCurveName,
                           const string& creditCurveId, const string& securityId, const string& referenceCurveId,
                           const string& incomeCurveId, const bool dirty) override {

        // id is _not_ part of the key
        std::string returnString = ccy.code() + "_" + creditCurveId + "_"  + securityId +
                                   "_" + referenceCurveId + "_" + incomeCurveId;

        return returnString;
    }

    void setCurves(const string& id, const Currency& ccy, const std::string& discountCurveName,
                   const string& creditCurveId, const string& securityId, const string& referenceCurveId,
                   const string& incomeCurveId, const bool dirty);

    Handle<YieldTermStructure> referenceCurve_;
    Handle<Quote> bondSpread_;
    Handle<YieldTermStructure> spreadedReferenceCurve_;
    Handle<YieldTermStructure> discountCurve_;
    Handle<YieldTermStructure> incomeCurve_;
    bool spreadOnIncome_;
    Handle<Quote> conversionFactor_;
    // not used in AMC yet
    Handle<DefaultProbabilityTermStructure> dpts_;
    Handle<Quote> recovery_;
    };

class DiscountingForwardBondEngineBuilder : public FwdBondEngineBuilder {
public:
    DiscountingForwardBondEngineBuilder()
        : FwdBondEngineBuilder("DiscountedCashflows", "DiscountingForwardBondEngine") {}

protected:
    virtual QuantLib::ext::shared_ptr<PricingEngine>
    engineImpl(const string& id, const Currency& ccy, const std::string& discountCurveName, const string& creditCurveId,
               const string& securityId, const string& referenceCurveId, const string& incomeCurveId,
               const bool dirty) override {

        string tsperiodStr = engineParameters_.at("TimestepPeriod");
        Period tsperiod = parsePeriod(tsperiodStr);

        setCurves(id, ccy, discountCurveName, creditCurveId, hasCreditRisk, securityId, referenceCurveId, incomeCurveId,
                  dirty);

        return QuantLib::ext::make_shared<QuantExt::DiscountingForwardBondEngine>(
            discountCurve_, incomeCurve_, spreadedReferenceCurve_, bondSpread_, dpts_, recovery_, conversionFactor_,
            tsperiod);
    }
};

class CamAmcFwdBondEngineBuilder : public FwdBondEngineBuilder {
public:
    CamAmcFwdBondEngineBuilder(const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel>& cam,
                               const std::vector<Date>& simulationDates, const std::vector<Date>& stickyCloseOutDates)
        : FwdBondEngineBuilder("CrossAssetModel", "AMC"), cam_(cam), simulationDates_(simulationDates),
          stickyCloseOutDates_(stickyCloseOutDates) {}

protected:
    // the pricing engine depends on the ccy only, can use the caching from SwapEngineBuilderBase
    virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& id, const Currency& ccy,
                                                                const std::string& discountCurveName,
                                                                const string& creditCurveId, const string& securityId,
                                                                const string& referenceCurveId,
                                                                const string& incomeCurveId, const bool dirty) override;

private:
    QuantLib::ext::shared_ptr<PricingEngine> buildMcEngine(const QuantLib::ext::shared_ptr<QuantExt::LGM>& lgm,
                                                           const Handle<YieldTermStructure>& discountCurve,
                                                           const std::vector<Date>& simulationDates,
                                                           const std::vector<Size>& externalModelIndices,
                                                           const Handle<YieldTermStructure>& incomeCurve,
                                                           const Handle<YieldTermStructure>& discountContractCurve,
                                                           const Handle<YieldTermStructure>& referenceCurve,
                                                           const Handle<Quote>& conversionFactor);
    const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel> cam_;
    const std::vector<Date> simulationDates_;
    const std::vector<Date> stickyCloseOutDates_;
};

} // namespace data
} // namespace ore
