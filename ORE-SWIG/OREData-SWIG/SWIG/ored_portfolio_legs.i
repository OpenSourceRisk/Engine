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

%shared_ptr(ore::data::ScheduleRules)
%shared_ptr(ore::data::ScheduleDates)
%shared_ptr(ore::data::ScheduleDerived)
%shared_ptr(ore::data::ScheduleData)
%shared_ptr(ore::data::LegAdditionalData)
%shared_ptr(ore::data::CashflowData)
%shared_ptr(ore::data::FixedLegData)
%shared_ptr(ore::data::ZeroCouponFixedLegData)
%shared_ptr(ore::data::FloatingLegData)
%shared_ptr(ore::data::AmortizationData)
%shared_ptr(ore::data::Indexing)
%shared_ptr(ore::data::LegData)
%shared_ptr(ore::data::CMSLegData)
%shared_ptr(ore::data::DigitalCMSLegData)
%shared_ptr(ore::data::CPILegData)
%shared_ptr(ore::data::YoYLegData)
%shared_ptr(ore::data::CMSSpreadLegData)
%shared_ptr(ore::data::DigitalCMSSpreadLegData)
%shared_ptr(ore::data::EquityLegData)
%shared_ptr(ore::data::LegDataFactory)
%shared_ptr(ore::data::CommodityFixedLegData)
%shared_ptr(ore::data::CommodityFloatingLegData)
%shared_ptr(ore::data::CMBLegData)
%shared_ptr(ore::data::FixedLegBuilder)
%shared_ptr(ore::data::ZeroCouponFixedLegBuilder)
%shared_ptr(ore::data::FloatingLegBuilder)
%shared_ptr(ore::data::CashflowLegBuilder)
%shared_ptr(ore::data::CPILegBuilder)
%shared_ptr(ore::data::YYLegBuilder)
%shared_ptr(ore::data::CMSLegBuilder)
%shared_ptr(ore::data::CMBLegBuilder)
%shared_ptr(ore::data::DigitalCMSLegBuilder)
%shared_ptr(ore::data::CMSSpreadLegBuilder)
%shared_ptr(ore::data::DigitalCMSSpreadLegBuilder)
%shared_ptr(ore::data::EquityLegBuilder)
%shared_ptr(ore::data::CommodityFixedLegBuilder)
%shared_ptr(ore::data::CommodityFloatingLegBuilder)
%shared_ptr(ore::data::DurationAdjustedCmsLegBuilder)
%shared_ptr(ore::data::DurationAdjustedCmsLegData)
%shared_ptr(ore::data::FormulaBasedLegBuilder)
%shared_ptr(ore::data::FormulaBasedLegData)
%shared_ptr(ore::data::EquityMarginLegBuilder)
%shared_ptr(ore::data::EquityMarginLegData)

