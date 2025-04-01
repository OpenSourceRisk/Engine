/*
 Copyright (C) 2017 Quaternion Risk Management Ltd.
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

/*! \file orea/simm/crifgenerator.hpp
    \brief Class that generates a CRIF report
 */

#pragma once

#include <orea/engine/sensitivitystream.hpp>
#include <orea/scenario/sensitivityscenariodata.hpp>
#include <orea/simm/crifrecord.hpp>
#include <orea/simm/simmnamemapper.hpp>
#include <ored/report/report.hpp>
#include <orea/simm/crifmarket.hpp>
#include <orea/simm/simmtradedata.hpp>
#include <ored/configuration/curveconfigurations.hpp>
#include <ored/portfolio/referencedata.hpp>
#include <ored/portfolio/additionalfieldgetter.hpp>

#include <ql/types.hpp>

#include <string>
#include <tuple>

namespace ore {
namespace analytics {

/*! Class used to ease retrieval of volatility data and shift size data during CRIF generation.
 */
class VolatilityDataCrif {
public:
    VolatilityDataCrif(const QuantLib::ext::shared_ptr<CrifMarket>& crifMarket);

    double vegaTimesVol(ore::analytics::RiskFactorKey::KeyType rfType, const std::string& rfName, double sensitivity,
                        const std::string& expiryTenor, const std::string& underlyingTerm);

    //! If we have a absolute shift it computes delta * atmvol(T_E, T_U) / shiftSize, 
    //! for relative shifts it returns delta / shiftSize
    QuantLib::Volatility getVolatility(ore::analytics::RiskFactorKey::KeyType rfType, const std::string& rfName,
                                       const std::string& expiryTenor, const std::string& underlyingTerm = "");

    ore::analytics::SensitivityScenarioData::SensitivityScenarioData::ShiftData getShiftData(ore::analytics::RiskFactorKey::KeyType rfType, const std::string& rfName);

    //! Clear the cached values.
    void reset();

    //! Key used to store data.
    struct Key {
        ore::analytics::RiskFactorKey::KeyType rfType;
        std::string rfName;
        std::string expiryTenor;
        std::string underlyingTerm;

        bool operator<(const Key& key) const;
    };

private:
    QuantLib::ext::shared_ptr<CrifMarket> crifMarket_;
    std::map<Key, QuantLib::Volatility> volatilities_;
    std::map<Key, ore::analytics::SensitivityScenarioData::ShiftData> shifts_;
};

//! Write \p key to \p os
std::ostream& operator<<(std::ostream& os, const VolatilityDataCrif::Key& key);

struct CrifRecordData {
    ore::analytics::CrifRecord::RiskType riskType = ore::analytics::CrifRecord::RiskType::Empty;
    std::string qualifier;
    std::string bucket;
    std::string label1;
    std::string label2;
    double sensitivity;
};

using ore::analytics::CrifRecord;
using ore::analytics::RiskFactorKey;

//! Base Class to convert a sensitivity record to CRIF record, having to implementation for SIMM and FRTB records
class CrifRecordGenerator {
public:
    CrifRecordGenerator(const QuantLib::ext::shared_ptr<ore::analytics::CrifConfiguration>& config,
                        const QuantLib::ext::shared_ptr<SimmNameMapper>& nameMapper,
			const QuantLib::ext::shared_ptr<SimmTradeData>& simmTradeData,
                        const QuantLib::ext::shared_ptr<CrifMarket>& crifMarket, bool xccyDiscounting,
                        const std::string& currency, QuantLib::Real usdSpot,
                        const QuantLib::ext::shared_ptr<ore::data::PortfolioFieldGetter>& fieldGetter,
                        const QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager>& referenceData,
                        const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs,
                        const string& discountIndex);
    virtual ~CrifRecordGenerator(){};

    virtual boost::optional<ore::analytics::CrifRecord> operator()(const ore::analytics::SensitivityRecord& sr,
                                                                   std::set<std::string>& failedTrades);
    

protected:
    QuantLib::ext::shared_ptr<ore::analytics::CrifConfiguration> config_;
    QuantLib::ext::shared_ptr<SimmNameMapper> nameMapper_;
    QuantLib::ext::shared_ptr<SimmTradeData> tradeData_;
    QuantLib::ext::shared_ptr<CrifMarket> crifMarket_;
    bool xccyDiscounting_;
    std::string currency_;
    QuantLib::Real usdSpot_;
    QuantLib::ext::shared_ptr<ore::data::PortfolioFieldGetter> fieldGetter_;
    QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager> referenceData_;
    QuantLib::ext::shared_ptr<ore::data::CurveConfigurations> curveConfigs_;
    std::string discountIndex_;
    //! Used to ease retrieval of volatility market data and shifts.
    VolatilityDataCrif volatilityData_;

