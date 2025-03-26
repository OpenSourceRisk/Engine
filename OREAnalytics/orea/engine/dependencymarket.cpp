/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

#include <orea/engine/dependencymarket.hpp>

#include <orea/scenario/scenariosimmarket.hpp>

#include <ored/configuration/correlationcurveconfig.hpp>
#include <ored/marketdata/basecorrelationcurve.hpp>
#include <ored/utilities/conventionsbasedfutureexpiry.hpp>
#include <ored/utilities/indexnametranslator.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/parsers.hpp>

#include <qle/indexes/fallbackiborindex.hpp>
#include <qle/indexes/fallbackovernightindex.hpp>
#include <qle/indexes/inflationindexwrapper.hpp>
#include <qle/termstructures/commoditybasispricecurvewrapper.hpp>
#include <qle/termstructures/flatcorrelation.hpp>
#include <qle/termstructures/pricecurve.hpp>
#include <qle/termstructures/zeroinflationcurveobservermoving.hpp>
#include <qle/termstructures/yoyinflationcurveobservermoving.hpp>

#include <ql/currencies/europe.hpp>
#include <ql/indexes/ibor/eonia.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/volatility/inflation/constantcpivolatility.hpp>
#include <ql/termstructures/volatility/inflation/yoyinflationoptionletvolatilitystructure.hpp>
#include <ql/termstructures/volatility/optionlet/constantoptionletvol.hpp>
#include <ql/termstructures/volatility/swaption/swaptionconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <ql/time/daycounters/actualactual.hpp>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/make_shared.hpp>

using QuantExt::EquityIndex2;
using QuantExt::FxIndex;
using QuantExt::CreditCurve;
using QuantExt::CreditVolCurve;
using QuantExt::InterpolatedPriceCurve;
using QuantExt::ZeroInflationCurveObserverMoving;
using QuantExt::YoYInflationCurveObserverMoving;
using QuantExt::PriceTermStructure;
using ore::data::MarketObject;
using ore::data::parseCommodityIndex;

namespace ore {
namespace analytics {

string DependencyMarket::ccyPair(const string& pair) const {
    QL_REQUIRE(pair.size() == 6, "Invalid ccypair " << pair);
    string ccy1 = pair.substr(0, 3);
    string ccy2 = pair.substr(3);
    return useFxDominance_ ? ore::data::fxDominance(ccy1, ccy2) : pair;
}

Handle<YieldTermStructure> DependencyMarket::flatRateYts() const {
    // All dummy yield term structures based on flat rate of 1%
    QuantLib::ext::shared_ptr<YieldTermStructure> yts(
        new FlatForward(asofDate(), 0.01, ActualActual(ActualActual::ISDA)));
    return Handle<YieldTermStructure>(yts);
}

Handle<PriceTermStructure> DependencyMarket::flatRatePts(const QuantLib::Currency& ccy) const {
    // Make flat ts
    std::vector<QuantLib::Period> times{1 * Days, 1 * Years};
    std::vector<QuantLib::Real> prices{10, 10};
    Currency commodityCcy;
    QuantLib::ext::shared_ptr<PriceTermStructure> pts(
        new InterpolatedPriceCurve<QuantLib::Linear>(times, prices, ActualActual(ActualActual::ISDA), ccy));
    pts->enableExtrapolation();
    return Handle<PriceTermStructure>(pts);
}

Handle<QuantLib::SwaptionVolatilityStructure> DependencyMarket::flatRateSvs() const {
    // All dummy swaption volatilities are Normal with 10bp
    QuantLib::ext::shared_ptr<QuantLib::SwaptionVolatilityStructure> svs(new QuantLib::ConstantSwaptionVolatility(
        asofDate(), NullCalendar(), ModifiedFollowing, 0.0010, ActualActual(ActualActual::ISDA), Normal));
    return Handle<QuantLib::SwaptionVolatilityStructure>(svs);
}

Handle<OptionletVolatilityStructure> DependencyMarket::flatRateCvs() const {
    // All dummy optionlet volatilities are Normal with 10bp
    QuantLib::ext::shared_ptr<OptionletVolatilityStructure> ts(new QuantLib::ConstantOptionletVolatility(
        asofDate(), NullCalendar(), ModifiedFollowing, 0.0010, ActualActual(ActualActual::ISDA), Normal));
    return Handle<OptionletVolatilityStructure>(ts);
}

Handle<BlackVolTermStructure> DependencyMarket::flatRateFxv() const {
    // All dummy FX volatilities are 10%
    QuantLib::ext::shared_ptr<BlackVolTermStructure> fxv(
        new BlackConstantVol(asofDate(), NullCalendar(), 0.10, ActualActual(ActualActual::ISDA)));
    return Handle<BlackVolTermStructure>(fxv);
}

Handle<CreditCurve> DependencyMarket::flatRateDcs(Real forward) const {
    QuantLib::ext::shared_ptr<DefaultProbabilityTermStructure> dcs(
        new FlatHazardRate(asofDate(), forward, ActualActual(ActualActual::ISDA)));
    return Handle<CreditCurve>(
        QuantLib::ext::make_shared<QuantExt::CreditCurve>(Handle<DefaultProbabilityTermStructure>(dcs), flatRateYts(),
                                                  Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.40))));
}

