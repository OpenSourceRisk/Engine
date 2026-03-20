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
using ore::data::ScheduleDates;
using ore::data::ScheduleDerived;
using ore::data::ScheduleData;
using ore::data::LegAdditionalData;
using ore::data::CashflowData;
using ore::data::FixedLegData;
using ore::data::ZeroCouponFixedLegData;
using ore::data::FloatingLegData;
using ore::data::AmortizationData;
using ore::data::Indexing;
using ore::data::LegData;
using ore::data::CMSLegData;
using ore::data::DigitalCMSLegData;
using ore::data::CPILegData;
using ore::data::YoYLegData;
using ore::data::CMSSpreadLegData;
using ore::data::DigitalCMSSpreadLegData;
using ore::data::EquityLegData;
using ore::data::CMBLegData;
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
  ScheduleRules();
  ScheduleRules(const std::string& startDate, const std::string& endDate, const std::string& tenor, const std::string& calendar,
          const std::string& convention, const std::string& termConvention, const std::string& rule,
          const std::string& endOfMonth = "N", const std::string& firstDate = "", const std::string& lastDate = "",
                  const bool removeFirstDate = false, const bool removeLastDate = false,
          const std::string& endOfMonthConvention = "");
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(ScheduleDates)
class ScheduleDates : public XMLSerializable {
public:
  ScheduleDates();
  ScheduleDates(const std::string& calendar, const std::string& convention, const std::string& tenor,
                const std::vector<std::string>& dates, const std::string& endOfMonth = "",
                const std::string& endOfMonthConvention = "", bool includeDuplicateDates = false);
  virtual void fromXML(XMLNode* node) override;
  virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(ScheduleDerived)
class ScheduleDerived : public XMLSerializable {
public:
  ScheduleDerived();
  ScheduleDerived(const std::string& baseSchedule, const std::string& calendar,
                  const std::string& convention, const std::string& shift,
                  const bool removeFirstDate = false, const bool removeLastDate = false);
  virtual void fromXML(XMLNode* node) override;
  virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(ScheduleData)
class ScheduleData : public XMLSerializable {
public:
  ScheduleData();
  ScheduleData(const ScheduleDates& dates, const std::string& name = "");
  ScheduleData(const ScheduleRules& rules, const std::string& name = "");
  ScheduleData(const ScheduleDerived& derived, const std::string& name = "");
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(LegAdditionalData)
class LegAdditionalData : public XMLSerializable {
};

// ore/OREData/ored/portfolio/legdata.hpp - CashflowData

%shared_ptr(CashflowData)
class CashflowData : public LegAdditionalData {
public:
    CashflowData();
    CashflowData(const std::vector<double>& amounts, const std::vector<std::string>& dates);
    const std::vector<double>& amounts() const;
    const std::vector<std::string>& dates() const;
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(FixedLegData)
class FixedLegData : public LegAdditionalData {
  public:
    FixedLegData();
    FixedLegData(const std::vector<double>& rates, const std::vector<std::string>& rateDates = std::vector<std::string>());
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(ZeroCouponFixedLegData)
class ZeroCouponFixedLegData : public LegAdditionalData {
public:
    ZeroCouponFixedLegData();
    ZeroCouponFixedLegData(const std::vector<double>& rates,
                           const std::vector<std::string>& rateDates = std::vector<std::string>(),
                           const std::string& compounding = "Compounded",
                           const bool subtractNotional = true);
    const std::vector<double>& rates() const;
    const std::vector<std::string>& rateDates() const;
    const std::string& compounding() const;
    const bool& subtractNotional() const;
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(FloatingLegData)
class FloatingLegData : public LegAdditionalData {
public:
  FloatingLegData();
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
  AmortizationData();
  AmortizationData(std::string type, double value, std::string startDate, std::string endDate, std::string frequency, bool underflow);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};
%template(AmortizationDataVector) std::vector<ext::shared_ptr<AmortizationData>>;

%shared_ptr(Indexing)
class Indexing : public XMLSerializable {
public:
    Indexing();
    explicit Indexing(const std::string& index,
                      const std::string& indexFixingCalendar = "",
                      const bool indexIsDirty = false,
                      const bool indexIsRelative = true,
                      const bool indexIsConditionalOnSurvival = true,
                      const QuantLib::Real quantity = 1.0,
                      const QuantLib::Real initialFixing = QuantLib::Null<QuantLib::Real>(),
                      const QuantLib::Real initialNotionalFixing = QuantLib::Null<QuantLib::Real>(),
                      const ScheduleData& valuationSchedule = ScheduleData(),
                      const QuantLib::Size fixingDays = 0,
                      const std::string& fixingCalendar = "",
                      const std::string& fixingConvention = "",
                      const bool inArrearsFixing = false);
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
%template(IndexingVector) std::vector<Indexing>;

%shared_ptr(LegData)
class LegData : public XMLSerializable {
  public:
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};
%extend LegData {
  LegData() { return new LegData(); }
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
SWIG_SHARED_PTR_VECTOR_TYPEMAP(LegData, LegDataVector)

%shared_ptr(CMSLegData)
class CMSLegData : public LegAdditionalData {
  public:
    CMSLegData();
    CMSLegData(const std::string& swapIndex, Size fixingDays, bool isInArrears, const std::vector<double>& spreads,
               const std::vector<std::string>& spreadDates = std::vector<std::string>(), const std::vector<double>& caps = std::vector<double>(),
               const std::vector<std::string>& capDates = std::vector<std::string>(), const std::vector<double>& floors = std::vector<double>(),
               const std::vector<std::string>& floorDates = std::vector<std::string>(), const std::vector<double>& gearings = std::vector<double>(),
               const std::vector<std::string>& gearingDates = std::vector<std::string>(), bool nakedOption = false);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(DigitalCMSLegData)
class DigitalCMSLegData : public LegAdditionalData {
public:
    DigitalCMSLegData();
    DigitalCMSLegData(
        const QuantLib::ext::shared_ptr<CMSLegData>& underlying,
        Position::Type callPosition = Position::Long,
        bool isCallATMIncluded = false,
        const std::vector<double> callStrikes = std::vector<double>(),
        const std::vector<std::string> callStrikeDates = std::vector<std::string>(),
        const std::vector<double> callPayoffs = std::vector<double>(),
        const std::vector<std::string> callPayoffDates = std::vector<std::string>(),
        Position::Type putPosition = Position::Long,
        bool isPutATMIncluded = false,
        const std::vector<double> putStrikes = std::vector<double>(),
        const std::vector<std::string> putStrikeDates = std::vector<std::string>(),
        const std::vector<double> putPayoffs = std::vector<double>(),
        const std::vector<std::string> putPayoffDates = std::vector<std::string>());
    const QuantLib::ext::shared_ptr<CMSLegData>& underlying() const;
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(CPILegData)
class CPILegData : public LegAdditionalData {
  public:
    CPILegData();
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
    YoYLegData();
    YoYLegData(std::string index, std::string observationLag, Size fixingDays,
               const std::vector<double>& gearings = std::vector<double>(),
               const std::vector<std::string>& gearingDates = std::vector<std::string>(),
               const std::vector<double>& spreads = std::vector<double>(),
               const std::vector<std::string>& spreadDates = std::vector<std::string>(),
               const std::vector<double>& caps = std::vector<double>(),
               const std::vector<std::string>& capDates = std::vector<std::string>(),
               const std::vector<double>& floors = std::vector<double>(),
               const std::vector<std::string>& floorDates = std::vector<std::string>(),
               bool nakedOption = false,
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

// ore/OREData/ored/portfolio/legbuilders.hpp

%{
using ore::data::FixedLegBuilder;
using ore::data::ZeroCouponFixedLegBuilder;
using ore::data::FloatingLegBuilder;
using ore::data::CashflowLegBuilder;
using ore::data::CPILegBuilder;
using ore::data::YYLegBuilder;
using ore::data::CMSLegBuilder;
using ore::data::CMBLegBuilder;
using ore::data::DigitalCMSLegBuilder;
using ore::data::CMSSpreadLegBuilder;
using ore::data::DigitalCMSSpreadLegBuilder;
using ore::data::EquityLegBuilder;
using ore::data::CommodityFixedLegBuilder;
using ore::data::CommodityFloatingLegBuilder;
using ore::data::DurationAdjustedCmsLegBuilder;
using ore::data::DurationAdjustedCmsLegData;
using ore::data::FormulaBasedLegBuilder;
using ore::data::FormulaBasedLegData;
using ore::data::EquityMarginLegBuilder;
using ore::data::EquityMarginLegData;
%}

%template(PositionTypeVector) std::vector<QuantLib::Position::Type>;

%shared_ptr(CMBLegData)
class CMBLegData : public LegAdditionalData {
public:
    CMBLegData();
    CMBLegData(const std::string& genericBond, bool hasCreditRisk, QuantLib::Size fixingDays,
               bool isInArrears, const std::vector<double>& spreads,
               const std::vector<std::string>& spreadDates = std::vector<std::string>(),
               const std::vector<double>& caps = std::vector<double>(),
               const std::vector<std::string>& capDates = std::vector<std::string>(),
               const std::vector<double>& floors = std::vector<double>(),
               const std::vector<std::string>& floorDates = std::vector<std::string>(),
               const std::vector<double>& gearings = std::vector<double>(),
               const std::vector<std::string>& gearingDates = std::vector<std::string>(),
               bool nakedOption = false);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(FixedLegBuilder)
class FixedLegBuilder : public LegBuilder {
public:
    FixedLegBuilder();
};

%shared_ptr(ZeroCouponFixedLegBuilder)
class ZeroCouponFixedLegBuilder : public LegBuilder {
public:
    ZeroCouponFixedLegBuilder();
};

%shared_ptr(FloatingLegBuilder)
class FloatingLegBuilder : public LegBuilder {
public:
    FloatingLegBuilder();
};

%shared_ptr(CashflowLegBuilder)
class CashflowLegBuilder : public LegBuilder {
public:
    CashflowLegBuilder();
};

%shared_ptr(CPILegBuilder)
class CPILegBuilder : public LegBuilder {
public:
    CPILegBuilder();
};

%shared_ptr(YYLegBuilder)
class YYLegBuilder : public LegBuilder {
public:
    YYLegBuilder();
};

%shared_ptr(CMSLegBuilder)
class CMSLegBuilder : public LegBuilder {
public:
    CMSLegBuilder();
};

%shared_ptr(CMBLegBuilder)
class CMBLegBuilder : public LegBuilder {
public:
    CMBLegBuilder();
};

%shared_ptr(DigitalCMSLegBuilder)
class DigitalCMSLegBuilder : public LegBuilder {
public:
    DigitalCMSLegBuilder();
};

%shared_ptr(CMSSpreadLegBuilder)
class CMSSpreadLegBuilder : public LegBuilder {
public:
    CMSSpreadLegBuilder();
};

%shared_ptr(DigitalCMSSpreadLegBuilder)
class DigitalCMSSpreadLegBuilder : public LegBuilder {
public:
    DigitalCMSSpreadLegBuilder();
};

%shared_ptr(EquityLegBuilder)
class EquityLegBuilder : public LegBuilder {
public:
    EquityLegBuilder();
};

%shared_ptr(CommodityFixedLegBuilder)
class CommodityFixedLegBuilder : public ore::data::LegBuilder {
public:
    CommodityFixedLegBuilder();
};

%shared_ptr(CommodityFloatingLegBuilder)
class CommodityFloatingLegBuilder : public ore::data::LegBuilder {
public:
    CommodityFloatingLegBuilder();
};

// ore/OREData/ored/portfolio/durationadjustedcmslegbuilder.hpp

%shared_ptr(DurationAdjustedCmsLegBuilder)
class DurationAdjustedCmsLegBuilder : public ore::data::LegBuilder {
public:
    DurationAdjustedCmsLegBuilder();
};

// ore/OREData/ored/portfolio/durationadjustedcmslegdata.hpp

%shared_ptr(DurationAdjustedCmsLegData)
class DurationAdjustedCmsLegData : public ore::data::LegAdditionalData {
public:
    DurationAdjustedCmsLegData();
    DurationAdjustedCmsLegData(const std::string& swapIndex, Size duration, Size fixingDays, bool isInArrears,
                               const std::vector<double>& spreads,
                               const std::vector<std::string>& spreadDates = std::vector<std::string>(),
                               const std::vector<double>& caps = std::vector<double>(),
                               const std::vector<std::string>& capDates = std::vector<std::string>(),
                               const std::vector<double>& floors = std::vector<double>(),
                               const std::vector<std::string>& floorDates = std::vector<std::string>(),
                               const std::vector<double>& gearings = std::vector<double>(),
                               const std::vector<std::string>& gearingDates = std::vector<std::string>(),
                               bool nakedOption = false);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/formulabasedlegbuilder.hpp

%shared_ptr(FormulaBasedLegBuilder)
class FormulaBasedLegBuilder : public ore::data::LegBuilder {
public:
    explicit FormulaBasedLegBuilder();
};

// ore/OREData/ored/portfolio/formulabasedlegdata.hpp

%shared_ptr(FormulaBasedLegData)
class FormulaBasedLegData : public LegAdditionalData {
public:
    FormulaBasedLegData();
    FormulaBasedLegData(const string& formulaBasedIndex, int fixingDays, bool isInArrears);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/equityfxlegbuilder.hpp

%shared_ptr(EquityMarginLegBuilder)
class EquityMarginLegBuilder : public ore::data::LegBuilder {
public:
    EquityMarginLegBuilder();
};

// ore/OREData/ored/portfolio/equityfxlegdata.hpp

%shared_ptr(EquityMarginLegData)
class EquityMarginLegData : public ore::data::LegAdditionalData {
public:
    EquityMarginLegData();
    EquityMarginLegData(QuantLib::ext::shared_ptr<ore::data::EquityLegData>& equityLegData, const vector<double>& rates,
        const vector<string>& rateDates = vector<string>(), const double& initialMarginFactor = QuantExt::Null<double>(),
        const double& multiplier = QuantExt::Null<double>());
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

#endif
