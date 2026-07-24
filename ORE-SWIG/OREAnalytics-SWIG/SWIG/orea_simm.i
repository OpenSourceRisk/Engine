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
%include std_pair.i
%include types.i

%include ored_portfolio.i

%shared_ptr(ore::analytics::Crif)
%shared_ptr(ore::analytics::SimmConfiguration)
%shared_ptr(ore::analytics::SimmConfigurationBase)
%shared_ptr(ore::analytics::SimmConfiguration_ISDA_V2_6)
%shared_ptr(ore::analytics::SimmConfiguration_ISDA_V2_7_2412)
%shared_ptr(ore::analytics::SimmConfiguration_ISDA_V2_8_2506)
%shared_ptr(ore::analytics::SimmConfiguration_ISDA_V2_8_2512)
%shared_ptr(ore::analytics::SimmBucketMapper)
%shared_ptr(ore::analytics::SimmBucketMapperBase)
%shared_ptr(ore::analytics::SimmCalculator)
%shared_ptr(ore::analytics::SimmConcentration)
%shared_ptr(ore::analytics::SimmConcentrationBase)
%shared_ptr(ore::analytics::SimmCalibration)
%shared_ptr(ore::analytics::SimmCalibrationData)

%nodefaultctor ore::analytics::SimmConfiguration;
%nodefaultctor ore::analytics::SimmBucketMapper;
%nodefaultctor ore::analytics::SimmConfigurationBase;
%nodefaultctor ore::analytics::SimmConcentration;

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

    // Public data members
    std::string tradeId;
    std::string tradeType;
    ore::data::NettingSetDetails nettingSetDetails;
    ProductClass productClass;
    RiskType riskType;
    std::string qualifier;
    std::string bucket;
    std::string label1;
    std::string label2;
    std::string amountCurrency;
    QuantLib::Real amount;
    QuantLib::Real amountUsd;
    std::string endDate;
    IMModel imModel;
    std::set<Regulation> collectRegulations;
    std::set<Regulation> postRegulations;

    CrifRecord();
    CrifRecord(std::string tradeId, std::string tradeType,
               const ore::data::NettingSetDetails& nettingSetDetails,
               ProductClass productClass, RiskType riskType,
               std::string qualifier, std::string bucket,
               std::string label1, std::string label2,
               std::string amountCurrency, QuantLib::Real amount,
               QuantLib::Real amountUsd,
               IMModel imModel = IMModel::Empty,
               std::set<Regulation> collectRegulations = {},
               std::set<Regulation> postRegulations = {},
               std::string endDate = "");

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

    // Standard readable/writable fields
    std::string tradeId;
    std::string tradeType;
    ore::data::NettingSetDetails nettingSetDetails;
    RiskType riskType;
    std::string qualifier;
    std::string bucket;
    std::string label1;
    std::string label2;
    QuantLib::Real amount;
    std::string amountCurrency;
    QuantLib::Real amountUsd;

    // SA-CCR scalar fields
    QuantLib::Real saccrEndDate;
    SaccrRegulation regulation;

    // saccrLabel1/saccrLabel2 are boost::variant — not directly wrappable.
    // Use the %extend helpers below: saccrLabel1Type(), saccrLabel1AsReal(),
    // saccrLabel1AsString(), saccrLabel1AsSize(), and the saccrLabel2 equivalents.
    %extend {
        // saccrLabel1: boost::variant<Real, string, Size>
        // Returns which() index: 0 = Real, 1 = string, 2 = Size
        int saccrLabel1Type() const { return $self->saccrLabel1.which(); }
        QuantLib::Real saccrLabel1AsReal() const {
            return boost::get<QuantLib::Real>($self->saccrLabel1);
        }
        std::string saccrLabel1AsString() const {
            return boost::get<std::string>($self->saccrLabel1);
        }
        QuantLib::Size saccrLabel1AsSize() const {
            return boost::get<QuantLib::Size>($self->saccrLabel1);
        }

        // saccrLabel2: boost::variant<Real, string>
        // Returns which() index: 0 = Real, 1 = string
        int saccrLabel2Type() const { return $self->saccrLabel2.which(); }
        QuantLib::Real saccrLabel2AsReal() const {
            return boost::get<QuantLib::Real>($self->saccrLabel2);
        }
        std::string saccrLabel2AsString() const {
            return boost::get<std::string>($self->saccrLabel2);
        }
    }
};

} // namespace analytics
} // namespace ore