Handle<CPICapFloorTermPriceSurface> DependencyMarket::flatRateCps(Handle<ZeroInflationIndex> infIndex) const {

    std::vector<Rate> cStrikes = {0.0, 0.01, 0.02};
    std::vector<Rate> fStrikes = {-1.0, -0.99};
    std::vector<Period> cfMaturities = {5 * Years, 10 * Years};
    Matrix cPrice(3, 2, 0.15);
    Matrix fPrice(2, 2, 0.15);
    QuantLib::ext::shared_ptr<CPICapFloorTermPriceSurface> ts(new InterpolatedCPICapFloorTermPriceSurface<QuantLib::Bilinear>(
        1.0, 0.0, infIndex->availabilityLag(), infIndex->zeroInflationTermStructure()->calendar(), Following,
        ActualActual(ActualActual::ISDA), infIndex.currentLink(), CPI::AsIndex,
        discountCurve(infIndex->currency().code()), cStrikes, fStrikes, cfMaturities, cPrice, fPrice));
    return Handle<CPICapFloorTermPriceSurface>(ts);
}

Handle<QuantLib::CPIVolatilitySurface> DependencyMarket::flatRateCPIvs(Handle<ZeroInflationIndex> infIndex) const {
    QuantLib::ext::shared_ptr<ConstantCPIVolatility> ts(new ConstantCPIVolatility(0.1, 2, infIndex->fixingCalendar(), 
        Following, ActualActual(ActualActual::ISDA), Period(3 * Months), infIndex->frequency(),
        false));
    return Handle<QuantLib::CPIVolatilitySurface>(ts);
}

Handle<YoYOptionletVolatilitySurface> DependencyMarket::flatRateYoYvs(Handle<YoYInflationIndex> infIndex) const {
    QuantLib::ext::shared_ptr<ConstantYoYOptionletVolatility> ts(
        new ConstantYoYOptionletVolatility(0.1, 2, infIndex->fixingCalendar(), Following, ActualActual(ActualActual::ISDA),
                                  infIndex->availabilityLag(), infIndex->frequency(), infIndex->interpolated()));
    return Handle<YoYOptionletVolatilitySurface>(ts);
}

Handle<YieldTermStructure> DependencyMarket::yieldCurve(const ore::data::YieldCurveType& ycType, const string& name,
                                                        const string& config) const {
    // ibor indices (not convention based) are allowed as keys, handle this first
    // FIXME: why not conventions based indices? 
    QuantLib::ext::shared_ptr<IborIndex> notUsed;
    //if (tryParseIborIndex(name, notUsed, false)) {
    if (tryParseIborIndex(name, notUsed)) {
        return iborIndex(name, config)->forwardingTermStructure();
    }
    // we have a genuine yield curve
    RiskFactorKey::KeyType type = yieldCurveRiskFactor(ycType);
    addRiskFactor(type, name);
    addMarketObject(MarketObject::YieldCurve, name, config);
    return flatRateYts();
}

Handle<YieldTermStructure> DependencyMarket::discountCurveImpl(const string& ccy, const string& config) const {
    addRiskFactor(RiskFactorKey::KeyType::DiscountCurve, ccy);
    addMarketObject(MarketObject::DiscountCurve, ccy, config);
    if (ccy != baseCcy_) {
        addRiskFactor(RiskFactorKey::KeyType::FXSpot, ccy + baseCcy_);
        addMarketObject(MarketObject::FXSpot, baseCcy_ + ccy, config);
    }
    return flatRateYts();
}

Handle<YieldTermStructure> DependencyMarket::yieldCurve(const string& name, const string& config) const {
    using ore::data::xccyCurveNamePrefix;
    if (boost::starts_with(name, xccyCurveNamePrefix)) {
        // If the yield curve is a reserved internal cross currency yield curve, fail here so that we fall back on
        // the discount curves. We will add these yield curves manually where we need them.
        DLOG("The yield curve name " << name << " starts with the reserved xccy prefix " << xccyCurveNamePrefix
                                     << " so dependency market intentionally fails to return a term structure here.");
        QL_FAIL("Dependency market returns nothing for internal cross currency yield curves.");
    } else {
        return yieldCurve(YieldCurveType::Yield, name, config);
    }
}