    //! Caches
    /*! Store the Label2 value for CRIF rows related to the base currency discount curve.
        Calculated up front in the constructor to avoid repeating the logic for each sensi record.
    */
    std::string baseCcyDiscLabel2_;
    //! Cache any `Label2` values against their currency to avoid logic.
    std::map<std::string, std::string> cachedCcyLabel2_;
    //! Cache shift sizes
    std::map<ore::analytics::RiskFactorKey, std::pair<ShiftType, QuantLib::Real>> cachedShifts_;

    std::string crifQualifier(const std::string& oreName) { return nameMapper_->qualifier(oreName); }

    std::string bucket(const ore::analytics::CrifRecord::RiskType& rt, const std::string& qualifier) {
        return config_->bucket(rt, qualifier);
    }
    std::string label2(const QuantLib::ext::shared_ptr<QuantLib::InterestRateIndex>& index) { return config_->label2(index); }

    std::string label2(const QuantLib::Period period) { return config_->label2(period); }

    std::string label2(const std::string& ccyCode) {
        auto it = cachedCcyLabel2_.find(ccyCode);
        if (it != cachedCcyLabel2_.end())
            return it->second;

        DLOG("Start deducing Label2 value for discount currency " << ccyCode);

        // If we have conventions, try to deduce it from them. Fallback/default value is OIS.
        string lbl2 = "OIS";
        QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();
        if (conventions) {
            if (conventions->has(ccyCode + "-ON-DEPOSIT") && conventions->has(ccyCode + "-OIS")) {
                DLOG("Currency " << ccyCode << " has overnight index conventions so assume OIS for Label2.");
            } else if (conventions->has(ccyCode + "-DEPOSIT") && conventions->has(ccyCode + "-SWAP")) {
                DLOG("Could not get overnight conventions for currency " << ccyCode << " so use IRS conventions.");
                if (auto swapConv =
                        QuantLib::ext::dynamic_pointer_cast<IRSwapConvention>(conventions->get(ccyCode + "-SWAP"))) {
                    auto swapFloatIndex = swapConv->index();
                    lbl2 = label2(swapFloatIndex);
                    DLOG("Got Label2 value, " << lbl2 << ", from swap convention's float index "
                                              << swapFloatIndex->name() << ".");
                }
            } else {
                DLOG("Could not get overnight or standard IR type conventions for currency "
                     << ccyCode << " so assuming Label2 is OIS.");
            }
        } else {
            DLOG("CRIF generator does not have conventions so assuming Label2 value of OIS for currency " << ccyCode
                                                                                                          << ".");
        }

        DLOG("Finished deducing Label2 value, " << lbl2 << ", for discount currency " << ccyCode);

        // Update the cached value and return it.
        cachedCcyLabel2_[ccyCode] = lbl2;
        return lbl2;
    }
    // Mapping from a tenor string to crif tenor label
    virtual std::string tenorLabel(const std::string& tenor) const { return boost::to_lower_copy(tenor); };

    //! Creates the record from the CrifRecordData
    virtual ore::analytics::CrifRecord record(const SensitivityRecord& sr, CrifRecord::RiskType riskType,
                                              const std::string& qualifier, const std::string& bucket,
                                              const std::string& label1, const std::string& label2, double sensitivity);
    // SensiRecord conversion methods
    // ! All these methods should return empty risktype if not relevant
    CrifRecordData discountCurveImpl(const ore::analytics::SensitivityRecord& sr,
                                     const std::vector<std::string>& rfTokens);

    CrifRecordData yieldCurveImpl(const ore::analytics::SensitivityRecord& sr,
                                  const std::vector<std::string>& rfTokens);

    CrifRecordData indexCurveImpl(const ore::analytics::SensitivityRecord& sr,
                                  const std::vector<std::string>& rfTokens);

    //! Scales the ir vega with the implied atm vol
    virtual CrifRecordData irVolatilityImpl(const ore::analytics::SensitivityRecord& sr,
                                            const std::vector<std::string>& rfTokens, bool swaptionVol = false);

    virtual CrifRecordData yieldVolatilityImpl(const ore::analytics::SensitivityRecord& sr,
                                               const std::vector<std::string>& rfTokens);

    virtual CrifRecordData survivalProbabilityCurveImpl(const ore::analytics::SensitivityRecord& sr,
                                                        const std::vector<std::string>& rfTokens) = 0;

    virtual CrifRecordData cdsVolatilityImpl(const ore::analytics::SensitivityRecord& sr,
                                             const std::vector<std::string>& rfTokens) = 0;