// Declare RegulationSet after CrifRecord::Regulation is fully defined so SWIG
// can match std::set<Regulation> data members in CrifRecord to this template.
%template(RegulationSet) std::set<ore::analytics::CrifRecord::Regulation>;

namespace ore {
namespace analytics {

class SimmConfiguration {
  public:
    enum class SimmSide { Call, Post };
    enum class RiskClass { InterestRate, CreditQualifying, CreditNonQualifying, Equity, Commodity, FX, All };
    enum class MarginType { Delta, Vega, Curvature, BaseCorr, AdditionalIM, All };
};

class SimmBucketMapper {
  public:
    virtual std::string bucket(const CrifRecord::RiskType& riskType, const std::string& qualifier) const = 0;
    virtual bool hasBuckets(const CrifRecord::RiskType& riskType) const = 0;
};

class Crif {
  public:
    enum class CrifType { FRTB, SIMM, SACCR, Empty };

    Crif();
    Crif(const QuantLib::ext::shared_ptr<SimmConfiguration>& configuration,
         const std::vector<std::set<std::string>>& additionalHeaders = {}, bool updateMapper = false,
         bool aggregateTrades = true, bool allowUseCounterpartyTrade = true, char eol = '\n', char delim = '\t',
         char quoteChar = '\0', char escapeChar = '\\', const std::string& nullString = "#N/A");
    CrifType type() const;
    void addRecord(const CrifRecord& record, bool aggregateDifferentAmountCurrencies = false,
                   bool sortFxVolQualifer = true);
    void clear();
    bool empty() const;
    size_t size() const;
    const bool hasCrifRecords() const;
    const bool hasSimmParameters() const;