Handle<IborIndex> DependencyMarket::iborIndex(const string& name, const string& config) const {

    // Expect ibor index name to be of the form CCY-INDEX[-TENOR]
    QL_REQUIRE(name.length() > 3, "Expected ibor index name to be of form CCY-INDEX[-TENOR] but got '" << name << "'");

    // Get a dummy forwarding term structure to pass to the ibor index parser
    string ccy = name.substr(0, 3);
    DLOG("Parsing '" << ccy << "' to check that we have a valid currency");
    parseCurrency(ccy);
    Handle<YieldTermStructure> yts = discountCurve(ccy, Market::defaultConfiguration);
    QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();
    QuantLib::ext::shared_ptr<IborIndex> iip = ore::data::parseIborIndex(name, yts);
    Handle<IborIndex> ii(iip);

    addRiskFactor(RiskFactorKey::KeyType::IndexCurve, name);
    addMarketObject(MarketObject::IndexCurve, name, config);

    // For an ibor fallback index, add its rfr index, if the index is replaced on the asof date.
    // FIXME The dependency market does not have an asof date currently, so we have to assume that the
    // results of the dependency market are used w.r.t. the global evaluation date only. This holds
    // for our main use case of the configuration builder using the dependency market via the portfolio
    // analyser.

    if (iborFallbackConfig_.isIndexReplaced(name, asofDate())) {
        auto rfrName = iborFallbackConfig_.fallbackData(name).rfrIndex;
        addRiskFactor(RiskFactorKey::KeyType::IndexCurve, rfrName);
        addMarketObject(MarketObject::IndexCurve, rfrName, config);
        // we don't support convention based indices here, this might change with ore ticket 1758
        auto oi = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(parseIborIndex(rfrName, yts));
        QL_REQUIRE(oi != nullptr, "DependencyMarket::iborIndex(): could not cast rfr index '"
                                      << rfrName << "' to OvernightIndex, this is unexpected.");
        auto fallbackData = iborFallbackConfig_.fallbackData(name);
	if (auto original = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(iip))
	    ii = Handle<IborIndex>(QuantLib::ext::make_shared<QuantExt::FallbackOvernightIndex>(original, oi, fallbackData.spread,
                                                                               fallbackData.switchDate, false));
	else
	    ii = Handle<IborIndex>(QuantLib::ext::make_shared<QuantExt::FallbackIborIndex>(*ii, oi, fallbackData.spread,
                                                                               fallbackData.switchDate, false));
        DLOG("Adding rfr fallback index '" << rfrName << "' for ibor index '" << name << "'");
    }

    return ii;
}

Handle<SwapIndex> DependencyMarket::swapIndex(const string& name, const string& config) const {

    // Expect swap index name to be of the form CCY-CMS-TENOR
    QL_REQUIRE(name.length() > 3, "Expected swap index name to be of form CCY-CMS-TENOR but got '" << name << "'");

    // Get a dummy discount and forwarding term structure to pass to the swap index parser
    string ccy = name.substr(0, 3);
    DLOG("Parsing '" << ccy << "' to check that we have a valid currency");
    parseCurrency(ccy);
    Handle<YieldTermStructure> yts = discountCurve(ccy, Market::defaultConfiguration);
    auto swapIndex = ore::data::parseSwapIndex(name, yts, yts);
    swapindices_.insert(name);
    addMarketObject(MarketObject::SwapIndexCurve, name, config);
    return Handle<SwapIndex>(swapIndex);
}

Handle<SwaptionVolatilityStructure> DependencyMarket::swaptionVol(const string& name, const string& config) const {
    addRiskFactor(RiskFactorKey::KeyType::SwaptionVolatility, name);
    addMarketObject(MarketObject::SwaptionVol, name, config);
    return flatRateSvs();
}

string DependencyMarket::shortSwapIndexBase(const string& key, const string&) const {
    QuantLib::ext::shared_ptr<IborIndex> index;
    string ccy = tryParseIborIndex(key, index) ? index->currency().code() : key;
    // try to get the index base from the curve configs
    if (curveConfigs_) {
        QuantLib::ext::shared_ptr<SwaptionVolatilityCurveConfig> cc;
        if (curveConfigs_->hasSwaptionVolCurveConfig(key)) {
            cc = curveConfigs_->swaptionVolCurveConfig(key);
        } else if (curveConfigs_->hasSwaptionVolCurveConfig(ccy)) {
            cc = curveConfigs_->swaptionVolCurveConfig(ccy);
        }
        if (cc && !cc->shortSwapIndexBase().empty())
            return cc->shortSwapIndexBase();
    }
    // default
    return ccy + "-CMS-1Y";
}