namespace ore {
namespace data {

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

class ScheduleDates : public XMLSerializable {
public:
  ScheduleDates();
  ScheduleDates(const std::string& calendar, const std::string& convention, const std::string& tenor,
                const std::vector<std::string>& dates, const std::string& endOfMonth = "",
                const std::string& endOfMonthConvention = "", bool includeDuplicateDates = false);
  virtual void fromXML(XMLNode* node) override;
  virtual XMLNode* toXML(XMLDocument& doc) const override;
};

class ScheduleDerived : public XMLSerializable {
public:
  ScheduleDerived();
  ScheduleDerived(const std::string& baseSchedule, const std::string& calendar,
                  const std::string& convention, const std::string& shift,
                  const bool removeFirstDate = false, const bool removeLastDate = false);
  virtual void fromXML(XMLNode* node) override;
  virtual XMLNode* toXML(XMLDocument& doc) const override;
};

class ScheduleData : public XMLSerializable {
public:
  ScheduleData();
  ScheduleData(const ScheduleDates& dates, const std::string& name = "");
  ScheduleData(const ScheduleRules& rules, const std::string& name = "");
  ScheduleData(const ScheduleDerived& derived, const std::string& name = "");
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

class LegAdditionalData : public XMLSerializable {
};

// ore/OREData/ored/portfolio/legdata.hpp - CashflowData

class CashflowData : public LegAdditionalData {
public:
    CashflowData();
    CashflowData(const std::vector<double>& amounts, const std::vector<std::string>& dates);
    const std::vector<double>& amounts() const;
    const std::vector<std::string>& dates() const;
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

class FixedLegData : public LegAdditionalData {
  public:
    FixedLegData();
    FixedLegData(const std::vector<double>& rates, const std::vector<std::string>& rateDates = std::vector<std::string>());
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

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

class AmortizationData : public XMLSerializable {
public:
  AmortizationData();
  AmortizationData(std::string type, double value, std::string startDate, std::string endDate, std::string frequency, bool underflow);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

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
%extend DigitalCMSLegData {
    DigitalCMSLegData(
        const CMSLegData& underlying,
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
        const std::vector<std::string> putPayoffDates = std::vector<std::string>()) {
        return new DigitalCMSLegData(
            QuantLib::ext::make_shared<CMSLegData>(underlying),
            callPosition, isCallATMIncluded, callStrikes, callStrikeDates, callPayoffs, callPayoffDates,
            putPosition, isPutATMIncluded, putStrikes, putStrikeDates, putPayoffs, putPayoffDates);
    }
}

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
%extend DigitalCMSSpreadLegData {
    DigitalCMSSpreadLegData(
        const CMSSpreadLegData& underlying, Position::Type callPosition = Position::Long,
        bool isCallATMIncluded = false, const std::vector<double> callStrikes = std::vector<double>(),
        const std::vector<std::string> callStrikeDates = std::vector<std::string>(), const std::vector<double> callPayoffs = std::vector<double>(),
        const std::vector<std::string> callPayoffDates = std::vector<std::string>(), Position::Type putPosition = Position::Long,
        bool isPutATMIncluded = false, const std::vector<double> putStrikes = std::vector<double>(),
        const std::vector<std::string> putStrikeDates = std::vector<std::string>(), const std::vector<double> putPayoffs = std::vector<double>(),
        const std::vector<std::string> putPayoffDates = std::vector<std::string>()) {
            return new DigitalCMSSpreadLegData(
                QuantLib::ext::make_shared<CMSSpreadLegData>(underlying),
                callPosition, isCallATMIncluded, callStrikes, callStrikeDates, callPayoffs, callPayoffDates,
                putPosition, isPutATMIncluded, putStrikes, putStrikeDates, putPayoffs, putPayoffDates);
    }
}

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

class CommodityFixedLegData : public LegAdditionalData {
  public:
    CommodityFixedLegData(const std::vector<QuantLib::Real>& quantities, const std::vector<std::string>& quantityDates,
                          const std::vector<QuantLib::Real>& prices, const std::vector<std::string>& priceDates,
                          CommodityPayRelativeTo commodityPayRelativeTo, const std::string& tag = std::string());
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

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

    }
    }

%template(AmortizationDataVector) std::vector<ext::shared_ptr<ore::data::AmortizationData>>;
%template(IndexingVector) std::vector<ore::data::Indexing>;
%template(LegDataVector) std::vector<ext::shared_ptr<ore::data::LegData>>;
SWIG_SHARED_PTR_VECTOR_TYPEMAP(ore::data::LegData, LegDataVector)

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

namespace ore {
namespace data {

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

class FixedLegBuilder : public LegBuilder {
public:
    FixedLegBuilder();
};

class ZeroCouponFixedLegBuilder : public LegBuilder {
public:
    ZeroCouponFixedLegBuilder();
};

class FloatingLegBuilder : public LegBuilder {
public:
    FloatingLegBuilder();
};

class CashflowLegBuilder : public LegBuilder {
public:
    CashflowLegBuilder();
};

class CPILegBuilder : public LegBuilder {
public:
    CPILegBuilder();
};

class YYLegBuilder : public LegBuilder {
public:
    YYLegBuilder();
};

class CMSLegBuilder : public LegBuilder {
public:
    CMSLegBuilder();
};

class CMBLegBuilder : public LegBuilder {
public:
    CMBLegBuilder();
};

class DigitalCMSLegBuilder : public LegBuilder {
public:
    DigitalCMSLegBuilder();
};

class CMSSpreadLegBuilder : public LegBuilder {
public:
    CMSSpreadLegBuilder();
};

class DigitalCMSSpreadLegBuilder : public LegBuilder {
public:
    DigitalCMSSpreadLegBuilder();
};

class EquityLegBuilder : public LegBuilder {
public:
    EquityLegBuilder();
};

        class CommodityFixedLegBuilder : public LegBuilder {
public:
    CommodityFixedLegBuilder();
};

        class CommodityFloatingLegBuilder : public LegBuilder {
public:
    CommodityFloatingLegBuilder();
};

// ore/OREData/ored/portfolio/durationadjustedcmslegbuilder.hpp

        class DurationAdjustedCmsLegBuilder : public LegBuilder {
public:
    DurationAdjustedCmsLegBuilder();
};

// ore/OREData/ored/portfolio/durationadjustedcmslegdata.hpp

        class DurationAdjustedCmsLegData : public LegAdditionalData {
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

class FormulaBasedLegBuilder : public LegBuilder {
public:
    explicit FormulaBasedLegBuilder();
};

// ore/OREData/ored/portfolio/formulabasedlegdata.hpp

class FormulaBasedLegData : public LegAdditionalData {
public:
    FormulaBasedLegData();
    FormulaBasedLegData(const std::string& formulaBasedIndex, int fixingDays, bool isInArrears);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/equityfxlegbuilder.hpp

class EquityMarginLegBuilder : public LegBuilder {
public:
    EquityMarginLegBuilder();
};

// ore/OREData/ored/portfolio/equityfxlegdata.hpp

class EquityMarginLegData : public LegAdditionalData {
public:
    EquityMarginLegData();
    EquityMarginLegData(QuantLib::ext::shared_ptr<ore::data::EquityLegData>& equityLegData, const vector<double>& rates,
        const vector<string>& rateDates = vector<string>(), const double& initialMarginFactor = QuantExt::Null<double>(),
        const double& multiplier = QuantExt::Null<double>());
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

}
}

#endif
