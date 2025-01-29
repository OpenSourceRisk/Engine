/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
 All rights reserved.
*/

/*! \file orepcapital/orea/engine/saccrtradedata.hpp
    \brief Class for holding SA-CCR trade data
*/

#pragma once

#include <boost/none.hpp>
#include <ored/portfolio/nettingsetdetails.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/optional.hpp>
#include <ql/time/period.hpp>
#include <ql/utilities/null.hpp>

#include <boost/optional.hpp>
#include <string>

namespace QuantLib {
class DayCounter;
}

namespace ore {
namespace data {
class CollateralBalances;
class Market;
class NettingSetManager;
class NettingSetDefinitions;
class Portfolio;
class ReferenceDataManager;
class Trade;
class OptionData;
class CounterpartyManager;
class CounterpartyInformation;
} // namespace data
} // namespace ore

namespace ore {
namespace analytics {
struct CrifRecord;
class SimmBucketMapper;
class SimmNameMapper;

// SA-CCR defaults for netting set (counterparty) entries that are missing from
// the collateral balances and netting set definitions (or counterparty information).
struct saCcrDefaults {
    struct CollateralBalances {
        std::string ccy = "USD";
        QuantLib::Real iaHeld = 0.0;
        QuantLib::Real im = 0.0;
        QuantLib::Real vm = 0.0;
    };
    struct CounterpartyInformation {
        bool ccp = false;
        QuantLib::Real saccrRW = 1.0;
        std::string counterpartyId = "SACCR_DEFAULT_CPTY";
    };
    struct NettingSetDefinitions {
        // collateralised
        bool activeCsaFlag = true;
        QuantLib::Period mpor = 2 * QuantLib::Weeks;
        QuantLib::Real iaHeld = 0.0;
        QuantLib::Real thresholdRcv = 0.0;
        QuantLib::Real mtaRcv = 0.0;
        bool calculateIMAmount = false;
        bool calculateVMAmount = false;
    };

    CollateralBalances collBalances;
    CounterpartyInformation cptyInfo;
    NettingSetDefinitions nettingSetDef;
};

class SaccrTradeData : public QuantLib::ext::enable_shared_from_this<SaccrTradeData> {

public:
    SaccrTradeData(
        const QuantLib::ext::shared_ptr<ore::data::Market>& market,
        const QuantLib::ext::shared_ptr<ore::analytics::SimmNameMapper>& nameMapper,
        const QuantLib::ext::shared_ptr<ore::analytics::SimmBucketMapper>& bucketMapper,
        const QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager>& refDataManager,
        const std::string& baseCurrency, const std::string& nullString = "#N/A",
        const QuantLib::ext::shared_ptr<ore::data::NettingSetManager>& nettingSetManager = nullptr,
        const QuantLib::ext::shared_ptr<ore::data::CounterpartyManager>& counterpartyManager = nullptr,
        const QuantLib::ext::shared_ptr<ore::data::CollateralBalances>& collateralBalances = nullptr,
        const QuantLib::ext::shared_ptr<ore::data::CollateralBalances>& calculatedCollateralBalances = nullptr)
        : market_(market), nameMapper_(nameMapper), bucketMapper_(bucketMapper), refDataManager_(refDataManager),
          nullString_(nullString), nettingSetManager_(nettingSetManager), counterpartyManager_(counterpartyManager),
          collateralBalances_(collateralBalances), calculatedCollateralBalances_(calculatedCollateralBalances),
          baseCurrency_(baseCurrency) {}
    class Impl;

    enum AssetClass : char { IR, FX, Credit, Equity, Commodity, None };
    enum class CommodityHedgingSet : char { Energy, Agriculture, Metal, Other };

    struct AdjustedNotional {
        AdjustedNotional() : notional(QuantLib::Null<QuantLib::Real>()), currency(""), currentPrice(boost::none) {}
        AdjustedNotional(QuantLib::Real notional, const std::string& currency,
                         const QuantLib::ext::optional<QuantLib::Real>& currentPrice = boost::none)
            : notional(notional), currency(currency), currentPrice(currentPrice) {}

        QuantLib::Real notional;
        std::string currency;
        QuantLib::ext::optional<QuantLib::Real> currentPrice;
    };

