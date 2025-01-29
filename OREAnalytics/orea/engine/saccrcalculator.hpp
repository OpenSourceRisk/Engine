/*
  Copyright (C) 2020 Quaternion Risk Management Ltd.
  All rights reserved.
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

#include <orea/simm/simmbasicnamemapper.hpp>
#include <orea/simm/simmbucketmapperbase.hpp>
#include <ored/portfolio/collateralbalance.hpp>
#include <ored/portfolio/nettingsetmanager.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/portfolio/referencedata.hpp>
#include <ored/report/report.hpp>
#include <ored/portfolio/counterpartymanager.hpp>
#include <orea/engine/saccrtradedata.hpp>

namespace ore {
namespace analytics {

class SaCcrDefaults;

using namespace ore::data;
using namespace QuantLib;

struct SaCcrAmounts {
    Real im;
    Real vm;
    Real mta;
    Real tha;
    Real iah;
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
class SaccrCalculator {
public:
    enum class ReportType { Summary };

    typedef std::pair<NettingSetDetails, SaccrTradeData::AssetClass> AssetClassKey;
    //                                                                          hedgingSet
    typedef QuantLib::ext::tuple<NettingSetDetails, SaccrTradeData::AssetClass, std::string> HedgingSetKey;
    //                                                                          hedgingSet,  hedgingSubset
    typedef QuantLib::ext::tuple<NettingSetDetails, SaccrTradeData::AssetClass, std::string, std::string>
        HedgingSubsetKey;

    SaccrCalculator(const QuantLib::ext::shared_ptr<Crif>& capitalCrif,
                    const QuantLib::ext::shared_ptr<SaccrTradeData>& saccrTradeData, const std::string& baseCurrency,
                    const QuantLib::ext::shared_ptr<NettingSetManager>& nettingSetManager,
                    const QuantLib::ext::shared_ptr<ore::data::CounterpartyManager>& counterpartyManager,
                    const QuantLib::ext::shared_ptr<Market>& market,
                    const std::map<ReportType, QuantLib::ext::shared_ptr<ore::data::Report>>& outReports = {});

    // getters
    const QuantLib::ext::shared_ptr<SaccrTradeData>& saccrTradeData() const { return saccrTradeData_; }
    const QuantLib::ext::shared_ptr<NettingSetManager>& nettingSetManager() const { return nettingSetManager_; }
    const QuantLib::ext::shared_ptr<CounterpartyManager>& counterpartyManager() const { return counterpartyManager_; }
    const QuantLib::ext::shared_ptr<Market>& market() const { return market_; }

    const vector<NettingSetDetails>& nettingSetDetails() const { return nettingSetDetails_; }
    const std::set<SaccrTradeData::AssetClass>& assetClasses(NettingSetDetails nettingSetDetails) const;
    const vector<string>& hedgingSets(NettingSetDetails nettingSetDetails, SaccrTradeData::AssetClass assetClass) const;
    Real NPV() const { return totalNPV_; }
    Real EAD(NettingSetDetails nettingSetDetails) const;
    Real EAD(string nettingSet) const;
    Real RW(NettingSetDetails nettingSetDetails) const;
    Real CC() const { return totalCC_; }
    Real CC(NettingSetDetails nettingSetDetails) const;
    Real RC(NettingSetDetails nettingSetDetails) const;
    Real PFE(NettingSetDetails nettingSetDetails) const;
    Real multiplier(NettingSetDetails nettingSetDetails) const;
    Real addOn(NettingSetDetails nettingSetDetails) const;
    Real addOn(NettingSetDetails nettingSetDetails, SaccrTradeData::AssetClass assetClass) const;
    Real addOn(NettingSetDetails nettingSetDetails, SaccrTradeData::AssetClass assetClass, string hedgingSet) const;

private:
    Real getFxRate(const string& ccy);
    // fill TradeData vector with trade-level information
    void aggregate();
    void clear();

    //! Reports that results are written to
    std::map<ReportType, QuantLib::ext::shared_ptr<ore::data::Report>> reports_;

    //! write any passed in reports
    void writeReports();

    void processCrifRecord(const CrifRecord& record);

    QuantLib::ext::shared_ptr<Crif> crif_;
    QuantLib::ext::shared_ptr<SaccrTradeData> saccrTradeData_;
    QuantLib::ext::shared_ptr<NettingSetManager> nettingSetManager_;
    QuantLib::ext::shared_ptr<CounterpartyManager> counterpartyManager_;
    QuantLib::ext::shared_ptr<Market> market_;
    std::string baseCurrency_;
    std::map<NettingSetDetails, SaCcrAmounts> amountsBase_;
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
    map<std::string, Real> tradeNPV_;
    map<NettingSetDetails, Real> RC_;
    map<NettingSetDetails, QuantLib::Size> MPOR_;
    map<NettingSetDetails, Real> addOn_;
    map<NettingSetDetails, Real> EAD_;
    map<NettingSetDetails, Real> RW_;
    Real totalCC_ = 0;
    map<NettingSetDetails, Real> CC_;
    map<NettingSetDetails, Real> PFE_;
    map<NettingSetDetails, Real> multiplier_;
    // per netting set and asset class
    map<AssetClassKey, Real> addOnAssetClass_;
    // per netting set, asset class and hedging set
    map<HedgingSetKey, Real> addOnHedgingSet_;
    map<HedgingSetKey, Real> effectiveNotional_;
    map<HedgingSubsetKey, Real> subsetEffectiveNotional_;
    map<string, bool> isIndex_;

    vector<NettingSetDetails> nettingSetDetails_;
    map<NettingSetDetails, std::set<SaccrTradeData::AssetClass>> assetClasses_;
    map<std::string, SaccrTradeData::AssetClass> tradeAssetClasses_;
    map<pair<NettingSetDetails, SaccrTradeData::AssetClass>, vector<string>> hedgingSets_;
    std::set<string> basisHedgingSets_;
    std::set<string> volatilityHedgingSets_;
    saCcrDefaults saCcrDefaults_;
    map<NettingSetDetails, std::set<string>> nettingSetToCpty_;
    std::set<NettingSetDetails> nettingSets_;
};

} // namespace analytics

} // namespace ore
