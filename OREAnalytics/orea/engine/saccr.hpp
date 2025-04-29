/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file orea/engine/saccr.hpp
    \brief Class for holding SA-CCR trade data
*/

#pragma once

#include <map>
#include <utility> // pair
#include <vector>

#include <ql/tuple.hpp>

#include <ql/time/period.hpp>

#include <ored/portfolio/portfolio.hpp>
#include <ored/portfolio/nettingsetmanager.hpp>
#include <ored/portfolio/referencedata.hpp>
#include <ored/report/report.hpp>
#include <orea/simm/simmbasicnamemapper.hpp>
#include <orea/simm/simmbucketmapperbase.hpp>
#include <ored/portfolio/counterpartymanager.hpp>
#include <ored/portfolio/collateralbalance.hpp>

namespace ore {
namespace analytics {
using namespace ore::data;
using namespace QuantLib;


struct SaCcrAmounts {
    Real im;
    Real vm;
    Real mta;
    Real tha;
    Real iah;
};


// SA-CCR defaults for netting set (counterparty) entries that are missing from
// the collateral balances and netting set definitions (or counterparty information).
struct saCcrDefaults {
    struct CollateralBalances {
        string ccy = "USD";
        Real iaHeld = 0.0;
        Real im = 0.0;
        Real vm = 0.0;
    };
    struct CounterpartyInformation {
        bool ccp = false;
        Real saccrRW = 1.0;
        string counterpartyId = "SACCR_DEFAULT_CPTY";
    };
    struct NettingSetDefinitions {
        // collateralised
        bool activeCsaFlag = true;
        Period mpor = 2 * Weeks;
        Real iaHeld = 0.0;
        Real thresholdRcv = 0.0;
        Real mtaRcv = 0.0;
        bool calculateIMAmount = false;
        bool calculateVMAmount = false;
    };

    CollateralBalances collBalances;
    CounterpartyInformation cptyInfo;
    NettingSetDefinitions nettingSetDef;
};

//! Compute derivative capital charge according to SA-CCR rules
// The portfolio is broken into a hierarchie of netting sets, asset classes and hedging sets
// 1) Results per netting set:
// - NPV, Exposure at Default (EAD), Replacement Cost (RC), PFE, Multiplier, aggregate AddOn
// 2) Results per asset class and netting set:
// - NPV and AddOn
// 3) Results per hedging set, asset class and netting set:
// - NPV and AddOn
// 4) Trade details
/*!
  \todo Refine maturity factor
  \todo Use sensitivities to determine direction delta for Swaps and Swaptions
  \todo Review strike and forward calculation for option deltas
 */

// FIXME: make it an observer of the portfolio
class SACCR {
public:
    enum class AssetClass {
        IR,
        FX,
        Credit,
        Equity,
        Commodity,
        None
    };
   
    enum class ReportType { Summary, Detail, TradeNPV };

    struct TradeData {
        string id, type, cpty;
        NettingSetDetails nettingSetDetails;
        AssetClass assetClass;
        string hedgingSet;
        string hedgingSubset; // for equity & commodity hedging sets are further subdivided
        Real NPV;
        string npvCcy;
        Real currentNotional;
        Real SD;
        Real delta; // adjustment for direction and non-linearity
        Real d;     // position size, duration-adjusted current notional
        Real MF;    // maturity factor;
        Real M;     // maturity date;
        Real S;     // start date (first exercise date for options?)
        Real E;     // end date (underlying maturity or last exercise for options?)
        Real T;     // latest exercise date (first or last?)
        Real price;
        Real strike;
        Size numNominalFlows;
        bool isEquityIndex;
        Real currentPrice1;
        Real currentPrice2;

        TradeData()
            : id(""), type(""), cpty(""), nettingSetDetails(NettingSetDetails()), assetClass(AssetClass::None),
              hedgingSet(""), hedgingSubset(""), NPV(QuantLib::Null<Real>()), npvCcy(""),
              currentNotional(QuantLib::Null<Real>()), SD(QuantLib::Null<Real>()), delta(QuantLib::Null<Real>()),
              d(QuantLib::Null<Real>()), MF(QuantLib::Null<Real>()),
              M(QuantLib::Null<Real>()), S(QuantLib::Null<Real>()), E(QuantLib::Null<Real>()), T(QuantLib::Null<Real>()),
              price(QuantLib::Null<Real>()), strike(QuantLib::Null<Real>()), numNominalFlows(QuantLib::Null<Size>()),
              isEquityIndex(false), currentPrice1(QuantLib::Null<Real>()), currentPrice2(QuantLib::Null<Real>()) {}