    struct FxAmounts {
        FxAmounts(QuantLib::Real boughtAmount, const std::string& boughtCurrency, QuantLib::Real soldAmount,
                  const std::string& soldCurrency)
            : boughtAmount(boughtAmount), soldAmount(soldAmount), boughtCurrency(boughtCurrency),
              soldCurrency(soldCurrency) {}

        QuantLib::Real boughtAmount = QuantLib::Null<QuantLib::Real>();
        QuantLib::Real soldAmount = QuantLib::Null<QuantLib::Real>();
        std::string boughtCurrency = "", soldCurrency = "";
        QuantLib::Real notional = QuantLib::Null<QuantLib::Real>();
        std::string notionalCurrency = "";
    };

    struct Dates {
        Dates() : M(QuantLib::Null<QuantLib::Real>()), S(boost::none), E(boost::none) {}
        Dates(QuantLib::Real M, const QuantLib::ext::optional<QuantLib::Real>& S = boost::none,
              const QuantLib::ext::optional<QuantLib::Real>& E = boost::none)
            : M(M), S(S), E(E) {}

        QuantLib::Real M;
        QuantLib::ext::optional<QuantLib::Real> S, E;
    };

    struct HedgingData {
        HedgingData(){};

        // "basis transaction" - Same asset class, same underlying currencies, different underlyings
        bool isBasis() const { return hedgingSet.find("_BASIS") != string::npos; }
        bool empty() const { return hedgingSet.empty() && (!hedgingSubset.has_value() || hedgingSubset.get().empty()); }

        std::string hedgingSet = "";
        bool isVol = false;
        QuantLib::ext::optional<std::string> hedgingSubset = boost::none;
    };

    struct UnderlyingData {
        UnderlyingData()
            : originalName(""), qualifier(""), saccrAssetClass(SaccrTradeData::AssetClass::None),
              oreAssetClass(ore::data::AssetClass::PORTFOLIO_DETAILS), isIndex(false) {}
        UnderlyingData(const std::string& originalName, const std::string& qualifier,
                       const SaccrTradeData::AssetClass& saccrAssetClass, const ore::data::AssetClass& oreAssetClass,
                       bool isIndex)
            : originalName(originalName), qualifier(qualifier), saccrAssetClass(saccrAssetClass),
              oreAssetClass(oreAssetClass), isIndex(isIndex) {}

        std::tuple<std::string, std::string, ore::data::AssetClass> key() const {
            return std::make_tuple(originalName, qualifier, oreAssetClass);
        }

        bool operator<(const UnderlyingData& u) const {
            return std::tie(originalName, qualifier, saccrAssetClass, oreAssetClass, isIndex) <
                   std::tie(u.originalName, u.qualifier, u.saccrAssetClass, u.oreAssetClass, u.isIndex);
        }
        bool operator==(const UnderlyingData& u) const {
            return std::tie(originalName, qualifier, saccrAssetClass, oreAssetClass, isIndex) ==
                   std::tie(u.originalName, u.qualifier, u.saccrAssetClass, u.oreAssetClass, u.isIndex);
        }
        bool operator!=(const UnderlyingData& u) const { return !(*this == u); }
        std::string originalName, qualifier;
        SaccrTradeData::AssetClass saccrAssetClass;
        ore::data::AssetClass oreAssetClass;
        bool isIndex;

        // Should only be set in Impl::addHedgingData()
        // HedgingData hedgingData;
    };

