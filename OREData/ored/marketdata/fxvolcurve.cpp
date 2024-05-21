/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 Copyright (C) 2023 Skandinaviska Enskilda Banken AB (publ)
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

#include <ored/marketdata/curvespecparser.hpp>
#include <ored/marketdata/fxvolcurve.hpp>
#include <ored/marketdata/structuredcurveerror.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>

#include <qle/indexes/fxindex.hpp>
#include <qle/models/carrmadanarbitragecheck.hpp>
#include <qle/termstructures/blackdeltautilities.hpp>
#include <qle/termstructures/blackinvertedvoltermstructure.hpp>
#include <qle/termstructures/blacktriangulationatmvol.hpp>
#include <qle/termstructures/blackvolsurfaceabsolute.hpp>
#include <qle/termstructures/blackvolsurfacebfrr.hpp>
#include <qle/termstructures/blackvolsurfacedelta.hpp>
#include <qle/termstructures/fxblackvolsurface.hpp>

#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancecurve.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

#include <string.h>
#include <algorithm>

using namespace QuantLib;
using namespace std;

namespace {

// utility to get a handle out of a Curve object
template <class T, class K> Handle<T> getHandle(const string& spec, const map<string, QuantLib::ext::shared_ptr<K>>& m) {
    auto it = m.find(spec);
    QL_REQUIRE(it != m.end(), "FXVolCurve: Can't find spec " << spec);
    return it->second->handle();
}

} // namespace

