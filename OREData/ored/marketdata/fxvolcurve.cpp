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

#include <algorithm>
#include <ored/marketdata/fxvolcurve.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancecurve.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <qle/termstructures/blackvolsurfacedelta.hpp>
#include <qle/termstructures/fxblackvolsurface.hpp>
#include <string.h>
#include <regex>

using namespace QuantLib;
using namespace std;

namespace {

// utility to get a handle out of a Curve object
template <class T, class K> Handle<T> getHandle(const string& spec, const map<string, boost::shared_ptr<K>>& m) {
    auto it = m.find(spec);
    QL_REQUIRE(it != m.end(), "FXVolCurve: Can't find spec " << spec);
    return it->second->handle();
}

// loop-up fx from map
class FXLookupMap : public ore::data::FXLookup {
public:
    FXLookupMap(const map<string, boost::shared_ptr<ore::data::FXSpot>>& fxSpots) : fxSpots_(fxSpots) {}

    Handle<Quote> fxPairLookup(const string& fxPair) const override { return getHandle<Quote>(fxPair, fxSpots_); };

private:
    // this is a reference
    const map<string, boost::shared_ptr<ore::data::FXSpot>>& fxSpots_;
};

// look-u[ fx from triangulation object
class FXLookupTriangulation : public ore::data::FXLookup {
public:
    FXLookupTriangulation(const ore::data::FXTriangulation& fxSpots) : fxSpots_(fxSpots) {}

    Handle<Quote> fxPairLookup(const string& fxPair) const override {
        // parse ID to get pair
        QL_REQUIRE(fxPair.size() == 10, "FX Pair should be of the form: FX/CCY/CCY");
        QL_REQUIRE(fxPair.substr(0, 3) == "FX/", "FX Pair should be of the form: FX/CCY/CCY");
        return fxSpots_.getQuote(fxPair.substr(3, 3) + fxPair.substr(7, 3));
    };

private:
    // this is a reference
    const ore::data::FXTriangulation& fxSpots_;
};
} // namespace