string DependencyMarket::swapIndexBase(const string& key, const string&) const {
    QuantLib::ext::shared_ptr<IborIndex> index;
    string ccy = tryParseIborIndex(key, index) ? index->currency().code() : key;
    // try to get the index base from the curve configs
    if (curveConfigs_) {
        QuantLib::ext::shared_ptr<SwaptionVolatilityCurveConfig> cc;
        if (curveConfigs_->hasSwaptionVolCurveConfig(key)) {
            cc = curveConfigs_->swaptionVolCurveConfig(key);
        } else if (curveConfigs_->hasSwaptionVolCurveConfig(ccy)) {
            cc = curveConfigs_->swaptionVolCurveConfig(ccy);
        }
        if (cc && !cc->swapIndexBase().empty())
            return cc->swapIndexBase();
    }
    // default
    return ccy + "-CMS-30Y";
}

Handle<SwaptionVolatilityStructure> DependencyMarket::yieldVol(const string& name, const string& config) const {
    addRiskFactor(RiskFactorKey::KeyType::YieldVolatility, name);
    addMarketObject(MarketObject::YieldVol, name, config);
    return flatRateSvs();
}

QuantLib::Handle<QuantExt::FxIndex> DependencyMarket::fxIndexImpl(const string& fxIndex, const string& config) const{
    string ccy1, ccy2, famName;
    if (isFxIndex(fxIndex)) {
        auto fxIndexBase = parseFxIndex(fxIndex);
        ccy1 = fxIndexBase->sourceCurrency().code();
        ccy2 = fxIndexBase->targetCurrency().code();
        famName = fxIndexBase->familyName();
    } else {
        ccy1 = fxIndex.substr(0, 3);
        ccy2 = fxIndex.substr(3);
        famName = fxIndex;
    }

    string adjpair = ccyPair(ccy1 + ccy2);
    if (ccy1 != ccy2) {
        addRiskFactor(RiskFactorKey::KeyType::FXSpot, adjpair);
        addMarketObject(MarketObject::FXSpot, adjpair, config);
    }
    auto sorTS = discountCurve(ccy1, config);
    auto tarTS = discountCurve(ccy2, config);

    // use correct conventions so correct fixings are picked up
    auto [spotDays, calendar, bdc] = getFxIndexConventions(adjpair);

    return Handle<FxIndex>(QuantLib::ext::make_shared<FxIndex>(famName, spotDays, parseCurrency(ccy1), parseCurrency(ccy2),
        calendar, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.0)), sorTS, tarTS));
}

Handle<Quote> DependencyMarket::fxRateImpl(const string& ccypair, const string& config) const {
    fxIndex(ccypair, config);
    return Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.0));
}

Handle<Quote> DependencyMarket::fxSpotImpl(const string& ccypair, const string& config) const {
    fxIndex(ccypair, config);
    return Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.0));
}

Handle<BlackVolTermStructure> DependencyMarket::fxVolImpl(const string& ccypair, const string& config) const {

    // scenario simmarket requires an FXSpot for every FXVol
    fxRate(ccypair, config);

    // And a discount curve for every ccy
    string ccy1 = ccypair.substr(0, 3);
    discountCurve(ccy1, config);
    string ccy2 = ccypair.substr(3);
    discountCurve(ccy2, config);

    string adjpair = ccyPair(ccypair);
    string revpair = adjpair.substr(3) + adjpair.substr(0,3);
    // if we have a curve config for the reverse pair instead of the adjusted pair, we'll choose this
    if (curveConfigs_ && !curveConfigs_->hasFxVolCurveConfig(adjpair) && curveConfigs_->hasFxVolCurveConfig(revpair))
        adjpair = revpair;

    addRiskFactor(RiskFactorKey::KeyType::FXVolatility, adjpair);
    addMarketObject(MarketObject::FXVol, adjpair, config);
    return flatRateFxv();
}

Handle<CreditCurve> DependencyMarket::defaultCurve(const string& name, const string& config) const {
    string tmp = recordSecuritySpecificCreditCurves_ ? name : creditCurveNameFromSecuritySpecificCreditCurveName(name);
    addRiskFactor(RiskFactorKey::KeyType::SurvivalProbability, tmp);
    addMarketObject(MarketObject::DefaultCurve, tmp, config);
    return flatRateDcs(0.01);
}

