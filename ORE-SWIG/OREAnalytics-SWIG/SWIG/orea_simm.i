/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

#ifndef orea_simm_i
#define orea_simm_i

%include stl.i
%include std_set.i
%include std_map.i
%include types.i

%include ored_portfolio.i

%shared_ptr(ore::analytics::Crif)
%shared_ptr(ore::analytics::CrifLoader)
%shared_ptr(ore::analytics::CsvFileCrifLoader)
%shared_ptr(ore::analytics::CsvBufferCrifLoader)
%shared_ptr(ore::analytics::SimmConfiguration)
%shared_ptr(ore::analytics::SimmConfigurationBase)
%shared_ptr(ore::analytics::SimmConfiguration_ISDA_V2_6)
%shared_ptr(ore::analytics::SimmBucketMapper)
%shared_ptr(ore::analytics::SimmBucketMapperBase)
%shared_ptr(ore::analytics::SimmCalculator)

%nodefaultctor ore::analytics::SimmConfiguration;
%nodefaultctor ore::analytics::SimmBucketMapper;
%nodefaultctor ore::analytics::SimmConfigurationBase;

%template(RegulationSet) std::set<ore::analytics::CrifRecord::Regulation>;

namespace ore {
namespace analytics {
class CrifRecord {
  public:
    enum class RecordType { SIMM, FRTB, SACCR, Generic };
    enum class CapitalModel : unsigned char { Empty, SACCR, SACVA, FRTB };
    enum class RiskType {
        Empty,
        Commodity,
        CommodityVol,
        CreditNonQ,
        CreditQ,
        CreditVol,
        CreditVolNonQ,
        Equity,
        EquityVol,
        FX,
        FXVol,
        Inflation,
        IRCurve,
        IRVol,
        InflationVol,
        BaseCorr,
        XCcyBasis,
        ProductClassMultiplier,
        AddOnNotionalFactor,
        Notional,
        AddOnFixedAmount,
        PV,
        GIRR_DELTA,
        GIRR_VEGA,
        GIRR_CURV,
        CSR_NS_DELTA,
        CSR_NS_VEGA,
        CSR_NS_CURV,
        CSR_SNC_DELTA,
        CSR_SNC_VEGA,
        CSR_SNC_CURV,
        CSR_SC_DELTA,
        CSR_SC_VEGA,
        CSR_SC_CURV,
        EQ_DELTA,
        EQ_VEGA,
        EQ_CURV,
        COMM_DELTA,
        COMM_VEGA,
        COMM_CURV,
        FX_DELTA,
        FX_VEGA,
        FX_CURV,
        DRC_NS,
        DRC_SNC,
        DRC_SC,
        RRAO_1_PERCENT,
        RRAO_01_PERCENT,
        CO,
        COLL,
        CR_IX,
        CR_SN,
        EQ_IX,
        EQ_SN,
        IR,
        All
    };
    enum class ProductClass {
        RatesFX,
        Rates,
        FX,
        Credit,
        Equity,
        Commodity,
        Empty,
        Other,
        AddOnNotionalFactor,
        AddOnFixedAmount,
        All
    };
    enum class IMModel { Schedule, SIMM, SIMM_R, SIMM_P, Empty };
    enum class Regulation {
        APRA,
        CFTC,
        ESA,
        FINMA,
        KFSC,
        HKMA,
        JFSA,
        MAS,
        OSFI,
        RBI,
        SEC,
        SEC_unseg,
        USPR,
        NONREG,
        BACEN,
        SANT,
        SFC,
        UK,
        AMFQ,
        BANX,
        OJK,
        Included,
        Unspecified,
        Excluded,
        Invalid
    };
    enum class SaccrRegulation : unsigned char { Basel, CRR2, Unspecified, Invalid };
    enum class CurvatureScenario { Empty, Up, Down };

    CrifRecord();
    RecordType type() const;
    bool hasAmountCcy() const;
    bool hasAmount() const;
    bool hasAmountUsd() const;
    bool hasResultCcy() const;
    bool hasAmountResultCcy() const;
    bool requiresAmountUsd() const;
    bool isSimmParameter() const;
    bool isEmpty() const;
    bool isFrtbCurvatureRisk() const;
    CurvatureScenario frtbCurveatureScenario() const;
};

class Crif {
  public:
    enum class CrifType { FRTB, SIMM, SACCR, Empty };