    struct Contribution {
        Contribution(const UnderlyingData& underlyingData, const std::string& currency = "",
                     QuantLib::Real adjustedNotional = QuantLib::Null<QuantLib::Real>(),
                     QuantLib::Real delta = QuantLib::Null<QuantLib::Real>(), bool isOption = false, bool isVol = false,
                     const QuantLib::ext::optional<QuantLib::Real>& supervisoryDuration = boost::none,
                     const QuantLib::ext::optional<QuantLib::Real>& startDate = boost::none,
                     const QuantLib::ext::optional<QuantLib::Real>& endDate = boost::none,
                     const QuantLib::ext::optional<QuantLib::Real>& lastExerciseDate = boost::none,
                     const QuantLib::ext::optional<QuantLib::Real>& currentPrice = boost::none,
                     const QuantLib::ext::optional<QuantLib::Real>& optionDeltaPrice = boost::none,
                     const QuantLib::ext::optional<QuantLib::Real>& strike = boost::none,
                     const QuantLib::ext::optional<QuantLib::Size>& numNominalFlows = boost::none)
            : underlyingData(underlyingData), hedgingData(HedgingData()), currency(currency),
              adjustedNotional(adjustedNotional), delta(delta), isOption(isOption), isVol(isVol),
              supervisoryDuration(supervisoryDuration), startDate(startDate), endDate(endDate),
              lastExerciseDate(lastExerciseDate), currentPrice(currentPrice), optionDeltaPrice(optionDeltaPrice),
              strike(strike), numNominalFlows(numNominalFlows) {}
        Contribution()
            : underlyingData(UnderlyingData()), hedgingData(HedgingData()), currency(""),
              delta(QuantLib::Null<QuantLib::Real>()), maturity(QuantLib::Null<QuantLib::Real>()),
              maturityFactor(QuantLib::Null<QuantLib::Real>()), isOption(false), isVol(false),
              supervisoryDuration(boost::none), startDate(boost::none), endDate(boost::none),
              lastExerciseDate(boost::none), currentPrice(boost::none), optionDeltaPrice(boost::none),
              strike(boost::none), numNominalFlows(boost::none) {}

        // Trade ID/Type, nettingSetDetails, npv,  can be taken from trade_
        UnderlyingData underlyingData; // SA-CCR/ORE assetClass, qualifier/underlyingName, isIndex
        HedgingData hedgingData;       // hedgingSet, hedgingSubset, isVol
        std::string currency;          // currency of the notional and other amounts
        QuantLib::Real adjustedNotional, delta;
        QuantLib::Real maturity = QuantLib::Null<QuantLib::Real>();
        QuantLib::Real maturityFactor = QuantLib::Null<QuantLib::Real>();
        bool isOption, isVol;

        // Optional values which may be required depending on asset class or optionality
        QuantLib::ext::optional<QuantLib::Real> supervisoryDuration, startDate, endDate, lastExerciseDate, currentPrice,
            optionDeltaPrice, strike;
        QuantLib::ext::optional<QuantLib::Size> numNominalFlows;
        string bucket = "";
    };

    void initialise(const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio);
    QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio() { return portfolio_; }
    QuantLib::ext::shared_ptr<ore::data::Market>& market() { return market_; }
    std::string getUnderlyingName(const std::string& index, const ore::data::AssetClass& assetClass,
                                  bool withPrefix = false) const;
    std::string getSimmQualifier(const std::string& name) const;
    std::string getCommodityHedgingSet(const std::string& comm) const;
    std::string getCommodityHedgingSubset(const std::string& comm, bool mapQualifier = true) const;
    std::string getQualifierCommodityMapping(const std::string& qualifier) const;
    QuantLib::Real getFxRate(const std::string& ccyPair) const;
    static std::set<ore::data::AssetClass> saccrAssetClassToOre(const AssetClass& saccrAssetClass);
    static AssetClass oreAssetClassToSaccr(const ore::data::AssetClass& oreAssetClass);

    const std::string& counterparty(const ore::data::NettingSetDetails& nsd) const;
    const QuantLib::Real NPV(const ore::data::NettingSetDetails& nsd) const;
    // std::vector<Contribution> getContributions() const;

    const QuantLib::ext::shared_ptr<ore::analytics::SimmNameMapper>& nameMapper() const { return nameMapper_; }
    const QuantLib::ext::shared_ptr<ore::analytics::SimmBucketMapper>& bucketMapper() const { return bucketMapper_; }
    const QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager>& refDataManager() const { return refDataManager_; }
    const QuantLib::ext::shared_ptr<ore::data::NettingSetManager>& nettingSetManager() const {
        return nettingSetManager_;
    }
    const QuantLib::ext::shared_ptr<ore::data::CounterpartyManager>& counterpartyManager() const {
        return counterpartyManager_;
    }
    const QuantLib::ext::shared_ptr<ore::data::CollateralBalances>& collateralBalances() const {
        return collateralBalances_;
    }
    const QuantLib::ext::shared_ptr<ore::data::CollateralBalances>& calculatedCollateralBalances() const {
        return calculatedCollateralBalances_;
    }
    const std::string& nullString() const { return nullString_; }
    const std::set<ore::data::NettingSetDetails>& nettingSets() const { return nettingSets_; }
    const std::set<ore::data::NettingSetDetails>& defaultIMBalances() const { return defaultIMBalances_; }
    const std::set<ore::data::NettingSetDetails>& defaultVMBalances() const { return defaultVMBalances_; }
    const std::string& baseCurrency() const { return baseCurrency_; }

