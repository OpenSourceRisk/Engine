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

#ifndef ored_portfolio_legs_i
#define ored_portfolio_legs_i

%{
using ore::data::ScheduleRules;
using ore::data::ScheduleData;
using ore::data::LegAdditionalData;
using ore::data::FixedLegData;
using ore::data::FloatingLegData;
using ore::data::AmortizationData;
using ore::data::Indexing;
using ore::data::LegData;
using ore::data::CMSLegData;
using ore::data::CPILegData;
using ore::data::YoYLegData;
using ore::data::CMSSpreadLegData;
using ore::data::DigitalCMSSpreadLegData;
using ore::data::EquityLegData;
using ore::data::LegDataFactory;
using ore::data::CommodityPayRelativeTo;
using ore::data::CommodityPriceType;
using ore::data::CommodityPricingDateRule;
using ore::data::CommodityFixedLegData;
using ore::data::CommodityFloatingLegData;
using ore::data::XMLSerializable;
using namespace std;
%}

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

%shared_ptr(CMSSpreadLegData)
class CMSSpreadLegData : public LegAdditionalData {
public:
  CMSSpreadLegData();
  CMSSpreadLegData(const string& swapIndex1, const string& swapIndex2, Size fixingDays, bool isInArrears,
           const vector<double>& spreads, const vector<string>& spreadDates = vector<string>(),
           const vector<double>& caps = vector<double>(), const vector<string>& capDates = vector<string>(),
           const vector<double>& floors = vector<double>(),
           const vector<string>& floorDates = vector<string>(),
           const vector<double>& gearings = vector<double>(),
           const vector<string>& gearingDates = vector<string>(), bool nakedOption = false);
  virtual void fromXML(XMLNode* node) override;
  virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(DigitalCMSSpreadLegData)
class DigitalCMSSpreadLegData : public LegAdditionalData {
public:
  DigitalCMSSpreadLegData();
  DigitalCMSSpreadLegData(
    const QuantLib::ext::shared_ptr<CMSSpreadLegData>& underlying, Position::Type callPosition = Position::Long,
    bool isCallATMIncluded = false, const vector<double> callStrikes = vector<double>(),
    const vector<string> callStrikeDates = vector<string>(), const vector<double> callPayoffs = vector<double>(),
    const vector<string> callPayoffDates = vector<string>(), Position::Type putPosition = Position::Long,
    bool isPutATMIncluded = false, const vector<double> putStrikes = vector<double>(),
    const vector<string> putStrikeDates = vector<string>(), const vector<double> putPayoffs = vector<double>(),
    const vector<string> putPayoffDates = vector<string>());
  virtual void fromXML(XMLNode* node) override;
  virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(EquityLegData)
class EquityLegData : public LegAdditionalData {
public:
  EquityLegData();
  EquityLegData(QuantExt::EquityReturnType returnType, Real dividendFactor, EquityUnderlying equityUnderlying,
          Real initialPrice, bool notionalReset, Natural fixingDays = 0,
          const ScheduleData& valuationSchedule = ScheduleData(), string eqCurrency = "", string fxIndex = "",
          Real quantity = Null<Real>(), string initialPriceCurrency = "");
  virtual void fromXML(XMLNode* node) override;
  virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(LegDataFactory)
class LegDataFactory {
public:
  static LegDataFactory& instance();
  ext::shared_ptr<LegAdditionalData> build(const std::string& legType);
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

#endif