namespace ore {
namespace data {

FXVolCurve::FXVolCurve(Date asof, FXVolatilityCurveSpec spec, const Loader& loader,
                       const CurveConfigurations& curveConfigs, const map<string, boost::shared_ptr<FXSpot>>& fxSpots,
                       const map<string, boost::shared_ptr<YieldCurve>>& yieldCurves, const Conventions& conventions) {
    init(asof, spec, loader, curveConfigs, FXLookupMap(fxSpots), yieldCurves, conventions);
}

// second ctor
FXVolCurve::FXVolCurve(Date asof, FXVolatilityCurveSpec spec, const Loader& loader,
                       const CurveConfigurations& curveConfigs, const FXTriangulation& fxSpots,
                       const std::map<string, boost::shared_ptr<YieldCurve>>& yieldCurves,
                       const Conventions& conventions) {
    init(asof, spec, loader, curveConfigs, FXLookupTriangulation(fxSpots), yieldCurves, conventions);
}

void FXVolCurve::buildSmileDeltaCurve(Date asof, FXVolatilityCurveSpec spec, const Loader& loader,
                                      boost::shared_ptr<FXVolatilityCurveConfig> config, const FXLookup& fxSpots,
                                      const map<string, boost::shared_ptr<YieldCurve>>& yieldCurves,
                                      const Conventions& conventions) {
    bool isRegex = false;
    for (Size i = 0; i < config->expiries().size(); i++) {
        if ((isRegex = config->expiries()[i].find("*") != string::npos)) {
            QL_REQUIRE(i == 0 && config->expiries().size() == 1,
                       "Wild card config, " << config->curveID() << ", should have exactly one regex provided.");
            break;
        }
    }

    vector<Period> expiries;
    vector<Period> unsortedExp;
    
    vector<std::pair<Real, string>> putDeltas, callDeltas;
    bool hasATM = false;

    for (auto const& delta : config->deltas()) {
        if (delta == "ATM")
            hasATM = true;
        else if (!delta.empty() && delta.back() == 'P')
            putDeltas.push_back(std::make_pair(-1 * parseReal(delta.substr(0, delta.size() - 1)) / 100, delta));
        else if (!delta.empty() && delta.back() == 'C')
            callDeltas.push_back(std::make_pair(parseReal(delta.substr(0, delta.size() - 1)) / 100, delta));
        else {
            QL_FAIL("invalid delta '" << delta << "', expected 10P, 40C, ATM, ...");
        }
    }

    // sort puts 10P, 15P, 20P, ... and calls 45C, 40C, 35C, ... (notice put deltas have a negative sign)
    auto comp = [](const std::pair<Real, string>& x, const std::pair<Real, string>& y) { return x.first > y.first; };
    std::sort(putDeltas.begin(), putDeltas.end(), comp);
    std::sort(callDeltas.begin(), callDeltas.end(), comp);

    vector<Date> dates;
    Matrix blackVolMatrix;

    vector<string> tokens;
    boost::split(tokens, config->fxSpotID(), boost::is_any_of("/"));
    string base = "FX_OPTION/RATE_LNVOL/" + tokens[1] + "/" + tokens[2] + "/";

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

    if (isRegex) {
        regex regexp(boost::replace_all_copy(config->expiries()[0], "*", ".*"));

        // we save relevant delta quotes to avoid looping twice
        std::vector<boost::shared_ptr<MarketDatum>> data;
        std::vector<std::string> expiriesStr;
        // get list of possible expiries
        for (auto& md : loader.loadQuotes(asof)) {
            if (md->asofDate() == asof && md->instrumentType() == MarketDatum::InstrumentType::FX_OPTION) {
                boost::shared_ptr<FXOptionQuote> q = boost::dynamic_pointer_cast<FXOptionQuote>(md);
                Strike s = parseStrike(q->strike());
                if (q->unitCcy() == spec.unitCcy() && q->ccy() == spec.ccy() && 
                    (s.type == Strike::Type::DeltaCall || s.type == Strike::Type::DeltaPut || s.type == Strike::Type::ATM)) {
                    vector<string> tokens;
                    boost::split(tokens, md->name(), boost::is_any_of("/"));
                    QL_REQUIRE(tokens.size() == 6, "6 tokens expected in " << md->name());
                    if (regex_match(tokens[4], regexp)) {
                        data.push_back(md);
                        auto it = std::find(expiries.begin(), expiries.end(), q->expiry());
                        if (it == expiries.end()) {
                            expiries.push_back(q->expiry());
                            expiriesStr.push_back(tokens[4]);
                        }
                    }
                }
            }
        }
        unsortedExp = expiries;
        std::sort(expiries.begin(), expiries.end());
        
        // we try to find all necessary quotes for each expiry 
        vector<Size> validExpiryIdx;
        Matrix tmpMatrix(expiries.size(), config->deltas().size());
        for (Size i = 0; i < expiries.size(); i++) {
            Size idx = std::find(unsortedExp.begin(), unsortedExp.end(), expiries[i]) - unsortedExp.begin();
            string e = expiriesStr[idx];
            for (Size j = 0; j < deltaNames.size(); ++j) {
                string qs = base + e + "/" + deltaNames[j];
                boost::shared_ptr<MarketDatum> md;
                for (auto& m : data) {
                    if (m->name() == qs) {
                        md = m;
                        break;
                    }
                }
                boost::shared_ptr<FXOptionQuote> q = boost::dynamic_pointer_cast<FXOptionQuote>(md);
                if (!q) {
                    DLOG("missing " << qs << ", expiry " << e << " will be excluded");
                    break;
                }
                tmpMatrix[i][j] = q->quote()->value();
                // if we have found all the quotes then this is a valid expiry
                if (j == deltaNames.size() - 1) {
                    dates.push_back(asof + expiries[i]);
                    validExpiryIdx.push_back(i);
                }
            }
        }

        QL_REQUIRE(validExpiryIdx.size() > 0, "no valid FxVol expiries found");
        DLOG("found " << validExpiryIdx.size() << " valid expiries:");
        for (auto& e : validExpiryIdx)
            DLOG(expiries[e]);
        // we build a matrix with just the valid expiries
        blackVolMatrix = Matrix(validExpiryIdx.size(), config->deltas().size());
        for (Size i = 0; i < validExpiryIdx.size(); i++) {
            for (Size j = 0; j < deltaNames.size(); ++j) {
                blackVolMatrix[i][j] = tmpMatrix[ validExpiryIdx[i]][j];
            }
        }

    } else {
        expiries = parseVectorOfValues<Period>(config->expiries(), &parsePeriod);
        unsortedExp = parseVectorOfValues<Period>(config->expiries(), &parsePeriod);
        std::sort(expiries.begin(), expiries.end());
        
        blackVolMatrix = Matrix(expiries.size(), config->deltas().size());
        for (Size i = 0; i < expiries.size(); i++) {
            Size idx = std::find(unsortedExp.begin(), unsortedExp.end(), expiries[i]) - unsortedExp.begin();
            string e = config->expiries()[idx];
            dates.push_back(asof + expiries[i]);
            for (Size j = 0; j < deltaNames.size(); ++j) {
                string qs = base + e + "/" + deltaNames[j];
                boost::shared_ptr<MarketDatum> md = loader.get(qs, asof);
                boost::shared_ptr<FXOptionQuote> q = boost::dynamic_pointer_cast<FXOptionQuote>(md);
                QL_REQUIRE(q, "quote not found, " << qs);
                blackVolMatrix[i][j] = q->quote()->value();
            }
        }
    }

    std::string conventionsID = config->conventionsID();
    DeltaVolQuote::AtmType atmType = DeltaVolQuote::AtmType::AtmDeltaNeutral;
    DeltaVolQuote::DeltaType deltaType = DeltaVolQuote::DeltaType::Spot;
    Period switchTenor = 2 * Years;
    DeltaVolQuote::AtmType longTermAtmType = DeltaVolQuote::AtmType::AtmDeltaNeutral;
    DeltaVolQuote::DeltaType longTermDeltaType = DeltaVolQuote::DeltaType::Fwd;

    if (conventionsID != "") {
        boost::shared_ptr<Convention> conv = conventions.get(conventionsID);
        auto fxOptConv = boost::dynamic_pointer_cast<FxOptionConvention>(conv);
        QL_REQUIRE(fxOptConv, "unable to cast convention (" << conventionsID << ") into FxOptionConvention");
        atmType = fxOptConv->atmType();
        deltaType = fxOptConv->deltaType();
        longTermAtmType = fxOptConv->longTermAtmType();
        longTermDeltaType = fxOptConv->longTermDeltaType();
        switchTenor = fxOptConv->switchTenor();
    }
    // daycounter used for interpolation in time.
    // TODO: push into conventions or config
    DayCounter dc = config->dayCounter();
    Calendar cal = config->calendar();
    auto fxSpot = fxSpots.fxPairLookup(config->fxSpotID());
    auto domYTS = getHandle<YieldTermStructure>(config->fxDomesticYieldCurveID(), yieldCurves);
    auto forYTS = getHandle<YieldTermStructure>(config->fxForeignYieldCurveID(), yieldCurves);
    std::vector<Real> putDeltasNum, callDeltasNum;
    std::transform(putDeltas.begin(), putDeltas.end(), std::back_inserter(putDeltasNum),
                   [](const std::pair<Real, string>& x) { return x.first; });
    std::transform(callDeltas.begin(), callDeltas.end(), std::back_inserter(callDeltasNum),
                   [](const std::pair<Real, string>& x) { return x.first; });
    vol_ = boost::make_shared<QuantExt::BlackVolatilitySurfaceDelta>(
        asof, dates, putDeltasNum, callDeltasNum, hasATM, blackVolMatrix, dc, cal, fxSpot, domYTS, forYTS, deltaType,
        atmType, boost::none, switchTenor, longTermDeltaType, longTermAtmType);

    vol_->enableExtrapolation();
}

void FXVolCurve::buildVannaVolgaOrATMCurve(Date asof, FXVolatilityCurveSpec spec, const Loader& loader,
                                           boost::shared_ptr<FXVolatilityCurveConfig> config, const FXLookup& fxSpots,
                                           const map<string, boost::shared_ptr<YieldCurve>>& yieldCurves,
                                           const Conventions& conventions) {

    bool isATM = config->dimension() == FXVolatilityCurveConfig::Dimension::ATM;
    Natural smileDelta = 0;
    std::string deltaRr;
    std::string deltaBf;
    if (!isATM) {
        smileDelta = config->smileDelta();
        deltaRr = to_string(smileDelta) + "RR";
        deltaBf = to_string(smileDelta) + "BF";
    }
    // We loop over all market data, looking for quotes that match the configuration
    // every time we find a matching expiry we remove it from the list
    // we replicate this for all 3 types of quotes were applicable.
    Size n = isATM ? 1 : 3; // [0] = ATM, [1] = RR, [2] = BF
    vector<vector<boost::shared_ptr<FXOptionQuote>>> quotes(n);

    bool isRegex = false;
    for (Size i = 0; i < config->expiries().size(); i++) {
        if ((isRegex = config->expiries()[i].find("*") != string::npos)) {
            QL_REQUIRE(isATM, "wildcards only supported for ATM or Delta FxVol Curves");
            QL_REQUIRE(i == 0 && config->expiries().size() == 1,
                       "Wild card config, " << config->curveID() << ", should have exactly one regex provided.");
            break;
        }
    }

    vector<Period> cExpiries;
    vector<vector<Period>> expiries;
    // Create the regular expression
    regex regexp;
    if (!isRegex) {
        cExpiries = parseVectorOfValues<Period>(config->expiries(), &parsePeriod);
        expiries = vector<vector<Period>>(n, cExpiries);
    } else {
        string regexstr = config->expiries()[0];
        regexstr = boost::replace_all_copy(regexstr, "*", ".*");
        regexp = regex(regexstr);
    }

    // Load the relevant quotes
    for (auto& md : loader.loadQuotes(asof)) {
        // skip irrelevant data
        if (md->asofDate() == asof && md->instrumentType() == MarketDatum::InstrumentType::FX_OPTION) {

            boost::shared_ptr<FXOptionQuote> q = boost::dynamic_pointer_cast<FXOptionQuote>(md);

            if (q->unitCcy() == spec.unitCcy() && q->ccy() == spec.ccy()) {

                Size idx = 999999;
                if (q->strike() == "ATM")
                    idx = 0;
                else if (!isATM && q->strike() == deltaRr)
                    idx = 1;
                else if (!isATM && q->strike() == deltaBf)
                    idx = 2;

                // silently skip unknown strike strings
                if ((isATM && idx == 0) || (!isATM && idx <= 2)) {
                    if (isRegex) {
                        vector<string> tokens;
                        boost::split(tokens, md->name(), boost::is_any_of("/"));
                        QL_REQUIRE(tokens.size() == 6, "6 tokens expected in " << md->name());
                        if (regex_match(tokens[4], regexp)) {
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
        }
    }

    // Check ATM first
    // Check that we have all the expiries we need
    LOG("FXVolCurve: read " << quotes[0].size() << " ATM vols");
    if (!isRegex) {
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
                  [](const boost::shared_ptr<FXOptionQuote>& a, const boost::shared_ptr<FXOptionQuote>& b) -> bool {
                      return a->expiry() < b->expiry();
                  });
    }

    // daycounter used for interpolation in time.
    // TODO: push into conventions or config
    DayCounter dc = config->dayCounter();
    Calendar cal = config->calendar();

    // build vol curve
    if (isATM && quotes[0].size() == 1) {
        vol_ = boost::shared_ptr<BlackVolTermStructure>(
            new BlackConstantVol(asof, Calendar(), quotes[0].front()->quote()->value(), dc));
    } else {

        Size numExpiries = quotes[0].size();
        vector<Date> dates(numExpiries);
        vector<vector<Volatility>> vols(n, vector<Volatility>(numExpiries)); // same as above: [0] = ATM, etc.

        for (Size i = 0; i < numExpiries; i++) {
            dates[i] = asof + quotes[0][i]->expiry();
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
            vol_ = boost::shared_ptr<BlackVolTermStructure>(new BlackVarianceCurve(asof, dates, vols[0], dc, false));
        } else {
            // Smile
            auto fxSpot = fxSpots.fxPairLookup(config->fxSpotID());
            auto domYTS = getHandle<YieldTermStructure>(config->fxDomesticYieldCurveID(), yieldCurves);
            auto forYTS = getHandle<YieldTermStructure>(config->fxForeignYieldCurveID(), yieldCurves);

            std::string conventionsID = config->conventionsID();
            DeltaVolQuote::AtmType atmType = DeltaVolQuote::AtmType::AtmDeltaNeutral;
            DeltaVolQuote::DeltaType deltaType = DeltaVolQuote::DeltaType::Spot;
            Period switchTenor = 2 * Years;
            DeltaVolQuote::AtmType longTermAtmType = DeltaVolQuote::AtmType::AtmDeltaNeutral;
            DeltaVolQuote::DeltaType longTermDeltaType = DeltaVolQuote::DeltaType::Fwd;

            if (conventionsID != "") {
                boost::shared_ptr<Convention> conv = conventions.get(conventionsID);
                auto fxOptConv = boost::dynamic_pointer_cast<FxOptionConvention>(conv);
                QL_REQUIRE(fxOptConv, "unable to cast convention (" << conventionsID << ") into FxOptionConvention");
                atmType = fxOptConv->atmType();
                deltaType = fxOptConv->deltaType();
                longTermAtmType = fxOptConv->longTermAtmType();
                longTermDeltaType = fxOptConv->longTermDeltaType();
                switchTenor = fxOptConv->switchTenor();
            }

            bool vvFirstApprox = false; // default to VannaVolga second approximation
            if (config->smileInterpolation() == FXVolatilityCurveConfig::SmileInterpolation::VannaVolga1) {
                vvFirstApprox = true;
            }

            vol_ = boost::make_shared<QuantExt::FxBlackVannaVolgaVolatilitySurface>(
                asof, dates, vols[0], vols[1], vols[2], dc, cal, fxSpot, domYTS, forYTS, false, vvFirstApprox, atmType,
                deltaType, smileDelta / 100.0, switchTenor, longTermAtmType, longTermDeltaType);
        }
    }
    vol_->enableExtrapolation();
}
void FXVolCurve::init(Date asof, FXVolatilityCurveSpec spec, const Loader& loader,
                      const CurveConfigurations& curveConfigs, const FXLookup& fxSpots,
                      const map<string, boost::shared_ptr<YieldCurve>>& yieldCurves, const Conventions& conventions) {
    try {

        const boost::shared_ptr<FXVolatilityCurveConfig>& config = curveConfigs.fxVolCurveConfig(spec.curveConfigID());

        QL_REQUIRE(config->dimension() == FXVolatilityCurveConfig::Dimension::ATM ||
                       config->dimension() == FXVolatilityCurveConfig::Dimension::SmileVannaVolga ||
                       config->dimension() == FXVolatilityCurveConfig::Dimension::SmileDelta,
                   "Unknown FX curve building dimension");

        if (config->dimension() == FXVolatilityCurveConfig::Dimension::SmileDelta) {
            buildSmileDeltaCurve(asof, spec, loader, config, fxSpots, yieldCurves, conventions);
        } else {
            buildVannaVolgaOrATMCurve(asof, spec, loader, config, fxSpots, yieldCurves, conventions);
        }
    } catch (std::exception& e) {
        QL_FAIL("fx vol curve building failed :" << e.what());
    } catch (...) {
        QL_FAIL("fx vol curve building failed: unknown error");
    }
}
} // namespace data
} // namespace ore