    // const std::set<std::string>& tradeIds() const { return tradeIds_; }
    const std::map<std::string, QuantLib::ext::shared_ptr<SaccrTradeData::Impl>>& data() const { return data_; }
    QuantLib::Size size() const { return data_.size(); }
    QuantLib::Size tradeCount(const ore::data::NettingSetDetails& nsd) const { return tradeCount_.at(nsd); }

private:
    void validate();

    QuantLib::ext::shared_ptr<ore::data::Portfolio> portfolio_;
    QuantLib::ext::shared_ptr<ore::data::Market> market_;
    QuantLib::ext::shared_ptr<ore::analytics::SimmNameMapper> nameMapper_;
    QuantLib::ext::shared_ptr<ore::analytics::SimmBucketMapper> bucketMapper_;
    QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager> refDataManager_;
    std::string nullString_;
    QuantLib::ext::shared_ptr<ore::data::NettingSetManager> nettingSetManager_;
    QuantLib::ext::shared_ptr<ore::data::CounterpartyManager> counterpartyManager_;
    QuantLib::ext::shared_ptr<ore::data::CollateralBalances> collateralBalances_;
    QuantLib::ext::shared_ptr<ore::data::CollateralBalances> calculatedCollateralBalances_;

    std::set<ore::data::NettingSetDetails> defaultIMBalances_;
    std::set<ore::data::NettingSetDetails> defaultVMBalances_;

    //		 trade ID
    std::map<std::string, QuantLib::ext::shared_ptr<SaccrTradeData::Impl>> data_;
    // std::set<std::string> tradeIds_;

    std::set<ore::data::NettingSetDetails> nettingSets_;
    std::map<ore::data::NettingSetDetails, set<string>> nettingSetToCpty_;
    std::map<ore::data::NettingSetDetails, QuantLib::Real> NPV_;
    saCcrDefaults saCcrDefaults_;
    std::string baseCurrency_;
    map<ore::data::NettingSetDetails, QuantLib::Size> tradeCount_;

    QuantLib::ext::shared_ptr<SaccrTradeData::Impl> getImpl(const QuantLib::ext::shared_ptr<ore::data::Trade>& trade);
};

std::ostream& operator<<(std::ostream& os, SaccrTradeData::AssetClass ac);
std::ostream& operator<<(std::ostream& os, SaccrTradeData::CommodityHedgingSet hs);

class SaccrTradeData::Impl {

public:
    Impl(){};
    virtual ~Impl(){};
    QuantLib::ext::shared_ptr<ore::data::Market>& market() const { return tradeData_.lock()->market(); }
    QuantLib::Real getFxRate(const std::string& ccyPair) const { return tradeData_.lock()->getFxRate(ccyPair); }
    virtual std::string name() const { return "SaccrTradeData::Impl"; }
    virtual QuantLib::ext::shared_ptr<Impl> copy() const = 0;
    // Assumption: any underlying returned by this method must qualify as a "primary risk factor", e.g. for TRS
    // the funding leg should not be considered as a PRF, since the (only) PRF there is the return leg underlying.
    // For Float-Float swaps, the underlying of each leg would typically qualify as a PRF (i.e. trade is a "basis
    // transaction")
    // Underlyings SaccrTradeData::Impl::getUnderlyings() const;
    SaccrTradeData::UnderlyingData
    getUnderlyingData(const std::string& originalName,
                      const QuantLib::ext::optional<ore::data::AssetClass>& oreAssetClass = boost::none) const;
    SaccrTradeData::UnderlyingData getUnderlyingData(const std::string& boughtCurrency,
                                                     const std::string& soldCurrency) const;
    virtual void calculate();
    virtual std::vector<SaccrTradeData::Contribution> calculateContributions() const;
    SaccrTradeData::AdjustedNotional getFxAdjustedNotional(const SaccrTradeData::FxAmounts& fxAmounts) const;
    SaccrTradeData::FxAmounts
    getSingleFxAmounts(const QuantLib::ext::shared_ptr<ore::data::Trade>& trade = nullptr) const;
    virtual std::set<std::string> getTradeTypes() const = 0;
    const std::vector<Contribution>& getContributions() const;
    void addHedgingData(std::vector<SaccrTradeData::Contribution>&) const;
    QuantLib::Real getMaturity(const QuantLib::ext::shared_ptr<ore::data::Trade>& trade = nullptr) const;
    virtual QuantLib::ext::optional<QuantLib::Real>
    getSupervisoryDuration(const AssetClass& assetClass, const QuantLib::ext::optional<QuantLib::Real>& startDate,
                           const QuantLib::ext::optional<QuantLib::Real>& endDate) const;
    virtual QuantLib::Real getSupervisoryOptionVolatility(const SaccrTradeData::UnderlyingData& underlyingData) const;
    std::vector<Contribution>
    calculateSingleOptionContribution(const QuantLib::ext::shared_ptr<ore::data::Trade>& trade = nullptr) const;
    QuantLib::Real getLastExerciseDate(const ore::data::OptionData& optionData) const;
    virtual bool isVol() const { QL_FAIL("Not yet implemented"); }
    virtual QuantLib::ext::optional<QuantLib::Size> getNominalFlowCount() const { return boost::none; }