        //! Full ctor to allow braced initialisation
        TradeData(const std::string& id, const std::string& type, const std::string& nettingSetId,
                  const AssetClass& assetClass, const std::string& hedgingSet, const std::string& hedgingSubset,
                  QuantLib::Real NPV, const std::string& npvCcy, QuantLib::Real currentNotional, QuantLib::Real delta,
                  QuantLib::Real d, QuantLib::Real MF, QuantLib::Real M = QuantLib::Null<Real>(),
                  QuantLib::Real S = QuantLib::Null<Real>(), QuantLib::Real E = QuantLib::Null<Real>(),
                  QuantLib::Real T = QuantLib::Null<Real>(), QuantLib::Real price = QuantLib::Null<Real>(),
                  QuantLib::Real strike = QuantLib::Null<Real>(), Size numNominalFlows = QuantLib::Null<Size>(),
                  const bool isEquityIndex = false, QuantLib::Real SD = QuantLib::Null<Real>(),
                  QuantLib::Real currentPrice1 = QuantLib::Null<Real>(),
                  QuantLib::Real currentPrice2 = QuantLib::Null<Real>())
            : id(id), type(type), nettingSetDetails(NettingSetDetails(nettingSetId)), assetClass(assetClass),
              hedgingSet(hedgingSet), hedgingSubset(hedgingSubset), NPV(NPV), npvCcy(npvCcy),
              currentNotional(currentNotional), SD(SD), delta(delta), d(d), MF(MF), M(M), S(S), E(E), T(T),
              price(price), strike(strike), numNominalFlows(numNominalFlows), isEquityIndex(isEquityIndex),
              currentPrice1(currentPrice1), currentPrice2(currentPrice2) {}
    };

    typedef std::pair<NettingSetDetails, AssetClass> AssetClassKey;
    typedef std::tuple<NettingSetDetails, AssetClass, std::string> HedgingSetKey;
    typedef std::tuple<NettingSetDetails, AssetClass, std::string, std::string> HedgingSubsetKey;

    SACCR(const QuantLib::ext::shared_ptr<Portfolio>& portfolio, const QuantLib::ext::shared_ptr<NettingSetManager>& nettingSetManager,
          const QuantLib::ext::shared_ptr<ore::data::CounterpartyManager>& counterpartyManager, const QuantLib::ext::shared_ptr<Market>& market, 
          const std::string& baseCurrency, const QuantLib::ext::shared_ptr<ore::data::CollateralBalances>& collateralBalances,
          const QuantLib::ext::shared_ptr<ore::data::CollateralBalances>& calculatedCollateralBalances,
          const QuantLib::ext::shared_ptr<SimmNameMapper>& nameMapper,
          const QuantLib::ext::shared_ptr<SimmBucketMapper>& bucketMapper,
          const QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager>& refDataManager,
          const std::map<ReportType, QuantLib::ext::shared_ptr<ore::data::Report>>& outReports = {});

    vector<TradeData>& tradeData() { return tradeData_; }

    // getters
    const QuantLib::ext::shared_ptr<Portfolio>& portfolio() const { return portfolio_; };
    const QuantLib::ext::shared_ptr<NettingSetManager>& nettingSetManager() const { return nettingSetManager_; };
    const QuantLib::ext::shared_ptr<CounterpartyManager>& counterpartyManager() const { return counterpartyManager_; };
    const QuantLib::ext::shared_ptr<Market>& market() const { return market_; };;

    const vector<NettingSetDetails>& nettingSetDetails() const { return nettingSetDetails_; }
    const vector<AssetClass>& assetClasses(NettingSetDetails nettingSetDetails) const;
    const vector<string>& hedgingSets(NettingSetDetails nettingSetDetails, AssetClass assetClass) const;
    Real NPV() const { return totalNPV_; }
    Real NPV(NettingSetDetails nettingSetDetails) const;
    Real NPV(NettingSetDetails nettingSetDetails, AssetClass assetClass) const;
    Real NPV(NettingSetDetails nettingSetDetails, AssetClass assetClass, string hedgingSet) const;
    Real EAD(NettingSetDetails nettingSetDetails) const;
    Real EAD(string nettingSet) const;
    Real RW(NettingSetDetails nettingSetDetails) const;
    Real CC() const { return totalCC_; }
    Real CC(NettingSetDetails nettingSetDetails) const;
    Real RC(NettingSetDetails nettingSetDetails) const;
    Real PFE(NettingSetDetails nettingSetDetails) const;
    Real multiplier(NettingSetDetails nettingSetDetails) const;
    Real addOn(NettingSetDetails nettingSetDetails) const;
    Real addOn(NettingSetDetails nettingSetDetails, AssetClass assetClass) const;
    Real addOn(NettingSetDetails nettingSetDetails, AssetClass assetClass, string hedgingSet) const;

private:
    void validate();
    Real getFxRate(const string& ccy);
    // fill TradeData vector with trade-level information
    void tradeDetails();
    void aggregate();
    // Combine SIMM-generated balances and user-provided balances into one, for the final collateral balances output
    void combineCollateralBalances();
    void clear();
    Real getLegAverageNotional(const QuantLib::ext::shared_ptr<Trade>&, Size, const DayCounter&, Real&);