    // CSV loading (replaces the former CrifLoader / CsvFileCrifLoader / CsvBufferCrifLoader classes).
    // Call setCsvLoaderConfig() to configure parsing, then fromCSVFile()/fromCSVString() to populate.
    void setCsvLoaderConfig(const ext::shared_ptr<SimmConfiguration>& configuration,
                            const std::vector<std::set<std::string>>& additionalHeaders = {}, bool updateMapper = false,
                            bool aggregateTrades = true, bool allowUseCounterpartyTrade = true, char eol = '\n',
                            char delim = '\t', char quoteChar = '\0', char escapeChar = '\\',
                            const std::string& nullString = "#N/A");
    void fromCSVFile(const std::string& fileName);
    void fromCSVString(const std::string& buffer);
};

class SimmConfiguration_ISDA_V2_6 : public SimmConfigurationBase {
  public:
    SimmConfiguration_ISDA_V2_6(const ext::shared_ptr<SimmBucketMapper>& simmBucketMapper,
                                const QuantLib::Size& mporDays = 10,
                                const std::string& name = "SIMM ISDA 2.6 (16 August 2023)",
                                const std::string version = "2.6");
  %extend {
    SimmConfiguration_ISDA_V2_6() {
      auto mapper = QuantLib::ext::make_shared<ore::analytics::SimmBucketMapperBase>();
      return new ore::analytics::SimmConfiguration_ISDA_V2_6(mapper);
    }
    SimmConfiguration_ISDA_V2_6(const ext::shared_ptr<ore::analytics::SimmBucketMapperBase>& simmBucketMapper,
                  const QuantLib::Size& mporDays = 10,
                  const std::string& name = "SIMM ISDA 2.6 (16 August 2023)",
                  const std::string version = "2.6") {
      return new ore::analytics::SimmConfiguration_ISDA_V2_6(
        QuantLib::ext::static_pointer_cast<ore::analytics::SimmBucketMapper>(simmBucketMapper), mporDays, name, version);
    }
  }
};

class SimmConfiguration_ISDA_V2_7_2412 : public SimmConfigurationBase {
  public:
    SimmConfiguration_ISDA_V2_7_2412(const ext::shared_ptr<SimmBucketMapper>& simmBucketMapper,
                                const QuantLib::Size& mporDays = 10,
                                const std::string& name = "SIMM ISDA 2.7+2412 (12 July 2025)",
                                const std::string version = "2.7+2412");
  %extend {
    SimmConfiguration_ISDA_V2_7_2412() {
      auto mapper = QuantLib::ext::make_shared<ore::analytics::SimmBucketMapperBase>();
      return new ore::analytics::SimmConfiguration_ISDA_V2_7_2412(mapper);
    }
    SimmConfiguration_ISDA_V2_7_2412(const ext::shared_ptr<ore::analytics::SimmBucketMapperBase>& simmBucketMapper,
                  const QuantLib::Size& mporDays = 10,
                  const std::string& name = "SIMM ISDA 2.7+2412 (12 July 2025)",
                  const std::string version = "2.7+2412") {
      return new ore::analytics::SimmConfiguration_ISDA_V2_7_2412(
        QuantLib::ext::static_pointer_cast<ore::analytics::SimmBucketMapper>(simmBucketMapper), mporDays, name, version);
    }
  }
};

class SimmConfiguration_ISDA_V2_8_2506 : public SimmConfigurationBase {
  public:
    SimmConfiguration_ISDA_V2_8_2506(const ext::shared_ptr<SimmBucketMapper>& simmBucketMapper,
                                const QuantLib::Size& mporDays = 10,
                                const std::string& name = "SIMM ISDA 2.8+2506 (06 December 2025)",
                                const std::string version = "2.8+2506");
  %extend {
    SimmConfiguration_ISDA_V2_8_2506() {
      auto mapper = QuantLib::ext::make_shared<ore::analytics::SimmBucketMapperBase>();
      return new ore::analytics::SimmConfiguration_ISDA_V2_8_2506(mapper);
    }
    SimmConfiguration_ISDA_V2_8_2506(const ext::shared_ptr<ore::analytics::SimmBucketMapperBase>& simmBucketMapper,
                  const QuantLib::Size& mporDays = 10,
                  const std::string& name = "SIMM ISDA 2.8+2506 (06 December 2025)",
                  const std::string version = "2.8+2506") {
      return new ore::analytics::SimmConfiguration_ISDA_V2_8_2506(
        QuantLib::ext::static_pointer_cast<ore::analytics::SimmBucketMapper>(simmBucketMapper), mporDays, name, version);
    }
  }
};

class SimmConfiguration_ISDA_V2_8_2512 : public SimmConfigurationBase {
  public:
    SimmConfiguration_ISDA_V2_8_2512(const ext::shared_ptr<SimmBucketMapper>& simmBucketMapper,
                                const QuantLib::Size& mporDays = 10,
                                const std::string& name = "SIMM ISDA 2.8+2512 (11 July 2026)",
                                const std::string version = "2.8+2512");
  %extend {
    SimmConfiguration_ISDA_V2_8_2512() {
      auto mapper = QuantLib::ext::make_shared<ore::analytics::SimmBucketMapperBase>();
      return new ore::analytics::SimmConfiguration_ISDA_V2_8_2512(mapper);
    }
    SimmConfiguration_ISDA_V2_8_2512(const ext::shared_ptr<ore::analytics::SimmBucketMapperBase>& simmBucketMapper,
                  const QuantLib::Size& mporDays = 10,
                  const std::string& name = "SIMM ISDA 2.8+2512 (11 July 2026)",
                  const std::string version = "2.8+2512") {
      return new ore::analytics::SimmConfiguration_ISDA_V2_8_2512(
        QuantLib::ext::static_pointer_cast<ore::analytics::SimmBucketMapper>(simmBucketMapper), mporDays, name, version);
    }
  }
};

%extend Crif {
    std::vector<ore::analytics::CrifRecord> records() const {
        std::vector<ore::analytics::CrifRecord> result;
        result.reserve($self->size());
        for (auto it = $self->cbegin(); it != $self->cend(); ++it)
            result.push_back(it->toCrifRecord());
        return result;
    }

    // Overloads accepting the concrete SIMM configuration so Python callers do
    // not have to up-cast to the abstract SimmConfiguration base.
    void setCsvLoaderConfig(const ext::shared_ptr<ore::analytics::SimmConfiguration_ISDA_V2_6>& configuration,
                            const std::vector<std::set<std::string>>& additionalHeaders = {}, bool updateMapper = false,
                            bool aggregateTrades = true, bool allowUseCounterpartyTrade = true, char eol = '\n',
                            char delim = '\t', char quoteChar = '\0', char escapeChar = '\\',
                            const std::string& nullString = "#N/A") {
        $self->setCsvLoaderConfig(
            QuantLib::ext::static_pointer_cast<ore::analytics::SimmConfiguration>(configuration),
            additionalHeaders, updateMapper, aggregateTrades, allowUseCounterpartyTrade,
            eol, delim, quoteChar, escapeChar, nullString);
    }
    void setCsvLoaderConfig(const ext::shared_ptr<ore::analytics::SimmConfiguration_ISDA_V2_7_2412>& configuration,
                            const std::vector<std::set<std::string>>& additionalHeaders = {}, bool updateMapper = false,
                            bool aggregateTrades = true, bool allowUseCounterpartyTrade = true, char eol = '\n',
                            char delim = '\t', char quoteChar = '\0', char escapeChar = '\\',
                            const std::string& nullString = "#N/A") {
        $self->setCsvLoaderConfig(
            QuantLib::ext::static_pointer_cast<ore::analytics::SimmConfiguration>(configuration),
            additionalHeaders, updateMapper, aggregateTrades, allowUseCounterpartyTrade,
            eol, delim, quoteChar, escapeChar, nullString);
    }
    void setCsvLoaderConfig(const ext::shared_ptr<ore::analytics::SimmConfiguration_ISDA_V2_8_2506>& configuration,
                            const std::vector<std::set<std::string>>& additionalHeaders = {}, bool updateMapper = false,
                            bool aggregateTrades = true, bool allowUseCounterpartyTrade = true, char eol = '\n',
                            char delim = '\t', char quoteChar = '\0', char escapeChar = '\\',
                            const std::string& nullString = "#N/A") {
        $self->setCsvLoaderConfig(
            QuantLib::ext::static_pointer_cast<ore::analytics::SimmConfiguration>(configuration),
            additionalHeaders, updateMapper, aggregateTrades, allowUseCounterpartyTrade,
            eol, delim, quoteChar, escapeChar, nullString);
    }
    void setCsvLoaderConfig(const ext::shared_ptr<ore::analytics::SimmConfiguration_ISDA_V2_8_2512>& configuration,
                            const std::vector<std::set<std::string>>& additionalHeaders = {}, bool updateMapper = false,
                            bool aggregateTrades = true, bool allowUseCounterpartyTrade = true, char eol = '\n',
                            char delim = '\t', char quoteChar = '\0', char escapeChar = '\\',
                            const std::string& nullString = "#N/A") {
        $self->setCsvLoaderConfig(
            QuantLib::ext::static_pointer_cast<ore::analytics::SimmConfiguration>(configuration),
            additionalHeaders, updateMapper, aggregateTrades, allowUseCounterpartyTrade,
            eol, delim, quoteChar, escapeChar, nullString);
    }

    %pythoncode %{
        def __iter__(self):
            return iter(self.records())
        def __len__(self):
            return int(self.size())
    %}
}

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

// ============================================================
// SimmConcentration — abstract base
// ============================================================
class SimmConcentration {
  public:
    virtual ~SimmConcentration();
    virtual QuantLib::Real threshold(const CrifRecord::RiskType& riskType,
                                     const std::string& qualifier) const = 0;
};

// ============================================================
// SimmConcentrationBase — concrete base returning QL_MAX_REAL
// ============================================================
class SimmConcentrationBase : public SimmConcentration {
  public:
    SimmConcentrationBase();
    QuantLib::Real threshold(const CrifRecord::RiskType& riskType,
                             const std::string& qualifier) const override;
};

// ============================================================
// SimmCalibration — XML-driven calibration data
// ============================================================
class SimmCalibration : public ore::data::XMLSerializable {
  public:
    SimmCalibration();
    const std::string& version() const;
    const std::vector<std::string>& versionNames() const;
    const std::string& id() const;
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
};

// ============================================================
// SimmCalibrationData — container for multiple calibrations
// ============================================================
class SimmCalibrationData : public ore::data::XMLSerializable {
  public:
    SimmCalibrationData();
    void add(const ext::shared_ptr<SimmCalibration>& cal);
    bool hasId(const std::string& id) const;
    ext::shared_ptr<SimmCalibration> getById(const std::string& id) const;
    ext::shared_ptr<SimmCalibration> getBySimmVersion(const std::string& id) const;
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
};

class SimmConfigurationBase : public SimmConfiguration {
  public:
    const std::string& name() const;
    const std::string& version() const;
    bool hasBuckets(const CrifRecord::RiskType& rt) const;