    std::string getBucket(const SaccrTradeData::Contribution& contribution) const;
    std::tuple<QuantLib::Real, std::string, QuantLib::ext::optional<QuantLib::Real>>
    getLegAverageNotional(QuantLib::Size legIdx, const std::string& legType) const;

    void setTradeData(const QuantLib::ext::weak_ptr<SaccrTradeData>& tradeData) { tradeData_ = tradeData; }
    void setTrade(const QuantLib::ext::shared_ptr<ore::data::Trade>& trade) { trade_ = trade; }
    const QuantLib::ext::shared_ptr<ore::data::Trade>& trade() const { return trade_; }
    QuantLib::Real getMaturityFactor(QuantLib::Real maturity) const;

    // Getters for underlying trade for convenience
    const ore::data::NettingSetDetails& nettingSetDetails() const { return trade_->envelope().nettingSetDetails(); }
    const std::string& counterparty() const { return trade_->envelope().counterparty(); }
    const QuantLib::Real NPV() const { return trade_->instrument()->NPV(); }

protected:
    virtual std::vector<SaccrTradeData::Contribution> calculateImplContributions() const = 0;
    QuantLib::DayCounter dc() const { return QuantLib::ActualActual(QuantLib::ActualActual::ISDA); }

    QuantLib::ext::weak_ptr<SaccrTradeData> tradeData_;
    QuantLib::ext::shared_ptr<ore::data::Trade> trade_;
    std::vector<Contribution> contributions_;
    bool calculated_ = false;
};

class CommodityForwardSaccrImpl : public SaccrTradeData::Impl {
public:
    virtual std::string name() const override { return "CommodityForwardSaccrImpl"; }
    virtual std::set<std::string> getTradeTypes() const override;
    virtual QuantLib::ext::shared_ptr<SaccrTradeData::Impl> copy() const override {
        return QuantLib::ext::make_shared<CommodityForwardSaccrImpl>(*this);
    }

protected:
    virtual std::vector<SaccrTradeData::Contribution> calculateImplContributions() const override;
};

class CommodityDigitalOptionSaccrImpl : public SaccrTradeData::Impl {
    virtual std::string name() const override { return "CommodityDigitalOptionSaccrImpl"; }
    virtual std::set<std::string> getTradeTypes() const override;
    virtual QuantLib::ext::shared_ptr<SaccrTradeData::Impl> copy() const override {
        return QuantLib::ext::make_shared<CommodityDigitalOptionSaccrImpl>(*this);
    }

protected:
    virtual std::vector<SaccrTradeData::Contribution> calculateImplContributions() const override;
};

class CommoditySpreadOptionSaccrImpl : public SaccrTradeData::Impl {
    virtual std::string name() const override { return "CommoditySpreadOptionSaccrImpl"; }
    virtual std::set<std::string> getTradeTypes() const override;
    virtual QuantLib::ext::shared_ptr<SaccrTradeData::Impl> copy() const override {
        return QuantLib::ext::make_shared<CommoditySpreadOptionSaccrImpl>(*this);
    }

protected:
    virtual std::vector<SaccrTradeData::Contribution> calculateImplContributions() const override;
};

class CommoditySwaptionSaccrImpl : public SaccrTradeData::Impl {
    virtual std::string name() const override { return "CommoditySwaptionSaccrImpl"; }
    virtual std::set<std::string> getTradeTypes() const override;
    virtual QuantLib::ext::shared_ptr<SaccrTradeData::Impl> copy() const override {
        return QuantLib::ext::make_shared<CommoditySwaptionSaccrImpl>(*this);
    }

protected:
    virtual std::vector<SaccrTradeData::Contribution> calculateImplContributions() const override;
};

class CommodityPositionSaccrImpl : public SaccrTradeData::Impl {
    virtual std::string name() const override { return "CommodityPositionSaccrImpl"; }
    virtual std::set<std::string> getTradeTypes() const override;
    virtual QuantLib::ext::shared_ptr<SaccrTradeData::Impl> copy() const override {
        return QuantLib::ext::make_shared<CommodityPositionSaccrImpl>(*this);
    }

protected:
    virtual std::vector<SaccrTradeData::Contribution> calculateImplContributions() const override;
};

// EQUITY

class EquityForwardSaccrImpl : public SaccrTradeData::Impl {
public:
    virtual std::string name() const override { return "EquityForwardSaccrImpl"; }
    virtual std::set<std::string> getTradeTypes() const override;
    virtual QuantLib::ext::shared_ptr<SaccrTradeData::Impl> copy() const override {
        return QuantLib::ext::make_shared<EquityForwardSaccrImpl>(*this);
    }

protected:
    virtual std::vector<SaccrTradeData::Contribution> calculateImplContributions() const override;
};

class EquityPositionSaccrImpl : public SaccrTradeData::Impl {
    virtual std::string name() const override { return "EquityPositionSaccrImpl"; }
    virtual std::set<std::string> getTradeTypes() const override;
    // virtual void calculate() override;
    virtual QuantLib::ext::shared_ptr<SaccrTradeData::Impl> copy() const override {
        return QuantLib::ext::make_shared<EquityPositionSaccrImpl>(*this);
    }

protected:
    virtual std::vector<SaccrTradeData::Contribution> calculateImplContributions() const override;
};

class EquityOptionPositionSaccrImpl : public SaccrTradeData::Impl {
    virtual std::string name() const override { return "EquityOptionPositionSaccrImpl"; }
    virtual std::set<std::string> getTradeTypes() const override;
    // virtual void calculate() override;
    virtual QuantLib::ext::shared_ptr<SaccrTradeData::Impl> copy() const override {
        return QuantLib::ext::make_shared<EquityOptionPositionSaccrImpl>(*this);
    }

protected:
    virtual std::vector<SaccrTradeData::Contribution> calculateImplContributions() const override;
};

class EquityDigitalOptionSaccrImpl : public SaccrTradeData::Impl {
    virtual std::string name() const override { return "EquityDigitalOptionSaccrImpl"; }
    virtual std::set<std::string> getTradeTypes() const override;
    virtual QuantLib::ext::shared_ptr<SaccrTradeData::Impl> copy() const override {
        return QuantLib::ext::make_shared<EquityDigitalOptionSaccrImpl>(*this);
    }

protected:
    virtual std::vector<SaccrTradeData::Contribution> calculateImplContributions() const override;
};

class EquityTouchOptionSaccrImpl : public SaccrTradeData::Impl {
    virtual std::string name() const override { return "EquityTouchOptionSaccrImpl"; }
    virtual std::set<std::string> getTradeTypes() const override;
    virtual QuantLib::ext::shared_ptr<SaccrTradeData::Impl> copy() const override {
        return QuantLib::ext::make_shared<EquityTouchOptionSaccrImpl>(*this);
    }

protected:
    virtual std::vector<SaccrTradeData::Contribution> calculateImplContributions() const override;
};

class EquityDoubleTouchOptionSaccrImpl : public SaccrTradeData::Impl {
    virtual std::string name() const override { return "EquityDoubleTouchOptionSaccrImpl"; }
    virtual std::set<std::string> getTradeTypes() const override;
    virtual QuantLib::ext::shared_ptr<SaccrTradeData::Impl> copy() const override {
        return QuantLib::ext::make_shared<EquityDoubleTouchOptionSaccrImpl>(*this);
    }

protected:
    virtual std::vector<SaccrTradeData::Contribution> calculateImplContributions() const override;
};

class EquityBarrierOptionSaccrImpl : public SaccrTradeData::Impl {
    virtual std::string name() const override { return "EquityBarrierOptionSaccrImpl"; }
    virtual std::set<std::string> getTradeTypes() const override;
    virtual QuantLib::ext::shared_ptr<SaccrTradeData::Impl> copy() const override {
        return QuantLib::ext::make_shared<EquityBarrierOptionSaccrImpl>(*this);
    }

protected:
    virtual std::vector<SaccrTradeData::Contribution> calculateImplContributions() const override;
};

class EquityDoubleBarrierOptionSaccrImpl : public SaccrTradeData::Impl {
    virtual std::string name() const override { return "EquityDoubleBarrierOptionSaccrImpl"; }
    virtual std::set<std::string> getTradeTypes() const override;
    virtual QuantLib::ext::shared_ptr<SaccrTradeData::Impl> copy() const override {
        return QuantLib::ext::make_shared<EquityDoubleBarrierOptionSaccrImpl>(*this);
    }

protected:
    virtual std::vector<SaccrTradeData::Contribution> calculateImplContributions() const override;
};

// FX
class FxSaccrImpl : public SaccrTradeData::Impl {
public:
    virtual std::string name() const override { return "FxSaccrImpl"; }
    virtual std::set<std::string> getTradeTypes() const override;
    virtual QuantLib::ext::shared_ptr<SaccrTradeData::Impl> copy() const override {
        return QuantLib::ext::make_shared<FxSaccrImpl>(*this);
    }
    // virtual bool isFlipped() const;
    //  virtual std::pair<QuantLib::Real, QuantLib::Real>
    //  getOptionType(const QuantLib::ext::shared_ptr<ore::data::Trade>& trade = nullptr) const override;
    //  virtual std::vector<QuantLib::Real>
    //  getOptionStrikes(const QuantLib::ext::shared_ptr<ore::data::Trade>& trade = nullptr) const override;
    //  virtual QuantLib::ext::optional<QuantLib::Real>
    //  getOptionPrice(const QuantLib::ext::shared_ptr<ore::data::Trade>& trade = nullptr) const override;
    //
    // virtual SaccrTradeData::AdjustedNotional getSignedAdjustedNotional() const override;
    // virtual SaccrTradeData::Delta getDelta() const override;

protected:
    virtual std::vector<SaccrTradeData::Contribution> calculateImplContributions() const override;
};

// IR
class CashPositionSaccrImpl : public SaccrTradeData::Impl {
public:
    virtual std::string name() const override { return "CashPositionSaccrImpl"; }
    virtual std::set<std::string> getTradeTypes() const override;
    virtual QuantLib::ext::shared_ptr<SaccrTradeData::Impl> copy() const override {
        return QuantLib::ext::make_shared<CashPositionSaccrImpl>(*this);
    }

protected:
    virtual std::vector<SaccrTradeData::Contribution> calculateImplContributions() const override;
};

class FRASaccrImpl : public SaccrTradeData::Impl {
public:
    virtual std::string name() const override { return "FRASaccrImpl"; }
    virtual std::set<std::string> getTradeTypes() const override;
    virtual QuantLib::ext::shared_ptr<SaccrTradeData::Impl> copy() const override {
        return QuantLib::ext::make_shared<FRASaccrImpl>(*this);
    }
    // virtual SaccrTradeData::Delta getDelta() const override;
protected:
    virtual std::vector<SaccrTradeData::Contribution> calculateImplContributions() const override;
};

class CapFloorSaccrImpl : public SaccrTradeData::Impl {
public:
    virtual std::string name() const override { return "CapFloorSaccrImpl"; }
    virtual std::set<std::string> getTradeTypes() const override;
    virtual QuantLib::ext::shared_ptr<SaccrTradeData::Impl> copy() const override {
        return QuantLib::ext::make_shared<CapFloorSaccrImpl>(*this);
    }

protected:
    virtual std::vector<SaccrTradeData::Contribution> calculateImplContributions() const override;
};

// Credit

class BondTRSSaccrImpl : public SaccrTradeData::Impl {
public:
    virtual std::string name() const override { return "BondTRSSaccrImpl"; }
    virtual std::set<std::string> getTradeTypes() const override;
    virtual QuantLib::ext::shared_ptr<SaccrTradeData::Impl> copy() const override {
        return QuantLib::ext::make_shared<BondTRSSaccrImpl>(*this);
    }

protected:
    virtual std::vector<SaccrTradeData::Contribution> calculateImplContributions() const override;
};

class BondRepoSaccrImpl : public SaccrTradeData::Impl {
public:
    virtual std::string name() const override { return "BondRepoSaccrImpl"; }
    virtual std::set<std::string> getTradeTypes() const override;
    virtual QuantLib::ext::shared_ptr<SaccrTradeData::Impl> copy() const override {
        return QuantLib::ext::make_shared<BondRepoSaccrImpl>(*this);
    }

protected:
    virtual std::vector<SaccrTradeData::Contribution> calculateImplContributions() const override;
};

// Generic/multiple product classes

class ScriptedTradeSaccrImpl : public SaccrTradeData::Impl {
    virtual std::string name() const override { return "ScriptedTradeSaccrImpl"; }
    virtual std::set<std::string> getTradeTypes() const override;
    virtual QuantLib::ext::shared_ptr<SaccrTradeData::Impl> copy() const override {
        return QuantLib::ext::make_shared<ScriptedTradeSaccrImpl>(*this);
    }

protected:
    virtual std::vector<SaccrTradeData::Contribution> calculateImplContributions() const override;
};

class VanillaOptionSaccrImpl : public SaccrTradeData::Impl {
public:
    virtual std::string name() const override { return "VanillaOptionSaccrImpl"; }
    virtual std::set<std::string> getTradeTypes() const override;
    virtual QuantLib::ext::shared_ptr<SaccrTradeData::Impl> copy() const override {
        return QuantLib::ext::make_shared<VanillaOptionSaccrImpl>(*this);
    }

protected:
    virtual std::vector<SaccrTradeData::Contribution> calculateImplContributions() const override;
};

class TotalReturnSwapSaccrImpl : public SaccrTradeData::Impl {
public:
    virtual std::string name() const override { return "TotalReturnSwapSaccrImpl"; }
    virtual std::set<std::string> getTradeTypes() const override;
    virtual QuantLib::ext::shared_ptr<SaccrTradeData::Impl> copy() const override {
        return QuantLib::ext::make_shared<TotalReturnSwapSaccrImpl>(*this);
    }

protected:
    virtual std::vector<SaccrTradeData::Contribution> calculateImplContributions() const override;
};

class SwapSaccrImpl : public SaccrTradeData::Impl {
public:
    virtual std::string name() const override { return "SwapSaccrImpl"; }
    virtual std::set<std::string> getTradeTypes() const override;
    virtual QuantLib::ext::shared_ptr<SaccrTradeData::Impl> copy() const override {
        return QuantLib::ext::make_shared<SwapSaccrImpl>(*this);
    }

protected:
    virtual std::vector<SaccrTradeData::Contribution> calculateImplContributions() const override;
};

class SwaptionSaccrImpl : public SaccrTradeData::Impl {
    virtual std::string name() const override { return "SwaptionSaccrImpl"; }
    virtual std::set<std::string> getTradeTypes() const override;
    virtual QuantLib::ext::shared_ptr<SaccrTradeData::Impl> copy() const override {
        return QuantLib::ext::make_shared<SwaptionSaccrImpl>(*this);
    }

protected:
    virtual std::vector<SaccrTradeData::Contribution> calculateImplContributions() const override;
};

class VarianceSwapSaccrImpl : public SaccrTradeData::Impl {
    virtual std::string name() const override { return "VarianceSwapSaccrImpl"; }
    virtual std::set<std::string> getTradeTypes() const override;
    virtual QuantLib::ext::shared_ptr<SaccrTradeData::Impl> copy() const override {
        return QuantLib::ext::make_shared<VarianceSwapSaccrImpl>(*this);
    }

protected:
    virtual std::vector<SaccrTradeData::Contribution> calculateImplContributions() const override;
};

class AsianOptionSaccrImpl : public SaccrTradeData::Impl {
    virtual std::string name() const override { return "AsianOptionSaccrImpl"; }
    virtual std::set<std::string> getTradeTypes() const override;
    virtual QuantLib::ext::shared_ptr<SaccrTradeData::Impl> copy() const override {
        return QuantLib::ext::make_shared<AsianOptionSaccrImpl>(*this);
    }

protected:
    virtual std::vector<SaccrTradeData::Contribution> calculateImplContributions() const override;
};
} // namespace analytics
} // namespace ore
