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
#include <qle/termstructures/fxblackvolsurface.hpp>
#include <qle/termstructures/blackvolsurfacedelta.hpp>
#include <string.h>

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

//look-u[ fx from triangulation object
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
                       const std::map<string, boost::shared_ptr<YieldCurve>>& yieldCurves, const Conventions& conventions) { 
    init(asof, spec, loader, curveConfigs, FXLookupTriangulation(fxSpots), yieldCurves, conventions);    
}
    
void FXVolCurve::buildSmileDeltaCurve(Date asof, FXVolatilityCurveSpec spec, const Loader& loader,
                      boost::shared_ptr<FXVolatilityCurveConfig> config, const FXLookup& fxSpots,
                      const map<string, boost::shared_ptr<YieldCurve>>& yieldCurves, const Conventions& conventions) {
    vector<Period> expiries = parseVectorOfValues<Period>(config->expiries(), &parsePeriod);
    vector<Period> unsortedExp = parseVectorOfValues<Period>(config->expiries(), &parsePeriod);
    std::sort(expiries.begin(), expiries.end());
    
    vector<string> deltas = config->deltas();
    vector<Real> putDeltas;
    vector<Real> callDeltas;
    vector<Date> dates;
    Matrix blackVolMatrix(expiries.size(), config->deltas().size());

    vector<string> tokens;
    boost::split(tokens, config->fxSpotID(), boost::is_any_of("/"));
    string base = "FX_OPTION/RATE_LNVOL/" + tokens[1] + "/" + tokens[2] + "/";
    bool hasATM = false;
    for (Size i = 0; i < expiries.size(); i++) {
        Size idx = std::find(unsortedExp.begin(), unsortedExp.end(), expiries[i]) - unsortedExp.begin();
        string e = config->expiries()[idx];
        dates.push_back(asof + expiries[i]);
        Size j = 0;
       
        for (auto d : deltas) {
            string qs = base + e + "/" + d;
            boost::shared_ptr<MarketDatum> md = loader.get(qs, asof);
            boost::shared_ptr<FXOptionQuote> q = boost::dynamic_pointer_cast<FXOptionQuote>(md);
            QL_REQUIRE(q, "quote not found, " << qs);
            blackVolMatrix[i][j] = q->quote()->value();
            j++;
            if (i == 0) {
                vector<string> tokens2;
                boost::split(tokens2, qs, boost::is_any_of("/"));
                string delta = tokens2.back();
                if( delta == "ATM")
                    hasATM = true;
                if (delta.back() == 'P') 
                    putDeltas.push_back(-1 * parseReal(delta.substr(0, delta.size()-1))/100);
                if (delta.back() == 'C') 
                    callDeltas.push_back(parseReal(delta.substr(0, delta.size()-1))/100);
            }
        }
    }

    std::string conventionsID = config->conventionsID(); 
    DeltaVolQuote::AtmType atmType = DeltaVolQuote::AtmType::AtmDeltaNeutral; 
    DeltaVolQuote::DeltaType deltaType = DeltaVolQuote::DeltaType::Spot; 

    if (conventionsID != "") { 
        boost::shared_ptr<Convention> conv = conventions.get(conventionsID); 
        auto fxOptConv = boost::dynamic_pointer_cast<FxOptionConvention>(conv); 
        QL_REQUIRE(fxOptConv, "unable to cast convention (" << conventionsID << ") into FxOptionConvention"); 
        atmType = fxOptConv->atmType(); 
        deltaType = fxOptConv->deltaType();

        QL_REQUIRE(atmType == DeltaVolQuote::AtmType::AtmDeltaNeutral, "only AtmDeltaNeutral ATM vol quotes are currently supported");
        QL_REQUIRE(deltaType == DeltaVolQuote::DeltaType::Spot, "only spot Delta vol quotes are currently supported");
    } 
    // daycounter used for interpolation in time.
    // TODO: push into conventions or config
    DayCounter dc = config->dayCounter();
    Calendar cal = config->calendar();
    auto fxSpot = fxSpots.fxPairLookup(config->fxSpotID());
    auto domYTS = getHandle<YieldTermStructure>(config->fxDomesticYieldCurveID(), yieldCurves);
    auto forYTS = getHandle<YieldTermStructure>(config->fxForeignYieldCurveID(), yieldCurves);
    vol_ = boost::shared_ptr<BlackVolTermStructure>(new QuantExt::BlackVolatilitySurfaceDelta(
                asof, dates, putDeltas, callDeltas, hasATM, blackVolMatrix, dc, cal, fxSpot, domYTS, forYTS));

    vol_->enableExtrapolation();
}

void FXVolCurve::buildVannaVolgaOrATMCurve(Date asof, FXVolatilityCurveSpec spec, const Loader& loader,
                      boost::shared_ptr<FXVolatilityCurveConfig> config, const FXLookup& fxSpots,
                      const map<string, boost::shared_ptr<YieldCurve>>& yieldCurves, const Conventions& conventions) {
    
    bool isATM = config->dimension() == FXVolatilityCurveConfig::Dimension::ATM;
    Natural smileDelta; 
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
    vector<Period> cExpiries = parseVectorOfValues<Period>(config->expiries(), &parsePeriod);
    vector<vector<Period>> expiries(n, cExpiries);

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

    // Check ATM first
    // Check that we have all the expiries we need
    LOG("FXVolCurve: read " << quotes[0].size() << " ATM vols");
    QL_REQUIRE(expiries[0].size() == 0,
               "No ATM quote found for spec " << spec << " with expiry " << expiries[0].front());
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
            vol_ =
                boost::shared_ptr<BlackVolTermStructure>(new BlackVarianceCurve(asof, dates, vols[0], dc, false));
        } else {
            // Smile
            auto fxSpot = fxSpots.fxPairLookup(config->fxSpotID());
            auto domYTS = getHandle<YieldTermStructure>(config->fxDomesticYieldCurveID(), yieldCurves);
            auto forYTS = getHandle<YieldTermStructure>(config->fxForeignYieldCurveID(), yieldCurves);
            
            std::string conventionsID = config->conventionsID(); 
            DeltaVolQuote::AtmType atmType = DeltaVolQuote::AtmType::AtmDeltaNeutral; 
            DeltaVolQuote::DeltaType deltaType = DeltaVolQuote::DeltaType::Spot; 

            if (conventionsID != "") { 
                boost::shared_ptr<Convention> conv = conventions.get(conventionsID); 
                auto fxOptConv = boost::dynamic_pointer_cast<FxOptionConvention>(conv); 
                QL_REQUIRE(fxOptConv, "unable to cast convention (" << conventionsID << ") into FxOptionConvention"); 
                atmType = fxOptConv->atmType(); 
                deltaType = fxOptConv->deltaType(); 
            }  

            bool vvFirstApprox = false;  // default to VannaVolga second approximation
            if (config->smileInterpolation() == FXVolatilityCurveConfig::SmileInterpolation::VannaVolga1) {
                vvFirstApprox = true;
            }

            vol_ = boost::shared_ptr<BlackVolTermStructure>(new QuantExt::FxBlackVannaVolgaVolatilitySurface(
                asof, dates, vols[0], vols[1], vols[2], dc, cal, fxSpot, domYTS, forYTS, false, vvFirstApprox, 
                atmType, deltaType, smileDelta / 100.0));
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
