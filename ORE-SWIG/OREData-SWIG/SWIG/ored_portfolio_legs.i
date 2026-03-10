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
%}

%shared_ptr(ScheduleRules)
class ScheduleRules : public XMLSerializable {
public:
  ScheduleRules(const std::string& startDate, const std::string& endDate, const std::string& tenor, const std::string& calendar,
          const std::string& convention, const std::string& termConvention, const std::string& rule,
          const std::string& endOfMonth = "N", const std::string& firstDate = "", const std::string& lastDate = "",
                  const bool removeFirstDate = false, const bool removeLastDate = false,
          const std::string& endOfMonthConvention = "");
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(ScheduleData)
class ScheduleData : public XMLSerializable {
public:
  ScheduleData(const ScheduleRules& rules, const std::string& name = "");
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(LegAdditionalData)
class LegAdditionalData : public XMLSerializable {
};

%shared_ptr(FixedLegData)
class FixedLegData : public LegAdditionalData {
  public:
    FixedLegData(const std::vector<double>& rates, const std::vector<std::string>& rateDates = std::vector<std::string>());
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(FloatingLegData)
class FloatingLegData : public LegAdditionalData {
public:
  FloatingLegData(const std::string& index, QuantLib::Size fixingDays, bool isInArrears, const std::vector<double>& spreads,
          const std::vector<std::string>& spreadDates = std::vector<std::string>(), const std::vector<double>& caps = std::vector<double>(),
          const std::vector<std::string>& capDates = std::vector<std::string>(), const std::vector<double>& floors = std::vector<double>(),
          const std::vector<std::string>& floorDates = std::vector<std::string>(),
          const std::vector<double>& gearings = std::vector<double>(),
          const std::vector<std::string>& gearingDates = std::vector<std::string>(), bool isAveraged = false,
                    bool nakedOption = false, bool hasSubPeriods = false, bool includeSpread = false,
                    QuantLib::Period lookback = 0 * Days, const Size rateCutoff = Null<Size>(),
                    bool localCapFloor = false, const QuantLib::ext::optional<Period>& lastRecentPeriod = QuantLib::ext::nullopt,
                    const std::string& lastRecentPeriodCalendar = std::string(), bool telescopicValueDates = false,
                    const std::map<QuantLib::Date, double>& historicalFixings = {}, const ScheduleData& valuationSchedule = ScheduleData(),
          const std::string& frontStubShortIndex = std::string(), const std::string& frontStubLongIndex = std::string(),
          const std::string& frontStubRoundingType = std::string(), const std::string& frontStubRoundingPrecision = std::string(),
          const std::string& backStubShortIndex = std::string(), const std::string& backStubLongIndex = std::string(),
          const std::string& backStubRoundingType = std::string(), const std::string& backStubRoundingPrecision = std::string(),
                    bool stubUseOriginalCurve = false);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(AmortizationData)
class AmortizationData : public XMLSerializable {
public:
  AmortizationData(std::string type, double value, std::string startDate, std::string endDate, std::string frequency, bool underflow);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};
%template(AmortizationDataVector) std::vector<ext::shared_ptr<AmortizationData>>;

%shared_ptr(LegData)
class LegData : public XMLSerializable {
  public:
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};
%extend LegData {
  LegData(const ext::shared_ptr<LegAdditionalData>& innerLegData, bool isPayer, const std::string& currency,
      const ScheduleData& scheduleData = ScheduleData(), const std::string& dayCounter = "",
            const std::vector<double>& notionals = std::vector<double>(),
      const std::vector<std::string>& notionalDates = std::vector<std::string>(), const std::string& paymentConvention = "F",
            const bool notionalInitialExchange = false, const bool notionalFinalExchange = false,
            const bool notionalAmortizingExchange = false, const bool isNotResetXCCY = true,
      const std::string& foreignCurrency = "", const double foreignAmount = 0, const std::string& resetStartDate = "", const std::string& fxIndex = "",
            const std::vector<ext::shared_ptr<AmortizationData>>& amortizationData = std::vector<ext::shared_ptr<AmortizationData>>(),
      const std::string& paymentLag = "", const std::string& notionalPaymentLag = "",
            const std::string& paymentCalendar = "",
            const std::vector<std::string>& paymentDates = std::vector<std::string>(),
            const std::vector<Indexing>& indexing = {}, const bool indexingFromAssetLeg = false,
      const std::string& lastPeriodDayCounter = "") {
                return new LegData(innerLegData, isPayer, currency, scheduleData,
                    dayCounter, notionals, notionalDates, paymentConvention,
                    notionalInitialExchange, notionalFinalExchange,
                    notionalAmortizingExchange, isNotResetXCCY, foreignCurrency,
                    foreignAmount, resetStartDate, fxIndex, VECTOR_SWIG_TO_ORE(amortizationData),
                    paymentLag, notionalPaymentLag, paymentCalendar, paymentDates,
                    indexing, indexingFromAssetLeg, lastPeriodDayCounter);
    }
}
  %template(LegDataVector) std::vector<ext::shared_ptr<LegData>>;

%shared_ptr(CMSLegData)
class CMSLegData : public LegAdditionalData {
  public:
    CMSLegData(const std::string& swapIndex, Size fixingDays, bool isInArrears, const std::vector<double>& spreads,
               const std::vector<std::string>& spreadDates = std::vector<std::string>(), const std::vector<double>& caps = std::vector<double>(),
               const std::vector<std::string>& capDates = std::vector<std::string>(), const std::vector<double>& floors = std::vector<double>(),
               const std::vector<std::string>& floorDates = std::vector<std::string>(), const std::vector<double>& gearings = std::vector<double>(),
               const std::vector<std::string>& gearingDates = std::vector<std::string>(), bool nakedOption = false);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(CPILegData)
class CPILegData : public LegAdditionalData {
  public:
    CPILegData(std::string index, std::string startDate, double baseCPI, std::string observationLag, std::string interpolation,
               const std::vector<double>& rates, const std::vector<std::string>& rateDates = std::vector<std::string>(),
               bool subtractInflationNominal = true, const std::vector<double>& caps = std::vector<double>(),
               const std::vector<std::string>& capDates = std::vector<std::string>(), const std::vector<double>& floors = std::vector<double>(),
               const std::vector<std::string>& floorDates = std::vector<std::string>(), double finalFlowCap = Null<Real>(),
               double finalFlowFloor = Null<Real>(), bool nakedOption = false,
               bool subtractInflationNominalCoupons = false);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(YoYLegData)
class YoYLegData : public LegAdditionalData {
  public:
    YoYLegData(std::string index, std::string observationLag, Size fixingDays,
               const std::vector<double>& gearings = std::vector<double>(),
               const std::vector<std::string>& gearingDates = std::vector<std::string>(),
               const std::vector<double>& spreads = std::vector<double>(),
               const std::vector<std::string>& spreadDates = std::vector<std::string>(), const std::vector<double>& caps = std::vector<double>(),
               const std::vector<std::string>& capDates = std::vector<std::string>(), const std::vector<double>& floors = std::vector<double>(),
               const std::vector<std::string>& floorDates = std::vector<std::string>(), bool nakedOption = false,
               bool addInflationNotional = false, bool irregularYoY = false);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(CMSSpreadLegData)
class CMSSpreadLegData : public LegAdditionalData {
public:
  CMSSpreadLegData();
  CMSSpreadLegData(const std::string& swapIndex1, const std::string& swapIndex2, Size fixingDays, bool isInArrears,
           const std::vector<double>& spreads, const std::vector<std::string>& spreadDates = std::vector<std::string>(),
           const std::vector<double>& caps = std::vector<double>(), const std::vector<std::string>& capDates = std::vector<std::string>(),
           const std::vector<double>& floors = std::vector<double>(),
           const std::vector<std::string>& floorDates = std::vector<std::string>(),
           const std::vector<double>& gearings = std::vector<double>(),
           const std::vector<std::string>& gearingDates = std::vector<std::string>(), bool nakedOption = false);
  virtual void fromXML(XMLNode* node) override;
  virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(DigitalCMSSpreadLegData)
class DigitalCMSSpreadLegData : public LegAdditionalData {
public:
  DigitalCMSSpreadLegData();
  DigitalCMSSpreadLegData(
    const QuantLib::ext::shared_ptr<CMSSpreadLegData>& underlying, Position::Type callPosition = Position::Long,
    bool isCallATMIncluded = false, const std::vector<double> callStrikes = std::vector<double>(),
    const std::vector<std::string> callStrikeDates = std::vector<std::string>(), const std::vector<double> callPayoffs = std::vector<double>(),
    const std::vector<std::string> callPayoffDates = std::vector<std::string>(), Position::Type putPosition = Position::Long,
    bool isPutATMIncluded = false, const std::vector<double> putStrikes = std::vector<double>(),
    const std::vector<std::string> putStrikeDates = std::vector<std::string>(), const std::vector<double> putPayoffs = std::vector<double>(),
    const std::vector<std::string> putPayoffDates = std::vector<std::string>());
  virtual void fromXML(XMLNode* node) override;
  virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(EquityLegData)
class EquityLegData : public LegAdditionalData {
public:
  EquityLegData();
  EquityLegData(QuantExt::EquityReturnType returnType, Real dividendFactor, EquityUnderlying equityUnderlying,
          Real initialPrice, bool notionalReset, Natural fixingDays = 0,
          const ScheduleData& valuationSchedule = ScheduleData(), std::string eqCurrency = "", std::string fxIndex = "",
          Real quantity = Null<Real>(), std::string initialPriceCurrency = "");
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
