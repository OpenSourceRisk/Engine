/*
 Copyright (C) 2020 Quaternion Risk Management Ltd

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

#include <ored/portfolio/builders/convertiblebond.hpp>
#include <qle/models/defaultableequityjumpdiffusionmodel.hpp>
#include <qle/pricingengines/fddefaultableequityjumpdiffusionconvertiblebondengine.hpp>

#include <ored/portfolio/legdata.hpp>
#include <ored/utilities/dategrid.hpp>
#include <ored/utilities/marketdata.hpp>

#include <qle/indexes/compoequityindex.hpp>
#include <qle/termstructures/adjusteddefaultcurve.hpp>
#include <qle/termstructures/blacktriangulationatmvol.hpp>
#include <qle/termstructures/discountratiomodifiedcurve.hpp>
#include <qle/termstructures/flatcorrelation.hpp>
#include <qle/termstructures/hazardspreadeddefaulttermstructure.hpp>

#include <ql/math/functional.hpp>
#include <ql/quotes/derivedquote.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>

namespace ore {
namespace data {

using namespace ore::data;
using namespace QuantLib;
using namespace QuantExt;

std::string ConvertibleBondEngineBuilder::keyImpl(
    const std::string& id, const std::string& ccy, const std::string& creditCurveId, const bool hasCreditRisk,
    const std::string& securityId, const std::string& referenceCurveId, const bool isExchangeable,
    QuantLib::ext::shared_ptr<QuantExt::EquityIndex2> equity, const QuantLib::ext::shared_ptr<QuantExt::FxIndex>& fx,
    const std::string& equityCreditCurveId, const QuantLib::Date& startDate, const QuantLib::Date& maturityDate) {
    return id;
}

QuantLib::ext::shared_ptr<QuantLib::PricingEngine> ConvertibleBondFDDefaultableEquityJumpDiffusionEngineBuilder::engineImpl(
    const std::string& id, const std::string& ccy, const std::string& creditCurveId, const bool hasCreditRisk,
    const std::string& securityId, const std::string& referenceCurveId, const bool isExchangeable,
    QuantLib::ext::shared_ptr<QuantExt::EquityIndex2> equity, const QuantLib::ext::shared_ptr<QuantExt::FxIndex>& fx,
    const std::string& equityCreditCurveId, const QuantLib::Date& startDate, const QuantLib::Date& maturityDate) {

    std::string config = this->configuration(ore::data::MarketContext::pricing);

    // get pricing engine config

    Real p = parseReal(modelParameter("p", {}, true));
    Real eta = parseReal(modelParameter("eta", {}, true));
    bool staticMesher = parseBool(engineParameter("MesherIsStatic", {}, true));
    Size modelTimeStepsPerYear = parseInteger(engineParameter("Bootstrap.TimeStepsPerYear", {}, true));
    Size modelStateGridPoints = parseInteger(engineParameter("Bootstrap.StateGridPoints", {}, true));
    Real modelMesherEpsilon = parseReal(engineParameter("Bootstrap.MesherEpsilon", {}, true));
    Real modelMesherScaling = parseReal(engineParameter("Bootstrap.MesherScaling", {}, true));
    auto modelMesherConcentrationStr = engineParameter("Bootstrap.MesherConcentration", {}, false, "");
    Real modelMesherConcentration = Null<Real>();
    if (!modelMesherConcentrationStr.empty())
        modelMesherConcentration = parseReal(modelMesherConcentrationStr);
    auto bootstrapModeStr = engineParameter("Bootstrap.Mode", {}, true);
    DefaultableEquityJumpDiffusionModelBuilder::BootstrapMode bootstrapMode;
    if (bootstrapModeStr == "Alternating") {
        bootstrapMode = DefaultableEquityJumpDiffusionModelBuilder::BootstrapMode::Alternating;
    } else if (bootstrapModeStr == "Simultaneously") {
        bootstrapMode = DefaultableEquityJumpDiffusionModelBuilder::BootstrapMode::Simultaneously;
    } else {
        QL_FAIL("invalid Bootstrap.Mode, expected Alternating or Simultaneously");
    }
    Size engineTimeStepsPerYear = parseInteger(engineParameter("Pricing.TimeStepsPerYear", {}, true));
    Size engineStateGridPoints = parseInteger(engineParameter("Pricing.StateGridPoints", {}, true));
    Real engineMesherEpsilon = parseReal(engineParameter("Pricing.MesherEpsilon", {}, true));
    Real engineMesherScaling = parseReal(engineParameter("Pricing.MesherScaling", {}, true));
    std::vector<Real> conversionRatioDiscretisationGrid =
        parseListOfValues<Real>(engineParameter("Pricing.ConversionRatioDiscretisationGrid", {}, true), &parseReal);

    // technical parameters, we might want to review these an replace by less technical settings

    bool adjustDiscounting = parseBool(modelParameter("AdjustDiscounting", {}, false, "true"));
    bool adjustEquityVolatility = parseBool(modelParameter("AdjustEquityVolatility", {}, false, "true"));
    bool adjustEquityForward = parseBool(modelParameter("AdjustEquityForward", {}, false, "true"));
    bool adjustCreditSpreadToRR = parseBool(modelParameter("AdjustCreditSpreadToRR", {}, false, "true"));
    bool zeroRecoveryOverwrite = parseBool(modelParameter("ZeroRecoveryOverwrite", {}, false, "false"));
    bool treatSecuritySpreadAsCreditSpread =
        parseBool(modelParameter("TreatSecuritySpreadAsCreditSpread", {}, false, "false"));

    // get equity curve and volatility

    std::string equityName;
    Handle<BlackVolTermStructure> volatility;

    if (equity != nullptr) {
        equityName = equity->name();
        volatility = market_->equityVol(equityName, config);
    } else {
        // create dummy equity and zero volatility if either of these are left empty (this is used for fixed amount
        // conversion)
        equity = QuantLib::ext::make_shared<QuantExt::EquityIndex2>(
            "dummyFamily", NullCalendar(), fx == nullptr ? parseCurrency(ccy) : fx->sourceCurrency(),
            Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.0)),
            Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.0, Actual365Fixed())),
            Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.0, Actual365Fixed())));

        volatility = Handle<BlackVolTermStructure>(QuantLib::ext::make_shared<BlackConstantVol>(
            0, NullCalendar(), Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.0)), Actual365Fixed()));
    }

    // get bond related curves

    Handle<YieldTermStructure> referenceCurve;
    if (adjustDiscounting)
        referenceCurve = market_->yieldCurve(referenceCurveId, config);

    Handle<DefaultProbabilityTermStructure> creditCurve;
    Real creditCurveRecovery = 0.0;
    if (!creditCurveId.empty() && hasCreditRisk) {
        creditCurve = securitySpecificCreditCurve(market_, securityId, creditCurveId, config)->curve();
        creditCurveRecovery = market_->recoveryRate(creditCurveId, config)->value();
    } else {
        if (!creditCurveId.empty()) {
            securitySpecificCreditCurve(market_, securityId, creditCurveId, config);
            market_->recoveryRate(creditCurveId, config)->value();
        }
        creditCurve = Handle<DefaultProbabilityTermStructure>(
            QuantLib::ext::make_shared<FlatHazardRate>(0, NullCalendar(), 0.0, Actual365Fixed()));
    }

    // get (bond) recovery rate, fallback is the recovery rate of the credit curve

    Handle<Quote> recovery;
    if (!zeroRecoveryOverwrite) {
        try {
            recovery = market_->recoveryRate(securityId, config);
        } catch (...) {
            if (!creditCurveId.empty() && hasCreditRisk) {
                recovery = market_->recoveryRate(creditCurveId, config);
            } else if (!creditCurveId.empty()) {
                market_->recoveryRate(creditCurveId, config);
            }
        }
    }

    // get security spread

    Handle<Quote> spread;
    if (adjustDiscounting || treatSecuritySpreadAsCreditSpread) {
        try {
            spread = market_->securitySpread(securityId, config);
        } catch (...) {
            spread = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.0));
        }
    }

    // for exchangeables get equity credit curve

    Handle<DefaultProbabilityTermStructure> equityCreditCurve;
    Real equityCreditCurveRecovery = 0.0;
    if (isExchangeable) {
        if (!equityCreditCurveId.empty() && hasCreditRisk) {
            equityCreditCurve = market_->defaultCurve(equityCreditCurveId, config)->curve();
            equityCreditCurveRecovery = market_->recoveryRate(equityCreditCurveId, config)->value();
        } else {
            if (!equityCreditCurveId.empty()) {
                market_->defaultCurve(equityCreditCurveId, config);
                market_->recoveryRate(equityCreditCurveId, config)->value();
            }
            equityCreditCurve = Handle<DefaultProbabilityTermStructure>(
                QuantLib::ext::make_shared<FlatHazardRate>(0, NullCalendar(), 0.0, Actual365Fixed()));
        }
    }

    // if adjust credit spread to recovery rate is enabled, build the adjusted curves

    if (adjustCreditSpreadToRR) {
        Real rr = recovery.empty() ? 0.0 : recovery->value();
        creditCurve = Handle<DefaultProbabilityTermStructure>(QuantLib::ext::make_shared<AdjustedDefaultCurve>(
            creditCurve, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>((1.0 - creditCurveRecovery) / (1.0 - rr)))));
        if (!equityCreditCurve.empty()) {
            equityCreditCurve = Handle<DefaultProbabilityTermStructure>(QuantLib::ext::make_shared<AdjustedDefaultCurve>(
                equityCreditCurve,
                Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>((1.0 - equityCreditCurveRecovery) / (1.0 - rr)))));
        }
    }

    // if treat security spread as credit spread is active, add the (scaled) security spread to the curves

    if (treatSecuritySpreadAsCreditSpread) {
        Real rr = recovery.empty() ? 0.0 : recovery->value();
	auto m = [rr](Real x) { return x / ( 1.0 -rr); };
        Handle<Quote> scaledSecuritySpread(QuantLib::ext::make_shared<DerivedQuote<decltype(m)>>(spread, m));
        creditCurve = Handle<DefaultProbabilityTermStructure>(
            QuantLib::ext::make_shared<HazardSpreadedDefaultTermStructure>(creditCurve, scaledSecuritySpread));
        if (!equityCreditCurve.empty()) {
            equityCreditCurve = Handle<DefaultProbabilityTermStructure>(
                QuantLib::ext::make_shared<HazardSpreadedDefaultTermStructure>(equityCreditCurve, scaledSecuritySpread));
        }
    }

    // for cross currency, set up converted equity index and eq vol

    if (fx != nullptr) {
        auto fxVol = market_->fxVol(equity->currency().code() + ccy, config);
        QuantLib::Handle<QuantExt::CorrelationTermStructure> corrCurve(
            QuantLib::ext::make_shared<FlatCorrelation>(0, NullCalendar(), 0.0, Actual365Fixed()));
        try {
            corrCurve = market_->correlationCurve("FX-GENERIC-" + equity->currency().code() + "-" + ccy,
                                                  "EQ-" + equity->name());
        } catch (...) {
            WLOG("correlation curve for FX-GENERIC-" + equity->currency().code()
                 << ", EQ-" << equity->name() << " not found, fall back to zero correlation.");
        }
        equity = QuantLib::ext::make_shared<CompoEquityIndex>(equity, fx, startDate);
        volatility = Handle<BlackVolTermStructure>(QuantLib::ext::make_shared<BlackTriangulationATMVolTermStructure>(
            volatility, fxVol, corrCurve, parseBool(engineParameter("FxVolIsStatic", {}, false, "false"))));
    }

    // set up calibration grid

    std::vector<Date> calibrationDates = DateGrid(engineParameter("Bootstrap.CalibrationGrid", {}, true)).dates();
    std::vector<Real> calibrationTimes;
    Date today = Settings::instance().evaluationDate();
    for (Size i = 0; i < calibrationDates.size(); ++i) {
        if (calibrationDates[i] < maturityDate) {
            calibrationTimes.push_back(!volatility.empty() ? volatility->timeFromReference(calibrationDates[i])
                                                           : Actual365Fixed().yearFraction(today, calibrationDates[i]));
        }
    }
    calibrationTimes.push_back(!volatility.empty() ? volatility->timeFromReference(maturityDate)
                                                   : Actual365Fixed().yearFraction(today, maturityDate));

    // read global parameters

    bool calibrate = true;
    auto calParam = globalParameters_.find("Calibrate");
    if (calParam != globalParameters_.end()) {
        calibrate = parseBool(calParam->second);
    }

    bool generateAdditionalResults = false;
    auto genAddParam = globalParameters_.find("GenerateAdditionalResults");
    if (genAddParam != globalParameters_.end()) {
        generateAdditionalResults = parseBool(genAddParam->second);
    }

    // set up model and pricing engine

    auto modelBuilder = QuantLib::ext::make_shared<DefaultableEquityJumpDiffusionModelBuilder>(
        calibrationTimes, equity, volatility, isExchangeable ? equityCreditCurve : creditCurve, p, eta, staticMesher,
        modelTimeStepsPerYear, modelStateGridPoints, modelMesherEpsilon, modelMesherScaling, modelMesherConcentration,
        bootstrapMode, false, calibrate, adjustEquityVolatility, adjustEquityForward);

    modelBuilders_.insert(std::make_pair(id, modelBuilder));

    return QuantLib::ext::make_shared<FdDefaultableEquityJumpDiffusionConvertibleBondEngine>(
        modelBuilder->model(), referenceCurve, treatSecuritySpreadAsCreditSpread ? Handle<Quote>() : spread,
        isExchangeable ? creditCurve : Handle<DefaultProbabilityTermStructure>(), recovery, Handle<FxIndex>(fx),
        staticMesher, engineTimeStepsPerYear, engineStateGridPoints, engineMesherEpsilon, engineMesherScaling,
        conversionRatioDiscretisationGrid, generateAdditionalResults);
}

} // namespace data
} // namespace ore