    virtual CrifRecordData baseCorrelationImpl(const ore::analytics::SensitivityRecord& sr,
                                               const std::vector<std::string>& rfTokens) = 0;

    CrifRecordData fxSpotImpl(const ore::analytics::SensitivityRecord& sr, const std::vector<std::string>& rfTokens);
    virtual CrifRecordData fxVolatilityImpl(const ore::analytics::SensitivityRecord& sr,
                                            const std::vector<std::string>& rfTokens);
    // Equity
    virtual CrifRecordData equitySpotImpl(const ore::analytics::SensitivityRecord& sr,
                                          const std::vector<std::string>& rfTokens);
    virtual CrifRecordData equityVolatilityImpl(const ore::analytics::SensitivityRecord& sr,
                                                const std::vector<std::string>& rfTokens);
    // Commodity
    virtual CrifRecordData commodityCurveImpl(const ore::analytics::SensitivityRecord& sr,
                                              const std::vector<std::string>& rfTokens);
    virtual CrifRecordData commodityVolatilityImpl(const ore::analytics::SensitivityRecord& sr,
                                                   const std::vector<std::string>& rfTokens);
    // Inflation`
    virtual CrifRecordData inflationCurveImpl(const ore::analytics::SensitivityRecord& sr,
                                              const std::vector<std::string>& rfTokens);
    virtual CrifRecordData inflationVolatilityImpl(const ore::analytics::SensitivityRecord& sr,
                                                   const std::vector<std::string>& rfTokens);
    //! Handle special case if all non basecurrency discount curves are treated as xccy basis risk
    virtual CrifRecordData xccyBasisImpl(const std::string qualifier, double sensitivity);

    virtual ore::analytics::CrifRecord::RiskType
    riskTypeImpl(const ore::analytics::RiskFactorKey::KeyType& rfKeyType) = 0;

    virtual CrifRecordData defaultRecord(const ore::analytics::SensitivityRecord& sr,
                                         const std::vector<std::string>& rfTokens, bool includeLabel1 = true,
                                         bool includeBucket = true) {
        CrifRecordData data;
        data.riskType = riskTypeImpl(sr.key_1.keytype);
        data.qualifier = crifQualifier(sr.key_1.name);
        if (includeLabel1 && !rfTokens.empty())
            data.label1 = tenorLabel(rfTokens.front());
        if (includeBucket) {
            data.bucket = bucket(data.riskType, data.qualifier);
        }
        data.sensitivity = sr.delta;
        return data;
    }

    virtual double CdsAtmVol(const std::string& tradeId, const std::string& optionExpiry) const;
};

class SimmRecordGenerator : public CrifRecordGenerator {
public:
    SimmRecordGenerator(const QuantLib::ext::shared_ptr<SimmConfiguration>& simmConfiguration,
                        const QuantLib::ext::shared_ptr<SimmNameMapper>& nameMapper,
			const QuantLib::ext::shared_ptr<SimmTradeData>& tradeData,
                        const QuantLib::ext::shared_ptr<CrifMarket>& crifMarket, bool xccyDiscounting = false,
                        const std::string& currency = "USD", QuantLib::Real usdSpot = 1.0,
                        const QuantLib::ext::shared_ptr<ore::data::PortfolioFieldGetter>& fieldGetter = nullptr,
                        const QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager>& referenceData = nullptr,
                        const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs = nullptr,
                        const std::string& discountIndex = "");

protected:
    ore::analytics::CrifRecord record(const SensitivityRecord& sr, CrifRecord::RiskType riskType,
                                                       const std::string& qualifier, const std::string& bucket,
                                                       const std::string& label1, const std::string& label2,
                                                       double sensitivity) override;

    CrifRecordData survivalProbabilityCurveImpl(const ore::analytics::SensitivityRecord& sr,
                                                const std::vector<std::string>& rfTokens) override;

    CrifRecordData cdsVolatilityImpl(const ore::analytics::SensitivityRecord& sr,
                                     const std::vector<std::string>& rfTokens) override;

    CrifRecordData baseCorrelationImpl(const ore::analytics::SensitivityRecord& sr,
                                       const std::vector<std::string>& rfTokens) override;

    ore::analytics::CrifRecord::RiskType
    riskTypeImpl(const ore::analytics::RiskFactorKey::KeyType& rfKeyType) override;

private:
    // Cache
    struct TradeCache {
        std::map<std::string, std::string> additionalFields;
        std::set<CrifRecord::Regulation> simmCollectRegs, simmPostRegs;
    };

    std::map<std::string, TradeCache> tradeCache_;
};

} // namespace analytics
} // namespace ore