Handle<Quote> DependencyMarket::recoveryRate(const string& name, const string&) const {
    addRiskFactor(RiskFactorKey::KeyType::RecoveryRate, name);
    return Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.0));
}

Handle<Quote> DependencyMarket::conversionFactor(const string& name, const string& config) const {
    addRiskFactor(RiskFactorKey::KeyType::ConversionFactor, name);
    addMarketObject(MarketObject::Security, name, config);
    return Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.0));
}

Handle<CreditVolCurve> DependencyMarket::cdsVol(const string& name, const string& config) const {
    addRiskFactor(RiskFactorKey::KeyType::CDSVolatility, name);
    addMarketObject(MarketObject::CDSVol, name, config);
    return Handle<QuantExt::CreditVolCurve>(QuantLib::ext::make_shared<QuantExt::CreditVolCurveWrapper>(flatRateFxv()));
}

Handle<QuantExt::BaseCorrelationTermStructure> DependencyMarket::baseCorrelation(const string& name,
                                                                               const string& config) const {
    addRiskFactor(RiskFactorKey::KeyType::BaseCorrelation, name);
    addMarketObject(MarketObject::BaseCorrelation, name, config);

    vector<vector<Handle<Quote>>> correls(2);
    Handle<Quote> sq1(QuantLib::ext::make_shared<SimpleQuote>(0.1));
    vector<Handle<Quote>> qt(2, sq1);
    correls[0] = qt;
    correls[1] = qt;

    return Handle<QuantExt::BaseCorrelationTermStructure>(
        QuantLib::ext::make_shared<QuantExt::InterpolatedBaseCorrelationTermStructure<Bilinear>>(
        0, NullCalendar(), Following, vector<Period>({1 * Days, 2 * Days}), vector<Real>({0.03, 0.06}), correls,
        ActualActual(ActualActual::ISDA)));
}

Handle<OptionletVolatilityStructure> DependencyMarket::capFloorVol(const string& name, const string& config) const {
    addRiskFactor(RiskFactorKey::KeyType::OptionletVolatility, name);
    addMarketObject(MarketObject::CapFloorVol, name, config);
    // ensure that the dependent ibor indiex is captured
    iborIndex(capFloorVolIndexBase(name, config).first, config);
    return flatRateCvs();
}

std::pair<string, QuantLib::Period> DependencyMarket::capFloorVolIndexBase(const string& name,
                                                                           const string& config) const {
    if (curveConfigs_ && curveConfigs_->hasCapFloorVolCurveConfig(name)) {
        auto cc = curveConfigs_->capFloorVolCurveConfig(name);
        if (!cc->proxyTargetIndex().empty())
            return std::make_pair(cc->proxyTargetIndex(), cc->proxyTargetRateComputationPeriod());
        else
            return std::make_pair(cc->index(), cc->rateComputationPeriod());
    }
    QuantLib::ext::shared_ptr<IborIndex> index;
    if (tryParseIborIndex(name, index)) {
        std::string ccy = index->currency().code();
        if (curveConfigs_ && curveConfigs_->hasCapFloorVolCurveConfig(ccy)) {
            auto cc = curveConfigs_->capFloorVolCurveConfig(ccy);
            if (!cc->proxyTargetIndex().empty())
                return std::make_pair(cc->proxyTargetIndex(), cc->proxyTargetRateComputationPeriod());
            else
                return std::make_pair(cc->index(), cc->rateComputationPeriod());
        }
        // no cc for name or ccy of index name => return index name itself and empty rate computation period
        return std::make_pair(name, 0 * Days);
    }
    // return unspecified index name and an empty rate computation period
    return std::make_pair(name + "", 0 * Days);
}

Handle<ZeroInflationIndex> DependencyMarket::zeroInflationIndex(const string& name, const string& config) const {
    addRiskFactor(RiskFactorKey::KeyType::ZeroInflationCurve, name);
    addMarketObject(MarketObject::ZeroInflationCurve, name, config);
    RelinkableHandle<ZeroInflationTermStructure> its;
    auto ii = ore::data::parseZeroInflationIndex(name, its);
    auto dc = ActualActual(ActualActual::ISDA);
    vector<Time> zeroCurveTimes = {0, 1, 2};
    vector<Handle<Quote>> quotes;
    QuantLib::ext::shared_ptr<SimpleQuote> q0(new SimpleQuote(0));
    Handle<Quote> qh0(q0);
    quotes.push_back(qh0);
    for (Size i = 0; i < 2; i++) {
        QuantLib::ext::shared_ptr<SimpleQuote> q1(new SimpleQuote(0.01));
        Handle<Quote> qh1(q1);
        quotes.push_back(qh1);
    }
    QuantLib::ext::shared_ptr<ZeroInflationTermStructure> zeroCurve =
        QuantLib::ext::shared_ptr<ZeroInflationCurveObserverMoving<Linear>>(new ZeroInflationCurveObserverMoving<Linear>(
            0, WeekendsOnly(), dc, Period(2, Months), QuantLib::Frequency::Semiannual, false, zeroCurveTimes, quotes));
    its.linkTo(zeroCurve);
    its->enableExtrapolation();
    return Handle<ZeroInflationIndex>(ore::data::parseZeroInflationIndex(name, its));
}