    // Bucket enumeration
    ext::shared_ptr<SimmBucketMapper> bucketMapper() const;
    std::string bucket(const CrifRecord::RiskType& rt, const std::string& qualifier) const;
    std::vector<std::string> buckets(const CrifRecord::RiskType& rt) const;
    std::vector<std::string> labels1(const CrifRecord::RiskType& rt) const;
    std::vector<std::string> labels2(const CrifRecord::RiskType& rt) const;

    // Risk weights
    QuantLib::Real weight(const CrifRecord::RiskType& rt,
                          QuantLib::ext::optional<std::string> qualifier = QuantLib::ext::nullopt,
                          QuantLib::ext::optional<std::string> label_1 = QuantLib::ext::nullopt,
                          const std::string& calculationCurrency = "") const;
    QuantLib::Real curvatureWeight(const CrifRecord::RiskType& rt, const std::string& label_1) const;
    QuantLib::Real historicalVolatilityRatio(const CrifRecord::RiskType& rt) const;
    QuantLib::Real sigma(const CrifRecord::RiskType& rt,
                         QuantLib::ext::optional<std::string> qualifier = QuantLib::ext::nullopt,
                         QuantLib::ext::optional<std::string> label_1 = QuantLib::ext::nullopt,
                         const std::string& calculationCurrency = "") const;
    QuantLib::Real curvatureMarginScaling() const;