    Crif();
    CrifType type() const;
    void addRecord(const CrifRecord& record, bool aggregateDifferentAmountCurrencies = false,
                   bool sortFxVolQualifer = true);
    void clear();
    bool empty() const;
    size_t size() const;
    const bool hasCrifRecords() const;
    const bool hasSimmParameters() const;
};

class SimmConfiguration {
  public:
    enum class SimmSide { Call, Post };
    enum class RiskClass { InterestRate, CreditQualifying, CreditNonQualifying, Equity, Commodity, FX, All };
    enum class MarginType { Delta, Vega, Curvature, BaseCorr, AdditionalIM, All };
};

class SimmConfigurationBase : public SimmConfiguration {
  public:
    const std::string& name() const;
    const std::string& version() const;
    bool hasBuckets(const CrifRecord::RiskType& rt) const;
};

class SimmBucketMapper {
  public:
    virtual std::string bucket(const CrifRecord::RiskType& riskType, const std::string& qualifier) const = 0;
    virtual bool hasBuckets(const CrifRecord::RiskType& riskType) const = 0;
};

class SimmBucketMapperBase : public SimmBucketMapper {
  public:
    SimmBucketMapperBase();
    std::string bucket(const CrifRecord::RiskType& riskType, const std::string& qualifier) const override;
    bool hasBuckets(const CrifRecord::RiskType& riskType) const override;
    bool has(const CrifRecord::RiskType& riskType, const std::string& qualifier,
             QuantLib::ext::optional<bool> fallback = QuantLib::ext::nullopt) const;
    void addMapping(const CrifRecord::RiskType& riskType, const std::string& qualifier,
                    const std::string& bucket, const std::string& validFrom = "", const std::string& validTo = "",
                    bool fallback = false);
};

class SimmConfiguration_ISDA_V2_6 : public SimmConfigurationBase {
  public:
    SimmConfiguration_ISDA_V2_6(const QuantLib::ext::shared_ptr<SimmBucketMapper>& simmBucketMapper,
                                const QuantLib::Size& mporDays = 10,
                                const std::string& name = "SIMM ISDA 2.6 (16 August 2023)",
                                const std::string version = "2.6");
  %extend {
    SimmConfiguration_ISDA_V2_6() {
      auto mapper = QuantLib::ext::make_shared<ore::analytics::SimmBucketMapperBase>();
      return new ore::analytics::SimmConfiguration_ISDA_V2_6(mapper);
    }
    SimmConfiguration_ISDA_V2_6(const QuantLib::ext::shared_ptr<ore::analytics::SimmBucketMapperBase>& simmBucketMapper,
                  const QuantLib::Size& mporDays = 10,
                  const std::string& name = "SIMM ISDA 2.6 (16 August 2023)",
                  const std::string version = "2.6") {
      return new ore::analytics::SimmConfiguration_ISDA_V2_6(
        QuantLib::ext::static_pointer_cast<ore::analytics::SimmBucketMapper>(simmBucketMapper), mporDays, name, version);
    }
  }
};

class SimmResults {
  public:
    SimmResults(const std::string& resultCcy = "", const std::string& calcCcy = "");
    void add(const CrifRecord::ProductClass& pc, const SimmConfiguration::RiskClass& rc,
             const SimmConfiguration::MarginType& mt, const std::string& b, QuantLib::Real im,
             const std::string& resultCurrency, const std::string& calculationCurrency, const bool overwrite);
    QuantLib::Real get(const CrifRecord::ProductClass& pc, const SimmConfiguration::RiskClass& rc,
                       const SimmConfiguration::MarginType& mt, const std::string b) const;
    bool has(const CrifRecord::ProductClass& pc, const SimmConfiguration::RiskClass& rc,
             const SimmConfiguration::MarginType& mt, const std::string b) const;
    bool empty() const;
    void clear();
    std::string& resultCurrency();
    const std::string& resultCurrency() const;
    std::string& calculationCurrency();
    const std::string& calculationCurrency() const;
};

%nodefaultctor CrifLoader;
class CrifLoader {
  public:
    virtual ~CrifLoader();
    virtual QuantLib::ext::shared_ptr<Crif> loadCrif();
    const QuantLib::ext::shared_ptr<SimmConfiguration>& simmConfiguration();
};

class CsvFileCrifLoader : public CrifLoader {
  public:
  CsvFileCrifLoader(const std::string& filename,
            const QuantLib::ext::shared_ptr<SimmConfiguration>& configuration,
            const std::vector<std::set<std::string>>& additionalHeaders = {}, bool updateMapper = false,
            bool aggregateTrades = true, bool allowUseCounterpartyTrade = true, char eol = '\n',
            char delim = '\t', char quoteChar = '\0', char escapeChar = '\\',
            const std::string& nullString = "#N/A");
};

class CsvBufferCrifLoader : public CrifLoader {
  public:
  CsvBufferCrifLoader(const std::string& buffer,
            const QuantLib::ext::shared_ptr<SimmConfiguration>& configuration,
            const std::vector<std::set<std::string>>& additionalHeaders = {}, bool updateMapper = false,
            bool aggregateTrades = true, bool allowUseCounterpartyTrade = true, char eol = '\n',
            char delim = '\t', char quoteChar = '\0', char escapeChar = '\\',
            const std::string& nullString = "#N/A");
};

%nodefaultctor SimmCalculator;
class SimmCalculator {
  public:
    %extend {
        SimmCalculator() {
      auto mapper = QuantLib::ext::make_shared<ore::analytics::SimmBucketMapperBase>();
      auto config = QuantLib::ext::make_shared<ore::analytics::SimmConfiguration_ISDA_V2_6>(mapper);
            auto crif = QuantLib::ext::make_shared<ore::analytics::Crif>();
      return new ore::analytics::SimmCalculator(crif, config);
        }
        SimmCalculator(const QuantLib::ext::shared_ptr<ore::analytics::Crif>& crif,
             const QuantLib::ext::shared_ptr<ore::analytics::SimmConfiguration>& simmConfiguration) {
      return new ore::analytics::SimmCalculator(crif, simmConfiguration);
        }
        SimmCalculator(const QuantLib::ext::shared_ptr<ore::analytics::Crif>& crif,
             const QuantLib::ext::shared_ptr<ore::analytics::SimmConfiguration_ISDA_V2_6>& simmConfiguration) {
      return new ore::analytics::SimmCalculator(
        crif, QuantLib::ext::static_pointer_cast<ore::analytics::SimmConfiguration>(simmConfiguration));
        }
    }

    const std::string& calculationCurrency(const SimmConfiguration::SimmSide& side) const;
    const std::string& resultCurrency() const;
};

  } // namespace analytics
  } // namespace ore

#endif