Handle<YoYInflationIndex> DependencyMarket::yoyInflationIndex(const string& name, const string& config) const {
    addRiskFactor(RiskFactorKey::KeyType::YoYInflationCurve, name);
    addMarketObject(MarketObject::YoYInflationCurve, name, config);
    Handle<ZeroInflationTermStructure> zits;
    auto ii = ore::data::parseZeroInflationIndex(name, zits);
    auto dc = ActualActual(ActualActual::ISDA);
    vector<Time> zeroCurveTimes = {0, 1, 2};
    vector<Handle<Quote>> quotes;
    QuantLib::ext::shared_ptr<SimpleQuote> q0(new SimpleQuote(0));
    Handle<Quote> qh0(q0);
    quotes.push_back(qh0);
    for (Size i = 0; i < 2; i++) {
        QuantLib::ext::shared_ptr<SimpleQuote> q1(new SimpleQuote(0.01));
        Handle<Quote> qh1(q1);
        quotes.push_back(qh1);
    }
    QuantLib::ext::shared_ptr<YoYInflationTermStructure> yoyCurve =
        QuantLib::ext::shared_ptr<YoYInflationCurveObserverMoving<Linear>>(new YoYInflationCurveObserverMoving<Linear>(
            0, WeekendsOnly(), dc, Period(2, Months), QuantLib::Frequency::Semiannual, true, zeroCurveTimes, quotes));
    Handle<YoYInflationTermStructure> its(yoyCurve);
    its->enableExtrapolation();
    return Handle<YoYInflationIndex>(
        QuantLib::ext::make_shared<QuantExt::YoYInflationIndexWrapper>(ii, false, its));
}

Handle<QuantLib::CPIVolatilitySurface> DependencyMarket::cpiInflationCapFloorVolatilitySurface(const string& name,
                                                                                     const string& config) const {
    addRiskFactor(RiskFactorKey::KeyType::ZeroInflationCapFloorVolatility, name);
    addMarketObject(MarketObject::ZeroInflationCapFloorVol, name, config);
    return flatRateCPIvs(this->zeroInflationIndex(name, config));
}

Handle<QuantExt::YoYOptionletVolatilitySurface> DependencyMarket::yoyCapFloorVol(const string& name,
                                                                                 const string& config) const {
    addRiskFactor(RiskFactorKey::KeyType::YoYInflationCapFloorVolatility, name);
    addMarketObject(MarketObject::YoYInflationCapFloorVol, name, config);
    return flatRateYoYvs(this->yoyInflationIndex(name, config));
}

Handle<Quote> DependencyMarket::equitySpot(const string& eqName, const string& config) const {
    IndexNameTranslator::instance().add(eqName, "EQ-" + eqName);
    addRiskFactor(RiskFactorKey::KeyType::EquitySpot, eqName);
    addMarketObject(MarketObject::EquityCurve, eqName, config);
    // Make the equity spot price non-zero - arbitrary value of 10 here
    return Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(10.0));
}

Handle<YieldTermStructure> DependencyMarket::equityDividendCurve(const string& name, const string& config) const {
    addRiskFactor(RiskFactorKey::KeyType::DividendYield, name);
    addMarketObject(MarketObject::EquityCurve, name, config);
    return flatRateYts();
}

Handle<BlackVolTermStructure> DependencyMarket::equityVol(const string& name, const string& config) const {
    addRiskFactor(RiskFactorKey::KeyType::EquityVolatility, name);
    addMarketObject(MarketObject::EquityVol, name, config);
    return flatRateFxv();
}

Handle<YieldTermStructure> DependencyMarket::equityForecastCurve(const string& name, const string& config) const {
    addMarketObject(MarketObject::EquityCurve, name, config);
    return flatRateYts();
}