namespace ore {
namespace data {

// second ctor
FXVolCurve::FXVolCurve(Date asof, FXVolatilityCurveSpec spec, const Loader& loader,
                       const CurveConfigurations& curveConfigs, const FXTriangulation& fxSpots,
                       const map<string, QuantLib::ext::shared_ptr<YieldCurve>>& yieldCurves,
                       const std::map<string, QuantLib::ext::shared_ptr<FXVolCurve>>& fxVols,
                       const map<string, QuantLib::ext::shared_ptr<CorrelationCurve>>& correlationCurves,
                       const bool buildCalibrationInfo) {
    init(asof, spec, loader, curveConfigs, fxSpots, yieldCurves, fxVols, correlationCurves, buildCalibrationInfo);
}

void FXVolCurve::buildSmileDeltaCurve(Date asof, FXVolatilityCurveSpec spec, const Loader& loader,
                                      QuantLib::ext::shared_ptr<FXVolatilityCurveConfig> config, const FXTriangulation& fxSpots,
                                      const map<string, QuantLib::ext::shared_ptr<YieldCurve>>& yieldCurves) {
    vector<Period> unsortedExp;

    vector<std::pair<Real, string>> putDeltas, callDeltas;
    bool hasATM = false;

    for (auto const& delta : config->deltas()) {
        DeltaString d(delta);
        if (d.isAtm())
            hasATM = true;
        else if (d.isPut())
            putDeltas.push_back(std::make_pair(d.delta(), delta));
        else if (d.isCall())
            callDeltas.push_back(std::make_pair(d.delta(), delta));
    }

    Calendar cal = config->calendar();

    // sort puts 10P, 15P, 20P, ... and calls 45C, 40C, 35C, ... (notice put deltas have a negative sign)
    auto comp = [](const std::pair<Real, string>& x, const std::pair<Real, string>& y) { return x.first > y.first; };
    std::sort(putDeltas.begin(), putDeltas.end(), comp);
    std::sort(callDeltas.begin(), callDeltas.end(), comp);

    vector<Date> dates;
    Matrix blackVolMatrix;

    string base = "FX_OPTION/RATE_LNVOL/" + sourceCcy_ + "/" + targetCcy_ + "/";

    // build quote names
    std::vector<std::string> deltaNames;
    for (auto const& d : putDeltas) {
        deltaNames.push_back(d.second);
    }
    if (hasATM) {
        deltaNames.push_back("ATM");
    }
    for (auto const& d : callDeltas) {
        deltaNames.push_back(d.second);
    }

    if (expiriesWildcard_) {

        // we save relevant delta quotes to avoid looping twice
        std::vector<QuantLib::ext::shared_ptr<MarketDatum>> data;
        std::vector<std::string> expiriesStr;
        // get list of possible expiries
        std::ostringstream ss;
        ss << MarketDatum::InstrumentType::FX_OPTION << "/RATE_LNVOL/" << spec.unitCcy()<< "/"
            << spec.ccy()<< "/*";
        Wildcard w(ss.str());
        for (const auto& md : loader.get(w, asof)) {
            QL_REQUIRE(md->asofDate() == asof, "MarketDatum asofDate '" << md->asofDate() << "' <> asof '" << asof << "'");
            QuantLib::ext::shared_ptr<FXOptionQuote> q = QuantLib::ext::dynamic_pointer_cast<FXOptionQuote>(md);
            QL_REQUIRE(q, "Internal error: could not downcast MarketDatum '" << md->name() << "' to FXOptionQuote");
            QL_REQUIRE(q->unitCcy() == spec.unitCcy(),
                "FXOptionQuote unit ccy '" << q->unitCcy() << "' <> FXVolatilityCurveSpec unit ccy '" << spec.unitCcy() << "'");
            QL_REQUIRE(q->ccy() == spec.ccy(),
                "FXOptionQuote ccy '" << q->ccy() << "' <> FXVolatilityCurveSpec ccy '" << spec.ccy() << "'");
            Strike s = parseStrike(q->strike());
            if (s.type == Strike::Type::DeltaCall || s.type == Strike::Type::DeltaPut || s.type == Strike::Type::ATM) {
                vector<string> tokens;
                boost::split(tokens, md->name(), boost::is_any_of("/"));
                QL_REQUIRE(tokens.size() == 6, "6 tokens expected in " << md->name());
                if (expiriesWildcard_->matches(tokens[4])) {
                    data.push_back(md);
                    auto it = std::find(expiries_.begin(), expiries_.end(), q->expiry());
                    if (it == expiries_.end()) {
                        expiries_.push_back(q->expiry());
                        expiriesStr.push_back(tokens[4]);
                    }
                }
            }
        }
        unsortedExp = expiries_;
        std::sort(expiries_.begin(), expiries_.end());

        // we try to find all necessary quotes for each expiry
        vector<Size> validExpiryIdx;
        Matrix tmpMatrix(expiries_.size(), config->deltas().size());
        for (Size i = 0; i < expiries_.size(); i++) {
            Size idx = std::find(unsortedExp.begin(), unsortedExp.end(), expiries_[i]) - unsortedExp.begin();
            string e = expiriesStr[idx];
            for (Size j = 0; j < deltaNames.size(); ++j) {
                string qs = base + e + "/" + deltaNames[j];
                QuantLib::ext::shared_ptr<MarketDatum> md;
                for (auto& m : data) {
                    if (m->name() == qs) {
                        md = m;
                        break;
                    }
                }
                QuantLib::ext::shared_ptr<FXOptionQuote> q = QuantLib::ext::dynamic_pointer_cast<FXOptionQuote>(md);
                if (!q) {
                    DLOG("missing " << qs << ", expiry " << e << " will be excluded");
                    break;
                }
                tmpMatrix[i][j] = q->quote()->value();
                // if we have found all the quotes then this is a valid expiry
                if (j == deltaNames.size() - 1) {
                    dates.push_back(cal.advance(asof, expiries_[i]));
                    validExpiryIdx.push_back(i);
                }
            }
        }

        QL_REQUIRE(validExpiryIdx.size() > 0, "no valid FxVol expiries found");
        DLOG("found " << validExpiryIdx.size() << " valid expiries:");
        for (auto& e : validExpiryIdx)
            DLOG(expiries_[e]);
        // we build a matrix with just the valid expiries
        blackVolMatrix = Matrix(validExpiryIdx.size(), config->deltas().size());
        for (Size i = 0; i < validExpiryIdx.size(); i++) {
            for (Size j = 0; j < deltaNames.size(); ++j) {
                blackVolMatrix[i][j] = tmpMatrix[validExpiryIdx[i]][j];
            }
        }

    } else {
        expiries_ = parseVectorOfValues<Period>(expiriesNoDuplicates_, &parsePeriod);
        unsortedExp = parseVectorOfValues<Period>(expiriesNoDuplicates_, &parsePeriod);
        std::sort(expiries_.begin(), expiries_.end());

        blackVolMatrix = Matrix(expiries_.size(), config->deltas().size());
        for (Size i = 0; i < expiries_.size(); i++) {
            Size idx = std::find(unsortedExp.begin(), unsortedExp.end(), expiries_[i]) - unsortedExp.begin();
            string e = expiriesNoDuplicates_[idx];
            dates.push_back(cal.advance(asof, expiries_[i]));
            for (Size j = 0; j < deltaNames.size(); ++j) {
                string qs = base + e + "/" + deltaNames[j];
                QuantLib::ext::shared_ptr<MarketDatum> md = loader.get(qs, asof);
                QuantLib::ext::shared_ptr<FXOptionQuote> q = QuantLib::ext::dynamic_pointer_cast<FXOptionQuote>(md);
                QL_REQUIRE(q, "quote not found, " << qs);
                blackVolMatrix[i][j] = q->quote()->value();
            }
        }
    }

    QuantExt::InterpolatedSmileSection::InterpolationMethod interp;
    if (config->smileInterpolation() == FXVolatilityCurveConfig::SmileInterpolation::Linear)
        interp = QuantExt::InterpolatedSmileSection::InterpolationMethod::Linear;
    else if (config->smileInterpolation() == FXVolatilityCurveConfig::SmileInterpolation::Cubic)
        interp = QuantExt::InterpolatedSmileSection::InterpolationMethod::CubicSpline;
    else {
        QL_FAIL("Delta FX vol surface: invalid interpolation, expected Linear, Cubic");
    }

    bool flatExtrapolation = true;
    auto smileExtrapType = parseExtrapolation(config->smileExtrapolation());
    if (smileExtrapType == Extrapolation::UseInterpolator) {
        DLOG("Smile extrapolation switched to using interpolator.");
        flatExtrapolation = false;
    } else if (smileExtrapType == Extrapolation::None) {
        DLOG("Smile extrapolation cannot be turned off on its own so defaulting to flat.");
    } else if (smileExtrapType == Extrapolation::Flat) {
        DLOG("Smile extrapolation has been set to flat.");
    } else {
        DLOG("Smile extrapolation " << smileExtrapType << " not expected so defaulting to flat.");
    }

    // daycounter used for interpolation in time.
    // TODO: push into conventions or config
    DayCounter dc = config->dayCounter();
    std::vector<Real> putDeltasNum, callDeltasNum;
    std::transform(putDeltas.begin(), putDeltas.end(), std::back_inserter(putDeltasNum),
                   [](const std::pair<Real, string>& x) { return x.first; });
    std::transform(callDeltas.begin(), callDeltas.end(), std::back_inserter(callDeltasNum),
                   [](const std::pair<Real, string>& x) { return x.first; });
    vol_ = QuantLib::ext::make_shared<QuantExt::BlackVolatilitySurfaceDelta>(
        asof, dates, putDeltasNum, callDeltasNum, hasATM, blackVolMatrix, dc, cal, fxSpot_, domYts_, forYts_,
        deltaType_, atmType_, boost::none, switchTenor_, longTermDeltaType_, longTermAtmType_, boost::none, interp,
        flatExtrapolation);

    vol_->enableExtrapolation();
}

void FXVolCurve::buildSmileBfRrCurve(Date asof, FXVolatilityCurveSpec spec, const Loader& loader,
                                     QuantLib::ext::shared_ptr<FXVolatilityCurveConfig> config, const FXTriangulation& fxSpots,
                                     const map<string, QuantLib::ext::shared_ptr<YieldCurve>>& yieldCurves) {

    // collect relevant market data and populate expiries (as per regex or configured list)

    std::set<Period> expiriesTmp;

    std::vector<QuantLib::ext::shared_ptr<FXOptionQuote>> data;
    std::ostringstream ss;
    ss << MarketDatum::InstrumentType::FX_OPTION << "/RATE_LNVOL/" << spec.unitCcy()<< "/"
        << spec.ccy()<< "/*";
    Wildcard w(ss.str());
    for (const auto& md : loader.get(w, asof)) {
        QuantLib::ext::shared_ptr<FXOptionQuote> q = QuantLib::ext::dynamic_pointer_cast<FXOptionQuote>(md);
        QL_REQUIRE(q, "Internal error: could not downcast MarketDatum '" << md->name() << "' to FXOptionQuote");
        QL_REQUIRE(q->unitCcy() == spec.unitCcy(),
            "FXOptionQuote unit ccy '" << q->unitCcy() << "' <> FXVolatilityCurveSpec unit ccy '" << spec.unitCcy() << "'");
        QL_REQUIRE(q->ccy() == spec.ccy(),
            "FXOptionQuote ccy '" << q->ccy() << "' <> FXVolatilityCurveSpec ccy '" << spec.ccy() << "'");
        Strike s = parseStrike(q->strike());
        if (s.type == Strike::Type::BF || s.type == Strike::Type::RR || s.type == Strike::Type::ATM) {
            vector<string> tokens;
            boost::split(tokens, md->name(), boost::is_any_of("/"));
            QL_REQUIRE(tokens.size() == 6, "6 tokens expected in " << md->name());
            if (expiriesWildcard_ && expiriesWildcard_->matches(tokens[4]))
                expiriesTmp.insert(q->expiry());
            data.push_back(q);
        }
    }

    if (!expiriesWildcard_) {
        auto tmp = parseVectorOfValues<Period>(expiriesNoDuplicates_, &parsePeriod);
        expiriesTmp = std::set<Period>(tmp.begin(), tmp.end());
    }

    // populate quotes

    std::vector<Size> smileDeltas = config->smileDelta();
    std::sort(smileDeltas.begin(), smileDeltas.end());

    std::vector<std::vector<Real>> bfQuotesTmp(expiriesTmp.size(), std::vector<Real>(smileDeltas.size(), Null<Real>()));
    std::vector<std::vector<Real>> rrQuotesTmp(expiriesTmp.size(), std::vector<Real>(smileDeltas.size(), Null<Real>()));
    std::vector<Real> atmQuotesTmp(expiriesTmp.size(), Null<Real>());

    for (auto const& q : data) {
        Size expiryIdx = std::distance(expiriesTmp.begin(), expiriesTmp.find(q->expiry()));
        if (expiryIdx >= expiriesTmp.size())
            continue;
        Strike s = parseStrike(q->strike());
        if (s.type == Strike::Type::ATM) {
            atmQuotesTmp[expiryIdx] = q->quote()->value();
        } else {
            Size deltaIdx = std::distance(smileDeltas.begin(), std::find(smileDeltas.begin(), smileDeltas.end(),
                                                                         static_cast<Size>(s.value + 0.5)));
            if (deltaIdx >= smileDeltas.size())
                continue;
            if (s.type == Strike::Type::BF) {
                bfQuotesTmp[expiryIdx][deltaIdx] = q->quote()->value();
            } else if (s.type == Strike::Type::RR) {
                rrQuotesTmp[expiryIdx][deltaIdx] = q->quote()->value();
            }
        }
    }

    // identify the rows with complete data

    std::vector<bool> dataComplete(expiriesTmp.size(), true);

    for (Size i = 0; i < expiriesTmp.size(); ++i) {
        for (Size j = 0; j < smileDeltas.size(); ++j) {
            if (bfQuotesTmp[i][j] == Null<Real>() || rrQuotesTmp[i][j] == Null<Real>() ||
                atmQuotesTmp[i] == Null<Real>())
                dataComplete[i] = false;
        }
    }

    // if we have an explicitly configured expiry list, we require that the data is complete for all expiries

    if (!expiriesWildcard_) {
        Size i = 0;
        for (auto const& e : expiriesTmp) {
            QL_REQUIRE(dataComplete[i++], "BFRR FX vol surface: incomplete data for expiry " << e);
        }
    }

    // build the final quotes for the expiries that have complete data

    Size i = 0;
    for (auto const& e : expiriesTmp) {
        if (dataComplete[i++]) {
            expiries_.push_back(e);
            TLOG("adding expiry " << e << " with complete data");
        } else {
            TLOG("removing expiry " << e << ", because data is not complete");
        }
    }

    std::vector<std::vector<Real>> bfQuotes(expiries_.size(), std::vector<Real>(smileDeltas.size()));
    std::vector<std::vector<Real>> rrQuotes(expiries_.size(), std::vector<Real>(smileDeltas.size()));
    std::vector<Real> atmQuotes(expiries_.size());

    Size row = 0;
    for (Size i = 0; i < expiriesTmp.size(); ++i) {
        if (!dataComplete[i])
            continue;
        atmQuotes[row] = atmQuotesTmp[i];
        for (Size j = 0; j < smileDeltas.size(); ++j) {
            bfQuotes[row][j] = bfQuotesTmp[i][j];
            rrQuotes[row][j] = rrQuotesTmp[i][j];
        }
        ++row;
    }

    // build BFRR surface

    DLOG("build BFRR fx vol surface with " << expiries_.size() << " expiries and " << smileDeltas.size()
                                           << " delta(s)");

    QuantExt::BlackVolatilitySurfaceBFRR::SmileInterpolation interp;
    if (config->smileInterpolation() == FXVolatilityCurveConfig::SmileInterpolation::Linear)
        interp = QuantExt::BlackVolatilitySurfaceBFRR::SmileInterpolation::Linear;
    else if (config->smileInterpolation() == FXVolatilityCurveConfig::SmileInterpolation::Cubic)
        interp = QuantExt::BlackVolatilitySurfaceBFRR::SmileInterpolation::Cubic;
    else {
        QL_FAIL("BFRR FX vol surface: invalid interpolation, expected Linear, Cubic");
    }

    std::vector<Date> dates;
    std::transform(expiries_.begin(), expiries_.end(), std::back_inserter(dates),
                   [&asof, &config](const Period& p) { return config->calendar().advance(asof, p); });

    std::vector<Real> smileDeltasScaled;
    std::transform(smileDeltas.begin(), smileDeltas.end(), std::back_inserter(smileDeltasScaled),
                   [](Size d) { return static_cast<Real>(d) / 100.0; });

    vol_ = QuantLib::ext::make_shared<QuantExt::BlackVolatilitySurfaceBFRR>(
        asof, dates, smileDeltasScaled, bfQuotes, rrQuotes, atmQuotes, config->dayCounter(), config->calendar(),
        fxSpot_, spotDays_, spotCalendar_, domYts_, forYts_, deltaType_, atmType_, switchTenor_, longTermDeltaType_,
        longTermAtmType_, riskReversalInFavorOf_, butterflyIsBrokerStyle_, interp);

    vol_->enableExtrapolation();
}

void FXVolCurve::buildVannaVolgaOrATMCurve(Date asof, FXVolatilityCurveSpec spec, const Loader& loader,
                                           QuantLib::ext::shared_ptr<FXVolatilityCurveConfig> config,
                                           const FXTriangulation& fxSpots,
                                           const map<string, QuantLib::ext::shared_ptr<YieldCurve>>& yieldCurves) {

    bool isATM = config->dimension() == FXVolatilityCurveConfig::Dimension::ATM;
    Natural smileDelta = 0;
    std::string deltaRr;
    std::string deltaBf;
    if (!isATM) {
        QL_REQUIRE(config->smileDelta().size() == 1,
                   "Exactly one SmileDelta required for VannaVolga Curve (got " << config->smileDelta().size() << ")");
        smileDelta = config->smileDelta().front();
        deltaRr = to_string(smileDelta) + "RR";
        deltaBf = to_string(smileDelta) + "BF";
    }
    // We loop over all market data, looking for quotes that match the configuration
    // every time we find a matching expiry we remove it from the list
    // we replicate this for all 3 types of quotes were applicable.
    Size n = isATM ? 1 : 3; // [0] = ATM, [1] = RR, [2] = BF
    vector<vector<QuantLib::ext::shared_ptr<FXOptionQuote>>> quotes(n);

    QL_REQUIRE(!expiriesWildcard_ || isATM, "wildcards only supported for ATM, Delta, BFRR FxVol Curves");

    vector<Period> cExpiries;
    vector<vector<Period>> expiries;
    // Create the regular expression
    if (!expiriesWildcard_) {
        cExpiries = parseVectorOfValues<Period>(expiriesNoDuplicates_, &parsePeriod);
        expiries = vector<vector<Period>>(n, cExpiries);
    }

    // Load the relevant quotes
    std::vector<QuantLib::ext::shared_ptr<FXOptionQuote>> data;
    std::ostringstream ss;
    ss << MarketDatum::InstrumentType::FX_OPTION << "/RATE_LNVOL/" << spec.unitCcy()<< "/"
        << spec.ccy()<< "/*";
    Wildcard w(ss.str());
    for (const auto& md : loader.get(w, asof)) {

        QuantLib::ext::shared_ptr<FXOptionQuote> q = QuantLib::ext::dynamic_pointer_cast<FXOptionQuote>(md);
        QL_REQUIRE(q, "Internal error: could not downcast MarketDatum '" << md->name() << "' to FXOptionQuote");
        QL_REQUIRE(q->unitCcy() == spec.unitCcy(),
            "FXOptionQuote unit ccy '" << q->unitCcy() << "' <> FXVolatilityCurveSpec unit ccy '" << spec.unitCcy() << "'");
        QL_REQUIRE(q->ccy() == spec.ccy(),
            "FXOptionQuote ccy '" << q->ccy() << "' <> FXVolatilityCurveSpec ccy '" << spec.ccy() << "'");


        Size idx = 999999;
        if (q->strike() == "ATM")
            idx = 0;
        else if (!isATM && q->strike() == deltaRr)
            idx = 1;
        else if (!isATM && q->strike() == deltaBf)
            idx = 2;

        // silently skip unknown strike strings
        if ((isATM && idx == 0) || (!isATM && idx <= 2)) {
            if (expiriesWildcard_) {
                vector<string> tokens;
                boost::split(tokens, md->name(), boost::is_any_of("/"));
                QL_REQUIRE(tokens.size() == 6, "6 tokens expected in " << md->name());
                if (expiriesWildcard_->matches(tokens[4])) {
                    quotes[idx].push_back(q);
                }
            } else {
                auto it = std::find(expiries[idx].begin(), expiries[idx].end(), q->expiry());
                if (it != expiries[idx].end()) {
                    // we have a hit
                    quotes[idx].push_back(q);
                    // remove it from the list
                    expiries[idx].erase(it);
                }

                // check if we are done
                // for ATM we just check expiries[0], otherwise we check all 3
                if (expiries[0].empty() && (isATM || (expiries[1].empty() && expiries[2].empty())))
                    break;
            }
        }
    }

    // Check ATM first
    // Check that we have all the expiries we need
    LOG("FXVolCurve: read " << quotes[0].size() << " ATM vols");
    if (!expiriesWildcard_) {
        QL_REQUIRE(expiries[0].size() == 0,
                   "No ATM quote found for spec " << spec << " with expiry " << expiries[0].front());
    }

    QL_REQUIRE(quotes[0].size() > 0, "No ATM quotes found for spec " << spec);
    // No check the rest
    if (!isATM) {
        LOG("FXVolCurve: read " << quotes[1].size() << " RR and " << quotes[2].size() << " BF quotes");
        QL_REQUIRE(expiries[1].size() == 0,
                   "No RR quote found for spec " << spec << " with expiry " << expiries[1].front());
        QL_REQUIRE(expiries[2].size() == 0,
                   "No BF quote found for spec " << spec << " with expiry " << expiries[2].front());
    }

    // sort all quotes
    for (Size i = 0; i < n; i++) {
        std::sort(quotes[i].begin(), quotes[i].end(),
                  [](const QuantLib::ext::shared_ptr<FXOptionQuote>& a, const QuantLib::ext::shared_ptr<FXOptionQuote>& b) -> bool {
                      return a->expiry() < b->expiry();
                  });
    }

    // daycounter used for interpolation in time.
    // TODO: push into conventions or config
    DayCounter dc = config->dayCounter();
    Calendar cal = config->calendar();

    // build vol curve
    if (isATM && quotes[0].size() == 1) {
        vol_ = QuantLib::ext::shared_ptr<BlackVolTermStructure>(
            new BlackConstantVol(asof, config->calendar(), quotes[0].front()->quote()->value(), dc));
        expiries_ = {quotes[0].front()->expiry()};
    } else {

        Size numExpiries = quotes[0].size();
        vector<Date> dates(numExpiries);
        vector<vector<Volatility>> vols(n, vector<Volatility>(numExpiries)); // same as above: [0] = ATM, etc.

        for (Size i = 0; i < numExpiries; i++) {
            dates[i] = cal.advance(asof, quotes[0][i]->expiry());
            expiries_.push_back(quotes[0][i]->expiry());
            DLOG("Spec Tenor Vol Variance");
            for (Size idx = 0; idx < n; idx++) {
                vols[idx][i] = quotes[idx][i]->quote()->value();
                Real variance = vols[idx][i] * vols[idx][i] * (dates[i] - asof) / 365.0; // approximate variance
                DLOG(spec << " " << quotes[0][i]->expiry() << " " << vols[idx][i] << " " << variance);
            }
        }

        if (isATM) {
            // ATM
            // Set forceMonotoneVariance to false - allowing decreasing variance
            vol_ = QuantLib::ext::shared_ptr<BlackVolTermStructure>(new BlackVarianceCurve(asof, dates, vols[0], dc, false));
        } else {
            // Smile
            bool vvFirstApprox = false; // default to VannaVolga second approximation
            if (config->smileInterpolation() == FXVolatilityCurveConfig::SmileInterpolation::VannaVolga1) {
                vvFirstApprox = true;
            }

            vol_ = QuantLib::ext::make_shared<QuantExt::FxBlackVannaVolgaVolatilitySurface>(
                asof, dates, vols[0], vols[1], vols[2], dc, cal, fxSpot_, domYts_, forYts_, false, vvFirstApprox,
                atmType_, deltaType_, smileDelta / 100.0, switchTenor_, longTermAtmType_, longTermDeltaType_);
        }
    }
    vol_->enableExtrapolation();
}

void FXVolCurve::buildSmileAbsoluteCurve(Date asof, FXVolatilityCurveSpec spec, const Loader& loader,
                                         QuantLib::ext::shared_ptr<FXVolatilityCurveConfig> config, const FXTriangulation& fxSpots,
                                         const map<string, QuantLib::ext::shared_ptr<YieldCurve>>& yieldCurves) {

    // collect relevant market data and populate expiries (as per regex or configured list)
    std::set<Period> expiriesTmp;

    std::vector<QuantLib::ext::shared_ptr<FXOptionQuote>> data;
    std::ostringstream ss;
    ss << MarketDatum::InstrumentType::FX_OPTION << "/RATE_LNVOL/" << spec.unitCcy() << "/" << spec.ccy() << "/*";
    Wildcard w(ss.str());
    for (const auto& md : loader.get(w, asof)) {
        QuantLib::ext::shared_ptr<FXOptionQuote> q = QuantLib::ext::dynamic_pointer_cast<FXOptionQuote>(md);
        QL_REQUIRE(q, "Internal error: could not downcast MarketDatum '" << md->name() << "' to FXOptionQuote");
        QL_REQUIRE(q->unitCcy() == spec.unitCcy(), "FXOptionQuote unit ccy '" << q->unitCcy()
                                                                              << "' <> FXVolatilityCurveSpec unit ccy '"
                                                                              << spec.unitCcy() << "'");
        QL_REQUIRE(q->ccy() == spec.ccy(),
                   "FXOptionQuote ccy '" << q->ccy() << "' <> FXVolatilityCurveSpec ccy '" << spec.ccy() << "'");
        Strike s = parseStrike(q->strike());
        if (s.type == Strike::Type::Absolute) {
            vector<string> tokens;
            boost::split(tokens, md->name(), boost::is_any_of("/"));
            QL_REQUIRE(tokens.size() == 6, "6 tokens expected in " << md->name());
            if (expiriesWildcard_ && expiriesWildcard_->matches(tokens[4]))
                expiriesTmp.insert(q->expiry());
            data.push_back(q);
        }
    }

    if (!expiriesWildcard_) {
        auto tmp = parseVectorOfValues<Period>(expiriesNoDuplicates_, &parsePeriod);
        expiriesTmp = std::set<Period>(tmp.begin(), tmp.end());
    }

    // populate quotes
    std::vector<std::map<Real, Real>> strikeQuotesTmp(expiriesTmp.size());

    for (auto const& q : data) {
        Size expiryIdx = std::distance(expiriesTmp.begin(), expiriesTmp.find(q->expiry()));
        if (expiryIdx >= expiriesTmp.size())
            continue;
        Strike s = parseStrike(q->strike());
        // If the strike for expirtIdx does not exist, read in the quote
        if (strikeQuotesTmp[expiryIdx].count(s.value) == 0)
            strikeQuotesTmp[expiryIdx][s.value] = q->quote()->value();
    }

    // identify the expiries with at least one strike quote
    std::vector<bool> dataComplete(expiriesTmp.size(), true);

    for (Size i = 0; i < expiriesTmp.size(); ++i) {
        if (strikeQuotesTmp[i].empty())
            dataComplete[i] = false;
    }

    // if we have an explicitly configured expiry list, we require that there is at least one strike quote for all expiries

    if (!expiriesWildcard_) {
        Size i = 0;
        for (auto const& e : expiriesTmp) {
            QL_REQUIRE(dataComplete[i++], "Absolute FX vol surface: missing data for expiry " << e);
        }
    }

    // build the final quotes for the expiries that have complete data
    Size i = 0;
    for (auto const& e : expiriesTmp) {
        if (dataComplete[i++]) {
            expiries_.push_back(e);
            TLOG("adding expiry " << e << " with at least one strike quote");
        } else {
            TLOG("removing expiry " << e << ", no strike quote found");
        }
    }

    std::vector<std::vector<Real>> strikeQuotes, strikes;
    std::vector<Real> strikeQuote, strike;

    for (Size i = 0; i < expiriesTmp.size(); ++i) {
        if (!dataComplete[i])
            continue;
        for (auto const& quote : strikeQuotesTmp[i]) {
            strike.push_back(quote.first);
            strikeQuote.push_back(quote.second);
        }
        strikeQuotes.push_back(strikeQuote);
        strikes.push_back(strike);
        strikeQuote.clear();
        strike.clear();
    }

    // build Absolute surface

    DLOG("build Absolute fx vol surface with " << expiries_.size() << " expiries");

    QuantExt::BlackVolatilitySurfaceAbsolute::SmileInterpolation interp;
    if (config->smileInterpolation() == FXVolatilityCurveConfig::SmileInterpolation::Linear)
        interp = QuantExt::BlackVolatilitySurfaceAbsolute::SmileInterpolation::Linear;
    else if (config->smileInterpolation() == FXVolatilityCurveConfig::SmileInterpolation::Cubic)
        interp = QuantExt::BlackVolatilitySurfaceAbsolute::SmileInterpolation::Cubic;
    else {
        QL_FAIL("Absolute FX vol surface: invalid interpolation, expected Linear, Cubic");
    }

    std::vector<Date> dates;
    std::transform(expiries_.begin(), expiries_.end(), std::back_inserter(dates),
                   [&asof, &config](const Period& p) { return config->calendar().advance(asof, p); });

    vol_ = QuantLib::ext::make_shared<QuantExt::BlackVolatilitySurfaceAbsolute>(
        asof, dates, strikes, strikeQuotes, config->dayCounter(), config->calendar(),
        fxSpot_, spotDays_, spotCalendar_, domYts_, forYts_, deltaType_, atmType_, switchTenor_, longTermDeltaType_,
        longTermAtmType_, interp);

    vol_->enableExtrapolation();
}

Handle<QuantExt::CorrelationTermStructure>
getCorrelationCurve(const std::string& index1, const std::string& index2,
                    const map<string, QuantLib::ext::shared_ptr<CorrelationCurve>>& correlationCurves) {
    // straight pair
    auto tmpCorr = correlationCurves.find("Correlation/" + index1 + "&" + index2);
    if (tmpCorr != correlationCurves.end()) {
        return Handle<QuantExt::CorrelationTermStructure>(tmpCorr->second->corrTermStructure());
    }
    // inverse pair
    tmpCorr = correlationCurves.find("Correlation/" + index2 + "&" + index1);
    if (tmpCorr != correlationCurves.end()) {
        return Handle<QuantExt::CorrelationTermStructure>(tmpCorr->second->corrTermStructure());
    }
    // inverse fx index1
    tmpCorr = correlationCurves.find("Correlation/" + inverseFxIndex(index1) + "&" + index2);
    if (tmpCorr != correlationCurves.end()) {
        Handle<QuantExt::CorrelationTermStructure> h =
            Handle<QuantExt::CorrelationTermStructure>(tmpCorr->second->corrTermStructure());
        return Handle<QuantExt::CorrelationTermStructure>(
            QuantLib::ext::make_shared<QuantExt::NegativeCorrelationTermStructure>(h));
    }
    tmpCorr = correlationCurves.find("Correlation/" + index2 + "&" + inverseFxIndex(index1));
    if (tmpCorr != correlationCurves.end()) {
        Handle<QuantExt::CorrelationTermStructure> h =
            Handle<QuantExt::CorrelationTermStructure>(tmpCorr->second->corrTermStructure());
        return Handle<QuantExt::CorrelationTermStructure>(
            QuantLib::ext::make_shared<QuantExt::NegativeCorrelationTermStructure>(h));
    }
    // inverse fx index2
    tmpCorr = correlationCurves.find("Correlation/" + index1 + "&" + inverseFxIndex(index2));
    if (tmpCorr != correlationCurves.end()) {
        Handle<QuantExt::CorrelationTermStructure> h =
            Handle<QuantExt::CorrelationTermStructure>(tmpCorr->second->corrTermStructure());
        return Handle<QuantExt::CorrelationTermStructure>(
            QuantLib::ext::make_shared<QuantExt::NegativeCorrelationTermStructure>(h));
    }
    tmpCorr = correlationCurves.find("Correlation/" + inverseFxIndex(index2) + "&" + index1);
    if (tmpCorr != correlationCurves.end()) {
        Handle<QuantExt::CorrelationTermStructure> h =
            Handle<QuantExt::CorrelationTermStructure>(tmpCorr->second->corrTermStructure());
        return Handle<QuantExt::CorrelationTermStructure>(
            QuantLib::ext::make_shared<QuantExt::NegativeCorrelationTermStructure>(h));
    }
    // both fx indices inverted
    tmpCorr = correlationCurves.find("Correlation/" + inverseFxIndex(index1) + "&" + inverseFxIndex(index2));
    if (tmpCorr != correlationCurves.end()) {
        return Handle<QuantExt::CorrelationTermStructure>(tmpCorr->second->corrTermStructure());
    }
    tmpCorr = correlationCurves.find("Correlation/" + inverseFxIndex(index2) + "&" + inverseFxIndex(index1));
    if (tmpCorr != correlationCurves.end()) {
        return Handle<QuantExt::CorrelationTermStructure>(tmpCorr->second->corrTermStructure());
    }

    QL_FAIL("no correlation curve found for " << index1 << ":" << index2);
}

void FXVolCurve::buildATMTriangulated(Date asof, FXVolatilityCurveSpec spec, const Loader& loader,
                                      QuantLib::ext::shared_ptr<FXVolatilityCurveConfig> config, const FXTriangulation& fxSpots,
                                      const map<string, QuantLib::ext::shared_ptr<YieldCurve>>& yieldCurves,
                                      const map<string, QuantLib::ext::shared_ptr<FXVolCurve>>& fxVols,
                                      const map<string, QuantLib::ext::shared_ptr<CorrelationCurve>>& correlationCurves) {

    DLOG("Triangulating FxVol curve " << config->curveID() << " from baseVols " << config->baseVolatility1() << ":"
                                      << config->baseVolatility2());

    std::string baseCcy;
    QL_REQUIRE(config->baseVolatility1().size() == 6, "invalid ccy pair length for baseVolatility1");
    auto forBase1 = config->baseVolatility1().substr(0, 3);
    auto domBase1 = config->baseVolatility1().substr(3);
    std::string spec1 = "FXVolatility/" + forBase1 + "/" + domBase1 + "/" + config->baseVolatility1();

    bool base1Inverted = false;
    if (forBase1 != sourceCcy_ && forBase1 != targetCcy_) {
        // we invert the pair
        base1Inverted = true;
        std::string tmp = forBase1;
        forBase1 = targetCcy_;
        domBase1 = tmp;

        QL_REQUIRE(forBase1 == sourceCcy_ || forBase1 == targetCcy_,
                   "FxVol: mismatch in the baseVolatility1 " << config->baseVolatility1() << " and Target Pair "
                                                             << sourceCcy_ << targetCcy_);
    }
    baseCcy = domBase1;

    QL_REQUIRE(config->baseVolatility2().size() == 6, "invalid ccy pair length for baseVolatility2");
    auto forBase2 = config->baseVolatility2().substr(0, 3);
    auto domBase2 = config->baseVolatility2().substr(3);
    std::string spec2 = "FXVolatility/" + forBase2 + "/" + domBase2 + "/" + config->baseVolatility2();
    bool base2Inverted = false;

    QL_REQUIRE(forBase2 == baseCcy || domBase2 == baseCcy,
               "baseVolatility2 must share a ccy code with the baseVolatility1");

    if (forBase2 != sourceCcy_ && forBase2 != targetCcy_) {
        // we invert the pair
        std::string tmp = forBase2;
        forBase2 = targetCcy_;
        domBase2 = tmp;
        base2Inverted = true;
    }

    auto tmp = fxVols.find(spec1);
    QL_REQUIRE(tmp != fxVols.end(), "fx vol not found for " << config->baseVolatility1());
    Handle<BlackVolTermStructure> forBaseVol;
    if (base1Inverted) {
        auto h = Handle<BlackVolTermStructure>(tmp->second->volTermStructure());
        if (!h.empty())
            forBaseVol = Handle<BlackVolTermStructure>(QuantLib::ext::make_shared<QuantExt::BlackInvertedVolTermStructure>(h));
    } else {
        forBaseVol = Handle<BlackVolTermStructure>(tmp->second->volTermStructure());
    }
    forBaseVol->enableExtrapolation();

    tmp = fxVols.find(spec2);
    QL_REQUIRE(tmp != fxVols.end(), "fx vol not found for " << config->baseVolatility2());
    Handle<BlackVolTermStructure> domBaseVol;
    if (base2Inverted) {
        auto h = Handle<BlackVolTermStructure>(tmp->second->volTermStructure());
        if (!h.empty())
            domBaseVol = Handle<BlackVolTermStructure>(QuantLib::ext::make_shared<QuantExt::BlackInvertedVolTermStructure>(h));

    } else {
        domBaseVol = Handle<BlackVolTermStructure>(tmp->second->volTermStructure());
    }
    domBaseVol->enableExtrapolation();

    string forIndex = "FX-" + config->fxIndexTag() + "-" + sourceCcy_ + "-" + baseCcy;
    string domIndex = "FX-" + config->fxIndexTag() + "-" + targetCcy_ + "-" + baseCcy;

    Handle<QuantExt::CorrelationTermStructure> rho = getCorrelationCurve(forIndex, domIndex, correlationCurves);

    vol_ = QuantLib::ext::make_shared<QuantExt::BlackTriangulationATMVolTermStructure>(forBaseVol, domBaseVol, rho);
    vol_->enableExtrapolation();
}

void FXVolCurve::init(Date asof, FXVolatilityCurveSpec spec, const Loader& loader,
                      const CurveConfigurations& curveConfigs, const FXTriangulation& fxSpots,
                      const map<string, QuantLib::ext::shared_ptr<YieldCurve>>& yieldCurves,
                      const std::map<string, QuantLib::ext::shared_ptr<FXVolCurve>>& fxVols,
                      const map<string, QuantLib::ext::shared_ptr<CorrelationCurve>>& correlationCurves,
                      const bool buildCalibrationInfo) {
    try {

        const QuantLib::ext::shared_ptr<FXVolatilityCurveConfig>& config = curveConfigs.fxVolCurveConfig(spec.curveConfigID());
        QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();

        QL_REQUIRE(config->dimension() == FXVolatilityCurveConfig::Dimension::ATM ||
                       config->dimension() == FXVolatilityCurveConfig::Dimension::ATMTriangulated ||
                       config->dimension() == FXVolatilityCurveConfig::Dimension::SmileVannaVolga ||
                       config->dimension() == FXVolatilityCurveConfig::Dimension::SmileDelta ||
                       config->dimension() == FXVolatilityCurveConfig::Dimension::SmileBFRR ||
                       config->dimension() == FXVolatilityCurveConfig::Dimension::SmileAbsolute,
                   "Unknown FX curve building dimension");

        expiriesWildcard_ = getUniqueWildcard(config->expiries());

        // remove expiries that would lead to duplicate expiry dates (keep the later expiry in this case)

        if (expiriesWildcard_) {
            DLOG("expiry wildcard is used: " << expiriesWildcard_->pattern());
        } else {
            std::vector<std::tuple<std::string, Period, Date>> tmp;
            for (auto const& e : config->expiries()) {
                auto p = parsePeriod(e);
                tmp.push_back(std::make_tuple(e, p, config->calendar().advance(asof, p)));
            }
            std::sort(tmp.begin(), tmp.end(),
                      [](const std::tuple<std::string, Period, Date>& a,
                         const std::tuple<std::string, Period, Date>& b) -> bool {
                          if (std::get<2>(a) == std::get<2>(b)) {
                              try {
                                  // equal dates => compare periods (might throw!)
                                  return std::get<1>(a) < std::get<1>(b);
                              } catch (...) {
                                  // period comparison not possible => compare the strings
                                  return std::get<0>(a) < std::get<0>(b);
                              }
                          }
                          // different dates => compare them
                          return std::get<2>(a) < std::get<2>(b);
                      });
            Date lastDate = Date::maxDate();
            for (Size i = tmp.size(); i > 0; --i) {
                if (std::get<2>(tmp[i - 1]) == lastDate)
                    continue;
                expiriesNoDuplicates_.insert(expiriesNoDuplicates_.begin(), std::get<0>(tmp[i - 1]));
                lastDate = std::get<2>(tmp[i - 1]);
            }

            DLOG("expiries in configuration:")
            for (auto const& e : config->expiries()) {
                DLOG(e);
            }

            DLOG("expiries after removing duplicate expiry dates and sorting:");
            for (auto const& e : expiriesNoDuplicates_) {
                DLOG(e);
            }
        }

        QL_REQUIRE(config->dimension() == FXVolatilityCurveConfig::Dimension::ATMTriangulated || expiriesWildcard_ ||
                       !expiriesNoDuplicates_.empty(),
                   "no expiries after removing duplicate expiry dates");

        std::vector<std::string> tokens;
        boost::split(tokens, config->fxSpotID(), boost::is_any_of("/"));
        QL_REQUIRE(tokens.size() == 3, "Expected 3 tokens in fx spot id '" << config->fxSpotID() << "'");
        sourceCcy_ = tokens[1];
        targetCcy_ = tokens[2];

        atmType_ = DeltaVolQuote::AtmType::AtmDeltaNeutral;
        deltaType_ = DeltaVolQuote::DeltaType::Spot;
        switchTenor_ = 2 * Years;
        longTermAtmType_ = DeltaVolQuote::AtmType::AtmDeltaNeutral;
        longTermDeltaType_ = DeltaVolQuote::DeltaType::Fwd;
        riskReversalInFavorOf_ = Option::Call;
        butterflyIsBrokerStyle_ = true;
        spotDays_ = 2;
        string calTmp = sourceCcy_ + "," + targetCcy_;
        spotCalendar_ = parseCalendar(calTmp);

        if (config->conventionsID() == "") {
            WLOG("no fx option conventions given in fxvol curve config for " << spec.curveConfigID()
                                                                             << ", assuming defaults");
        } else {
            auto fxOptConv = QuantLib::ext::dynamic_pointer_cast<FxOptionConvention>(conventions->get(config->conventionsID()));
            QL_REQUIRE(fxOptConv,
                       "unable to cast convention '" << config->conventionsID() << "' into FxOptionConvention");
            QuantLib::ext::shared_ptr<FXConvention> fxConv;
            if (!fxOptConv->fxConventionID().empty()) {
                fxConv = QuantLib::ext::dynamic_pointer_cast<FXConvention>(conventions->get(fxOptConv->fxConventionID()));
                QL_REQUIRE(fxConv, "unable to cast convention '" << fxOptConv->fxConventionID()
                                                                 << "', from FxOptionConvention '"
                                                                 << config->conventionsID() << "' into FxConvention");
            }
            atmType_ = fxOptConv->atmType();
            deltaType_ = fxOptConv->deltaType();
            longTermAtmType_ = fxOptConv->longTermAtmType();
            longTermDeltaType_ = fxOptConv->longTermDeltaType();
            switchTenor_ = fxOptConv->switchTenor();
            riskReversalInFavorOf_ = fxOptConv->riskReversalInFavorOf();
            butterflyIsBrokerStyle_ = fxOptConv->butterflyIsBrokerStyle();
            if (fxConv) {
                spotDays_ = fxConv->spotDays();
                spotCalendar_ = fxConv->advanceCalendar();
            }
        }

        auto spotSpec = QuantLib::ext::dynamic_pointer_cast<FXSpotSpec>(parseCurveSpec(config->fxSpotID()));
        QL_REQUIRE(spotSpec != nullptr,
                   "could not parse '" << config->fxSpotID() << "' to FXSpotSpec, expected FX/CCY1/CCY2");
        fxSpot_ = fxSpots.getQuote(spotSpec->unitCcy() + spotSpec->ccy());
        if (!config->fxDomesticYieldCurveID().empty())
            domYts_ = getHandle<YieldTermStructure>(config->fxDomesticYieldCurveID(), yieldCurves);
        if (!config->fxForeignYieldCurveID().empty())
            forYts_ = getHandle<YieldTermStructure>(config->fxForeignYieldCurveID(), yieldCurves);

        if (config->dimension() == FXVolatilityCurveConfig::Dimension::SmileDelta) {
            buildSmileDeltaCurve(asof, spec, loader, config, fxSpots, yieldCurves);
        } else if (config->dimension() == FXVolatilityCurveConfig::Dimension::SmileBFRR) {
            buildSmileBfRrCurve(asof, spec, loader, config, fxSpots, yieldCurves);
        } else if (config->dimension() == FXVolatilityCurveConfig::Dimension::ATMTriangulated) {
            buildATMTriangulated(asof, spec, loader, config, fxSpots, yieldCurves, fxVols, correlationCurves);
        } else if (config->dimension() == FXVolatilityCurveConfig::Dimension::SmileAbsolute) {
            buildSmileAbsoluteCurve(asof, spec, loader, config, fxSpots, yieldCurves);
        } else {
            buildVannaVolgaOrATMCurve(asof, spec, loader, config, fxSpots, yieldCurves);
        }

        // build calibration info

        if (buildCalibrationInfo) {

            DLOG("Building calibration info for fx vol surface");

            if (domYts_.empty() || forYts_.empty()) {
                WLOG("no domestic / foreign yield curves given in fx vol curve config for "
                     << spec.curveConfigID() << ", skip building calibration info");
                return;
            }

            ReportConfig rc = effectiveReportConfig(curveConfigs.reportConfigFxVols(), config->reportConfig());

            bool reportOnDeltaGrid = *rc.reportOnDeltaGrid();
            bool reportOnMoneynessGrid = *rc.reportOnMoneynessGrid();
            std::vector<Real> moneyness = *rc.moneyness();
            std::vector<std::string> deltas = *rc.deltas();
            std::vector<Period> expiries = *rc.expiries();

            calibrationInfo_ = QuantLib::ext::make_shared<FxEqCommVolCalibrationInfo>();

            calibrationInfo_->dayCounter = config->dayCounter().empty() ? "na" : config->dayCounter().name();
            calibrationInfo_->calendar = config->calendar().empty() ? "na" : config->calendar().name();
            calibrationInfo_->atmType = ore::data::to_string(atmType_);
            calibrationInfo_->deltaType = ore::data::to_string(deltaType_);
            calibrationInfo_->longTermAtmType = ore::data::to_string(longTermAtmType_);
            calibrationInfo_->longTermDeltaType = ore::data::to_string(longTermDeltaType_);
            calibrationInfo_->switchTenor = ore::data::to_string(switchTenor_);
            calibrationInfo_->riskReversalInFavorOf = riskReversalInFavorOf_ == Option::Call ? "Call" : "Put";
            calibrationInfo_->butterflyStyle = butterflyIsBrokerStyle_ ? "Broker" : "Smile";

            std::vector<Real> times, forwards, domDisc, forDisc;
            Date settl = spotCalendar_.advance(asof, spotDays_ * Days);
            for (auto const& p : expiries) {
                Date d = vol_->optionDateFromTenor(p);
                Date settlFwd = spotCalendar_.advance(d, spotDays_ * Days);
                calibrationInfo_->expiryDates.push_back(d);
                times.push_back(vol_->dayCounter().empty() ? Actual365Fixed().yearFraction(asof, d)
                                                           : vol_->timeFromReference(d));
                domDisc.push_back(domYts_->discount(settlFwd) / domYts_->discount(settl));
                forDisc.push_back(forYts_->discount(settlFwd) / forYts_->discount(settl));
                forwards.push_back(fxSpot_->value() / domDisc.back() * forDisc.back());
            }

            calibrationInfo_->times = times;
            calibrationInfo_->forwards = forwards;

            Date switchExpiry =
                vol_->calendar().empty() ? asof + switchTenor_ : vol_->optionDateFromTenor(switchTenor_);
            Real switchTime = vol_->dayCounter().empty() ? Actual365Fixed().yearFraction(asof, switchExpiry)
                                                         : vol_->timeFromReference(switchExpiry);
            if (switchTenor_ == 0 * Days)
                switchTime = QL_MAX_REAL;

            std::vector<std::vector<Real>> callPricesDelta(times.size(), std::vector<Real>(deltas.size(), 0.0));
            std::vector<std::vector<Real>> callPricesMoneyness(times.size(), std::vector<Real>(moneyness.size(), 0.0));

            calibrationInfo_->isArbitrageFree = true;

            if (reportOnDeltaGrid) {
                calibrationInfo_->deltas = deltas;
                calibrationInfo_->deltaCallPrices =
                    std::vector<std::vector<Real>>(times.size(), std::vector<Real>(deltas.size(), 0.0));
                calibrationInfo_->deltaPutPrices =
                    std::vector<std::vector<Real>>(times.size(), std::vector<Real>(deltas.size(), 0.0));
                calibrationInfo_->deltaGridStrikes =
                    std::vector<std::vector<Real>>(times.size(), std::vector<Real>(deltas.size(), 0.0));
                calibrationInfo_->deltaGridProb =
                    std::vector<std::vector<Real>>(times.size(), std::vector<Real>(deltas.size(), 0.0));
                calibrationInfo_->deltaGridImpliedVolatility =
                    std::vector<std::vector<Real>>(times.size(), std::vector<Real>(deltas.size(), 0.0));
                calibrationInfo_->deltaGridCallSpreadArbitrage =
                    std::vector<std::vector<bool>>(times.size(), std::vector<bool>(deltas.size(), true));
                calibrationInfo_->deltaGridButterflyArbitrage =
                    std::vector<std::vector<bool>>(times.size(), std::vector<bool>(deltas.size(), true));
                DeltaVolQuote::DeltaType dt;
                DeltaVolQuote::AtmType at;
                TLOG("Delta surface arbitrage analysis result (no calendar spread arbitrage included):");
                Real maxTime = QL_MAX_REAL;
                if (!expiries_.empty())
                    maxTime = vol_->timeFromReference(vol_->optionDateFromTenor(expiries_.back()));
                for (Size i = 0; i < times.size(); ++i) {
                    Real t = times[i];
                    if (t <= switchTime || close_enough(t, switchTime)) {
                        at = atmType_;
                        dt = deltaType_;
                    } else {
                        at = longTermAtmType_;
                        dt = longTermDeltaType_;
                    }
                    // for times after the last quoted expiry we use artificial conventions to avoid problems with
                    // strike from delta conversions: we keep the pa feature, but use fwd delta always and ATM DNS
                    if (t > maxTime) {
                        at = DeltaVolQuote::AtmDeltaNeutral;
                        dt = (deltaType_ == DeltaVolQuote::Spot || deltaType_ == DeltaVolQuote::Fwd)
                                 ? DeltaVolQuote::Fwd
                                 : DeltaVolQuote::PaFwd;
                    }
                    bool validSlice = true;
                    for (Size j = 0; j < deltas.size(); ++j) {
                        DeltaString d(deltas[j]);
                        try {
                            Real strike;
                            if (d.isAtm()) {
                                strike =
                                    QuantExt::getAtmStrike(dt, at, fxSpot_->value(), domDisc[i], forDisc[i], vol_, t);
                            } else if (d.isCall()) {
                                strike = QuantExt::getStrikeFromDelta(Option::Call, d.delta(), dt, fxSpot_->value(),
                                                                      domDisc[i], forDisc[i], vol_, t);
                            } else {
                                strike = QuantExt::getStrikeFromDelta(Option::Put, d.delta(), dt, fxSpot_->value(),
                                                                      domDisc[i], forDisc[i], vol_, t);
                            }
                            Real stddev = std::sqrt(vol_->blackVariance(t, strike));
                            callPricesDelta[i][j] = blackFormula(Option::Call, strike, forwards[i], stddev);
                            
                            if (d.isPut()) {
                                calibrationInfo_->deltaPutPrices[i][j] = blackFormula(Option::Put, strike, forwards[i], stddev, domDisc[i]);
                            } else {
                                calibrationInfo_->deltaCallPrices[i][j] = blackFormula(Option::Call, strike, forwards[i], stddev, domDisc[i]);
                            }
                            
                            calibrationInfo_->deltaGridStrikes[i][j] = strike;
                            calibrationInfo_->deltaGridImpliedVolatility[i][j] = stddev / std::sqrt(t);
                        } catch (const std::exception& e) {
                            validSlice = false;
                            TLOG("error for time " << t << " delta " << deltas[j] << ": " << e.what());
                        }
                    }
                    if (validSlice) {
                        try {
                            QuantExt::CarrMadanMarginalProbability cm(calibrationInfo_->deltaGridStrikes[i],
                                                                      forwards[i], callPricesDelta[i]);
                            calibrationInfo_->deltaGridCallSpreadArbitrage[i] = cm.callSpreadArbitrage();
                            calibrationInfo_->deltaGridButterflyArbitrage[i] = cm.butterflyArbitrage();
                            if (!cm.arbitrageFree())
                                calibrationInfo_->isArbitrageFree = false;
                            calibrationInfo_->deltaGridProb[i] = cm.density();
                            TLOGGERSTREAM(arbitrageAsString(cm));
                        } catch (const std::exception& e) {
                            TLOG("error for time " << t << ": " << e.what());
                            calibrationInfo_->isArbitrageFree = false;
                            TLOGGERSTREAM("..(invalid slice)..");
                        }
                    } else {
                        calibrationInfo_->isArbitrageFree = false;
                        TLOGGERSTREAM("..(invalid slice)..");
                    }
                }
                TLOG("Delta surface arbitrage analysis completed.");
            }

            if (reportOnMoneynessGrid) {
                calibrationInfo_->moneyness = moneyness;
                calibrationInfo_->moneynessCallPrices =
                    std::vector<std::vector<Real>>(times.size(), std::vector<Real>(moneyness.size(), 0.0));
                calibrationInfo_->moneynessPutPrices =
                    std::vector<std::vector<Real>>(times.size(), std::vector<Real>(moneyness.size(), 0.0));
                calibrationInfo_->moneynessGridStrikes =
                    std::vector<std::vector<Real>>(times.size(), std::vector<Real>(moneyness.size(), 0.0));
                calibrationInfo_->moneynessGridProb =
                    std::vector<std::vector<Real>>(times.size(), std::vector<Real>(moneyness.size(), 0.0));
                calibrationInfo_->moneynessGridImpliedVolatility =
                    std::vector<std::vector<Real>>(times.size(), std::vector<Real>(moneyness.size(), 0.0));
                calibrationInfo_->moneynessGridCallSpreadArbitrage =
                    std::vector<std::vector<bool>>(times.size(), std::vector<bool>(moneyness.size(), true));
                calibrationInfo_->moneynessGridButterflyArbitrage =
                    std::vector<std::vector<bool>>(times.size(), std::vector<bool>(moneyness.size(), true));
                calibrationInfo_->moneynessGridCalendarArbitrage =
                    std::vector<std::vector<bool>>(times.size(), std::vector<bool>(moneyness.size(), true));
                for (Size i = 0; i < times.size(); ++i) {
                    Real t = times[i];
                    for (Size j = 0; j < moneyness.size(); ++j) {
                        try {
                            Real strike = moneyness[j] * forwards[i];
                            calibrationInfo_->moneynessGridStrikes[i][j] = strike;
                            Real stddev = std::sqrt(vol_->blackVariance(t, strike));
                            callPricesMoneyness[i][j] = blackFormula(Option::Call, strike, forwards[i], stddev);
                            calibrationInfo_->moneynessGridImpliedVolatility[i][j] = stddev / std::sqrt(t);
                            if (moneyness[j] >= 1) {
                                calibrationInfo_->moneynessCallPrices[i][j] = blackFormula(Option::Call, strike, forwards[i], stddev, domDisc[i]);
                            } else {
                                calibrationInfo_->moneynessPutPrices[i][j] = blackFormula(Option::Put, strike, forwards[i], stddev, domDisc[i]);
                            };
                        } catch (const std::exception& e) {
                            TLOG("error for time " << t << " moneyness " << moneyness[j] << ": " << e.what());
                        }
                    }
                }
                if (!times.empty() && !moneyness.empty()) {
                    try {
                        QuantExt::CarrMadanSurface cm(times, moneyness, fxSpot_->value(), forwards,
                                                      callPricesMoneyness);
                        for (Size i = 0; i < times.size(); ++i) {
                            calibrationInfo_->moneynessGridProb[i] = cm.timeSlices()[i].density();
                        }
                        calibrationInfo_->moneynessGridCallSpreadArbitrage = cm.callSpreadArbitrage();
                        calibrationInfo_->moneynessGridButterflyArbitrage = cm.butterflyArbitrage();
                        calibrationInfo_->moneynessGridCalendarArbitrage = cm.calendarArbitrage();
                        if (!cm.arbitrageFree())
                            calibrationInfo_->isArbitrageFree = false;
                        TLOG("Moneyness surface Arbitrage analysis result:");
                        TLOGGERSTREAM(arbitrageAsString(cm));
                    } catch (const std::exception& e) {
                        TLOG("error: " << e.what());
                        calibrationInfo_->isArbitrageFree = false;
                    }
                    TLOG("Moneyness surface Arbitrage analysis completed:");
                }
            }

            // the bfrr surface provides info on smiles with error, which we report here

            if (reportOnDeltaGrid || reportOnMoneynessGrid) {
                if (auto bfrr = QuantLib::ext::dynamic_pointer_cast<QuantExt::BlackVolatilitySurfaceBFRR>(vol_)) {
                    if (bfrr->deltas().size() != bfrr->currentDeltas().size()) {
                        calibrationInfo_->messages.push_back(
                            "Warning: Used only " + std::to_string(bfrr->currentDeltas().size()) + " deltas of the " +
                            std::to_string(bfrr->deltas().size()) +
                            " deltas that were initially provided, because all smiles were invalid.");
                    }
                    for (Size i = 0; i < bfrr->dates().size(); ++i) {
                        if (bfrr->smileHasError()[i]) {
                            calibrationInfo_->messages.push_back("Ignore invalid smile at expiry " +
                                                                 ore::data::to_string(bfrr->dates()[i]) + ": " +
                                                                 bfrr->smileErrorMessage()[i]);
                        }
                    }
                }
            }

            DLOG("Building calibration info for fx vol surface completed.");
        }

    } catch (std::exception& e) {
        QL_FAIL("fx vol curve building failed: " << e.what());
    } catch (...) {
        QL_FAIL("fx vol curve building failed: unknown error");
    }
}
} // namespace data
} // namespace ore
