/*
 Copyright (C) 2018, 2020 Quaternion Risk Management Ltd
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

#ifndef ored_portfolio_i
#define ored_portfolio_i

%include cashflows.i
%include ored_xmlutils.i

%{
using ore::data::Portfolio;
using ore::data::Envelope;
using ore::data::MarketContext;
using ore::data::EngineData;
using ore::data::ReferenceDataManager;
using ore::data::LegBuilder;
using ore::data::EngineBuilder;
using ore::data::EngineFactory;
using OREForwardRateAgreement = ore::data::ForwardRateAgreement;
using ore::data::IborFallbackConfig;
using ore::data::Trade;
using ore::data::TradeFactory;
using ore::data::InstrumentWrapper;
using ore::data::Envelope;
using ORESwap = ore::data::Swap;
using ore::data::XMLUtils;
using ore::data::XMLSerializable;
using ore::data::RequiredFixings;
using ore::data::TradeActions;
using ore::data::TradeCashflowReportData;
using QuantLib::CashFlow;
using namespace std;

using ore::data::ScheduleRules;
using ore::data::ScheduleData;
using ore::data::LegAdditionalData;
using ore::data::FixedLegData;
using ore::data::FloatingLegData;
using ore::data::AmortizationData;
using ore::data::Indexing;
using ore::data::LegData;
using ore::data::PremiumData;
using ore::data::OptionData;
using ore::data::OptionExerciseData;
using ore::data::OptionPaymentData;
using ORESwaption = ore::data::Swaption;
using ore::data::VanillaOptionTrade;
using ore::data::FxOption;
using OREFxForward = ore::data::FxForward;
using ore::data::TradeStrike;
using ore::data::Underlying;
using ore::data::EquityUnderlying;
using ore::data::EquityOption;
using OREEquityForward = ore::data::EquityForward;
using ORECapFloor = ore::data::CapFloor;
using ore::data::BondData;
using OREBond = ore::data::Bond;
using ore::data::CreditDefaultSwapData;
using ORECreditDefaultSwap = ore::data::CreditDefaultSwap;
using ore::data::BasketConstituent;
using ore::data::BasketData;
using ore::data::IndexCreditDefaultSwapData;
using ore::data::SyntheticCDO;
using ore::data::CMSLegData;
using ORECapFloor = ore::data::CapFloor;
using ore::data::CPILegData;
using ore::data::YoYLegData;
using ORECommodityForward = ore::data::CommodityForward;
using ore::data::CommodityPayRelativeTo;
using ore::data::CommodityPriceType;
using ore::data::CommodityPricingDateRule;
using ore::data::CommodityFixedLegData;
using ore::data::CommodityFloatingLegData;
using ore::data::CommodityPricingDateRule;
using ORECommoditySwap = ore::data::CommoditySwap;
using ORECommodityOption = ore::data::CommodityOption;
using ore::data::TradeMonetary;
using ore::data::TradeBarrier;
using ore::data::BarrierData;
using ore::data::FxBarrierOption;
using ore::data::FxTouchOption;
%}

%include std_string.i
%inline %{
using namespace std;
%}

%template(TradeVector) std::vector<ext::shared_ptr<Trade>>;
%template(StringStringMap) std::map<std::string, std::string>;
%template(StringTradeMap) std::map<std::string, ext::shared_ptr<Trade>>;

// SWIG represents vectors as
// a) std::vector<ext::shared_ptr<T>>
// Many ORE functions expect inputs as
// b) std::vector<T>
// This function converts from a) to b).
%{
template<typename T>
std::vector<T> VECTOR_SWIG_TO_ORE(const std::vector<ext::shared_ptr<T>>& v) {
    std::vector<T> ret;
    for (const ext::shared_ptr<T>& t : v)
        ret.push_back(*t);
    return ret;
}
%}

enum class MarketContext { irCalibration, fxCalibration, eqCalibration, pricing };

%shared_ptr(EngineData)
class EngineData : public XMLSerializable {
public:
    EngineData();
    bool hasProduct(const std::string& productName);
    const std::string& model(const std::string& productName) const;
    const std::map<std::string, std::string>& modelParameters(const std::string& productName) const;
    const std::string& engine(const std::string& productName) const;
    const std::map<std::string, std::string>& engineParameters(const std:: string& productName) const;
    const std::map<std::string, std::string>& globalParameters() const;
    std::vector<std::string> products() const;
    void setModel(const std::string& productName, const std::string& model) { model_[productName] = model; }
    void setModelParameters(const std::string& productName, const std::map<std::string, std::string>& params) { modelParams_[productName] = params; }
    void setEngine(const std::string& productName, const std::string& engine) { engine_[productName] = engine; }
    void setEngineParameters(const std::string& productName, const std::map<std::string, std::string>& params) { engineParams_[productName] = params; }
    void setGlobalParameter(const std::string& name, const std::string& param) { globalParams_[name] = param; }
    void clear();
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(ScheduleRules)
class ScheduleRules : public XMLSerializable {
public:
    ScheduleRules(const string& startDate, const string& endDate, const string& tenor, const string& calendar,
                  const string& convention, const string& termConvention, const string& rule,
                  const string& endOfMonth = "N", const string& firstDate = "", const string& lastDate = "",
                  const bool removeFirstDate = false, const bool removeLastDate = false,
                  const string& endOfMonthConvention = "");
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(ScheduleData)
class ScheduleData : public XMLSerializable {
public:
    ScheduleData(const ScheduleRules& rules, const string& name = "");
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(LegAdditionalData)
class LegAdditionalData : public XMLSerializable {
};

%shared_ptr(FixedLegData)
class FixedLegData : public LegAdditionalData {
  public:
    FixedLegData(const vector<double>& rates, const vector<string>& rateDates = vector<string>());
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(FloatingLegData)
class FloatingLegData : public LegAdditionalData {
public:
    FloatingLegData(const string& index, QuantLib::Size fixingDays, bool isInArrears, const vector<double>& spreads,
                    const vector<string>& spreadDates = vector<string>(), const vector<double>& caps = vector<double>(),
                    const vector<string>& capDates = vector<string>(), const vector<double>& floors = vector<double>(),
                    const vector<string>& floorDates = vector<string>(),
                    const vector<double>& gearings = vector<double>(),
                    const vector<string>& gearingDates = vector<string>(), bool isAveraged = false,
                    bool nakedOption = false, bool hasSubPeriods = false, bool includeSpread = false,
                    QuantLib::Period lookback = 0 * Days, const Size rateCutoff = Null<Size>(),
                    bool localCapFloor = false, const QuantLib::ext::optional<Period>& lastRecentPeriod = QuantLib::ext::nullopt,
                    const std::string& lastRecentPeriodCalendar = std::string(), bool telescopicValueDates = false,
                    const std::map<QuantLib::Date, double>& historicalFixings = {}, const ScheduleData& valuationSchedule = ScheduleData(),
                    const string& frontStubShortIndex = std::string(), const string& frontStubLongIndex = std::string(),
                    const string& frontStubRoundingType = std::string(), const string& frontStubRoundingPrecision = std::string(),
                    const string& backStubShortIndex = std::string(), const string& backStubLongIndex = std::string(),
                    const string& backStubRoundingType = std::string(), const string& backStubRoundingPrecision = std::string(),
                    bool stubUseOriginalCurve = false);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(AmortizationData)
class AmortizationData : public XMLSerializable {
public:
    AmortizationData(string type, double value, string startDate, string endDate, string frequency, bool underflow);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};
%template(AmortizationDataVector) vector<ext::shared_ptr<AmortizationData>>;

%shared_ptr(LegData)
class LegData : public XMLSerializable {
  public:
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};
// Call VECTOR_SWIG_TO_ORE to convert vectors from SWIG format to ORE format
%extend LegData {
    LegData(const ext::shared_ptr<LegAdditionalData>& innerLegData, bool isPayer, const string& currency,
            const ScheduleData& scheduleData = ScheduleData(), const string& dayCounter = "",
            const std::vector<double>& notionals = std::vector<double>(),
            const std::vector<string>& notionalDates = std::vector<string>(), const string& paymentConvention = "F",
            const bool notionalInitialExchange = false, const bool notionalFinalExchange = false,
            const bool notionalAmortizingExchange = false, const bool isNotResetXCCY = true,
            const string& foreignCurrency = "", const double foreignAmount = 0, const string& resetStartDate = "", const string& fxIndex = "",
            const std::vector<ext::shared_ptr<AmortizationData>>& amortizationData = std::vector<ext::shared_ptr<AmortizationData>>(),
            const string& paymentLag = "", const string& notionalPaymentLag = "",
            const std::string& paymentCalendar = "",
            const std::vector<std::string>& paymentDates = std::vector<std::string>(),
            const std::vector<Indexing>& indexing = {}, const bool indexingFromAssetLeg = false,
            const string& lastPeriodDayCounter = "") {
                return new LegData(innerLegData, isPayer, currency, scheduleData,
                    dayCounter, notionals, notionalDates, paymentConvention,
                    notionalInitialExchange, notionalFinalExchange,
                    notionalAmortizingExchange, isNotResetXCCY, foreignCurrency,
                    foreignAmount, resetStartDate, fxIndex, VECTOR_SWIG_TO_ORE(amortizationData),
                    paymentLag, notionalPaymentLag, paymentCalendar, paymentDates,
                    indexing, indexingFromAssetLeg, lastPeriodDayCounter);
    }
}
%template(LegDataVector) vector<ext::shared_ptr<LegData>>;

%shared_ptr(PremiumData)
class PremiumData : public XMLSerializable {
public:
    PremiumData();
    PremiumData(double amount, const string& ccy, const QuantLib::Date& payDate);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(OptionData)
class OptionData : public XMLSerializable {
public:
    OptionData(string longShort, string callPut, string style, bool payoffAtExpiry, vector<string> exerciseDates,
               string settlement = "Cash", string settlementMethod = std::string(), const PremiumData& premiumData = {},
               vector<double> exerciseFees = vector<Real>(), vector<double> exercisePrices = vector<Real>(),
               string noticePeriod = std::string(), string noticeCalendar = std::string(), string noticeConvention = std::string(),
               const vector<string>& exerciseFeeDates = vector<string>(),
               const vector<string>& exerciseFeeTypes = vector<string>(), string exerciseFeeSettlementPeriod = std::string(),
               string exerciseFeeSettlementCalendar = std::string(), string exerciseFeeSettlementConvention = std::string(),
               string payoffType = std::string(), string payoffType2 = std::string(),
               const QuantLib::ext::optional<bool>& automaticExercise = QuantLib::ext::nullopt,
               const QuantLib::ext::optional<OptionExerciseData>& exerciseData = QuantLib::ext::nullopt,
               const QuantLib::ext::optional<OptionPaymentData>& paymentData = QuantLib::ext::nullopt,
               const bool midCouponExercise = false, const std::string& cashSettlementCurrency = std::string(),
               const std::string& cashSettlementFxIndex = std::string(),
               const std::string& cashSettlementFixingDate = std::string());
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(LegBuilder)
class LegBuilder {
  private:
    LegBuilder(const std::string& legType);
};

%shared_ptr(EngineBuilder)
class EngineBuilder {
  public:
    EngineBuilder(const std::string& model, const std::string& engine,
                  const std::set<std::string>& tradeTypes);
};

%shared_ptr(EngineFactory)
class EngineFactory {
  public:
    EngineFactory(const ext::shared_ptr<EngineData>& data,
                  const ext::shared_ptr<Market>& market,
                  const std::map<MarketContext, std::string>& configurations = std::map<MarketContext, std::string>(),
                  const ext::shared_ptr<ReferenceDataManager>& referenceData = nullptr,
                  const QuantLib::ext::shared_ptr<IborFallbackConfig>& iborFallbackConfig =
                     QuantLib::ext::make_shared<IborFallbackConfig>(IborFallbackConfig::defaultConfig()),
                  const std::vector<ext::shared_ptr<EngineBuilder>> extraEngineBuilders = {});
};

// TradeFactory just needed as a return type, no construction, no member functions.
%shared_ptr(TradeFactory)
class TradeFactory {
public:
};


%shared_ptr(Envelope)
class Envelope : public XMLSerializable {
public:
    Envelope();
    Envelope(const std::string& counterparty, const std::string& nettingSetId, const std::set<std::string>& portfolioIds = std::set<std::string>());;
    Envelope(const std::string& counterparty, const NettingSetDetails& nettingSetDetails = NettingSetDetails(),
             const std::set<std::string>& portfolioIds = std::set<std::string>());;
    Envelope(const std::string& counterparty, const std::map<std::string, std::string>& additionalFields);;
    Envelope(const std::string& counterparty, const std::string& nettingSetId, const std::map<std::string, std::string>& additionalFields,
             const std::set<std::string>& portfolioIds = std::set<std::string>());
    Envelope(const std::string& counterparty, const NettingSetDetails& nettingSetDetails,
             const std::map<std::string, std::string>& additionalFields, const std::set<std::string>& portfolioIds = std::set<std::string>());

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    const std::string& counterparty() const;
    const std::string& nettingSetId() const;
    const NettingSetDetails nettingSetDetails() const;
    const std::set<std::string>& portfolioIds() const;
    const std::map<std::string, std::string> additionalFields() const;
    const std::map<std::string, QuantLib::ext::any>& fullAdditionalFields() const;
    string additionalField(const std::string& name, const bool mandatory = true,
                           const std::string& defaultValue = std::string()) const;
    QuantLib::ext::any additionalAnyField(const std::string& name, const bool mandatory = true,
                                  const QuantLib::ext::any& defaultValue = boost::none) const;
    void setAdditionalField(const std::string& key, const QuantLib::ext::any& value);
    bool initialized() const;
    bool hasNettingSetDetails() const;
};

// InstrumentWrapper pointer required as a return type only
%shared_ptr(InstrumentWrapper)
class InstrumentWrapper {
  private:
    InstrumentWrapper();
  public:
    Real NPV() const;
    ext::shared_ptr<QuantLib::Instrument> qlInstrument() const;
};

// Trade pointer required as a return type only
%shared_ptr(Trade)
class Trade : public XMLSerializable {                          
  private:
    Trade();
  public:
    const std::string& id();
    void setId(const std::string& id);
    const std::string& tradeType();
    const ext::shared_ptr<InstrumentWrapper>& instrument();
    std::vector<std::vector<ext::shared_ptr<QuantLib::CashFlow>>> legs();
    const Envelope& envelope() const;
    const QuantLib::Date& maturity();
    Real notional();
};

// Portfolio
%shared_ptr(Portfolio)
class Portfolio : public XMLSerializable {
  public:
    Portfolio(bool buildFailedTrades = true);
    std::size_t size() const;
    std::set<std::string> ids() const;
    void add(const ext::shared_ptr<Trade>& trade);
    bool has(const std::string& id);
    ext::shared_ptr<Trade> get(const std::string& id) const;
    const std::map<std::string, ext::shared_ptr<Trade>>& trades() const;
    bool remove(const std::string& tradeID);
    void fromFile(const std::string& fileName);
    void fromXMLString(const std::string& xmlString);    
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    std::string toXMLString();
    void build(const ext::shared_ptr<EngineFactory>& factory,
               const std::string& context = "unspecified",
               const bool emitStructuredError = true);
 };

//ORESwap
%shared_ptr(ORESwap)
class ORESwap : public Trade {
public:
    ORESwap(const std::string swapType = "Swap");
    ORESwap(const Envelope& env, const std::string swapType = "Swap");
    ORESwap(const Envelope& env, const std::vector<LegData>& legData, const std::string swapType = "Swap",
         const std::string settlement = "Physical");
    ORESwap(const Envelope& env, const LegData& leg0, const LegData& leg1, const std::string swapType = "Swap",
         const std::string settlement = "Physical");

    void build(const ext::shared_ptr<EngineFactory>&) override;
    virtual void setIsdaTaxonomyFields();
    QuantLib::Real notional() const override;
    std::string notionalCurrency() const override;

    std::map<AssetClass, std::set<std::string>>
    underlyingIndices(const ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const override;

    const std::string& settlement() const { return settlement_; }
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    void fromXMLString(const std::string& xmlString);
    std::string toXMLString();
    const std::vector<LegData>& legData() const { return legData_; }
    const std::map<std::string,QuantLib::ext::any>& additionalData() const override;
};

%shared_ptr(OREForwardRateAgreement)
class OREForwardRateAgreement : public ORESwap {
public:
    OREForwardRateAgreement();
    OREForwardRateAgreement(Envelope& env, std::string longShort, std::string currency, std::string startDate, std::string endDate,
                         std::string index, double strike, double amount);

    void build(const ext::shared_ptr<EngineFactory>& engineFactory) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    void fromXMLString(const std::string& xmlString);
    std::string toXMLString();
    const std::string& index() const { return index_; }

    const std::map<std::string,QuantLib::ext::any>& additionalData() const override;
};

%shared_ptr(ORESwaption)
class ORESwaption : public Trade {
public:
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
// Call VECTOR_SWIG_TO_ORE to convert vectors from SWIG format to ORE format
%extend ORESwaption {
    ORESwaption(const Envelope& env, const OptionData& optionData, const vector<ext::shared_ptr<LegData>>& legData) {
        return new ORESwaption(env, optionData, VECTOR_SWIG_TO_ORE(legData));
    }
}

%shared_ptr(VanillaOptionTrade)
class VanillaOptionTrade : public Trade { };

%shared_ptr(FxOption)
class FxOption : public VanillaOptionTrade {
public:
    FxOption(const Envelope& env, const OptionData& option, const string& boughtCurrency, double boughtAmount,
             const string& soldCurrency, double soldAmount, const std::string& fxIndex = "", double delta = 0.0);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(OREFxForward)
class OREFxForward : public Trade {
public:
    OREFxForward(const Envelope& env, const string& maturityDate, const string& boughtCurrency, double boughtAmount,
              const string& soldCurrency, double soldAmount, const string& settlement = "Physical",
              const string& fxIndex = "", const string& payDate = "");
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(TradeStrike)
class TradeStrike {
public:
    enum class Type {
        Price,
        Yield
    };
    TradeStrike(Type type, const QuantLib::Real& value);
    TradeStrike(const QuantLib::Real& value, const std::string& currency);
};

%shared_ptr(Underlying)
class Underlying : public ore::data::XMLSerializable {
public:
    virtual void fromXML(ore::data::XMLNode* node) override;
    virtual ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
};

%shared_ptr(EquityUnderlying)
class EquityUnderlying : public Underlying {
public:
    explicit EquityUnderlying(const std::string& equityName);
};

%shared_ptr(EquityOption)
class EquityOption : public VanillaOptionTrade {
public:
    EquityOption(Envelope& env, OptionData option, EquityUnderlying equityUnderlying, string currency,
        QuantLib::Real quantity, TradeStrike tradeStrike);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(OREEquityForward)
class OREEquityForward : public Trade {
public:
    OREEquityForward(Envelope& env, const string& longShort, EquityUnderlying equityUnderlying, const string& currency,
                  QuantLib::Real quantity, const string& maturityDate, QuantLib::Real strike,
                  const string& strikeCurrency = "", const std::string& fxIndex = "", const std::string& payDate = "",
                  std::string payLag = "", const std::string& payCalendar = "" , const std::string& payConvention = "");
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(ORECapFloor)
class ORECapFloor : public Trade {
public:
    ORECapFloor(const Envelope& env, const string& longShort, const LegData& leg, const vector<double>& caps,
             const vector<double>& floors, const PremiumData& premiumData = {});
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(BondData)
class BondData : public XMLSerializable {
public:
    BondData(string issuerId, string creditCurveId, string securityId, string referenceCurveId, string settlementDays,
             string calendar, Real faceAmount, string maturityDate, string currency, string issueDate,
             bool hasCreditRisk = true);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(OREBond)
class OREBond : public Trade {
public:
    OREBond(Envelope env, const BondData& bondData);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// This declaration is missing from file QuantLib-SWIG/SWIG/creditdefaultswap.i:
%extend CreditDefaultSwap {
    enum ProtectionPaymentTime {
        atDefault,
        atPeriodEnd,
        atMaturity
    };
}

%shared_ptr(CreditDefaultSwapData)
class CreditDefaultSwapData : public XMLSerializable {
public:
    using PPT = QuantLib::CreditDefaultSwap::ProtectionPaymentTime;
    CreditDefaultSwapData(const string& issuerId, const string& creditCurveId, const LegData& leg,
                          const bool settlesAccrual = true,
                          const PPT protectionPaymentTime = PPT::atDefault,
                          const Date& protectionStart = Date(), const Date& upfrontDate = Date(),
                          const Real upfrontFee = Null<Real>(),
                          QuantLib::Real recoveryRate = QuantLib::Null<QuantLib::Real>(),
                          const std::string& referenceObligation = "",
                          const Date& tradeDate = Date(),
                          const std::string& cashSettlementDays = "",
                          const bool rebatesAccrual = true);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(ORECreditDefaultSwap)
class ORECreditDefaultSwap : public Trade {
public:
    ORECreditDefaultSwap(const Envelope& env, const CreditDefaultSwapData& swap);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(BasketConstituent)
class BasketConstituent : public XMLSerializable {
public:
    BasketConstituent(const std::string& issuerName, const std::string& creditCurveId, QuantLib::Real notional,
                      const std::string& currency, const std::string& qualifier,
                      QuantLib::Real priorNotional = QuantLib::Null<QuantLib::Real>(),
                      QuantLib::Real recovery = QuantLib::Null<QuantLib::Real>(),
                      const QuantLib::Date& auctionDate = QuantLib::Date(),
                      const QuantLib::Date& auctionSettlementDate = QuantLib::Date(),
                      const QuantLib::Date& defaultDate = QuantLib::Date(),
                      const QuantLib::Date& eventDeterminationDate = QuantLib::Date());
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};
%template(BasketConstituentVector) vector<ext::shared_ptr<BasketConstituent>>;

%shared_ptr(BasketData)
class BasketData : public XMLSerializable {
public:
    BasketData(const std::vector<BasketConstituent>& constituents);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};
// Call VECTOR_SWIG_TO_ORE to convert vectors from SWIG format to ORE format
%extend BasketData {
    BasketData(const std::vector<ext::shared_ptr<BasketConstituent>>& constituents) {
        return new BasketData(VECTOR_SWIG_TO_ORE(constituents));
    }
}

%shared_ptr(IndexCreditDefaultSwapData)
class IndexCreditDefaultSwapData : public ore::data::CreditDefaultSwapData {
public:
    using PPT = QuantLib::CreditDefaultSwap::ProtectionPaymentTime;
    IndexCreditDefaultSwapData(const std::string& creditCurveId,
        const BasketData& basket,
        const LegData& leg,
        const bool settlesAccrual = true,
        const PPT protectionPaymentTime = PPT::atDefault,
        const QuantLib::Date& protectionStart = QuantLib::Date(),
        const QuantLib::Date& upfrontDate = QuantLib::Date(),
        const QuantLib::Real upfrontFee = QuantLib::Null<QuantLib::Real>(),
        const QuantLib::Date& tradeDate = QuantLib::Date(),
        const std::string& cashSettlementDays = "",
        const bool rebatesAccrual = true);
};

%shared_ptr(SyntheticCDO)
class SyntheticCDO : public Trade {
public:
    SyntheticCDO(const Envelope& env, const LegData& leg, const string& qualifier, const BasketData& basketData,
                 double attachmentPoint, double detachmentPoint, const bool settlesAccrual = true,
                 const QuantExt::CreditDefaultSwap::ProtectionPaymentTime protectionPaymentTime =
                     QuantExt::CreditDefaultSwap::ProtectionPaymentTime::atDefault,
                 const string& protectionStart = string(), const string& upfrontDate = string(),
                 const Real upfrontFee = Null<Real>(), const bool rebatesAccrual = true,
                 Real recoveryRate = Null<Real>());
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(CMSLegData)
class CMSLegData : public LegAdditionalData {
  public:
    CMSLegData(const string& swapIndex, Size fixingDays, bool isInArrears, const vector<double>& spreads,
               const vector<string>& spreadDates = vector<string>(), const vector<double>& caps = vector<double>(),
               const vector<string>& capDates = vector<string>(), const vector<double>& floors = vector<double>(),
               const vector<string>& floorDates = vector<string>(), const vector<double>& gearings = vector<double>(),
               const vector<string>& gearingDates = vector<string>(), bool nakedOption = false);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(ORECapFloor)
class ORECapFloor : public Trade {
public:
    ORECapFloor(const Envelope& env, const string& longShort, const LegData& leg, const vector<double>& caps,
             const vector<double>& floors, const PremiumData& premiumData = {});
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(CPILegData)
class CPILegData : public LegAdditionalData {
  public:
    CPILegData(string index, string startDate, double baseCPI, string observationLag, string interpolation,
               const vector<double>& rates, const vector<string>& rateDates = std::vector<string>(),
               bool subtractInflationNominal = true, const vector<double>& caps = vector<double>(),
               const vector<string>& capDates = vector<string>(), const vector<double>& floors = vector<double>(),
               const vector<string>& floorDates = vector<string>(), double finalFlowCap = Null<Real>(),
               double finalFlowFloor = Null<Real>(), bool nakedOption = false,
               bool subtractInflationNominalCoupons = false);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(YoYLegData)
class YoYLegData : public LegAdditionalData {
  public:
    YoYLegData(string index, string observationLag, Size fixingDays,
               const vector<double>& gearings = std::vector<double>(),
               const vector<string>& gearingDates = std::vector<string>(),
               const vector<double>& spreads = std::vector<double>(),
               const vector<string>& spreadDates = std::vector<string>(), const vector<double>& caps = vector<double>(),
               const vector<string>& capDates = vector<string>(), const vector<double>& floors = vector<double>(),
               const vector<string>& floorDates = vector<string>(), bool nakedOption = false,
               bool addInflationNotional = false, bool irregularYoY = false);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(ORECommodityForward)
class ORECommodityForward : public Trade {
public:
    ORECommodityForward(const Envelope& envelope, const std::string& position, const std::string& commodityName,
                     const std::string& currency, QuantLib::Real quantity, const std::string& maturityDate,
                     QuantLib::Real strike);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

enum class CommodityPayRelativeTo {
    CalculationPeriodEndDate,
    CalculationPeriodStartDate,
    TerminationDate,
    FutureExpiryDate
};

enum class CommodityPriceType { Spot, FutureSettlement };

%shared_ptr(CommodityFixedLegData)
class CommodityFixedLegData : public LegAdditionalData {
  public:
    CommodityFixedLegData(const std::vector<QuantLib::Real>& quantities, const std::vector<std::string>& quantityDates,
                          const std::vector<QuantLib::Real>& prices, const std::vector<std::string>& priceDates,
                          CommodityPayRelativeTo commodityPayRelativeTo, const std::string& tag = std::string());
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(CommodityFloatingLegData)
class CommodityFloatingLegData : public LegAdditionalData {
  public:
    CommodityFloatingLegData(
        const std::string& name, CommodityPriceType priceType, const std::vector<QuantLib::Real>& quantities,
        const std::vector<std::string>& quantityDates,
        QuantExt::CommodityQuantityFrequency commodityQuantityFrequency =
            QuantExt::CommodityQuantityFrequency::PerCalculationPeriod,
        CommodityPayRelativeTo commodityPayRelativeTo = CommodityPayRelativeTo::CalculationPeriodEndDate,
        const std::vector<QuantLib::Real>& spreads = {}, const std::vector<std::string>& spreadDates = {},
        const std::vector<QuantLib::Real>& gearings = {}, const std::vector<std::string>& gearingDates = {},
        CommodityPricingDateRule pricingDateRule = CommodityPricingDateRule::FutureExpiryDate,
        const std::string& pricingCalendar = std::string(), QuantLib::Natural pricingLag = 0,
        const std::vector<std::string>& pricingDates = {}, bool isAveraged = false, bool isInArrears = true,
        QuantLib::Integer futureMonthOffset = 0, QuantLib::Natural deliveryRollDays = 0, bool includePeriodEnd = true,
        bool excludePeriodStart = true, QuantLib::Natural hoursPerDay = QuantLib::Null<QuantLib::Natural>(),
        bool useBusinessDays = true, const std::string& tag = std::string(),
        QuantLib::Natural dailyExpiryOffset = QuantLib::Null<QuantLib::Natural>(), bool unrealisedQuantity = false,
        QuantLib::Natural lastNDays = QuantLib::Null<QuantLib::Natural>(), std::string fxIndex = std::string(),
        QuantLib::Natural avgPricePrecision = QuantLib::Null<QuantLib::Natural>());
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(ORECommoditySwap)
class ORECommoditySwap : public Trade {
public:
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
// Call VECTOR_SWIG_TO_ORE to convert vectors from SWIG format to ORE format
%extend ORECommoditySwap {
    ORECommoditySwap(const Envelope& env, const vector<ext::shared_ptr<LegData>>& legs) {
        return new ORECommoditySwap(env, VECTOR_SWIG_TO_ORE(legs));
    }
}

%shared_ptr(ORECommodityOption)
class ORECommodityOption : public VanillaOptionTrade {
public:
    ORECommodityOption(const Envelope& env, const OptionData& optionData, const std::string& commodityName,
                    const std::string& currency, QuantLib::Real quantity, TradeStrike strike,
                    const QuantLib::ext::optional<bool>& isFuturePrice = QuantLib::ext::nullopt,
                    const QuantLib::Date& futureExpiryDate = QuantLib::Date());
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(TradeBarrier)
class TradeBarrier : public TradeMonetary {
public:
    TradeBarrier(QuantLib::Real value, std::string currency);
};
%template(TradeBarrierVector) vector<ext::shared_ptr<TradeBarrier>>;

%shared_ptr(BarrierData)
class BarrierData : public ore::data::XMLSerializable {
public:
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};
// Call VECTOR_SWIG_TO_ORE to convert vectors from SWIG format to ORE format
%extend BarrierData {
    BarrierData(const std::string& barrierType, const std::vector<double>& levels, const double rebate,
                const std::vector<ext::shared_ptr<TradeBarrier>>& tradeBarriers, const std::string& style = std::string(),
                const std::optional<string>& strictComparison = std::nullopt, const std::optional<bool>& overrideTriggered = std::nullopt) {
        return new BarrierData(barrierType, levels, rebate, VECTOR_SWIG_TO_ORE(tradeBarriers),
            style, strictComparison, overrideTriggered);
    }
}

%shared_ptr(FxBarrierOption)
class FxBarrierOption : public Trade {
public:
    FxBarrierOption(Envelope& env, OptionData option, BarrierData barrier, QuantLib::Date startDate,
                    string calendar, string boughtCurrency, double boughtAmount,
                    string soldCurrency, double soldAmount, string fxIndex = "");

    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(FxTouchOption)
class FxTouchOption : public Trade {
public:
    FxTouchOption(Envelope& env, OptionData option, BarrierData barrier, const string& foreignCurrency,
                  const string& domesticCurrency, const string& payoffCurrency, double payoffAmount, const string& startDate = "",
                  const string& calendar = "", const string& fxIndex = "", const string& fxIndexDailyLows = "",
                  const string& fxIndexDailyHighs = "");
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

#endif