Handle<EquityIndex2> DependencyMarket::equityCurve(const string& name, const string& config) const {
    IndexNameTranslator::instance().add(name, "EQ-" + name);
    auto fyts = equityForecastCurve(name, config);
    auto dyts = equityDividendCurve(name, config);
    auto spot = equitySpot(name, config);
    Calendar equityCal;
    Currency equityCcy;
    if (curveConfigs_ && curveConfigs_->hasEquityCurveConfig(name)) {
        auto ccyStr = curveConfigs_->equityCurveConfig(name)->currency();
        auto calStr = curveConfigs_->equityCurveConfig(name)->calendar();
        equityCal = parseCalendar(calStr.empty() ? ccyStr : calStr);
        equityCcy = parseCurrencyWithMinors(ccyStr);
    } else {
        equityCal = WeekendsOnly();
        equityCcy = Currency();
    }
    return Handle<EquityIndex2>(QuantLib::ext::make_shared<EquityIndex2>(name, equityCal, equityCcy, spot, fyts, dyts));
}

Handle<Quote> DependencyMarket::securitySpread(const string& name, const string& config) const {
    addRiskFactor(RiskFactorKey::KeyType::SecuritySpread, name);
    addMarketObject(MarketObject::Security, name, config);
    return Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.0));
}

Handle<PriceTermStructure> DependencyMarket::commodityPriceCurve(const string& name, const string& config) const {
    addRiskFactor(RiskFactorKey::KeyType::CommodityCurve, name);
    addMarketObject(MarketObject::CommodityCurve, name, config);
    Currency commCcy;
    if (curveConfigs_ && curveConfigs_->hasCommodityCurveConfig(name)) {
        auto curveconf = curveConfigs_->commodityCurveConfig(name);
        if (curveconf->type() == CommodityCurveConfig::Type::Basis) {
            auto baseIndex = commodityIndex(curveconf->basePriceCurveId(), config);
            QL_REQUIRE(!baseIndex.empty() && baseIndex.currentLink() != nullptr && !baseIndex->priceCurve().empty(),
                       "Internal error in dependency market, couldn't build commodity basis price curve  "
                           << name << ", missing baseIndex with curve.");
            QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();

            // We need to have commodity future conventions for both the base curve and the basis curve
            QL_REQUIRE(conventions->has(curveconf->conventionsId()),
                       "Commodity conventions " << curveconf->conventionsId() << " requested by commodity config "
                                                << curveconf->curveID() << " not found");
            auto basisConvention =
                QuantLib::ext::dynamic_pointer_cast<CommodityFutureConvention>(conventions->get(curveconf->conventionsId()));
            QL_REQUIRE(basisConvention,
                       "Convention " << curveconf->conventionsId() << " not of expected type CommodityFutureConvention");
            auto basisFec = QuantLib::ext::make_shared<ore::data::ConventionsBasedFutureExpiry>(*basisConvention);

            QL_REQUIRE(conventions->has(curveconf->baseConventionsId()),
                       "Commodity conventions " << curveconf->baseConventionsId() << " requested by commodity config "
                                                << curveconf->curveID() << " not found");
            auto baseConvention =
                QuantLib::ext::dynamic_pointer_cast<CommodityFutureConvention>(conventions->get(curveconf->baseConventionsId()));
            QL_REQUIRE(baseConvention, "Convention " << curveconf->baseConventionsId()
                                                     << " not of expected type CommodityFutureConvention");
            auto baseFec = QuantLib::ext::make_shared<ConventionsBasedFutureExpiry>(*baseConvention);

            auto baseFutureIndex = QuantLib::ext::dynamic_pointer_cast<QuantExt::CommodityFuturesIndex>(*baseIndex);
            commCcy = parseCurrency(curveConfigs_->commodityCurveConfig(name)->currency());
            auto dummyCurve = flatRatePts(commCcy);
            return Handle<PriceTermStructure>(QuantLib::ext::make_shared<QuantExt::CommodityBasisPriceCurveWrapper>(
                asofDate(), *dummyCurve, basisFec, baseFutureIndex, baseFec));
        } else {
            commCcy = parseCurrency(curveConfigs_->commodityCurveConfig(name)->currency());
            return flatRatePts(commCcy);
        }
    } 
    QL_FAIL("Didn't find commodity curve config for " << name);
    
}

Handle<QuantExt::CommodityIndex> DependencyMarket::commodityIndex(const string& name, const string& config) const {
    auto pts = commodityPriceCurve(name, config);
    // if (conventions_)
    //     return Handle<CommodityIndex>(parseCommodityIndex(name, *conventions_, false, pts));
    // else
    //     return Handle<CommodityIndex>(parseCommodityIndex(name, false, NullCalendar(), pts));
    return Handle<QuantExt::CommodityIndex>(parseCommodityIndex(name, false, pts));
}