    //! Reports that results are written to
    std::map<ReportType, QuantLib::ext::shared_ptr<ore::data::Report>> reports_;
    
    //! write any passed in reports
    void writeReports();

    SACCR::AssetClass getAssetClass(const string& tradeType, bool isXCCYSwap);
    std::pair<std::string, QuantLib::ext::optional<std::string>> getHedgingSet(const QuantLib::ext::shared_ptr<Trade>& trade, TradeData& tradeData);
    Real getCurrentNotional(const QuantLib::ext::shared_ptr<Trade>& trade, SACCR::AssetClass assetClass,
                            const string& baseCcy, const DayCounter& dc, Real& currentPrice1,
                            Real& currentPrice2, const string& hedgingSet = "",
                            const std::string& hedgingSubset = "");
    Real getSupervisoryDuration(const TradeData& tradeData);
    Real getDelta(const QuantLib::ext::shared_ptr<Trade>& trade, TradeData& tradeData, Date today);
    Real getSupervisoryOptionVolatility(const TradeData& tradeData);
    string getCommodityName(const string& commodity, bool withPrefix = false);
    string getCommodityHedgingSet(const string& commodity);
    string getCommodityHedgingSubset(const string& commodity);
    std::string getFirstRiskFactor(const std::string& hedgingSet, const std::string& hedgingSubset,
                                   const SACCR::AssetClass& assetClass, const QuantLib::ext::shared_ptr<Trade>& trade);

    QuantLib::ext::shared_ptr<Portfolio> portfolio_;
    QuantLib::ext::shared_ptr<NettingSetManager> nettingSetManager_;
    QuantLib::ext::shared_ptr<CounterpartyManager> counterpartyManager_;
    QuantLib::ext::shared_ptr<Market> market_;
    std::string baseCurrency_;
    std::map<NettingSetDetails, SaCcrAmounts> amountsBase_;
    vector<TradeData> tradeData_;
    // per netting set:
    QuantLib::ext::shared_ptr<ore::data::CollateralBalances> collateralBalances_, calculatedCollateralBalances_;
    std::set<NettingSetDetails> defaultIMBalances_;
    std::set<NettingSetDetails> defaultVMBalances_;
    QuantLib::ext::shared_ptr<SimmNameMapper> nameMapper_;
    QuantLib::ext::shared_ptr<SimmBucketMapper> bucketMapper_;
    QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager> refDataManager_;

    bool hasNettingSetDetails_;
    Real totalNPV_ = 0;
    map<NettingSetDetails, Real> NPV_;
    map<NettingSetDetails, Real> RC_;
    map<NettingSetDetails, Real> addOn_;
    map<NettingSetDetails, Real> EAD_;
    map<NettingSetDetails, Real> RW_;
    Real totalCC_ = 0;
    map<NettingSetDetails, Real> CC_;
    map<NettingSetDetails, Real> PFE_;
    map<NettingSetDetails, Real> multiplier_;
    // per netting set and asset class
    map<AssetClassKey, Real> npvAssetClass_;
    map<AssetClassKey, Real> addOnAssetClass_;
    // per netting set, asset class and hedging set
    map<HedgingSetKey, Real> npvHedgingSet_;
    map<HedgingSetKey, Real> addOnHedgingSet_;
    map<HedgingSetKey, Real> effectiveNotional_;
    map<HedgingSubsetKey, Real> subsetEffectiveNotional_;

    vector<NettingSetDetails> nettingSetDetails_;
    map<NettingSetDetails, vector<AssetClass>> assetClasses_;
    map<pair<NettingSetDetails, AssetClass>, vector<string>> hedgingSets_;
    set<string> basisHedgingSets_;
    set<string> volatilityHedgingSets_;
    saCcrDefaults saCcrDefaults_;
    map<NettingSetDetails, set<string>> nettingSetToCpty_;
    set<NettingSetDetails> nettingSets_;
};

std::ostream& operator<<(std::ostream& os, SACCR::AssetClass ac);

} // namespace analytics

} // namespace ore
