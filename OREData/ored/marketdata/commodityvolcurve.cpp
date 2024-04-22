/*
Copyright (C) 2018 Quaternion Risk Management Ltd
Copyright (C) 2022 Skandinaviska Enskilda Banken AB (publ)
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

#include <algorithm>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/range/adaptor/indexed.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <ored/marketdata/commodityvolcurve.hpp>
#include <ored/utilities/conventionsbasedfutureexpiry.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/utilities/wildcard.hpp>
#include <ql/math/interpolations/bicubicsplineinterpolation.hpp>
#include <ql/math/interpolations/loginterpolation.hpp>
#include <ql/math/matrix.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancecurve.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancesurface.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <qle/indexes/commodityindex.hpp>
#include <qle/math/flatextrapolation.hpp>
#include <qle/termstructures/aposurface.hpp>
#include <qle/termstructures/blackinvertedvoltermstructure.hpp>
#include <qle/termstructures/blackvariancesurfacemoneyness.hpp>
#include <qle/termstructures/blackvariancesurfacesparse.hpp>
#include <qle/termstructures/blackvolsurfacedelta.hpp>
#include <qle/termstructures/eqcommoptionsurfacestripper.hpp>
#include <qle/termstructures/blackvolsurfaceproxy.hpp>
#include <qle/termstructures/pricetermstructureadapter.hpp>
#include <qle/termstructures/blackdeltautilities.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <qle/models/carrmadanarbitragecheck.hpp>

using namespace std;
using namespace QuantLib;
using namespace QuantExt;

namespace {

// Utility struct to represent (expiry, strike) pair when storing volatilities below in map.
struct ExpiryStrike {
    ExpiryStrike(const Date& exp = Date(), Real stk = Null<Real>()) : expiry(exp), strike(stk) {}
    Date expiry;
    Real strike;
};

ostream& operator<<(ostream& os, const ExpiryStrike& es) {
    return os << "(" << io::iso_date(es.expiry) << "," << es.strike << ")";
}

// Comparator so that ExpiryStrike may be used as a map key.
struct ExpiryStrikeComp {
    bool operator() (const ExpiryStrike& lhs, const ExpiryStrike& rhs) const {
        return (lhs.expiry < rhs.expiry) || (!(rhs.expiry < lhs.expiry) && 
            !close(lhs.strike, rhs.strike) && (lhs.strike < rhs.strike));
    }
};

// Struct that stores a call data point (price or vol) and a put data point (price or vol).
// Null<Real>() indicates the absence of the data point.
struct CallPutDatum {
    CallPutDatum() : call(Null<Real>()), put(Null<Real>()) {}

    CallPutDatum(Option::Type optionType, Real value)
        : call(Null<Real>()), put(Null<Real>()) {
        if (optionType == Option::Call) {
            call = value;
        } else {
            put = value;
        }
    }

    Real call;
    Real put;
};

// Container that stores all the relevant points.
struct CallPutData {

    void addDatum(ExpiryStrike node, Option::Type optionType, Real value) {

        auto it = data.find(node);
        if (it != data.end()) {
            // Already have call or put data for the (expiry, strike)
            CallPutDatum& cpd = it->second;
            Real& toUpdate = optionType == Option::Call ? cpd.call : cpd.put;
            if (toUpdate == Null<Real>()) {
                toUpdate = value;
                TLOG("Updated " << optionType << " option data point with value " << fixed <<
                    setprecision(9) << value << " for expiry strike pair " << node << ".");
            } else {
                TLOG("Expiry strike pair " << node << " already has " << optionType << " option data " <<
                    "point with value " << fixed << setprecision(9) << toUpdate <<
                    " so did not update with value " << value << ".");
            }
        } else {
            // Have no call or put data for the (expiry, strike)
            data[node] = CallPutDatum(optionType, value);
            TLOG("Added " << optionType << " option data point with value " << fixed << setprecision(9) <<
                value << " for expiry strike pair " << node << ".");
        }
    }

    map<ExpiryStrike, CallPutDatum, ExpiryStrikeComp> data;
};

// Return the relevant surface using the data
QuantLib::ext::shared_ptr<OptionPriceSurface> optPriceSurface(const CallPutData& cpData,
    const Date& asof, const DayCounter& dc, bool forCall) {

    DLOG("Creating " << (forCall ? "Call" : "Put") << " option price surface.");

    const auto& priceData = cpData.data;
    auto n = priceData.size();

    vector<Date> expiries; expiries.reserve(n);
    vector<Real> strikes; strikes.reserve(n);
    vector<Real> prices; prices.reserve(n);

    for (const auto& kv : cpData.data) {

        if ((forCall && kv.second.call != Null<Real>()) || (!forCall && kv.second.put != Null<Real>())) {

            expiries.push_back(kv.first.expiry);
            strikes.push_back(kv.first.strike);
            prices.push_back(forCall ? kv.second.call : kv.second.put);

            TLOG("Using option datum (" << (forCall ? "Call" : "Put") << "," << io::iso_date(expiries.back()) <<
                "," << fixed << setprecision(9) << strikes.back() << "," << prices.back() << ")");
        }
    }

    // This is not enough but OptionPriceSurface should throw if data is not sufficient.
    QL_REQUIRE(!prices.empty(), "Need at least one point for " << (forCall ? "Call" : "Put") <<
        " commodity option price surface.");

    return QuantLib::ext::make_shared<OptionPriceSurface>(asof, expiries, strikes, prices, dc);
}

}

namespace ore {
namespace data {

CommodityVolCurve::CommodityVolCurve(const Date& asof, const CommodityVolatilityCurveSpec& spec, const Loader& loader,
                                     const CurveConfigurations& curveConfigs,
                                     const map<string, QuantLib::ext::shared_ptr<YieldCurve>>& yieldCurves,
                                     const map<string, QuantLib::ext::shared_ptr<CommodityCurve>>& commodityCurves,
                                     const map<string, QuantLib::ext::shared_ptr<CommodityVolCurve>>& commodityVolCurves,
                                     const map<string, QuantLib::ext::shared_ptr<FXVolCurve>>& fxVolCurves,
                                     const map<string, QuantLib::ext::shared_ptr<CorrelationCurve>>& correlationCurves,
                                     const Market* fxIndices, const bool buildCalibrationInfo) {

    try {
        LOG("CommodityVolCurve: start building commodity volatility structure with ID " << spec.curveConfigID());

        auto config = *curveConfigs.commodityVolatilityConfig(spec.curveConfigID());

        QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();

        if (!config.futureConventionsId().empty()) {
            const auto& cId = config.futureConventionsId();
            QL_REQUIRE(conventions->has(cId),
                       "Conventions, " << cId << " for config " << config.curveID() << " not found.");
            convention_ = QuantLib::ext::dynamic_pointer_cast<CommodityFutureConvention>(conventions->get(cId));
            QL_REQUIRE(convention_, "Convention with ID '" << cId << "' should be of type CommodityFutureConvention");
            expCalc_ = QuantLib::ext::make_shared<ConventionsBasedFutureExpiry>(*convention_);
        }

        calendar_ = parseCalendar(config.calendar());
        dayCounter_ = parseDayCounter(config.dayCounter());

        // loop over the volatility configs attempting to build in the order provided
        DLOG("CommodityVolCurve: Attempting to build commodity vol curve from volatilityConfig, "
             << config.volatilityConfig().size() << " volatility configs provided.");
        for (auto vc : config.volatilityConfig()) {
            try {
                // if the volatility config has it's own calendar, we use that.
                if (!vc->calendar().empty())
                    calendar_ = vc->calendar();
                // Do different things depending on the type of volatility configured
                if (auto eqvc = QuantLib::ext::dynamic_pointer_cast<ProxyVolatilityConfig>(vc)) {
                    buildVolatility(asof, spec, curveConfigs, *eqvc, commodityCurves, commodityVolCurves, fxVolCurves,
                                    correlationCurves, fxIndices);
                } else if (auto qvc = QuantLib::ext::dynamic_pointer_cast<QuoteBasedVolatilityConfig>(vc)) {

                    if (auto cvc = QuantLib::ext::dynamic_pointer_cast<ConstantVolatilityConfig>(vc)) {
                        buildVolatility(asof, config, *cvc, loader);
                    } else if (auto vcc = QuantLib::ext::dynamic_pointer_cast<VolatilityCurveConfig>(vc)) {
                        buildVolatility(asof, config, *vcc, loader);
                    } else if (auto vssc = QuantLib::ext::dynamic_pointer_cast<VolatilityStrikeSurfaceConfig>(vc)) {
                        // Try to populate the price and yield term structure. Need them in some cases here.
                        populateCurves(config, yieldCurves, commodityCurves, true, true);
                        buildVolatility(asof, config, *vssc, loader);
                    } else if (auto vdsc = QuantLib::ext::dynamic_pointer_cast<VolatilityDeltaSurfaceConfig>(vc)) {
                        // Need a yield curve and price curve to create a delta surface.
                        populateCurves(config, yieldCurves, commodityCurves, true);
                        buildVolatility(asof, config, *vdsc, loader);
                    } else if (auto vmsc = QuantLib::ext::dynamic_pointer_cast<VolatilityMoneynessSurfaceConfig>(vc)) {
                        // Need a yield curve (if forward moneyness) and price curve to create a moneyness surface.
                        MoneynessStrike::Type moneynessType = parseMoneynessType(vmsc->moneynessType());
                        bool fwdMoneyness = moneynessType == MoneynessStrike::Type::Forward;
                        populateCurves(config, yieldCurves, commodityCurves, fwdMoneyness);
                        buildVolatility(asof, config, *vmsc, loader);
                    } else if (auto vapo = QuantLib::ext::dynamic_pointer_cast<VolatilityApoFutureSurfaceConfig>(vc)) {

                        // Get the base conventions and create the associated expiry calculator.
                        QL_REQUIRE(!vapo->baseConventionsId().empty(),
                                   "The APO FutureConventions must be populated to build a future APO surface");
                        QL_REQUIRE(conventions->has(vapo->baseConventionsId()),
                                   "Conventions, " << vapo->baseConventionsId() << " for config " << config.curveID()
                                                   << " not found.");
                        auto convention = QuantLib::ext::dynamic_pointer_cast<CommodityFutureConvention>(
                            conventions->get(vapo->baseConventionsId()));
                        QL_REQUIRE(convention, "Convention with ID '"
                                                   << config.futureConventionsId()
                                                   << "' should be of type CommodityFutureConvention");
                        auto baseExpCalc = QuantLib::ext::make_shared<ConventionsBasedFutureExpiry>(*convention);

                        // Need to get the base commodity volatility structure.
                        QL_REQUIRE(!vapo->baseVolatilityId().empty(),
                                   "The APO VolatilityId must be populated to build a future APO surface.");
                        auto itVs = commodityVolCurves.find(vapo->baseVolatilityId());
                        QL_REQUIRE(itVs != commodityVolCurves.end(),
                                   "Can't find commodity volatility with id " << vapo->baseVolatilityId());
                        auto baseVs = Handle<BlackVolTermStructure>(itVs->second->volatility());

                        // Need to get the base price curve
                        QL_REQUIRE(!vapo->basePriceCurveId().empty(),
                                   "The APO PriceCurveId must be populated to build a future APO surface.");
                        auto itPts = commodityCurves.find(vapo->basePriceCurveId());
                        QL_REQUIRE(itPts != commodityCurves.end(),
                                   "Can't find price curve with id " << vapo->basePriceCurveId());
                        auto basePts = Handle<PriceTermStructure>(itPts->second->commodityPriceCurve());

                        // Need a yield curve and price curve to create an APO surface.
                        populateCurves(config, yieldCurves, commodityCurves, true);

                        buildVolatility(asof, config, *vapo, baseVs, basePts);

                    } else {
                        QL_FAIL("Unexpected VolatilityConfig in CommodityVolatilityConfig");
                    }
                } else {
                    QL_FAIL("CommodityVolCurve: VolatilityConfig must be QuoteBased or a Proxy");
                }
            if (buildCalibrationInfo)
                    this->buildVolCalibrationInfo(asof, vc, curveConfigs, config);
            } catch (std::exception& e) {
                DLOG("CommodityVolCurve: commodity vol curve building failed :" << e.what());
            } catch (...) {
                DLOG("CommodityVolCurve: commodity vol curve building failed: unknown error");
            }
            if (volatility_)
                break;
        }
        QL_REQUIRE(volatility_ , "CommodityVolCurve: Failed to build commodity volatility structure from "
                    << config.volatilityConfig().size() << " volatility configs provided.");

        LOG("CommodityVolCurve: finished building commodity volatility structure with ID " << spec.curveConfigID());

    } catch (exception& e) {
        QL_FAIL("Commodity volatility curve building failed : " << e.what());
    } catch (...) {
        QL_FAIL("Commodity volatility curve building failed: unknown error");
    }
}

void CommodityVolCurve::buildVolatility(const Date& asof, const CommodityVolatilityConfig& vc,
                                        const ConstantVolatilityConfig& cvc, const Loader& loader) {

    LOG("CommodityVolCurve: start building constant volatility structure");

    const QuantLib::ext::shared_ptr<MarketDatum>& md = loader.get(cvc.quote(), asof);
    QL_REQUIRE(md->asofDate() == asof, "MarketDatum asofDate '" << md->asofDate() << "' <> asof '" << asof << "'");
    QL_REQUIRE(md->instrumentType() == MarketDatum::InstrumentType::COMMODITY_OPTION,
        "MarketDatum instrument type '" << md->instrumentType() << "' <> 'MarketDatum::InstrumentType::COMMODITY_OPTION'");
    QuantLib::ext::shared_ptr<CommodityOptionQuote> q = QuantLib::ext::dynamic_pointer_cast<CommodityOptionQuote>(md);
    QL_REQUIRE(q, "Internal error: could not downcast MarketDatum '" << md->name() << "' to CommodityOptionQuote");
    QL_REQUIRE(q->name() == cvc.quote(),
        "CommodityOptionQuote name '" << q->name() << "' <> ConstantVolatilityConfig quote '" << cvc.quote() << "'");
    TLOG("Found the constant volatility quote " << q->name());
    Real quoteValue = q->quote()->value();

    DLOG("Creating BlackConstantVol structure");
    volatility_ = QuantLib::ext::make_shared<BlackConstantVol>(asof, calendar_, quoteValue, dayCounter_);

    LOG("CommodityVolCurve: finished building constant volatility structure");
}

void CommodityVolCurve::buildVolatility(const QuantLib::Date& asof, const CommodityVolatilityConfig& vc,
                                        const VolatilityCurveConfig& vcc, const Loader& loader) {

    LOG("CommodityVolCurve: start building 1-D volatility curve");

    // Must have at least one quote
    QL_REQUIRE(vcc.quotes().size() > 0, "No quotes specified in config " << vc.curveID());

    QL_REQUIRE(vcc.quoteType() == MarketDatum::QuoteType::RATE_LNVOL, "CommodityVolCurve: only quote type" <<
        " RATE_LNVOL is currently supported for 1-D commodity volatility curves.");

    // Check if we are using a regular expression to select the quotes for the curve. If we are, the quotes should
    // contain exactly one element.
    auto wildcard = getUniqueWildcard(vcc.quotes());

    // curveData will be populated with the expiry dates and volatility values.
    map<Date, Real> curveData;

    // Different approaches depending on whether we are using a regex or searching for a list of explicit quotes.
    if (wildcard) {

        DLOG("Have single quote with pattern " << wildcard->pattern());

        // Loop over quotes and process commodity option quotes matching pattern on asof
        for (const auto& md : loader.get(*wildcard, asof)) {

            // Go to next quote if the market data point's date does not equal our asof
            if (md->asofDate() != asof)
                continue;

            auto q = QuantLib::ext::dynamic_pointer_cast<CommodityOptionQuote>(md);
            if (!q)
                continue;
            QL_REQUIRE(q->quoteType() == vcc.quoteType(),
                "CommodityOptionQuote type '" << q->quoteType() << "' <> VolatilityCurveConfig quote type '" << vcc.quoteType() << "'");

            TLOG("The quote " << q->name() << " matched the pattern");

            Date expiryDate = getExpiry(asof, q->expiry(), vc.futureConventionsId(), vc.optionExpiryRollDays());
            if (expiryDate > asof) {
                // Add the quote to the curve data
                auto it = curveData.find(expiryDate);
                if (it != curveData.end()) {
                    LOG("Already added quote " << fixed << setprecision(9) << it->second << " for the expiry" <<
                        " date " << io::iso_date(expiryDate) << " for commodity curve " << vc.curveID() <<
                        " so not adding " << q->name());
                } else {
                    curveData[expiryDate] = q->quote()->value();
                    TLOG("Added quote " << q->name() << ": (" << io::iso_date(expiryDate) << "," <<
                        fixed << setprecision(9) << q->quote()->value() << ")");
                }
            }
        }

        // Check that we have quotes in the end
        QL_REQUIRE(curveData.size() > 0, "No quotes found matching regular expression " << vcc.quotes()[0]);

    } else {

        DLOG("Have " << vcc.quotes().size() << " explicit quotes");

        // Loop over quotes and process commodity option quotes that are explicitly specified in the config
        Size quotesAdded = 0;
        Size skippedExpiredQuotes = 0;
        for (const auto& quoteName : vcc.quotes()) {
            auto md = loader.get(quoteName, asof);
            QL_REQUIRE(md, "Unable to load quote '" << quoteName << "'");

            QL_REQUIRE(md->asofDate() == asof, "MarketDatum asofDate '" << md->asofDate() << "' <> asof '" << asof << "'");

            auto q = QuantLib::ext::dynamic_pointer_cast<CommodityOptionQuote>(md);
            QL_REQUIRE(q, "Internal error: could not downcast MarketDatum '" << md->name() << "' to CommodityOptionQuote");

            TLOG("Found the configured quote " << q->name());

            Date expiryDate = getExpiry(asof, q->expiry(), vc.futureConventionsId(), vc.optionExpiryRollDays());
            if (expiryDate > asof) {
                QL_REQUIRE(expiryDate > asof, "Commodity volatility quote '"
                                                  << q->name() << "' has expiry in the past ("
                                                  << io::iso_date(expiryDate) << ")");
                QL_REQUIRE(curveData.count(expiryDate) == 0, "Duplicate quote for the date "
                                                                 << io::iso_date(expiryDate)
                                                                 << " provided by commodity volatility config "
                                                                 << vc.curveID());
                curveData[expiryDate] = q->quote()->value();
                quotesAdded++;
                TLOG("Added quote " << q->name() << ": (" << io::iso_date(expiryDate) << "," << fixed
                                    << setprecision(9) << q->quote()->value() << ")");
            } else {
                skippedExpiredQuotes++;
                WLOG("Skipped quote " << q->name() << ": (" << io::iso_date(expiryDate) << "," << fixed
                                      << setprecision(9) << q->quote()->value() << ")");
            }
        }

        // Check that we have found all of the explicitly configured quotes
        QL_REQUIRE((quotesAdded + skippedExpiredQuotes) == vcc.quotes().size(),
                   "Found " << quotesAdded << " live quotes and " << skippedExpiredQuotes << " expired quotes"
                            << " but " << vcc.quotes().size() << " quotes were given in config.");
    }

    // Create the dates and volatility vector
    vector<Date> dates;
    vector<Volatility> volatilities;
    for (const auto& datum : curveData) {
        dates.push_back(datum.first);
        volatilities.push_back(datum.second);
        TLOG("Added data point (" << io::iso_date(dates.back()) << "," << fixed << setprecision(9)
                                  << volatilities.back() << ")");
    }
    // set max expiry date (used in buildCalibrationInfo())
    if (!dates.empty())
        maxExpiry_ = dates.back();
    DLOG("Creating BlackVarianceCurve object.");
    auto tmp = QuantLib::ext::make_shared<BlackVarianceCurve>(asof, dates, volatilities, dayCounter_);

    // Set the interpolation.
    if (vcc.interpolation() == "Linear") {
        DLOG("Interpolation set to Linear.");
    } else if (vcc.interpolation() == "Cubic") {
        DLOG("Setting interpolation to Cubic.");
        tmp->setInterpolation<Cubic>();
    } else if (vcc.interpolation() == "LogLinear") {
        DLOG("Setting interpolation to LogLinear.");
        tmp->setInterpolation<LogLinear>();
    } else {
        DLOG("Interpolation " << vcc.interpolation() << " not recognised so leaving it Linear.");
    }

    // Set the volatility_ member after we have possibly updated the interpolation.
    volatility_ = tmp;

    // Set the extrapolation
    if (parseExtrapolation(vcc.extrapolation()) == Extrapolation::Flat) {
        DLOG("Enabling BlackVarianceCurve flat volatility extrapolation.");
        volatility_->enableExtrapolation();
    } else if (parseExtrapolation(vcc.extrapolation()) == Extrapolation::None) {
        DLOG("Disabling BlackVarianceCurve extrapolation.");
        volatility_->disableExtrapolation();
    } else if (parseExtrapolation(vcc.extrapolation()) == Extrapolation::UseInterpolator) {
        DLOG("BlackVarianceCurve does not support using interpolator for extrapolation "
             << "so default to flat volatility extrapolation.");
        volatility_->enableExtrapolation();
    } else {
        DLOG("Unexpected extrapolation so default to flat volatility extrapolation.");
        volatility_->enableExtrapolation();
    }

    LOG("CommodityVolCurve: finished building 1-D volatility curve");
}

void CommodityVolCurve::buildVolatility(const Date& asof, CommodityVolatilityConfig& vc,
                                        const VolatilityStrikeSurfaceConfig& vssc, const Loader& loader) {

    LOG("CommodityVolCurve: start building 2-D volatility absolute strike surface");

    // We are building a commodity volatility surface here of the form expiry vs strike where the strikes are absolute
    // numbers. The list of expiries may be explicit or contain a single wildcard character '*'. Similarly, the list
    // of strikes may be explicit or contain a single wildcard character '*'. So, we have four options here which
    // ultimately lead to two different types of surface being built:
    // 1. explicit strikes and explicit expiries => BlackVarianceSurface
    // 2. wildcard strikes and/or wildcard expiries (3 combinations) => BlackVarianceSurfaceSparse

    bool expWc = false;
    if (find(vssc.expiries().begin(), vssc.expiries().end(), "*") != vssc.expiries().end()) {
        expWc = true;
        QL_REQUIRE(vssc.expiries().size() == 1, "Wild card expiry specified but more expiries also specified.");
        DLOG("Have expiry wildcard pattern " << vssc.expiries()[0]);
    }

    bool strkWc = false;
    if (find(vssc.strikes().begin(), vssc.strikes().end(), "*") != vssc.strikes().end()) {
        strkWc = true;
        QL_REQUIRE(vssc.strikes().size() == 1, "Wild card strike specified but more strikes also specified.");
        DLOG("Have strike wildcard pattern " << vssc.strikes()[0]);
    }

    // If we do not have a strike wild card, we expect a list of absolute strike values
    vector<Real> configuredStrikes;
    if (!strkWc) {
        // Parse the list of absolute strikes
        configuredStrikes = parseVectorOfValues<Real>(vssc.strikes(), &parseReal);
        sort(configuredStrikes.begin(), configuredStrikes.end(), [](Real x, Real y) { return !close(x, y) && x < y; });
        QL_REQUIRE(adjacent_find(configuredStrikes.begin(), configuredStrikes.end(),
                                 [](Real x, Real y) { return close(x, y); }) == configuredStrikes.end(),
                   "The configured strikes contain duplicates");
        DLOG("Parsed " << configuredStrikes.size() << " unique configured absolute strikes");
    }

    // If we do not have an expiry wild card, parse the configured expiries.
    vector<QuantLib::ext::shared_ptr<Expiry>> configuredExpiries;
    if (!expWc) {
        // Parse the list of expiry strings.
        for (const string& strExpiry : vssc.expiries()) {
            configuredExpiries.push_back(parseExpiry(strExpiry));
        }
        DLOG("Parsed " << configuredExpiries.size() << " unique configured expiries");
    }

    // If there are no wildcard strikes or wildcard expiries, delegate to buildVolatilityExplicit.
    if (!expWc && !strkWc) {
        buildVolatilityExplicit(asof, vc, vssc, loader, configuredStrikes);
        return;
    }

    DLOG("Expiries and or strikes have been configured via wildcards so building a "
         << "wildcard based absolute strike surface");

    // Store grid points along with call and/or put data point
    CallPutData cpData;

    // If the price term structure is not empty, populate the map with (expiry, forward) pairs 
    // that will be used below when choosing between a call and a put if have volatilities.
    map<Date, Real> fwdCurve;

    // Loop over quotes and process any commodity option quote that matches a wildcard
    std::ostringstream ss;
    ss << MarketDatum::InstrumentType::COMMODITY_OPTION << "/" << vssc.quoteType() << "/" << vc.curveID() << "/"
       << vc.currency() << "/*";
    Wildcard w(ss.str());
    for (const auto& md : loader.get(w, asof)) {

        QL_REQUIRE(md->asofDate() == asof, "MarketDatum asofDate '" << md->asofDate() << "' <> asof '" << asof << "'");

        auto q = QuantLib::ext::dynamic_pointer_cast<CommodityOptionQuote>(md);
        QL_REQUIRE(q, "Internal error: could not downcast MarketDatum '" << md->name() << "' to CommodityOptionQuote");

        QL_REQUIRE(vc.curveID() == q->commodityName(),
            "CommodityVolatilityConfig curve ID '" << vc.curveID() <<
            "' <> CommodityOptionQuote commodity name '" << q->commodityName() << "'");
        QL_REQUIRE(vc.currency() == q->quoteCurrency(),
            "CommodityVolatilityConfig currency '" << vc.currency() <<
            "' <> CommodityOptionQuote currency '" << q->quoteCurrency() << "'");
        QL_REQUIRE(vssc.quoteType() == q->quoteType(),
            "VolatilityStrikeSurfaceConfig quote type '" << vssc.quoteType() <<
            "' <> CommodityOptionQuote quote type '" << q->quoteType() << "'");

        // This surface is for absolute strikes only.
        auto strike = QuantLib::ext::dynamic_pointer_cast<AbsoluteStrike>(q->strike());
        if (!strike)
            continue;

        // If we have been given a list of explicit expiries, check that the quote matches one of them.
        // Move to the next quote if it does not.
        if (!expWc) {
            auto expiryIt = find_if(configuredExpiries.begin(), configuredExpiries.end(),
                                    [&q](QuantLib::ext::shared_ptr<Expiry> e) { return *e == *q->expiry(); });
            if (expiryIt == configuredExpiries.end())
                continue;
        }

        // If we have been given a list of explicit strikes, check that the quote matches one of them.
        // Move to the next quote if it does not.
        if (!strkWc) {
            auto strikeIt = find_if(configuredStrikes.begin(), configuredStrikes.end(),
                                    [&strike](Real s) { return close(s, strike->strike()); });
            if (strikeIt == configuredStrikes.end())
                continue;
        }

        // Store in the data container.
        Date expiry = getExpiry(asof, q->expiry(), vc.futureConventionsId(), vc.optionExpiryRollDays());
        if (expiry > asof) {
            ExpiryStrike node(expiry, strike->strike());
            cpData.addDatum(ExpiryStrike(expiry, strike->strike()), q->optionType(), q->quote()->value());

            // If we have a price term structure, add forward for expiry if not there already.
            if (vssc.quoteType() == MarketDatum::QuoteType::RATE_LNVOL && !pts_.empty() &&
                fwdCurve.count(expiry) == 0) {
                fwdCurve[expiry] = pts_->price(expiry);
            }

            TLOG("Added quote " << q->name() << " to intermediate data.");
        } else {
            TLOG("Skipped quote " << q->name() << " to intermediate data, already expired at " << QuantLib::io::iso_date(expiry));
        }
    }

    // Set the strike extrapolation which only matters if extrapolation is turned on for the whole surface.
    bool flatStrikeExtrap = true;
    bool flatTimeExtrap = true;
    if (vssc.extrapolation()) {

        auto strikeExtrapType = parseExtrapolation(vssc.strikeExtrapolation());
        if (strikeExtrapType == Extrapolation::UseInterpolator) {
            DLOG("Strike extrapolation switched to using interpolator.");
            flatStrikeExtrap = false;
        } else if (strikeExtrapType == Extrapolation::None) {
            DLOG("Strike extrapolation cannot be turned off on its own so defaulting to flat.");
        } else if (strikeExtrapType == Extrapolation::Flat) {
            DLOG("Strike extrapolation has been set to flat.");
        } else {
            DLOG("Strike extrapolation " << strikeExtrapType << " not expected so default to flat.");
        }

        auto timeExtrapType = parseExtrapolation(vssc.timeExtrapolation());
        if (timeExtrapType == Extrapolation::UseInterpolator) {
            DLOG("Time extrapolation switched to using interpolator.");
            flatTimeExtrap = false;
        } else if (timeExtrapType == Extrapolation::None) {
            DLOG("Time extrapolation cannot be turned off on its own so defaulting to flat.");
        } else if (timeExtrapType == Extrapolation::Flat) {
            DLOG("Time extrapolation has been set to flat.");
        } else {
            DLOG("Time extrapolation " << timeExtrapType << " not expected so default to flat.");
        }

    } else {
        DLOG("Extrapolation is turned off for the whole surface so the time and"
             << " strike extrapolation settings are ignored");
    }

    // Determine which option type to pick at each expiry and strike.
    bool preferOutOfTheMoney = !vc.preferOutOfTheMoney() ? true : *vc.preferOutOfTheMoney();
    TLOG("Prefer out of the money set to: " << ore::data::to_string(preferOutOfTheMoney) << ".");

    // Build the surface depending on the quote type.
    if (vssc.quoteType() == MarketDatum::QuoteType::RATE_LNVOL) {

        DLOG("Creating the BlackVarianceSurfaceSparse object");

        // Now pick the quotes that we want from the data container. If the fwdCurve was populated above, we use 
        // it in conjunction with preferOutOfTheMoney, to pick the volatilities. If fwdCurve is empty, we just pick 
        // a call volatility if there is one else a put volatility (should have at least one).
        vector<Real> strikes;
        vector<Date> expiries;
        vector<Volatility> vols;
        Size quotesAdded = 0;

        for (const auto& kv : cpData.data) {

            const Date& expiry = kv.first.expiry;
            const Real& stk = kv.first.strike;
            const CallPutDatum& cpd = kv.second;
            QL_REQUIRE(cpd.call != Null<Real>() || cpd.put != Null<Real>(),
                "CommodityVolCurve: expected the call or put value to be populated.");

            expiries.push_back(expiry);
            strikes.push_back(stk);
            bool useCall = cpd.call != Null<Real>();

            if ((cpd.call != Null<Real>() && cpd.put != Null<Real>()) && !fwdCurve.empty()) {
                // We have a call and a put and a forward curve so use it to decide.
                auto it = fwdCurve.find(expiry);
                QL_REQUIRE(it != fwdCurve.end(), "CommodityVolCurve: expected forwards for all expiries.");
                const Real& fwd = it->second;
                useCall = (preferOutOfTheMoney && stk >= fwd) || (!preferOutOfTheMoney && stk <= fwd);
                TLOG("(Forward,Strike,UseCall) = (" << fixed << setprecision(6) <<
                    fwd << "," << stk << "," << to_string(useCall) << ").");
            }

            vols.push_back(useCall ? cpd.call : cpd.put);
            quotesAdded++;

            TLOG("Using option datum (" << (useCall ? "Call" : "Put") << "," << io::iso_date(expiries.back()) <<
                "," << fixed << setprecision(9) << strikes.back() << "," << vols.back() << ")");
        }

        LOG("CommodityVolCurve: added " << quotesAdded << " quotes building wildcard based absolute strike surface.");
        QL_REQUIRE(quotesAdded > 0, "No quotes loaded for " << vc.curveID());

        volatility_ = QuantLib::ext::make_shared<BlackVarianceSurfaceSparse>(asof, calendar_, expiries, strikes,
            vols, dayCounter_, flatStrikeExtrap, flatStrikeExtrap, flatTimeExtrap);

    } else if (vssc.quoteType() == MarketDatum::QuoteType::PRICE) {

        QL_REQUIRE(!pts_.empty(), "CommodityVolCurve: require non-empty price term structure" <<
            " to strip volatilities from prices.");
        QL_REQUIRE(!yts_.empty(), "CommodityVolCurve: require non-empty yield term structure" <<
            " to strip volatilities from prices.");

        // Create the 1D solver options used in the price stripping.
        Solver1DOptions solverOptions = vc.solverConfig();

        // Need to create the call and put option price surfaces.
        auto cSurface = optPriceSurface(cpData, asof, dayCounter_, true);
        auto pSurface = optPriceSurface(cpData, asof, dayCounter_, false);

        DLOG("CommodityVolCurve: stripping volatility surface from the option premium surfaces.");
        auto coss = QuantLib::ext::make_shared<CommodityOptionSurfaceStripper>(pts_, yts_, cSurface, pSurface, calendar_,
            dayCounter_, vssc.exerciseType(), flatStrikeExtrap, flatStrikeExtrap, flatTimeExtrap,
            preferOutOfTheMoney, solverOptions);
        volatility_ = coss->volSurface();

        // If data level logging, output the stripped volatilities.
        if (Log::instance().filter(ORE_DATA)) {
            volatility_->enableExtrapolation(vssc.extrapolation());
            TLOG("CommodityVolCurve: stripped volatilities:");
            TLOG("expiry,strike,forward_price,call_price,put_price,discount,volatility");
            for (const auto& kv : cpData.data) {
                const Date& expiry = kv.first.expiry;
                const Real& strike = kv.first.strike;
                Real fwd = pts_->price(expiry);
                string cPrice;
                if (kv.second.call != Null<Real>()) {
                    stringstream ss;
                    ss << fixed << setprecision(6) << kv.second.call;
                    cPrice = ss.str();
                }
                string pPrice;
                if (kv.second.put != Null<Real>()) {
                    stringstream ss;
                    ss << fixed << setprecision(6) << kv.second.put;
                    pPrice = ss.str();
                }
                Real discount = yts_->discount(expiry);
                Real vol = volatility_->blackVol(expiry, strike);
                TLOG(io::iso_date(expiry) << "," << fixed << setprecision(6) << strike << "," <<
                    fwd << "," << cPrice << "," << pPrice << "," << discount << "," << vol);
            }
        }

    } else {
        QL_FAIL("CommodityVolCurve: invalid quote type " << vssc.quoteType() << " provided.");
    }

    DLOG("Setting BlackVarianceSurfaceSparse extrapolation to " << to_string(vssc.extrapolation()));
    volatility_->enableExtrapolation(vssc.extrapolation());

    LOG("CommodityVolCurve: finished building 2-D volatility absolute strike surface");
}

void CommodityVolCurve::buildVolatilityExplicit(const Date& asof, CommodityVolatilityConfig& vc,
                                                const VolatilityStrikeSurfaceConfig& vssc, const Loader& loader,
                                                const vector<Real>& configuredStrikes) {

    LOG("CommodityVolCurve: start building 2-D volatility absolute strike surface with explicit strikes and expiries");

    QL_REQUIRE(vssc.quoteType() == MarketDatum::QuoteType::RATE_LNVOL, "CommodityVolCurve: only quote type" <<
        " RATE_LNVOL is currently supported for 2-D volatility strike surface with explicit strikes and expiries.");

    // Map to hold the rows of the commodity volatility matrix. The keys are the expiry dates and the values are the
    // vectors of volatilities, one for each configured strike.
    map<Date, vector<Real>> surfaceData;

    // Count the number of quotes added. We check at the end that we have added all configured quotes.
    Size quotesAdded = 0;
    Size skippedExpiredQuotes = 0;
    // Loop over quotes and process commodity option quotes that have been requested
    std::ostringstream ss;
    ss << MarketDatum::InstrumentType::COMMODITY_OPTION << "/" << vssc.quoteType() << "/" << vc.curveID() << "/"
       << vc.currency() << "/*";
    Wildcard w(ss.str());
    for (const auto& md : loader.get(w, asof)) {

        QL_REQUIRE(md->asofDate() == asof, "MarketDatum asofDate '" << md->asofDate() << "' <> asof '" << asof << "'");

        auto q = QuantLib::ext::dynamic_pointer_cast<CommodityOptionQuote>(md);
        QL_REQUIRE(q, "Internal error: could not downcast MarketDatum '" << md->name() << "' to CommodityOptionQuote");

        // This surface is for absolute strikes only.
        auto strike = QuantLib::ext::dynamic_pointer_cast<AbsoluteStrike>(q->strike());
        if (!strike)
            continue;

        // If the quote is not in the configured quotes continue
        auto it = find(vc.quotes().begin(), vc.quotes().end(), q->name());
        if (it == vc.quotes().end())
            continue;

        // Process the quote
        Date eDate = getExpiry(asof, q->expiry(), vc.futureConventionsId(), vc.optionExpiryRollDays());
        
        if (eDate > asof) {

            // Position of quote in vector of strikes
            auto strikeIt = find_if(configuredStrikes.begin(), configuredStrikes.end(),
                                    [&strike](Real s) { return close(s, strike->strike()); });
            Size pos = distance(configuredStrikes.begin(), strikeIt);
            QL_REQUIRE(pos < configuredStrikes.size(),
                       "The quote '"
                           << q->name()
                           << "' is in the list of configured quotes but does not match any of the configured strikes");

            // Add quote to surface
            if (surfaceData.count(eDate) == 0)
                surfaceData[eDate] = vector<Real>(configuredStrikes.size(), Null<Real>());

            QL_REQUIRE(surfaceData[eDate][pos] == Null<Real>(),
                       "Quote " << q->name() << " provides a duplicate quote for the date " << io::iso_date(eDate)
                                << " and the strike " << configuredStrikes[pos]);
            surfaceData[eDate][pos] = q->quote()->value();
            quotesAdded++;

            TLOG("Added quote " << q->name() << ": (" << io::iso_date(eDate) << "," << fixed << setprecision(9)
                                << configuredStrikes[pos] << "," << q->quote()->value() << ")");
        } else {
            skippedExpiredQuotes++;
            TLOG("Skipped quote " << q->name() << ": (" << io::iso_date(eDate) << "," << fixed << setprecision(9)
                                  << q->quote()->value() << ")");
        }
    }

    LOG("CommodityVolCurve: added " << quotesAdded << " quotes in building explicit absolute strike surface.");

    QL_REQUIRE(vc.quotes().size() == quotesAdded + skippedExpiredQuotes,
               "Found " << quotesAdded << " live quotes and " << skippedExpiredQuotes << "expired quotes, but " << vc.quotes().size()
                        << " quotes required by config.");

    // Set up the BlackVarianceSurface. Note that the rows correspond to strikes and that the columns correspond to
    // the expiry dates in the matrix that is fed to the BlackVarianceSurface ctor.
    vector<Date> expiryDates;
    Matrix volatilities = Matrix(configuredStrikes.size(), surfaceData.size());
    Size expiryIdx = 0;
    for (const auto& expiryRow : surfaceData) {
        expiryDates.push_back(expiryRow.first);
        for (Size i = 0; i < configuredStrikes.size(); i++) {
            volatilities[i][expiryIdx] = expiryRow.second[i];
        }
        expiryIdx++;
    }

    // set max expiry date (used in buildCalibrationInfo())
    if (!expiryDates.empty())
        maxExpiry_ = *max_element(expiryDates.begin(), expiryDates.end());

    // Trace log the surface
    TLOG("Explicit strike surface grid points:");
    TLOG("expiry,strike,volatility");
    for (Size i = 0; i < volatilities.rows(); i++) {
        for (Size j = 0; j < volatilities.columns(); j++) {
            TLOG(io::iso_date(expiryDates[j])
                 << "," << fixed << setprecision(9) << configuredStrikes[i] << "," << volatilities[i][j]);
        }
    }

    // Set the strike extrapolation which only matters if extrapolation is turned on for the whole surface.
    // BlackVarianceSurface time extrapolation is hard-coded to constant in volatility.
    BlackVarianceSurface::Extrapolation strikeExtrap = BlackVarianceSurface::ConstantExtrapolation;
    if (vssc.extrapolation()) {

        auto strikeExtrapType = parseExtrapolation(vssc.strikeExtrapolation());
        if (strikeExtrapType == Extrapolation::UseInterpolator) {
            DLOG("Strike extrapolation switched to using interpolator.");
            strikeExtrap = BlackVarianceSurface::InterpolatorDefaultExtrapolation;
        } else if (strikeExtrapType == Extrapolation::None) {
            DLOG("Strike extrapolation cannot be turned off on its own so defaulting to flat.");
        } else if (strikeExtrapType == Extrapolation::Flat) {
            DLOG("Strike extrapolation has been set to flat.");
        } else {
            DLOG("Strike extrapolation " << strikeExtrapType << " not expected so default to flat.");
        }

        auto timeExtrapType = parseExtrapolation(vssc.timeExtrapolation());
        if (timeExtrapType != Extrapolation::Flat) {
            DLOG("BlackVarianceSurface only supports flat volatility extrapolation in the time direction");
        }
    } else {
        DLOG("Extrapolation is turned off for the whole surface so the time and"
             << " strike extrapolation settings are ignored");
    }

    DLOG("Creating BlackVarianceSurface object");
    auto tmp = QuantLib::ext::make_shared<BlackVarianceSurface>(asof, calendar_, expiryDates, configuredStrikes, volatilities,
                                                        dayCounter_, strikeExtrap, strikeExtrap);

    // Set the interpolation if configured properly. The default is Bilinear.
    if (!(vssc.timeInterpolation() == "Linear" && vssc.strikeInterpolation() == "Linear")) {
        if (vssc.timeInterpolation() != vssc.strikeInterpolation()) {
            DLOG("Time and strike interpolation must be the same for BlackVarianceSurface but we got strike "
                 << "interpolation " << vssc.strikeInterpolation() << " and time interpolation "
                 << vssc.timeInterpolation());
        } else if (vssc.timeInterpolation() == "Cubic") {
            DLOG("Setting interpolation to BiCubic");
            tmp->setInterpolation<Bicubic>();
        } else {
            DLOG("Interpolation type " << vssc.timeInterpolation() << " not supported in 2 dimensions");
        }
    }

    // Set the volatility_ member after we have possibly updated the interpolation.
    volatility_ = tmp;

    DLOG("Setting BlackVarianceSurface extrapolation to " << to_string(vssc.extrapolation()));
    volatility_->enableExtrapolation(vssc.extrapolation());

    LOG("CommodityVolCurve: finished building 2-D volatility absolute strike "
        << "surface with explicit strikes and expiries");
}

void CommodityVolCurve::buildVolatility(const Date& asof, CommodityVolatilityConfig& vc,
                                        const VolatilityDeltaSurfaceConfig& vdsc, const Loader& loader) {

    using boost::adaptors::transformed;
    using boost::algorithm::join;

    LOG("CommodityVolCurve: start building 2-D volatility delta strike surface");

    QL_REQUIRE(vdsc.quoteType() == MarketDatum::QuoteType::RATE_LNVOL, "CommodityVolCurve: only quote type" <<
        " RATE_LNVOL is currently supported for a 2-D volatility delta strike surface.");

    // Parse, sort and check the vector of configured put deltas
    vector<Real> putDeltas = parseVectorOfValues<Real>(vdsc.putDeltas(), &parseReal);
    sort(putDeltas.begin(), putDeltas.end(), [](Real x, Real y) { return !close(x, y) && x < y; });
    QL_REQUIRE(adjacent_find(putDeltas.begin(), putDeltas.end(), [](Real x, Real y) { return close(x, y); }) ==
                   putDeltas.end(),
               "The configured put deltas contain duplicates");
    DLOG("Parsed " << putDeltas.size() << " unique configured put deltas");
    DLOG("Put deltas are: " << join(putDeltas | transformed([](Real d) { return ore::data::to_string(d); }), ","));

    // Parse, sort descending and check the vector of configured call deltas
    vector<Real> callDeltas = parseVectorOfValues<Real>(vdsc.callDeltas(), &parseReal);
    sort(callDeltas.begin(), callDeltas.end(), [](Real x, Real y) { return !close(x, y) && x > y; });
    QL_REQUIRE(adjacent_find(callDeltas.begin(), callDeltas.end(), [](Real x, Real y) { return close(x, y); }) ==
                   callDeltas.end(),
               "The configured call deltas contain duplicates");
    DLOG("Parsed " << callDeltas.size() << " unique configured call deltas");
    DLOG("Call deltas are: " << join(callDeltas | transformed([](Real d) { return ore::data::to_string(d); }), ","));

    // Expiries may be configured with a wildcard or given explicitly
    bool expWc = false;
    if (find(vdsc.expiries().begin(), vdsc.expiries().end(), "*") != vdsc.expiries().end()) {
        expWc = true;
        QL_REQUIRE(vdsc.expiries().size() == 1, "Wild card expiry specified but more expiries also specified.");
        DLOG("Have expiry wildcard pattern " << vdsc.expiries()[0]);
    }

    // Map to hold the rows of the commodity volatility matrix. The keys are the expiry dates and the values are the
    // vectors of volatilities, one for each configured delta.
    map<Date, vector<Real>> surfaceData;

    // Number of strikes = number of put deltas + ATM + number of call deltas
    Size numStrikes = putDeltas.size() + 1 + callDeltas.size();

    // Count the number of quotes added. We check at the end that we have added all configured quotes.
    Size quotesAdded = 0;
    Size skippedExpiredQuotes = 0;
    // Configured delta and Atm types.
    DeltaVolQuote::DeltaType deltaType = parseDeltaType(vdsc.deltaType());
    DeltaVolQuote::AtmType atmType = parseAtmType(vdsc.atmType());
    boost::optional<DeltaVolQuote::DeltaType> atmDeltaType;
    if (!vdsc.atmDeltaType().empty()) {
        atmDeltaType = parseDeltaType(vdsc.atmDeltaType());
    }

    // Populate the configured strikes.
    vector<QuantLib::ext::shared_ptr<BaseStrike>> strikes;
    for (const auto& pd : putDeltas) {
        strikes.push_back(QuantLib::ext::make_shared<DeltaStrike>(deltaType, Option::Put, pd));
    }
    strikes.push_back(QuantLib::ext::make_shared<AtmStrike>(atmType, atmDeltaType));
    for (const auto& cd : callDeltas) {
        strikes.push_back(QuantLib::ext::make_shared<DeltaStrike>(deltaType, Option::Call, cd));
    }

    // Read the quotes to fill the expiry dates and vols matrix.
    std::ostringstream ss;
    ss << MarketDatum::InstrumentType::COMMODITY_OPTION << "/" << vdsc.quoteType() << "/" << vc.curveID() << "/"
       << vc.currency() << "/*";
    Wildcard w(ss.str());
    for (const auto& md : loader.get(w, asof)) {

        QL_REQUIRE(md->asofDate() == asof, "MarketDatum asofDate '" << md->asofDate() << "' <> asof '" << asof << "'");

        auto q = QuantLib::ext::dynamic_pointer_cast<CommodityOptionQuote>(md);
        QL_REQUIRE(q, "Internal error: could not downcast MarketDatum '" << md->name() << "' to CommodityOptionQuote");

        QL_REQUIRE(vc.curveID() == q->commodityName(),
            "CommodityVolatilityConfig curve ID '" << vc.curveID() <<
            "' <> CommodityOptionQuote commodity name '" << q->commodityName() << "'");
        QL_REQUIRE(vc.currency() == q->quoteCurrency(),
            "CommodityVolatilityConfig currency '" << vc.currency() <<
            "' <> CommodityOptionQuote currency '" << q->quoteCurrency() << "'");

        // Iterator to one of the configured strikes.
        vector<QuantLib::ext::shared_ptr<BaseStrike>>::iterator strikeIt;

        if (expWc) {

            // Check if quote's strike is in the configured strikes and continue if it is not.
            strikeIt = find_if(strikes.begin(), strikes.end(),
                               [&q](QuantLib::ext::shared_ptr<BaseStrike> s) { return *s == *q->strike(); });
            if (strikeIt == strikes.end())
                continue;
        } else {

            // If we have explicitly configured expiries and the quote is not in the configured quotes continue.
            auto it = find(vc.quotes().begin(), vc.quotes().end(), q->name());
            if (it == vc.quotes().end())
                continue;

            // Check if quote's strike is in the configured strikes.
            // It should be as we have selected from the explicitly configured quotes in the last step.
            strikeIt = find_if(strikes.begin(), strikes.end(),
                               [&q](QuantLib::ext::shared_ptr<BaseStrike> s) { return *s == *q->strike(); });
            QL_REQUIRE(strikeIt != strikes.end(),
                       "The quote '"
                           << q->name()
                           << "' is in the list of configured quotes but does not match any of the configured strikes");
        }

        // Position of quote in vector of strikes
        Size pos = std::distance(strikes.begin(), strikeIt);

        // Process the quote
        Date eDate = getExpiry(asof, q->expiry(), vc.futureConventionsId(), vc.optionExpiryRollDays());
        if (eDate > asof) {

            // Add quote to surface
            if (surfaceData.count(eDate) == 0)
                surfaceData[eDate] = vector<Real>(numStrikes, Null<Real>());

            QL_REQUIRE(surfaceData[eDate][pos] == Null<Real>(),
                       "Quote " << q->name() << " provides a duplicate quote for the date " << io::iso_date(eDate)
                                << " and strike " << *q->strike());
            surfaceData[eDate][pos] = q->quote()->value();
            quotesAdded++;

            TLOG("Added quote " << q->name() << ": (" << io::iso_date(eDate) << "," << *q->strike() << "," << fixed
                                << setprecision(9) << "," << q->quote()->value() << ")");
        } else {
            skippedExpiredQuotes++;
            TLOG("Skiped quote, already expired: " << q->name() << ": (" << io::iso_date(eDate) << "," << *q->strike() << "," << fixed
                                << setprecision(9) << "," << q->quote()->value() << ")");
        }
    }

    LOG("CommodityVolCurve: added " << quotesAdded << " quotes in building delta strike surface.");

    // Check the data gathered.
    if (expWc) {
        // If the expiries were configured via a wildcard, check that no surfaceData element has a Null<Real>().
        for (const auto& kv : surfaceData) {
            for (Size j = 0; j < numStrikes; j++) {
                QL_REQUIRE(kv.second[j] != Null<Real>(), "Volatility for expiry date "
                                                             << io::iso_date(kv.first) << " and strike " << *strikes[j]
                                                             << " not found. Cannot proceed with a sparse matrix.");
            }
        }
    } else {
        // If expiries were configured explicitly, the number of configured quotes should equal the
        // number of quotes added.
        QL_REQUIRE(vc.quotes().size() == quotesAdded + skippedExpiredQuotes,
                   "Found " << quotesAdded << " quotes and "<< skippedExpiredQuotes <<" expired quotes , but " << vc.quotes().size() << " quotes required by config.");
    }

    // Populate the matrix of volatilities and the expiry dates.
    vector<Date> expiryDates;
    Matrix vols(surfaceData.size(), numStrikes);
    for (const auto row : surfaceData | boost::adaptors::indexed(0)) {
        expiryDates.push_back(row.value().first);
        copy(row.value().second.begin(), row.value().second.end(), vols.row_begin(row.index()));
    }

    if (!expiryDates.empty())
        maxExpiry_ = *std::max_element(expiryDates.begin(), expiryDates.end());

    // Need to multiply each put delta value by -1 before passing it to the BlackVolatilitySurfaceDelta ctor
    // i.e. a put delta of 0.25 that is passed in to the config must be -0.25 when passed to the ctor.
    transform(putDeltas.begin(), putDeltas.end(), putDeltas.begin(), [](Real pd) { return -1.0 * pd; });
    DLOG("Multiply put deltas by -1.0 before creating BlackVolatilitySurfaceDelta object.");
    DLOG("Put deltas are: " << join(putDeltas | transformed([](Real d) { return ore::data::to_string(d); }), ","));

    // Set the strike extrapolation which only matters if extrapolation is turned on for the whole surface.
    // BlackVolatilitySurfaceDelta time extrapolation is hard-coded to constant in volatility.
    bool flatExtrapolation = true;
    if (vdsc.extrapolation()) {

        auto strikeExtrapType = parseExtrapolation(vdsc.strikeExtrapolation());
        if (strikeExtrapType == Extrapolation::UseInterpolator) {
            DLOG("Strike extrapolation switched to using interpolator.");
            flatExtrapolation = false;
        } else if (strikeExtrapType == Extrapolation::None) {
            DLOG("Strike extrapolation cannot be turned off on its own so defaulting to flat.");
        } else if (strikeExtrapType == Extrapolation::Flat) {
            DLOG("Strike extrapolation has been set to flat.");
        } else {
            DLOG("Strike extrapolation " << strikeExtrapType << " not expected so default to flat.");
        }

        auto timeExtrapType = parseExtrapolation(vdsc.timeExtrapolation());
        if (timeExtrapType != Extrapolation::Flat) {
            DLOG("BlackVolatilitySurfaceDelta only supports flat volatility extrapolation in the time direction");
        }
    } else {
        DLOG("Extrapolation is turned off for the whole surface so the time and"
             << " strike extrapolation settings are ignored");
    }

    // Time interpolation
    if (vdsc.timeInterpolation() != "Linear") {
        DLOG("BlackVolatilitySurfaceDelta only supports linear time interpolation.");
    }

    // Strike interpolation
    QuantExt::InterpolatedSmileSection::InterpolationMethod im;
    if (vdsc.strikeInterpolation() == "Linear") {
        im = InterpolatedSmileSection::InterpolationMethod::Linear;
    } else if (vdsc.strikeInterpolation() == "NaturalCubic") {
        im = InterpolatedSmileSection::InterpolationMethod::NaturalCubic;
    } else if (vdsc.strikeInterpolation() == "FinancialCubic") {
        im = InterpolatedSmileSection::InterpolationMethod::FinancialCubic;
    } else if (vdsc.strikeInterpolation() == "CubicSpline") {
        im = InterpolatedSmileSection::InterpolationMethod::CubicSpline;
    } else {
        im = InterpolatedSmileSection::InterpolationMethod::Linear;
        DLOG("BlackVolatilitySurfaceDelta does not support strike interpolation '" << vdsc.strikeInterpolation()
                                                                                   << "' so setting it to linear.");
    }

    // Apply correction to future price term structure if requested and we have a valid expiry calculator
    QL_REQUIRE(!pts_.empty(), "Expected the price term structure to be populated for a delta surface.");
    Handle<PriceTermStructure> cpts = pts_;
    if (vdsc.futurePriceCorrection() && expCalc_)
        cpts = correctFuturePriceCurve(asof, vc.futureConventionsId(), *pts_, expiryDates);

    // Need to construct a dummy spot and foreign yts such that fwd = spot * DF_for / DF
    QL_REQUIRE(!yts_.empty(), "Expected the yield term structure to be populated for a delta surface.");
    Handle<Quote> spot(QuantLib::ext::make_shared<DerivedPriceQuote>(cpts));
    Handle<YieldTermStructure> pyts =
        Handle<YieldTermStructure>(QuantLib::ext::make_shared<PriceTermStructureAdapter>(*cpts, *yts_));
    pyts->enableExtrapolation();

    DLOG("Creating BlackVolatilitySurfaceDelta object");
    bool hasAtm = true;
    volatility_ = QuantLib::ext::make_shared<BlackVolatilitySurfaceDelta>(
        asof, expiryDates, putDeltas, callDeltas, hasAtm, vols, dayCounter_, calendar_, spot, yts_, pyts, deltaType,
        atmType, atmDeltaType, 0 * Days, deltaType, atmType, atmDeltaType, im, flatExtrapolation);

    DLOG("Setting BlackVolatilitySurfaceDelta extrapolation to " << to_string(vdsc.extrapolation()));
    volatility_->enableExtrapolation(vdsc.extrapolation());

    LOG("CommodityVolCurve: finished building 2-D volatility delta strike surface");
}

void CommodityVolCurve::buildVolatility(const Date& asof, CommodityVolatilityConfig& vc,
                                        const VolatilityMoneynessSurfaceConfig& vmsc, const Loader& loader) {

    using boost::adaptors::transformed;
    using boost::algorithm::join;

    LOG("CommodityVolCurve: start building 2-D volatility moneyness strike surface");

    QL_REQUIRE(vmsc.quoteType() == MarketDatum::QuoteType::RATE_LNVOL, "CommodityVolCurve: only quote type" <<
        " RATE_LNVOL is currently supported for a 2-D volatility moneyness strike surface.");

    // Parse, sort and check the vector of configured moneyness levels
    vector<Real> moneynessLevels = checkMoneyness(vmsc.moneynessLevels());

    // Expiries may be configured with a wildcard or given explicitly
    bool expWc = false;
    if (find(vmsc.expiries().begin(), vmsc.expiries().end(), "*") != vmsc.expiries().end()) {
        expWc = true;
        QL_REQUIRE(vmsc.expiries().size() == 1, "Wild card expiry specified but more expiries also specified.");
        DLOG("Have expiry wildcard pattern " << vmsc.expiries()[0]);
    }

    // Map to hold the rows of the commodity volatility matrix. The keys are the expiry dates and the values are the
    // vectors of volatilities, one for each configured moneyness.
    map<Date, vector<Real>> surfaceData;

    // Count the number of quotes added. We check at the end that we have added all configured quotes.
    Size quotesAdded = 0;
    Size skippedExpiredQuotes = 0;

    // Configured moneyness type.
    MoneynessStrike::Type moneynessType = parseMoneynessType(vmsc.moneynessType());

    // Populate the configured strikes.
    vector<QuantLib::ext::shared_ptr<BaseStrike>> strikes;
    for (const auto& moneynessLevel : moneynessLevels) {
        strikes.push_back(QuantLib::ext::make_shared<MoneynessStrike>(moneynessType, moneynessLevel));
    }

    // Read the quotes to fill the expiry dates and vols matrix.
    std::ostringstream ss;
    ss << MarketDatum::InstrumentType::COMMODITY_OPTION << "/" << vmsc.quoteType() << "/" << vc.curveID() << "/"
       << vc.currency() << "/*";
    Wildcard w(ss.str());
    for (const auto& md : loader.get(w, asof)) {

        QL_REQUIRE(md->asofDate() == asof, "MarketDatum asofDate '" << md->asofDate() << "' <> asof '" << asof << "'");

        auto q = QuantLib::ext::dynamic_pointer_cast<CommodityOptionQuote>(md);
        QL_REQUIRE(q, "Internal error: could not downcast MarketDatum '" << md->name() << "' to CommodityOptionQuote");

        QL_REQUIRE(vc.curveID() == q->commodityName(),
            "CommodityVolatilityConfig curve ID '" << vc.curveID() <<
            "' <> CommodityOptionQuote commodity name '" << q->commodityName() << "'");
        QL_REQUIRE(vc.currency() == q->quoteCurrency(),
            "CommodityVolatilityConfig currency '" << vc.currency() <<
            "' <> CommodityOptionQuote currency '" << q->quoteCurrency() << "'");

        // Iterator to one of the configured strikes.
        vector<QuantLib::ext::shared_ptr<BaseStrike>>::iterator strikeIt;

        if (expWc) {

            // Check if quote's strike is in the configured strikes and continue if it is not.
            strikeIt = find_if(strikes.begin(), strikes.end(),
                               [&q](QuantLib::ext::shared_ptr<BaseStrike> s) { return *s == *q->strike(); });
            if (strikeIt == strikes.end())
                continue;
        } else {

            // If we have explicitly configured expiries and the quote is not in the configured quotes continue.
            auto it = find(vc.quotes().begin(), vc.quotes().end(), q->name());
            if (it == vc.quotes().end())
                continue;

            // Check if quote's strike is in the configured strikes.
            // It should be as we have selected from the explicitly configured quotes in the last step.
            strikeIt = find_if(strikes.begin(), strikes.end(),
                               [&q](QuantLib::ext::shared_ptr<BaseStrike> s) { return *s == *q->strike(); });
            QL_REQUIRE(strikeIt != strikes.end(),
                       "The quote '"
                           << q->name()
                           << "' is in the list of configured quotes but does not match any of the configured strikes");
        }

        // Position of quote in vector of strikes
        Size pos = std::distance(strikes.begin(), strikeIt);

        // Process the quote
        Date eDate = getExpiry(asof, q->expiry(), vc.futureConventionsId(), vc.optionExpiryRollDays());
        if (eDate > asof) {
            // Add quote to surface
            if (surfaceData.count(eDate) == 0)
                surfaceData[eDate] = vector<Real>(moneynessLevels.size(), Null<Real>());

            QL_REQUIRE(surfaceData[eDate][pos] == Null<Real>(),
                       "Quote " << q->name() << " provides a duplicate quote for the date " << io::iso_date(eDate)
                                << " and strike " << *q->strike());
            surfaceData[eDate][pos] = q->quote()->value();
            quotesAdded++;

            TLOG("Added quote " << q->name() << ": (" << io::iso_date(eDate) << "," << *q->strike() << "," << fixed
                                << setprecision(9) << "," << q->quote()->value() << ")");
        }
        else {
            skippedExpiredQuotes++;
            TLOG("Skip quote, already expired: " << q->name() << ": (" << io::iso_date(eDate) << "," << *q->strike() << "," << fixed
                                << setprecision(9) << "," << q->quote()->value() << ")");
        }
    }

    LOG("CommodityVolCurve: added " << quotesAdded << " quotes in building moneyness strike surface.");

    // Check the data gathered.
    if (expWc) {
        // If the expiries were configured via a wildcard, check that no surfaceData element has a Null<Real>().
        for (const auto& kv : surfaceData) {
            for (Size j = 0; j < moneynessLevels.size(); j++) {
                QL_REQUIRE(kv.second[j] != Null<Real>(), "Volatility for expiry date "
                                                             << io::iso_date(kv.first) << " and strike " << *strikes[j]
                                                             << " not found. Cannot proceed with a sparse matrix.");
            }
        }
    } else {
        // If expiries were configured explicitly, the number of configured quotes should equal the
        // number of quotes added.
        QL_REQUIRE(vc.quotes().size() == quotesAdded + skippedExpiredQuotes,
                   "Found " << quotesAdded << " quotes and " << skippedExpiredQuotes << " expired quotes, but " << vc.quotes().size()
                            << " quotes required by config.");
    }

    // Populate the volatility quotes and the expiry times.
    // Rows are moneyness levels and columns are expiry times - this is what the ctor needs below.
    vector<Date> expiryDates(surfaceData.size());
    vector<Time> expiryTimes(surfaceData.size());
    vector<vector<Handle<Quote>>> vols(moneynessLevels.size());
    for (const auto row : surfaceData | boost::adaptors::indexed(0)) {
        expiryDates[row.index()] = row.value().first;
        expiryTimes[row.index()] = dayCounter_.yearFraction(asof, row.value().first);
        for (Size i = 0; i < row.value().second.size(); i++) {
            vols[i].push_back(Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(row.value().second[i])));
        }
    }

    if (!expiryDates.empty())
        maxExpiry_ = *std::max_element(expiryDates.begin(), expiryDates.end());

    // Set the strike extrapolation which only matters if extrapolation is turned on for the whole surface.
    // BlackVarianceSurfaceMoneyness time extrapolation is hard-coded to constant in volatility.
    bool flatExtrapolation = true;
    if (vmsc.extrapolation()) {

        auto strikeExtrapType = parseExtrapolation(vmsc.strikeExtrapolation());
        if (strikeExtrapType == Extrapolation::UseInterpolator) {
            DLOG("Strike extrapolation switched to using interpolator.");
            flatExtrapolation = false;
        } else if (strikeExtrapType == Extrapolation::None) {
            DLOG("Strike extrapolation cannot be turned off on its own so defaulting to flat.");
        } else if (strikeExtrapType == Extrapolation::Flat) {
            DLOG("Strike extrapolation has been set to flat.");
        } else {
            DLOG("Strike extrapolation " << strikeExtrapType << " not expected so default to flat.");
        }

        auto timeExtrapType = parseExtrapolation(vmsc.timeExtrapolation());
        if (timeExtrapType != Extrapolation::Flat) {
            DLOG("BlackVarianceSurfaceMoneyness only supports flat volatility extrapolation in the time direction");
        }
    } else {
        DLOG("Extrapolation is turned off for the whole surface so the time and"
             << " strike extrapolation settings are ignored");
    }

    // Time interpolation
    if (vmsc.timeInterpolation() != "Linear") {
        DLOG("BlackVarianceSurfaceMoneyness only supports linear time interpolation in variance.");
    }

    // Strike interpolation
    if (vmsc.strikeInterpolation() != "Linear") {
        DLOG("BlackVarianceSurfaceMoneyness only supports linear strike interpolation in variance.");
    }

    // Apply correction to future price term structure if requested and we have a valid expiry calculator
    QL_REQUIRE(!pts_.empty(), "Expected the price term structure to be populated for a moneyness surface.");
    Handle<PriceTermStructure> cpts = pts_;
    if (vmsc.futurePriceCorrection() && expCalc_)
        cpts = correctFuturePriceCurve(asof, vc.futureConventionsId(), *pts_, expiryDates);

    // Both moneyness surfaces need a spot quote.
    Handle<Quote> spot(QuantLib::ext::make_shared<DerivedPriceQuote>(cpts));

    // The choice of false here is important for forward moneyness. It means that we use the cpts and yts in the
    // BlackVarianceSurfaceMoneynessForward to get the forward value at all times and in particular at times that
    // are after the last expiry time. If we set it to true, BlackVarianceSurfaceMoneynessForward uses a linear
    // interpolated forward curve on the expiry times internally which is poor.
    bool stickyStrike = false;

    if (moneynessType == MoneynessStrike::Type::Forward) {

        QL_REQUIRE(!yts_.empty(), "Expected yield term structure to be populated for a forward moneyness surface.");
        Handle<YieldTermStructure> pyts =
            Handle<YieldTermStructure>(QuantLib::ext::make_shared<PriceTermStructureAdapter>(*cpts, *yts_));
        pyts->enableExtrapolation();

        DLOG("Creating BlackVarianceSurfaceMoneynessForward object");
        volatility_ = QuantLib::ext::make_shared<BlackVarianceSurfaceMoneynessForward>(calendar_, spot, expiryTimes,
                                                                               moneynessLevels, vols, dayCounter_, pyts,
                                                                               yts_, stickyStrike, flatExtrapolation);

    } else {

        DLOG("Creating BlackVarianceSurfaceMoneynessSpot object");
        volatility_ = QuantLib::ext::make_shared<BlackVarianceSurfaceMoneynessSpot>(
            calendar_, spot, expiryTimes, moneynessLevels, vols, dayCounter_, stickyStrike, flatExtrapolation);
    }

    DLOG("Setting BlackVarianceSurfaceMoneyness extrapolation to " << to_string(vmsc.extrapolation()));
    volatility_->enableExtrapolation(vmsc.extrapolation());

    LOG("CommodityVolCurve: finished building 2-D volatility moneyness strike surface");
}

void CommodityVolCurve::buildVolatility(const Date& asof, CommodityVolatilityConfig& vc,
                                        const VolatilityApoFutureSurfaceConfig& vapo,
                                        const Handle<BlackVolTermStructure>& baseVts,
                                        const Handle<PriceTermStructure>& basePts) {

    LOG("CommodityVolCurve: start building the APO surface");

    QL_REQUIRE(vapo.quoteType() == MarketDatum::QuoteType::RATE_LNVOL, "CommodityVolCurve: only quote type" <<
        " RATE_LNVOL is currently supported for an APO surface.");

    // Get the base conventions and create the associated expiry calculator.
    QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();
    QL_REQUIRE(!vapo.baseConventionsId().empty(),
               "The APO FutureConventions must be populated to build a future APO surface");
    QL_REQUIRE(conventions->has(vapo.baseConventionsId()),
               "Conventions, " << vapo.baseConventionsId() << " for config " << vc.curveID() << " not found.");
    auto baseConvention =
        QuantLib::ext::dynamic_pointer_cast<CommodityFutureConvention>(conventions->get(vapo.baseConventionsId()));
    QL_REQUIRE(baseConvention,
               "Convention with ID '" << vapo.baseConventionsId() << "' should be of type CommodityFutureConvention");

    auto baseExpCalc = QuantLib::ext::make_shared<ConventionsBasedFutureExpiry>(*baseConvention);

    // Get the max tenor from the config if provided.
    boost::optional<QuantLib::Period> maxTenor;
    if (!vapo.maxTenor().empty())
        maxTenor = parsePeriod(vapo.maxTenor());

    // Get the moneyness levels
    vector<Real> moneynessLevels = checkMoneyness(vapo.moneynessLevels());

    // Get the beta parameter to use for valuing the APOs in the surface
    Real beta = vapo.beta();

    // Construct the commodity index.
    auto index = parseCommodityIndex(baseConvention->id(), false, basePts);

    // Set the strike extrapolation which only matters if extrapolation is turned on for the whole surface.
    // BlackVarianceSurfaceMoneyness, which underlies the ApoFutureSurface, has time extrapolation hard-coded to
    // constant in volatility.
    bool flatExtrapolation = true;
    if (vapo.extrapolation()) {

        auto strikeExtrapType = parseExtrapolation(vapo.strikeExtrapolation());
        if (strikeExtrapType == Extrapolation::UseInterpolator) {
            DLOG("Strike extrapolation switched to using interpolator.");
            flatExtrapolation = false;
        } else if (strikeExtrapType == Extrapolation::None) {
            DLOG("Strike extrapolation cannot be turned off on its own so defaulting to flat.");
        } else if (strikeExtrapType == Extrapolation::Flat) {
            DLOG("Strike extrapolation has been set to flat.");
        } else {
            DLOG("Strike extrapolation " << strikeExtrapType << " not expected so default to flat.");
        }

        auto timeExtrapType = parseExtrapolation(vapo.timeExtrapolation());
        if (timeExtrapType != Extrapolation::Flat) {
            DLOG("ApoFutureSurface only supports flat volatility extrapolation in the time direction");
        }
    } else {
        DLOG("Extrapolation is turned off for the whole surface so the time and"
             << " strike extrapolation settings are ignored");
    }

    // Time interpolation
    if (vapo.timeInterpolation() != "Linear") {
        DLOG("ApoFutureSurface only supports linear time interpolation in variance.");
    }

    // Strike interpolation
    if (vapo.strikeInterpolation() != "Linear") {
        DLOG("ApoFutureSurface only supports linear strike interpolation in variance.");
    }

    DLOG("Creating ApoFutureSurface object");
    volatility_ = QuantLib::ext::make_shared<ApoFutureSurface>(asof, moneynessLevels, index, pts_, yts_, expCalc_, baseVts,
                                                       baseExpCalc, beta, flatExtrapolation, maxTenor);

    DLOG("Setting ApoFutureSurface extrapolation to " << to_string(vapo.extrapolation()));
    volatility_->enableExtrapolation(vapo.extrapolation());

    LOG("CommodityVolCurve: finished building the APO surface");
}

void CommodityVolCurve::buildVolatility(
    const QuantLib::Date& asof, const CommodityVolatilityCurveSpec& spec, const CurveConfigurations& curveConfigs,
    const ProxyVolatilityConfig& pvc, const map<string, QuantLib::ext::shared_ptr<CommodityCurve>>& comCurves,
    const map<string, QuantLib::ext::shared_ptr<CommodityVolCurve>>& volCurves,
    const map<string, QuantLib::ext::shared_ptr<FXVolCurve>>& fxVolCurves,
    const map<string, QuantLib::ext::shared_ptr<CorrelationCurve>>& requiredCorrelationCurves, const Market* fxIndices) {

    DLOG("Build Proxy Vol surface");
    // get all the configurations and the curve needed for proxying
    auto config = *curveConfigs.commodityVolatilityConfig(spec.curveConfigID());

    auto proxy = pvc.proxyVolatilityCurve();
    auto comConfig = *curveConfigs.commodityCurveConfig(spec.curveConfigID());
    auto proxyConfig = *curveConfigs.commodityCurveConfig(proxy);
    auto proxyVolConfig = *curveConfigs.commodityVolatilityConfig(proxy);

    // create dummy specs to look up the required curves
    CommodityCurveSpec comSpec(comConfig.currency(), spec.curveConfigID());
    CommodityCurveSpec proxySpec(proxyConfig.currency(), proxy);
    CommodityVolatilityCurveSpec proxyVolSpec(proxyVolConfig.currency(), proxy);

    // Get all necessary curves
    auto curve = comCurves.find(comSpec.name());
    QL_REQUIRE(curve != comCurves.end(),
               "CommodityVolCurve: Failed to find commodity curve, when building commodity vol curve " << spec.name());
    auto proxyCurve = comCurves.find(proxySpec.name());
    QL_REQUIRE(proxyCurve != comCurves.end(), "currency: Failed to find commodity curve for proxy "
                                                 << proxySpec.name() << ", when building commodity vol curve "
                                                 << spec.name());
    auto proxyVolCurve = volCurves.find(proxyVolSpec.name());
    QL_REQUIRE(proxyVolCurve != volCurves.end(), "CommodityVolCurve: Failed to find commodity vol curve for proxy "
                                                       << proxyVolSpec.name() << ", when building currency vol curve "
                                                       << spec.name());

    // check the currency against the proxy surface currrency

    QuantLib::ext::shared_ptr<BlackVolTermStructure> fxSurface;
    QuantLib::ext::shared_ptr<FxIndex> fxIndex;
    QuantLib::ext::shared_ptr<QuantExt::CorrelationTermStructure> correlation;
    if (config.currency() != proxyVolConfig.currency() && fxIndices != nullptr) {
        QL_REQUIRE(!pvc.fxVolatilityCurve().empty(),
                   "CommodityVolCurve: FXVolatilityCurve must be provided for commodity vol config "
                       << spec.curveConfigID() << " as proxy currencies if different from commodity currency.");
        QL_REQUIRE(!pvc.correlationCurve().empty(),
                   "CommodityVolCurve: CorrelationCurve must be provided for commodity vol config "
                       << spec.curveConfigID() << " as proxy currencies if different from commodity currency.");

        // get the fx vol surface
        QL_REQUIRE(pvc.fxVolatilityCurve().size() == 6, "CommodityVolCurve: FXVolatilityCurve provided "
                                                             << pvc.fxVolatilityCurve() << " for commodity vol config "
                                                             << spec.curveConfigID()
                                                             << " must be of length 6, and of form CC1CCY2 e.g EURUSD");
        string proxyVolForCcy = pvc.fxVolatilityCurve().substr(0, 3);
        string proxyVolDomCcy = pvc.fxVolatilityCurve().substr(3, 3);
        FXVolatilityCurveSpec fxSpec(proxyVolForCcy, proxyVolDomCcy, pvc.fxVolatilityCurve());
        auto volIt = fxVolCurves.find(fxSpec.name());
        if (volIt == fxVolCurves.end())
            QL_FAIL("CommodityVolCurve: cannot find required Fx volatility surface "
                    << fxSpec.name() << " to build proxy vol surface for " << comSpec.name());
        fxSurface = volIt->second->volTermStructure();

        // check if the fx vol surface needs to be inverted
        if (proxyVolForCcy != proxyVolConfig.currency()) {
            Handle<BlackVolTermStructure> hFx(fxSurface);
            fxSurface = QuantLib::ext::make_shared<QuantExt::BlackInvertedVolTermStructure>(hFx);
            fxSurface->enableExtrapolation();
        }

        fxIndex = fxIndices->fxIndex(proxyVolConfig.currency() + config.currency()).currentLink();
        FXSpotSpec spotSpec(proxyVolConfig.currency(), config.currency());
        
        CorrelationCurveSpec corrSpec(pvc.correlationCurve());
        auto corrIt = requiredCorrelationCurves.find(corrSpec.name());
        if (corrIt == requiredCorrelationCurves.end())
            QL_FAIL("CommodityVolCurve: cannot find required correlation curve "
                    << pvc.correlationCurve() << " to build proxy vol surface for " << comSpec.name());
        correlation = corrIt->second->corrTermStructure();
    }

    volatility_ = QuantLib::ext::make_shared<BlackVolatilitySurfaceProxy>(
        proxyVolCurve->second->volatility(), curve->second->commodityIndex(), proxyCurve->second->commodityIndex(),
        fxSurface, fxIndex, correlation);
}

Handle<PriceTermStructure> CommodityVolCurve::correctFuturePriceCurve(const Date& asof, const string& contractName,
                                                                      const QuantLib::ext::shared_ptr<PriceTermStructure>& pts,
                                                                      const vector<Date>& optionExpiries) const {

    LOG("CommodityVolCurve: start adding future price correction at option expiry.");

    // Gather curve dates and prices
    map<Date, Real> curveData;

    // Get existing curve dates and prices. Messy but can't think of another way.
    vector<Date> ptsDates;
    if (auto ipc = QuantLib::ext::dynamic_pointer_cast<InterpolatedPriceCurve<Linear>>(pts)) {
        ptsDates = ipc->pillarDates();
    } else if (auto ipc = QuantLib::ext::dynamic_pointer_cast<InterpolatedPriceCurve<LogLinear>>(pts)) {
        ptsDates = ipc->pillarDates();
    } else if (auto ipc = QuantLib::ext::dynamic_pointer_cast<InterpolatedPriceCurve<Cubic>>(pts)) {
        ptsDates = ipc->pillarDates();
    } else if (auto ipc = QuantLib::ext::dynamic_pointer_cast<InterpolatedPriceCurve<LinearFlat>>(pts)) {
        ptsDates = ipc->pillarDates();
    } else if (auto ipc = QuantLib::ext::dynamic_pointer_cast<InterpolatedPriceCurve<LogLinearFlat>>(pts)) {
        ptsDates = ipc->pillarDates();
    } else if (auto ipc = QuantLib::ext::dynamic_pointer_cast<InterpolatedPriceCurve<CubicFlat>>(pts)) {
        ptsDates = ipc->pillarDates();
    } else {
        DLOG("Could not cast the price term structure so do not have its pillar dates.");
    }

    // Add existing dates and prices to data.
    DLOG("Got " << ptsDates.size() << " pillar dates from the price curve.");
    for (const Date& d : ptsDates) {
        curveData[d] = pts->price(d);
        TLOG("Added (" << io::iso_date(d) << "," << fixed << setprecision(9) << curveData.at(d) << ").");
    }

    // Add future price at option expiry to the curve i.e. override any interpolation between future option
    // expiry (oed) and future expiry (fed).
    auto futureExpiryCalculator = expCalc_;
    if (convention_ && !convention_->optionUnderlyingFutureConvention().empty()) {
        QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();
        auto [found, conv] =
            conventions->get(convention_->optionUnderlyingFutureConvention(), Convention::Type::CommodityFuture);
        if (found) {
            futureExpiryCalculator = QuantLib::ext::make_shared<ConventionsBasedFutureExpiry>(
                *QuantLib::ext::dynamic_pointer_cast<CommodityFutureConvention>(conv));
        }
    }
    for (const Date& oed : optionExpiries) {
        Date fed = futureExpiryCalculator->nextExpiry(true, oed, 0, false);
        // In general, expect that the future expiry date will already be in curveData but maybe not.
        auto it = curveData.find(fed);
        if (it != curveData.end()) {
            curveData[oed] = it->second;
            TLOG("Found future expiry in existing data. (Option Expiry,Future Expiry,Future Price) is ("
                 << io::iso_date(oed) << "," << io::iso_date(fed) << "," << fixed << setprecision(9)
                 << curveData.at(oed) << ").");
        } else {
            curveData[oed] = pts->price(fed);
            curveData[fed] = pts->price(fed);
            TLOG("Future expiry not found in existing data. (Option Expiry,Future Expiry,Future Price) is ("
                 << io::iso_date(oed) << "," << io::iso_date(fed) << "," << fixed << setprecision(9)
                 << curveData.at(oed) << ").");
        }
    }

    // Gather data for building the "corrected" curve.
    vector<Date> curveDates;
    vector<Real> curvePrices;
    for (const auto& kv : curveData) {
        curveDates.push_back(kv.first);
        curvePrices.push_back(kv.second);
    }
    DayCounter dc = pts->dayCounter();

    // Create the "corrected" curve. Again messy but can't think of another way.
    QuantLib::ext::shared_ptr<PriceTermStructure> cpts;
    if (auto ipc = QuantLib::ext::dynamic_pointer_cast<InterpolatedPriceCurve<Linear>>(pts)) {
        cpts = QuantLib::ext::make_shared<InterpolatedPriceCurve<Linear>>(asof, curveDates, curvePrices, dc, pts->currency());
    } else if (auto ipc = QuantLib::ext::dynamic_pointer_cast<InterpolatedPriceCurve<LogLinear>>(pts)) {
        cpts =
            QuantLib::ext::make_shared<InterpolatedPriceCurve<LogLinear>>(asof, curveDates, curvePrices, dc, pts->currency());
    } else if (auto ipc = QuantLib::ext::dynamic_pointer_cast<InterpolatedPriceCurve<Cubic>>(pts)) {
        cpts = QuantLib::ext::make_shared<InterpolatedPriceCurve<Cubic>>(asof, curveDates, curvePrices, dc, pts->currency());
    } else if (auto ipc = QuantLib::ext::dynamic_pointer_cast<InterpolatedPriceCurve<LinearFlat>>(pts)) {
        cpts =
            QuantLib::ext::make_shared<InterpolatedPriceCurve<LinearFlat>>(asof, curveDates, curvePrices, dc, pts->currency());
    } else if (auto ipc = QuantLib::ext::dynamic_pointer_cast<InterpolatedPriceCurve<LogLinearFlat>>(pts)) {
        cpts = QuantLib::ext::make_shared<InterpolatedPriceCurve<LogLinearFlat>>(asof, curveDates, curvePrices, dc,
                                                                         pts->currency());
    } else if (auto ipc = QuantLib::ext::dynamic_pointer_cast<InterpolatedPriceCurve<CubicFlat>>(pts)) {
        cpts =
            QuantLib::ext::make_shared<InterpolatedPriceCurve<CubicFlat>>(asof, curveDates, curvePrices, dc, pts->currency());
    } else {
        DLOG("Could not cast the price term structure so corrected curve is a linear InterpolatedPriceCurve.");
        cpts = QuantLib::ext::make_shared<InterpolatedPriceCurve<Linear>>(asof, curveDates, curvePrices, dc, pts->currency());
    }
    cpts->enableExtrapolation(pts->allowsExtrapolation());

    LOG("CommodityVolCurve: finished adding future price correction at option expiry.");

    return Handle<PriceTermStructure>(cpts);
}

Date CommodityVolCurve::getExpiry(const Date& asof, const QuantLib::ext::shared_ptr<Expiry>& expiry, const string& name,
                                  Natural rollDays) const {

    Date result;

    if (auto expiryDate = QuantLib::ext::dynamic_pointer_cast<ExpiryDate>(expiry)) {
        result = expiryDate->expiryDate();
    } else if (auto expiryPeriod = QuantLib::ext::dynamic_pointer_cast<ExpiryPeriod>(expiry)) {
        // We may need more conventions here eventually.
        result = calendar_.adjust(asof + expiryPeriod->expiryPeriod());
    } else if (auto fcExpiry = QuantLib::ext::dynamic_pointer_cast<FutureContinuationExpiry>(expiry)) {

        QL_REQUIRE(expCalc_, "CommodityVolCurve::getExpiry: need a future expiry calculator for continuation quotes.");
        QL_REQUIRE(convention_, "CommodityVolCurve::getExpiry: need a future convention for continuation quotes.");
        DLOG("Future option continuation expiry is " << *fcExpiry);

        // Firstly, get the next option expiry on or after the asof date
        result = expCalc_->nextExpiry(true, asof, 0, true);
        TLOG("CommodityVolCurve::getExpiry: next option expiry relative to " << io::iso_date(asof) << " is "
                                                                             << io::iso_date(result) << ".");

        // Market quotes may be delivered with a given number of roll days.
        if (rollDays > 0) {
            Date roll;
            roll = calendar_.advance(result, -static_cast<Integer>(rollDays), Days);
            TLOG("CommodityVolCurve::getExpiry: roll days is " << rollDays << " giving a roll date "
                                                               << io::iso_date(roll) << ".");
            // Take the next option expiry if the roll days means the roll date is before asof.
            if (roll < asof) {
                result = expCalc_->nextExpiry(true, asof, 1, true);
                roll = calendar_.advance(result, -static_cast<Integer>(rollDays), Days);
                QL_REQUIRE(roll > asof, "CommodityVolCurve::getExpiry: expected roll to be greater than asof.");
                TLOG("CommodityVolCurve::getExpiry: roll date " << io::iso_date(roll) << " is less than asof "
                                                                << io::iso_date(asof) << " so take next option expiry "
                                                                << io::iso_date(result));
            }
        }

        // At this stage, 'result' should hold the next option expiry on or after the asof date accounting for roll 
        // days.
        TLOG("CommodityVolCurve::getExpiry: first option expiry is " << io::iso_date(result) << ".");

        // If the continuation index is greater than 1 get the corresponding expiry.
        Natural fcIndex = fcExpiry->expiryIndex();

        // The option continuation expiry may be mapped to another one.
        const auto& ocm = convention_->optionContinuationMappings();
        auto it = ocm.find(fcIndex);
        if (it != ocm.end())
            fcIndex = it->second;

        if (fcIndex > 1) {
            result += 1 * Days;
            result = expCalc_->nextExpiry(true, result, fcIndex - 2, true);
        }

        DLOG("Expiry date corresponding to continuation expiry, " << *fcExpiry <<
            ", is " << io::iso_date(result) << ".");

    } else {
        QL_FAIL("CommodityVolCurve::getExpiry: cannot determine expiry type.");
    }

    return result;
}

void CommodityVolCurve::populateCurves(const CommodityVolatilityConfig& config,
                                       const map<string, QuantLib::ext::shared_ptr<YieldCurve>>& yieldCurves,
                                       const map<string, QuantLib::ext::shared_ptr<CommodityCurve>>& commodityCurves,
                                       bool searchYield, bool dontThrow) {

    if (searchYield) {
        const string& ytsId = config.yieldCurveId();
        if (!ytsId.empty()) {
            auto itYts = yieldCurves.find(ytsId);
            if (itYts != yieldCurves.end()) {
                yts_ = itYts->second->handle();
            } else if (!dontThrow) {
                QL_FAIL("CommodityVolCurve: can't find yield curve with id " << ytsId);
            }
        } else if (!dontThrow) {
            QL_FAIL("CommodityVolCurve: YieldCurveId was not populated for " << config.curveID());
        }
    }

    const string& ptsId = config.priceCurveId();
    if (!ptsId.empty()) {
        auto itPts = commodityCurves.find(ptsId);
        if (itPts != commodityCurves.end()) {
            pts_ = Handle<PriceTermStructure>(itPts->second->commodityPriceCurve());
        } else if (!dontThrow) {
            QL_FAIL("CommodityVolCurve: can't find price curve with id " << ptsId);
        }
    } else if (!dontThrow) {
        QL_FAIL("CommodityVolCurve: PriceCurveId was not populated for " << config.curveID());
    }
}

vector<Real> CommodityVolCurve::checkMoneyness(const vector<string>& strMoneynessLevels) const {

    using boost::adaptors::transformed;
    using boost::algorithm::join;

    vector<Real> moneynessLevels = parseVectorOfValues<Real>(strMoneynessLevels, &parseReal);
    sort(moneynessLevels.begin(), moneynessLevels.end(), [](Real x, Real y) { return !close(x, y) && x < y; });
    QL_REQUIRE(adjacent_find(moneynessLevels.begin(), moneynessLevels.end(),
                             [](Real x, Real y) { return close(x, y); }) == moneynessLevels.end(),
               "The configured moneyness levels contain duplicates");
    DLOG("Parsed " << moneynessLevels.size() << " unique configured moneyness levels.");
    DLOG("The moneyness levels are: " << join(
             moneynessLevels | transformed([](Real d) { return ore::data::to_string(d); }), ","));

    return moneynessLevels;
}
void CommodityVolCurve::buildVolCalibrationInfo(const Date& asof, QuantLib::ext::shared_ptr<VolatilityConfig>& vc,
                                                const CurveConfigurations& curveConfigs,
                                                const CommodityVolatilityConfig& config) {
    DLOG("CommodityVolCurve: building volatility calibration info");
    try{

        ReportConfig rc = effectiveReportConfig(curveConfigs.reportConfigCommVols(), config.reportConfig());
        bool reportOnDeltaGrid = *rc.reportOnDeltaGrid();
        bool reportOnMoneynessGrid = *rc.reportOnMoneynessGrid();
        std::vector<Real> moneyness = *rc.moneyness();
        std::vector<std::string> deltas = *rc.deltas();
        std::vector<Period> expiries = *rc.expiries();


        auto info = QuantLib::ext::make_shared<FxEqCommVolCalibrationInfo>();

        DeltaVolQuote::AtmType atmType = DeltaVolQuote::AtmType::AtmDeltaNeutral;
        DeltaVolQuote::DeltaType deltaType = DeltaVolQuote::DeltaType::Fwd;

        if (auto vdsc = QuantLib::ext::dynamic_pointer_cast<VolatilityDeltaSurfaceConfig>(vc)) {
            atmType = parseAtmType(vdsc->atmType());
            deltaType = parseDeltaType(vdsc->deltaType());
        }

        info->dayCounter = to_string(dayCounter_);
        info->calendar = to_string(calendar_).empty() ? "na" : calendar_.name();
        info->atmType = ore::data::to_string(atmType);
        info->deltaType = ore::data::to_string(deltaType);
        info->longTermAtmType = ore::data::to_string(atmType);
        info->longTermDeltaType = ore::data::to_string(deltaType);
        info->switchTenor = "na";
        info->riskReversalInFavorOf = "na";
        info->butterflyStyle = "na";

        std::vector<Real> times, forwards;
        for (auto const& p : expiries) {
            auto d = volatility_->optionDateFromTenor(p);
            info->expiryDates.push_back(d);
            times.push_back(volatility_->dayCounter().empty() ? Actual365Fixed().yearFraction(asof, d)
                                                              : volatility_->timeFromReference(d));
            forwards.push_back(pts_->price(d));
        }

        info->times = times;
        info->forwards = forwards;
        std::vector<std::vector<Real>> callPricesDelta(times.size(), std::vector<Real>(deltas.size(), 0.0));
        if (reportOnDeltaGrid) {
            info->deltas = deltas;
            info->deltaCallPrices =
                    std::vector<std::vector<Real>>(times.size(), std::vector<Real>(deltas.size(), 0.0));
            info->deltaPutPrices =
                std::vector<std::vector<Real>>(times.size(), std::vector<Real>(deltas.size(), 0.0));
            info->deltaGridStrikes =
                std::vector<std::vector<Real>>(times.size(), std::vector<Real>(deltas.size(), 0.0));
            info->deltaGridProb = std::vector<std::vector<Real>>(times.size(), std::vector<Real>(deltas.size(), 0.0));
            info->deltaGridImpliedVolatility =
                std::vector<std::vector<Real>>(times.size(), std::vector<Real>(deltas.size(), 0.0));
            info->deltaGridCallSpreadArbitrage =
                std::vector<std::vector<bool>>(times.size(), std::vector<bool>(deltas.size(), true));
            info->deltaGridButterflyArbitrage =
                std::vector<std::vector<bool>>(times.size(), std::vector<bool>(deltas.size(), true));

            Real maxTime = QL_MAX_REAL;
            if (maxExpiry_ != Date()) {
                if (volatility_->dayCounter().empty())
                    maxTime = Actual365Fixed().yearFraction(asof, maxExpiry_);
                else
                    maxTime = volatility_->timeFromReference(maxExpiry_);
            }

            DeltaVolQuote::AtmType at;
            DeltaVolQuote::DeltaType dt;
            for (Size i = 0; i < times.size(); ++i) {
                Real t = times[i];
                at = atmType;
                dt = deltaType;
                // for times after the last quoted expiry we use artificial conventions to avoid problems with strike (as in equities)
                // from delta conversions: we use fwd delta always and ATM DNS
                if (t > maxTime) {
                    at = DeltaVolQuote::AtmDeltaNeutral;
                    dt = DeltaVolQuote::Fwd;
                }
                bool validSlice = true;
                for (Size j = 0; j < deltas.size(); ++j) {
                    DeltaString d(deltas[j]);
                    try {
                        Real strike;
                        if (d.isAtm()) {
                            strike =
                                QuantExt::getAtmStrike(dt, at, pts_->price(asof,true), yts_->discount(t), 1., volatility_, t);
                        } else if (d.isCall()) {
                            strike = QuantExt::getStrikeFromDelta(Option::Call, d.delta(), dt, pts_->price(asof,true),
                                                                  yts_->discount(t), 1., volatility_, t);
                        } else {
                            strike = QuantExt::getStrikeFromDelta(Option::Put, d.delta(), dt, pts_->price(asof,true),
                                                                  yts_->discount(t), 1., volatility_, t);
                        }
                        Real stddev = std::sqrt(volatility_->blackVariance(t, strike));
                        callPricesDelta[i][j] = QuantExt::blackFormula(Option::Call, strike, forwards[i], stddev);

                        if (d.isPut()) {
                            info->deltaPutPrices[i][j] = blackFormula(Option::Put, strike, forwards[i], stddev, yts_->discount(t));
                        } else {
                            info->deltaCallPrices[i][j] = blackFormula(Option::Call, strike, forwards[i], stddev, yts_->discount(t));
                        }

                        info->deltaGridStrikes[i][j] = strike;
                        info->deltaGridImpliedVolatility[i][j] = stddev / std::sqrt(t);
                    } catch (const std::exception& e) {
                        validSlice = false;
                        TLOG("CommodityVolCurve: error for time " << t << " delta " << deltas[j] << ": " << e.what());
                    }
                }
                if (validSlice) {
                    try {
                        QuantExt::CarrMadanMarginalProbability cm(info->deltaGridStrikes[i], forwards[i],
                                                                  callPricesDelta[i]);
                        info->deltaGridCallSpreadArbitrage[i] = cm.callSpreadArbitrage();
                        info->deltaGridButterflyArbitrage[i] = cm.butterflyArbitrage();
                        if (!cm.arbitrageFree())
                            info->isArbitrageFree = false;
                        info->deltaGridProb[i] = cm.density();
                        TLOGGERSTREAM(arbitrageAsString(cm));
                    } catch (const std::exception& e) {
                        TLOG("error for time " << t << ": " << e.what());
                        info->isArbitrageFree = false;
                        TLOGGERSTREAM("..(invalid slice)..");
                    }
                } else {
                    info->isArbitrageFree = false;
                    TLOGGERSTREAM("..(invalid slice)..");
                }
            }
            TLOG("CommodityVolCurve: Delta surface arbitrage analysis completed.");
        }
        std::vector<std::vector<Real>> callPricesMoneyness(times.size(), std::vector<Real>(moneyness.size(), 0.0));
        if (reportOnMoneynessGrid) {
            info->moneyness = moneyness;
            info->moneynessCallPrices =
                std::vector<std::vector<Real>>(times.size(), std::vector<Real>(moneyness.size(), 0.0));
            info->moneynessPutPrices =
                std::vector<std::vector<Real>>(times.size(), std::vector<Real>(moneyness.size(), 0.0));
            info->moneynessGridStrikes =
                std::vector<std::vector<Real>>(times.size(), std::vector<Real>(moneyness.size(), 0.0));
            info->moneynessGridProb =
                std::vector<std::vector<Real>>(times.size(), std::vector<Real>(moneyness.size(), 0.0));
            info->moneynessGridImpliedVolatility =
                std::vector<std::vector<Real>>(times.size(), std::vector<Real>(moneyness.size(), 0.0));
            info->moneynessGridCallSpreadArbitrage =
                std::vector<std::vector<bool>>(times.size(), std::vector<bool>(moneyness.size(), true));
            info->moneynessGridButterflyArbitrage =
                std::vector<std::vector<bool>>(times.size(), std::vector<bool>(moneyness.size(), true));
            info->moneynessGridCalendarArbitrage =
                std::vector<std::vector<bool>>(times.size(), std::vector<bool>(moneyness.size(), true));
            for (Size i = 0; i < times.size(); ++i) {
                Real t = times[i];
                for (Size j = 0; j < moneyness.size(); ++j) {
                    try {
                        Real strike = moneyness[j] * forwards[i];
                        info->moneynessGridStrikes[i][j] = strike;
                        Real stddev = std::sqrt(volatility_->blackVariance(t, strike));
                        callPricesMoneyness[i][j] = blackFormula(Option::Call, strike, forwards[i], stddev);
                        info->moneynessGridImpliedVolatility[i][j] = stddev / std::sqrt(t);
                        if (moneyness[j] >= 1) {
                            calibrationInfo_->moneynessCallPrices[i][j] = blackFormula(Option::Call, strike, forwards[i], stddev, yts_->discount(t));
                        } else {
                            calibrationInfo_->moneynessPutPrices[i][j] = blackFormula(Option::Put, strike, forwards[i], stddev, yts_->discount(t));
                        };
                    } catch (const std::exception& e) {
                        TLOG("CommodityVolCurve: error for time " << t << " moneyness " << moneyness[j] << ": " << e.what());
                    }
                }
            }
            if (!times.empty() && !moneyness.empty()) {
                try {
                    QuantExt::CarrMadanSurface cm(times, moneyness, pts_->price(asof,true), forwards,
                                                  callPricesMoneyness);
                    for (Size i = 0; i < times.size(); ++i) {
                        info->moneynessGridProb[i] = cm.timeSlices()[i].density();
                    }
                    info->moneynessGridCallSpreadArbitrage = cm.callSpreadArbitrage();
                    info->moneynessGridButterflyArbitrage = cm.butterflyArbitrage();
                    info->moneynessGridCalendarArbitrage = cm.calendarArbitrage();
                    if (!cm.arbitrageFree())
                        info->isArbitrageFree = false;
                    TLOG("CommodityVolCurve: Moneyness surface Arbitrage analysis result:");
                    TLOGGERSTREAM(arbitrageAsString(cm));
                } catch (const std::exception& e) {
                    TLOG("CommodityVolCurve: error: " << e.what());
                    info->isArbitrageFree = false;
                }
                TLOG("CommodityVolCurve: Moneyness surface Arbitrage analysis completed:");
            }
        }
    calibrationInfo_ = info;
    }
    catch (std::exception& e){
        QL_FAIL("CommodityVolCurve: calibration info building failed: " << e.what());
    } catch (...) {
        QL_FAIL("CommodityVolCurve:  calibration info building failed: unknown error");
    }
    }


} // namespace data
} // namespace ore