    // Thresholds
    QuantLib::Real concentrationThreshold(const CrifRecord::RiskType& rt, const std::string& qualifier) const;

    // Validity and correlation
    bool isValidRiskType(const CrifRecord::RiskType& rt) const;
    QuantLib::Real correlationRiskClasses(const SimmConfiguration::RiskClass& rc_1,
                                          const SimmConfiguration::RiskClass& rc_2) const;
    QuantLib::Real correlation(const CrifRecord::RiskType& firstRt, const std::string& firstQualifier,
                               const std::string& firstBucket, const std::string& firstLabel_1,
                               const std::string& firstLabel_2, const CrifRecord::RiskType& secondRt,
                               const std::string& secondQualifier, const std::string& secondBucket,
                               const std::string& secondLabel_1, const std::string& secondLabel_2,
                               const std::string& calculationCurrency) const;

    // MPOR
    QuantLib::Size mporDays() const;
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
        SimmCalculator(const ext::shared_ptr<ore::analytics::Crif>& crif,
             const ext::shared_ptr<ore::analytics::SimmConfiguration>& simmConfiguration) {
      return new ore::analytics::SimmCalculator(crif, simmConfiguration);
        }
        SimmCalculator(const ext::shared_ptr<ore::analytics::Crif>& crif,
             const ext::shared_ptr<ore::analytics::SimmConfiguration_ISDA_V2_6>& simmConfiguration) {
      return new ore::analytics::SimmCalculator(
        crif, QuantLib::ext::static_pointer_cast<ore::analytics::SimmConfiguration>(simmConfiguration));
        }
        SimmCalculator(const ext::shared_ptr<ore::analytics::Crif>& crif,
             const ext::shared_ptr<ore::analytics::SimmConfiguration_ISDA_V2_7_2412>& simmConfiguration) {
      return new ore::analytics::SimmCalculator(
        crif, QuantLib::ext::static_pointer_cast<ore::analytics::SimmConfiguration>(simmConfiguration));
        }
        SimmCalculator(const ext::shared_ptr<ore::analytics::Crif>& crif,
             const ext::shared_ptr<ore::analytics::SimmConfiguration_ISDA_V2_8_2506>& simmConfiguration) {
      return new ore::analytics::SimmCalculator(
        crif, QuantLib::ext::static_pointer_cast<ore::analytics::SimmConfiguration>(simmConfiguration));
        }
        SimmCalculator(const ext::shared_ptr<ore::analytics::Crif>& crif,
             const ext::shared_ptr<ore::analytics::SimmConfiguration_ISDA_V2_8_2512>& simmConfiguration) {
      return new ore::analytics::SimmCalculator(
        crif, QuantLib::ext::static_pointer_cast<ore::analytics::SimmConfiguration>(simmConfiguration));
        }
        // Wrapper for simmParameters() to dereference the const reference
        ext::shared_ptr<ore::analytics::Crif> simmParameters() const {
          return $self->simmParameters();
        }
        // Convenience overload: query results for a single Regulation value.
        // winningRegulations() returns CrifRecord::Regulation; passing that enum
        // value directly to the set-based simmResults() is not possible from
        // Python because SWIG's std::set template does not accept plain ints.
        // This overload wraps the single value in a singleton set internally.
        const ore::analytics::SimmResults& simmResults(
            const ore::analytics::SimmConfiguration::SimmSide& side,
            const ore::data::NettingSetDetails& nettingSetDetails,
            const ore::analytics::CrifRecord::Regulation& regulation) const {
            return $self->simmResults(side, nettingSetDetails,
                                      std::set<ore::analytics::CrifRecord::Regulation>{regulation});
        }
    }

   const std::string& calculationCurrency(const SimmConfiguration::SimmSide& side) const;
   const std::string& resultCurrency() const;

   const CrifRecord::Regulation& winningRegulations(const SimmConfiguration::SimmSide& side,
       const ore::data::NettingSetDetails& nettingSetDetails) const;
   const SimmResults& simmResults(const SimmConfiguration::SimmSide& side,
       const ore::data::NettingSetDetails& nettingSetDetails,
       const std::set<CrifRecord::Regulation>& regulation) const;
   const std::pair<CrifRecord::Regulation, SimmResults>& finalSimmResults(
       const SimmConfiguration::SimmSide& side,
       const ore::data::NettingSetDetails& nettingSetDetails) const;
};

  } // namespace analytics
  } // namespace ore

%template(RegulationSimmResultsPair) std::pair<ore::analytics::CrifRecord::Regulation, ore::analytics::SimmResults>;
%template(CrifRecordVector) std::vector<ore::analytics::CrifRecord>;

#endif