Handle<BlackVolTermStructure> DependencyMarket::commodityVolatility(const string& name, const string& config) const {
    addRiskFactor(RiskFactorKey::KeyType::CommodityVolatility, name);
    addMarketObject(MarketObject::CommodityVolatility, name, config);
    if (curveConfigs_ && curveConfigs_->hasCommodityVolatilityConfig(name)) {
        auto volCurveConfig = curveConfigs_->commodityVolatilityConfig(name);
        for (const auto& priceCurve : volCurveConfig->requiredCurveIds(CurveSpec::CurveType::Commodity)) {
            commodityIndex(priceCurve, config);
        }
    }
    return flatRateFxv();
}

Handle<Quote> DependencyMarket::cpr(const string& name, const string& config) const {
    addRiskFactor(RiskFactorKey::KeyType::CPR, name);
    addMarketObject(MarketObject::Security, name, config);
    return Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.0));
}

QuantLib::Handle<QuantExt::CorrelationTermStructure>
DependencyMarket::correlationCurve(const std::string& index1, const std::string& index2, const std::string& config) const {
    // normalise the correlation pair such that a) index1 and index2 are in line with the fx dominance rules and b)
    // index2 > index1 in index2:index1
    std::string index1Norm = index1, index2Norm = index2;
    if (useFxDominance_ && ore::data::isFxIndex(index1)) {
        index1Norm = ore::data::normaliseFxIndex(index1);
    }
    if (useFxDominance_ && ore::data::isFxIndex(index2)) {
        index2Norm = ore::data::normaliseFxIndex(index2);
    }
    // if a correlation was retrieved for index1 = index2, do not record this curve, MarketImpl handles this edge case
    if (index1Norm != index2Norm) {
        string label;
        string delim = "&";
        if (ore::data::indexNameLessThan(index1Norm, index2Norm))
            label = index2Norm + delim + index1Norm;
        else
            label = index1Norm + delim + index2Norm;
        addRiskFactor(RiskFactorKey::KeyType::Correlation, label);
        addMarketObject(MarketObject::Correlation, label, config);
    }
    return Handle<QuantExt::CorrelationTermStructure>(
        QuantLib::ext::make_shared<QuantExt::FlatCorrelation>(0, NullCalendar(), 0, ActualActual(ActualActual::ISDA)));
}

bool DependencyMarket::hasRiskFactorType(const RiskFactorKey::KeyType& riskFactorType) const {
    return riskFactors_.find(riskFactorType) != riskFactors_.end();
}

set<string> DependencyMarket::riskFactorNames(const RiskFactorKey::KeyType& riskFactorType) const {
    auto tmp = riskFactors_.find(riskFactorType);
    if (tmp != riskFactors_.end()) {
        return tmp->second;
    } else {
        return set<string>();
    }
}

set<RiskFactorKey::KeyType> DependencyMarket::riskFactorTypes() const {
    set<RiskFactorKey::KeyType> result;
    for (auto const& kv : riskFactors_) {
        result.insert(kv.first);
    }
    return result;
}

void DependencyMarket::addRiskFactor(RiskFactorKey::KeyType type, const string& name) const {
    riskFactors_[type].insert(name);
}

bool DependencyMarket::hasMarketObjectType(const ore::data::MarketObject& marketObjectType) const {
    for (auto const& mo : marketObjects_)
        if (mo.second.find(marketObjectType) != mo.second.end())
            return true;
    return false;
}

set<string> DependencyMarket::marketObjectNames(const ore::data::MarketObject& marketObjectType) const {
    std::set<std::string> result;
    for (auto const& mo : marketObjects_) {
        auto tmp = mo.second.find(marketObjectType);
        if (tmp != mo.second.end())
            result.insert(tmp->second.begin(), tmp->second.end());
    }
    return result;
}

set<ore::data::MarketObject> DependencyMarket::marketObjectTypes() const {
    set<ore::data::MarketObject> result;
    for (auto const& mo : marketObjects_) {
        for (auto const& kv : mo.second)
            result.insert(kv.first);
    }
    return result;
}

void DependencyMarket::addMarketObject(ore::data::MarketObject type, const string& name, const string& config) const {
    marketObjects_[config][type].insert(name);
}

std::map<ore::data::MarketObject, std::set<std::string>>
DependencyMarket::marketObjects(const boost::optional<string> config) const {
    if (config)
        return marketObjects_[*config];
    std::map<ore::data::MarketObject, std::set<std::string>> result;
    for (auto const& m : marketObjects_) {
        for (auto const& e : m.second) {
            result[e.first].insert(e.second.begin(), e.second.end());
        }
    }
    return result;
}

} // namespace analytics
} // namespace ore
