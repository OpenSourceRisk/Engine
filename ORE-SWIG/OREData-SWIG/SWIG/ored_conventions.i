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

#ifndef ored_conventions_i
#define ored_conventions_i

%include indexes.i
%include inflation.i

%{
// put c++ declarations here
using ore::data::Conventions;
using ore::data::Convention;
using ore::data::ZeroRateConvention;
using ore::data::DepositConvention;
using ore::data::FutureConvention;
using ore::data::FraConvention;
using ore::data::OisConvention;
using ore::data::IborIndexConvention;
using ore::data::OvernightIndexConvention;
using ore::data::SwapIndexConvention;
using ore::data::IRSwapConvention;
using ore::data::AverageOisConvention;
using ore::data::TenorBasisSwapConvention;
using ore::data::TenorBasisTwoSwapConvention;
using ore::data::BMABasisSwapConvention;
using ore::data::FXConvention;
using ore::data::CrossCcyBasisSwapConvention;
using ore::data::CrossCcyFixFloatSwapConvention;
using ore::data::CdsConvention;
using ore::data::InflationSwapConvention;
using ore::data::SecuritySpreadConvention;
using ore::data::CmsSpreadOptionConvention;
using ore::data::CommodityForwardConvention;
using ore::data::CommodityFutureConvention;
using ore::data::FxOptionConvention;
using ore::data::FxOptionTimeWeightingConvention;
using ore::data::ZeroInflationIndexConvention;
using ore::data::BondYieldConvention;

using QuantLib::DayCounter;
using QuantLib::Calendar;
using QuantLib::Compounding;
using QuantLib::Frequency;
using QuantLib::Period;
using QuantLib::Natural;
using QuantLib::BusinessDayConvention;
using QuantLib::Size;
using QuantLib::DateGeneration;
using QuantExt::SubPeriodsCoupon1;
using QuantExt::BMAIndexWrapper;
using QuantExt::FutureExpiryCalculator;
%}

%shared_ptr(Conventions)
class Conventions {
  public:
    Conventions();
    ext::shared_ptr<Convention> get(const std::string& id) const;
    void clear();
    void add(const ext::shared_ptr<Convention>& convention);
    void fromXMLString(const std::string& xmlString);
    void fromFile(const std::string& xmlFileName);
};

%shared_ptr(Convention)
class Convention {
  public:
    enum class Type {
        Zero,
        Deposit,
        Future,
        FRA,
        OIS,
        Swap,
        AverageOIS,
        TenorBasisSwap,
        TenorBasisTwoSwap,
        BMABasisSwap,
        FX,
        CrossCcyBasis,
        CrossCcyFixFloat,
        CDS,
        SwapIndex,
        InflationSwap,
        SecuritySpread
    };
    const std::string& id() const;
    Convention::Type type() const;
    void fromXMLString(const std::string& xmlString);
    std::string toXMLString();

    %extend {
      static const Convention::Type Zero = Convention::Type::Zero;
      static const Convention::Type Deposit = Convention::Type::Deposit;
      static const Convention::Type Future = Convention::Type::Future;
      static const Convention::Type FRA = Convention::Type::FRA;
      static const Convention::Type OIS = Convention::Type::OIS;
      static const Convention::Type Swap = Convention::Type::Swap;
      static const Convention::Type AverageOis = Convention::Type::AverageOIS;
      static const Convention::Type TenorBasisSwap = Convention::Type::TenorBasisSwap;
      static const Convention::Type TenorBasisTwoSwap = Convention::Type::TenorBasisTwoSwap;
      static const Convention::Type BMABasisSwap = Convention::Type::BMABasisSwap;
      static const Convention::Type FX = Convention::Type::FX;
      static const Convention::Type CrossCcyBasis = Convention::Type::CrossCcyBasis;
      static const Convention::Type CrossCcyFixFloat = Convention::Type::CrossCcyFixFloat;
      static const Convention::Type CDS = Convention::Type::CDS;
      static const Convention::Type SwapIndex = Convention::Type::SwapIndex;
      static const Convention::Type InflationSwap = Convention::Type::InflationSwap;
      static const Convention::Type SecuritySpread = Convention::Type::SecuritySpread;
    }

 private:
    Convention();
};

%shared_ptr(ZeroRateConvention)
class ZeroRateConvention : public Convention {
  public:
    ZeroRateConvention();
    ZeroRateConvention(const std::string& id, const std::string& dayCounter,
                       const std::string& tenorCalendar, const std::string& compounding /*= "Continuous"*/,
                       const std::string& compoundingFrequency /*= "Annual"*/, const std::string& spotLag = "",
                       const std::string& spotCalendar = "", const std::string& rollConvention = "",
                       const std::string& eom = "");
    const DayCounter& dayCounter() const;
    const Calendar& tenorCalendar() const;
    Compounding compounding() const;
    Frequency compoundingFrequency() const;
    Natural spotLag() const;
    const Calendar& spotCalendar() const;
    BusinessDayConvention rollConvention() const;
    bool eom();
    bool tenorBased();
    %extend {
      static const ext::shared_ptr<ZeroRateConvention> getFullView(ext::shared_ptr<Convention> baseInput) {
          return ext::dynamic_pointer_cast<ore::data::ZeroRateConvention>(baseInput);
      }
    }
};

%shared_ptr(DepositConvention)
class DepositConvention : public Convention {
  public:
    DepositConvention();
    DepositConvention(const std::string& id, const std::string& index);
    DepositConvention(const std::string& id, const std::string& calendar,
                      const std::string& convention, const std::string& eom, const std::string& dayCounter,
                      const std::string& settlementDays);
    const std::string& index() const;
    const Calendar& calendar() const;
    BusinessDayConvention convention() const;
    bool eom();
    const DayCounter& dayCounter() const;
    const Size settlementDays() const;
    bool indexBased();
    %extend {
      static const ext::shared_ptr<DepositConvention> getFullView(ext::shared_ptr<Convention> baseInput) {
          return ext::dynamic_pointer_cast<ore::data::DepositConvention>(baseInput);
      }
    }
};

%shared_ptr(FutureConvention)
class FutureConvention : public Convention {
  public:
    FutureConvention();
    FutureConvention(const std::string& id, const std::string& index);
    const ext::shared_ptr<IborIndex> index() const;
    %extend {
      static const ext::shared_ptr<FutureConvention> getFullView(ext::shared_ptr<Convention> baseInput) {
          return ext::dynamic_pointer_cast<ore::data::FutureConvention>(baseInput);
      }
    }
};

%shared_ptr(FraConvention)
class FraConvention : public Convention {
  public:
    FraConvention();
    FraConvention(const std::string& id,const std::string& index);
    const ext::shared_ptr<IborIndex> index() const;
    %extend {
      static const ext::shared_ptr<FraConvention> getFullView(ext::shared_ptr<Convention> baseInput) {
          return ext::dynamic_pointer_cast<ore::data::FraConvention>(baseInput);
      }
    }
};

%shared_ptr(OisConvention)
class OisConvention : public Convention {
  public:
    OisConvention();
    OisConvention(const std::string& id, const std::string& spotLag, const std::string& index, const std::string& fixedDayCounter,
                  const std::string& fixedCalendar, const std::string& paymentLag = "", const std::string& eom = "",
                  const std::string& fixedFrequency = "", const std::string& fixedConvention = "",
                  const std::string& fixedPaymentConvention = "", const std::string& rule = "",
                  const std::string& paymentCalendar = "");
    Natural spotLag() const;
    const std::string& indexName();
    const ext::shared_ptr<OvernightIndex> index();
    const DayCounter& fixedDayCounter() const;
    Natural paymentLag() const;
    bool eom();
    Frequency fixedFrequency() const;
    BusinessDayConvention fixedConvention() const;
    BusinessDayConvention fixedPaymentConvention() const;
    DateGeneration::Rule rule() const;
    %extend {
      static const ext::shared_ptr<OisConvention> getFullView(ext::shared_ptr<Convention> baseInput) {
          return ext::dynamic_pointer_cast<ore::data::OisConvention>(baseInput);
      }
    }
};

%shared_ptr(IborIndexConvention)
class IborIndexConvention : public Convention {
  public:
    IborIndexConvention();
    IborIndexConvention(const std::string& id, const std::string& fixingCalendar, const std::string& dayCounter,
                        const QuantLib::Size settlementDays, const std::string& businessDayConvention, const bool endOfMonth);

    const std::string& fixingCalendar() const;
    const std::string& dayCounter() const;
    const QuantLib::Size settlementDays() const;
    const std::string& businessDayConvention() const;
    const bool endOfMonth() const;

    %extend {
      static const ext::shared_ptr<IborIndexConvention> getFullView(ext::shared_ptr<Convention> baseInput) {
          return ext::dynamic_pointer_cast<ore::data::IborIndexConvention>(baseInput);
      }
    }
};

%shared_ptr(OvernightIndexConvention)
class OvernightIndexConvention : public Convention {
  public:
    OvernightIndexConvention();
    OvernightIndexConvention(const std::string& id, const std::string& fixingCalendar, const std::string& dayCounter,
                             const QuantLib::Size settlementDays);

    const std::string& fixingCalendar() const;
    const std::string& dayCounter() const;
    const QuantLib::Size settlementDays() const;

    %extend {
      static const ext::shared_ptr<OvernightIndexConvention> getFullView(ext::shared_ptr<Convention> baseInput) {
          return ext::dynamic_pointer_cast<ore::data::OvernightIndexConvention>(baseInput);
      }
    }
};

%shared_ptr(SwapIndexConvention)
class SwapIndexConvention : public Convention {
  public:
    SwapIndexConvention();
    SwapIndexConvention(const std::string& id,const std::string& conventions);
    const std::string& conventions() const;
    %extend {
      static const ext::shared_ptr<SwapIndexConvention> getFullView(ext::shared_ptr<Convention> baseInput) {
          return ext::dynamic_pointer_cast<ore::data::SwapIndexConvention>(baseInput);
      }
    }
};

%shared_ptr(IRSwapConvention)
class IRSwapConvention : public Convention {
  public:
    IRSwapConvention();
    IRSwapConvention(const std::string& id,const std::string& fixedCalendar,
                     const std::string& fixedFrequency, const std::string& fixedConvention,
                     const std::string& fixedDayCounter, const std::string& index, bool hasSubPeriod = false,
                     const std::string& floatFrequency = "", const std::string& subPeriodsCouponType = "");
    const Calendar& fixedCalendar() const;
    Frequency fixedFrequency() const;
    BusinessDayConvention fixedConvention() const;
    const DayCounter& fixedDayCounter() const;
    const std::string& indexName() const;
    const ext::shared_ptr<IborIndex> index() const;
    bool hasSubPeriod() const;
    Frequency floatFrequency() const;
    QuantExt::SubPeriodsCoupon1::Type subPeriodsCouponType() const;
    %extend {
      static const ext::shared_ptr<IRSwapConvention> getFullView(ext::shared_ptr<Convention> baseInput) {
          return ext::dynamic_pointer_cast<ore::data::IRSwapConvention>(baseInput);
      }
    }
};

%shared_ptr(AverageOisConvention)
class AverageOisConvention : public Convention {
  public:
    AverageOisConvention();
    AverageOisConvention(const std::string& id,const std::string& spotLag, const std::string& fixedTenor,
                         const std::string& fixedDayCounter, const std::string& fixedCalendar,
                         const std::string& fixedConvention, const std::string& fixedPaymentConvention, const std::string& fixedFrequency,
                         const std::string& index, const std::string& onTenor, const std::string& rateCutoff);
    Natural spotLag() const;
    const Period& fixedTenor() const;
    const DayCounter& fixedDayCounter() const;
    const Calendar& fixedCalendar() const;
    BusinessDayConvention fixedConvention() const;
    BusinessDayConvention fixedPaymentConvention() const;
    Frequency fixedFrequency() const;
    const std::string& indexName() const;
    const ext::shared_ptr<OvernightIndex> index() const;
    const Period& onTenor() const;
    Natural rateCutoff() const;
    %extend {
      static const ext::shared_ptr<AverageOisConvention> getFullView(ext::shared_ptr<Convention> baseInput) {
          return ext::dynamic_pointer_cast<ore::data::AverageOisConvention>(baseInput);
      }
    }
};

%shared_ptr(TenorBasisSwapConvention)
class TenorBasisSwapConvention : public Convention {
  public:
    TenorBasisSwapConvention();
    TenorBasisSwapConvention(const string& id, const string& payIndex, const string& receiveIndex,
                             const string& receiveFrequency = "", const string& payFrequency = "",
                             const string& spreadOnRec = "", const string& includeSpread = "", 
                             const string& subPeriodsCouponType = "");
    const ext::shared_ptr<IborIndex> payIndex() const;
    const ext::shared_ptr<IborIndex> receiveIndex() const;
    const std::string& payIndexName() const;
    const std::string& receiveFrequency() const;
    const Period& payFrequency() const;
    bool spreadOnRec() const;
    bool includeSpread() const;
    QuantExt::SubPeriodsCoupon1::Type subPeriodsCouponType() const;
    %extend {
      static const ext::shared_ptr<TenorBasisSwapConvention> getFullView(ext::shared_ptr<Convention> baseInput) {
          return ext::dynamic_pointer_cast<ore::data::TenorBasisSwapConvention>(baseInput);
      }
    }
};

%shared_ptr(TenorBasisTwoSwapConvention)
class TenorBasisTwoSwapConvention : public Convention {
  public:
    TenorBasisTwoSwapConvention();
    TenorBasisTwoSwapConvention(const std::string& id, const std::string& calendar,
                                const std::string& longFixedFrequency, const std::string& longFixedConvention,
                                const std::string& longFixedDayCounter, const std::string& longIndex,
                                const std::string& shortFixedFrequency, const std::string& shortFixedConvention,
                                const std::string& shortFixedDayCounter, const std::string& shortIndex,
                                const std::string& longMinusShort = "");
    const Calendar& calendar() const;
    Frequency longFixedFrequency() const;
    BusinessDayConvention longFixedConvention() const;
    const DayCounter& longFixedDayCounter() const;
    const ext::shared_ptr<IborIndex> longIndex() const;
    Frequency shortFixedFrequency() const;
    BusinessDayConvention shortFixedConvention() const;
    const DayCounter& shortFixedDayCounter() const;
    const ext::shared_ptr<IborIndex> shortIndex() const;
    bool longMinusShort() const;
    %extend {
      static const ext::shared_ptr<TenorBasisTwoSwapConvention> getFullView(ext::shared_ptr<Convention> baseInput) {
          return ext::dynamic_pointer_cast<ore::data::TenorBasisTwoSwapConvention>(baseInput);
      }
    }
};

%shared_ptr(BMABasisSwapConvention)
class BMABasisSwapConvention : public Convention {
  public:
    BMABasisSwapConvention();
    BMABasisSwapConvention(const std::string& id,const std::string& index,
                           const std::string& bmaIndex);
    const ext::shared_ptr<IborIndex> index() const;
    const ext::shared_ptr<BMAIndexWrapper> bmaIndex() const;
    const std::string& indexName() const;
    const std::string& bmaIndexName() const;
    %extend {
      static const ext::shared_ptr<BMABasisSwapConvention> getFullView(ext::shared_ptr<Convention> baseInput) {
          return ext::dynamic_pointer_cast<ore::data::BMABasisSwapConvention>(baseInput);
      }
    }
};

%shared_ptr(FXConvention)
class FXConvention : public Convention {
  public:
    FXConvention();
    FXConvention(const std::string& id,const std::string& spotDays,
                 const std::string& sourceCurrency, const std::string& targetCurrency,
                 const std::string& pointsFactor, const std::string& advanceCalendar = "",
                 const std::string& spotRelative = "");
    Natural spotDays() const;
    const Currency& sourceCurrency() const;
    const Currency& targetCurrency() const;
    Real pointsFactor() const;
    const Calendar& advanceCalendar() const;
    bool spotRelative() const;
    %extend {
      static const ext::shared_ptr<FXConvention> getFullView(ext::shared_ptr<Convention> baseInput) {
          return ext::dynamic_pointer_cast<ore::data::FXConvention>(baseInput);
      }
    }
};

%shared_ptr(CrossCcyBasisSwapConvention)
class CrossCcyBasisSwapConvention : public Convention {
  public:
    CrossCcyBasisSwapConvention();
    CrossCcyBasisSwapConvention(const std::string& id,const std::string& strSettlementDays,
                                const std::string& strSettlementCalendar, const std::string& strRollConvention,
                                const std::string& flatIndex, const std::string& spreadIndex,
                                const std::string& strEom = "");
    Natural settlementDays() const;
    const Calendar& settlementCalendar() const;
    BusinessDayConvention rollConvention() const;
    const ext::shared_ptr<IborIndex> flatIndex() const;
    const ext::shared_ptr<IborIndex> spreadIndex() const;
    const std::string& flatIndexName() const;
    const std::string& spreadIndexName() const;
    bool eom() const;
    %extend {
      static const ext::shared_ptr<CrossCcyBasisSwapConvention> getFullView(ext::shared_ptr<Convention> baseInput) {
          return ext::dynamic_pointer_cast<ore::data::CrossCcyBasisSwapConvention>(baseInput);
      }
    }
};

%shared_ptr(CrossCcyFixFloatSwapConvention)
class CrossCcyFixFloatSwapConvention : public Convention {
  public:
    CrossCcyFixFloatSwapConvention();
    CrossCcyFixFloatSwapConvention(const std::string& id,const std::string& strSettlementDays,
                                   const std::string& strSettlementCalendar, const std::string& strRollConvention,
                                   const std::string& fixedCcy, const std::string& fixedFreq, const std::string& fixedConv,
                                   const std::string& fixedDayCount, const std::string& index,
                                   const std::string& strEom = "");
    Natural settlementDays() const;
    const Calendar& settlementCalendar() const;
    BusinessDayConvention settlementConvention() const;
    const Currency& fixedCurrency() const;
    Frequency fixedFrequency() const;
    BusinessDayConvention fixedConvention() const;
    const DayCounter& fixedDayCounter() const;
    const ext::shared_ptr<IborIndex> index() const;
    bool eom() const;
    %extend {
      static const ext::shared_ptr<CrossCcyFixFloatSwapConvention> getFullView(ext::shared_ptr<Convention> baseInput) {
          return ext::dynamic_pointer_cast<ore::data::CrossCcyFixFloatSwapConvention>(baseInput);
      }
    }
};

%shared_ptr(CdsConvention)
class CdsConvention : public Convention {
  public:
    CdsConvention();
    CdsConvention(const std::string& id, const std::string& strSettlementDays,
                  const std::string& strCalendar, const std::string& strFrequency,
                  const std::string& strPaymentConvention, const std::string& strRule,
                  const std::string& dayCounter, const std::string& settlesAccrual,
                  const std::string& paysAtDefaultTime);
    Natural settlementDays() const;
    const Calendar& calendar() const;
    Frequency frequency() const;
    BusinessDayConvention paymentConvention() const;
    DateGeneration::Rule rule() const;
    const DayCounter& dayCounter() const;
    bool settlesAccrual() const;
    bool paysAtDefaultTime() const;
    %extend {
      static const ext::shared_ptr<CdsConvention> getFullView(ext::shared_ptr<Convention> baseInput) {
          return ext::dynamic_pointer_cast<ore::data::CdsConvention>(baseInput);
      }
    }
};

%shared_ptr(InflationSwapConvention)
class InflationSwapConvention : public Convention {
  public:
    InflationSwapConvention();
    InflationSwapConvention(const std::string& id,const std::string& strFixCalendar,
                            const std::string& strFixConvention, const std::string& strDayCounter, const std::string& strIndex,
                            const std::string& strInterpolated, const std::string& strObservationLag,
                            const std::string& strAdjustInfObsDates, const std::string& strInfCalendar,
                            const std::string& strInfConvention);
    const Calendar& fixCalendar() const;
    BusinessDayConvention fixConvention() const;
    const DayCounter& dayCounter() const;
    const ext::shared_ptr<ZeroInflationIndex> index() const;
    const std::string& indexName() const;
    bool interpolated() const;
    Period observationLag() const;
    bool adjustInfObsDates() const;
    const Calendar& infCalendar() const;
    BusinessDayConvention infConvention() const;
    %extend {
      static const ext::shared_ptr<InflationSwapConvention> getFullView(ext::shared_ptr<Convention> baseInput) {
          return ext::dynamic_pointer_cast<ore::data::InflationSwapConvention>(baseInput);
      }
    }
};

%shared_ptr(SecuritySpreadConvention)
class SecuritySpreadConvention : public Convention {
  public:
    SecuritySpreadConvention();
    SecuritySpreadConvention(const std::string& id, const std::string& dayCounter,
                             const std::string& tenorCalendar, const std::string& compounding /*= "Continuous"*/,
                             const std::string& compoundingFrequency /*= "Annual"*/, const std::string& spotLag = "",
                             const std::string& spotCalendar = "", const std::string& rollConvention = "",
                             const std::string& eom = "");
    const DayCounter& dayCounter() const;
    const Calendar& tenorCalendar() const;
    Compounding compounding() const;
    Frequency compoundingFrequency() const;
    Natural spotLag() const;
    const Calendar& spotCalendar() const;
    BusinessDayConvention rollConvention() const;
    bool eom();
    %extend {
      static const ext::shared_ptr<SecuritySpreadConvention> getFullView(ext::shared_ptr<Convention> baseInput) {
          return ext::dynamic_pointer_cast<ore::data::SecuritySpreadConvention>(baseInput);
      }
    }
};

%shared_ptr(CmsSpreadOptionConvention)
class CmsSpreadOptionConvention : public Convention {
  public:
    CmsSpreadOptionConvention();
    CmsSpreadOptionConvention(const std::string& id, const std::string& strForwardStart, const std::string& strSpotDays,
                            const std::string& strSwapTenor, const std::string& strFixingDays, const std::string& strCalendar,
                            const std::string& strDayCounter, const std::string& strConvention);
    
    const QuantLib::Period& forwardStart() const;
    const QuantLib::Period spotDays() const;
    const QuantLib::Period& swapTenor();
    QuantLib::Natural fixingDays() const;
    const QuantLib::Calendar& calendar() const;
    const QuantLib::DayCounter& dayCounter() const;
    QuantLib::BusinessDayConvention rollConvention() const;
    %extend {
      static const ext::shared_ptr<CmsSpreadOptionConvention> getFullView(ext::shared_ptr<Convention> baseInput) {
          return ext::dynamic_pointer_cast<ore::data::CmsSpreadOptionConvention>(baseInput);
      }
    }
};

%shared_ptr(CommodityForwardConvention)
class CommodityForwardConvention : public Convention {
  public:
    CommodityForwardConvention();
    CommodityForwardConvention(const std::string& id, const std::string& spotDays = "", const std::string& pointsFactor = "",
                            const std::string& advanceCalendar = "", const std::string& spotRelative = "",
                            QuantLib::BusinessDayConvention bdc = Following, bool outright = true);
    
    QuantLib::Natural spotDays() const;
    QuantLib::Real pointsFactor() const;
    const QuantLib::Calendar& advanceCalendar() const;
    std::string strAdvanceCalendar() const;
    bool spotRelative() const;
    QuantLib::BusinessDayConvention bdc() const;
    bool outright() const;
    %extend {
      static const ext::shared_ptr<CommodityForwardConvention> getFullView(ext::shared_ptr<Convention> baseInput) {
          return ext::dynamic_pointer_cast<ore::data::CommodityForwardConvention>(baseInput);
      }
    }
};

%feature("flatnested");
//%rename (CommodityFutureConvention_AveragingData) CommodityFutureConvention::AveragingData;
%shared_ptr(CommodityFutureConvention)
%shared_ptr(CommodityFutureConvention::AveragingData)
%shared_ptr(CommodityFutureConvention::OffPeakPowerIndexData)
%shared_ptr(CommodityFutureConvention::ProhibitedExpiry)
class CommodityFutureConvention : public Convention {
  public:

    enum class AnchorType { DayOfMonth, NthWeekday, CalendarDaysBefore, LastWeekday, BusinessDaysAfter, WeeklyDayOfTheWeek };
    enum class OptionAnchorType { DayOfMonth, NthWeekday, BusinessDaysBefore, LastWeekday, WeeklyDayOfTheWeek };

    struct DayOfMonth {
        DayOfMonth(const std::string& dayOfMonth) : dayOfMonth_(dayOfMonth) {}
    };

    struct CalendarDaysBefore {
        CalendarDaysBefore(const std::string& calendarDaysBefore) : calendarDaysBefore_(calendarDaysBefore) {}
    };
    
    struct BusinessDaysAfter {
        BusinessDaysAfter(const std::string& businessDaysAfter) : businessDaysAfter_(businessDaysAfter) {}
    };

    struct WeeklyWeekday {
        WeeklyWeekday(const std::string& weekday) : weekday_(weekday) {}
    };

    struct OptionExpiryAnchorDateRule {
        OptionExpiryAnchorDateRule()
            : type_(OptionAnchorType::BusinessDaysBefore), daysBefore_("0"), expiryDay_(""), nth_(""), weekday_("") {}
        OptionExpiryAnchorDateRule(const DayOfMonth& expiryDay)
            : type_(OptionAnchorType::DayOfMonth), daysBefore_(""), expiryDay_(expiryDay.dayOfMonth_), nth_(""),
              weekday_("") {}
        OptionExpiryAnchorDateRule(const CalendarDaysBefore& businessDaysBefore)
            : type_(OptionAnchorType::BusinessDaysBefore), daysBefore_(businessDaysBefore.calendarDaysBefore_),
              expiryDay_(""), nth_(""), weekday_("") {}
        OptionExpiryAnchorDateRule(const std::string& nth, const std::string& weekday)
            : type_(OptionAnchorType::NthWeekday), daysBefore_(""), expiryDay_(""), nth_(nth), weekday_(weekday) {}
        OptionExpiryAnchorDateRule(const std::string& lastWeekday)
            : type_(OptionAnchorType::LastWeekday), daysBefore_(""), expiryDay_(""), nth_(""), weekday_(lastWeekday) {}
        OptionExpiryAnchorDateRule(const WeeklyWeekday& weekday)
            : type_(OptionAnchorType::WeeklyDayOfTheWeek), daysBefore_(""), expiryDay_(""), nth_(""),
              weekday_(weekday.weekday_) {}

    };

    class AveragingData : public XMLSerializable {
    public:
        enum class CalculationPeriod { PreviousMonth, ExpiryToExpiry };

        AveragingData();

        AveragingData(const std::string& commodityName, const std::string& period, const std::string& pricingCalendar,
            bool useBusinessDays, const std::string& conventionsId = "", QuantLib::Natural deliveryRollDays = 0,
            QuantLib::Natural futureMonthOffset = 0, QuantLib::Natural dailyExpiryOffset =
            QuantLib::Null<QuantLib::Natural>());

        const std::string& commodityName() const;
        CalculationPeriod period() const;
        const QuantLib::Calendar& pricingCalendar() const;
        bool useBusinessDays() const;
        const std::string& conventionsId() const;
        QuantLib::Natural deliveryRollDays() const;
        QuantLib::Natural futureMonthOffset() const;
        QuantLib::Natural dailyExpiryOffset() const;

        bool empty() const;

        void fromXML(XMLNode* node) override;
        XMLNode* toXML(XMLDocument& doc) const override;

    };

    //! Class to store conventions for creating an off peak power index 
    class OffPeakPowerIndexData : public XMLSerializable {
    public:
        //! Constructor.
        OffPeakPowerIndexData();

        //! Detailed constructor.
        OffPeakPowerIndexData(
            const std::string& offPeakIndex,
            const std::string& peakIndex,
            const std::string& offPeakHours,
            const std::string& peakCalendar);

        const std::string& offPeakIndex() const;
        const std::string& peakIndex() const;
        QuantLib::Real offPeakHours() const;
        const QuantLib::Calendar& peakCalendar() const;

        void fromXML(XMLNode* node) override;
        XMLNode* toXML(XMLDocument& doc) const override;
        void build();
    };

    class ProhibitedExpiry : public XMLSerializable {
    public:
        ProhibitedExpiry();

        ProhibitedExpiry(
            const QuantLib::Date& expiry,
            bool forFuture = true,
            QuantLib::BusinessDayConvention futureBdc = QuantLib::Preceding,
            bool forOption = true,
            QuantLib::BusinessDayConvention optionBdc = QuantLib::Preceding);

        const QuantLib::Date& expiry() const;
        bool forFuture() const;
        QuantLib::BusinessDayConvention futureBdc() const;
        bool forOption() const;
        QuantLib::BusinessDayConvention optionBdc() const;

        void fromXML(XMLNode* node) override;
        XMLNode* toXML(XMLDocument& doc) const override;

    };

    CommodityFutureConvention();

    //! Day of month based constructor
    CommodityFutureConvention(const std::string& id, const DayOfMonth& dayOfMonth, const std::string& contractFrequency,
                              const std::string& calendar, const std::string& expiryCalendar = "",
                              QuantLib::Size expiryMonthLag = 0, const std::string& oneContractMonth = "",
                              const std::string& offsetDays = "", const std::string& bdc = "",
                              bool adjustBeforeOffset = true, bool isAveraging = false,
                              const OptionExpiryAnchorDateRule& optionExpiryDateRule = OptionExpiryAnchorDateRule(),
                              const std::set<ProhibitedExpiry>& prohibitedExpiries = {},
                              QuantLib::Size optionExpiryMonthLag = 0,
                              const std::string& optionBdc = "",
                              const std::map<QuantLib::Natural, QuantLib::Natural>& futureContinuationMappings = {},
                              const std::map<QuantLib::Natural, QuantLib::Natural>& optionContinuationMappings = {},
                              const AveragingData& averagingData = AveragingData(),
                              QuantLib::Natural hoursPerDay = QuantLib::Null<QuantLib::Natural>(),
                              const boost::optional<OffPeakPowerIndexData>& offPeakPowerIndexData = boost::none,
                              const std::string& indexName = "", const std::string& optionFrequency = "");

    //! N-th weekday based constructor
    CommodityFutureConvention(const std::string& id, const std::string& nth, const std::string& weekday,
                              const std::string& contractFrequency, const std::string& calendar,
                              const std::string& expiryCalendar = "", QuantLib::Size expiryMonthLag = 0,
                              const std::string& oneContractMonth = "", const std::string& offsetDays = "",
                              const std::string& bdc = "", bool adjustBeforeOffset = true, bool isAveraging = false,
                              const OptionExpiryAnchorDateRule& optionExpiryDateRule = OptionExpiryAnchorDateRule(),
                              const std::set<ProhibitedExpiry>& prohibitedExpiries = {},
                              QuantLib::Size optionExpiryMonthLag = 0,
                              const std::string& optionBdc = "",
                              const std::map<QuantLib::Natural, QuantLib::Natural>& futureContinuationMappings = {},
                              const std::map<QuantLib::Natural, QuantLib::Natural>& optionContinuationMappings = {},
                              const AveragingData& averagingData = AveragingData(),
                              QuantLib::Natural hoursPerDay = QuantLib::Null<QuantLib::Natural>(),
                              const boost::optional<OffPeakPowerIndexData>& offPeakPowerIndexData = boost::none,
                              const std::string& indexName = "", const std::string& optionFrequency = "");

    //! Calendar days before based constructor
    CommodityFutureConvention(const std::string& id, const CalendarDaysBefore& calendarDaysBefore,
                              const std::string& contractFrequency, const std::string& calendar,
                              const std::string& expiryCalendar = "", QuantLib::Size expiryMonthLag = 0,
                              const std::string& oneContractMonth = "", const std::string& offsetDays = "",
                              const std::string& bdc = "", bool adjustBeforeOffset = true, bool isAveraging = false,
                              const OptionExpiryAnchorDateRule& optionExpiryDateRule = OptionExpiryAnchorDateRule(),
                              const std::set<ProhibitedExpiry>& prohibitedExpiries = {},
                              QuantLib::Size optionExpiryMonthLag = 0,
                              const std::string& optionBdc = "",
                              const std::map<QuantLib::Natural, QuantLib::Natural>& futureContinuationMappings = {},
                              const std::map<QuantLib::Natural, QuantLib::Natural>& optionContinuationMappings = {},
                              const AveragingData& averagingData = AveragingData(),
                              QuantLib::Natural hoursPerDay = QuantLib::Null<QuantLib::Natural>(),
                              const boost::optional<OffPeakPowerIndexData>& offPeakPowerIndexData = boost::none,
                              const std::string& indexName = "", const std::string& optionFrequency = "");
    
    //! Business days before based constructor
    CommodityFutureConvention(const std::string& id, const BusinessDaysAfter& businessDaysAfter,
                              const std::string& contractFrequency, const std::string& calendar,
                              const std::string& expiryCalendar = "", QuantLib::Size expiryMonthLag = 0,
                              const std::string& oneContractMonth = "", const std::string& offsetDays = "",
                              const std::string& bdc = "", bool adjustBeforeOffset = true, bool isAveraging = false,
                              const OptionExpiryAnchorDateRule& optionExpiryDateRule = OptionExpiryAnchorDateRule(),
                              const std::set<ProhibitedExpiry>& prohibitedExpiries = {},
                              QuantLib::Size optionExpiryMonthLag = 0,
                              const std::string& optionBdc = "",
                              const std::map<QuantLib::Natural, QuantLib::Natural>& futureContinuationMappings = {},
                              const std::map<QuantLib::Natural, QuantLib::Natural>& optionContinuationMappings = {},
                              const AveragingData& averagingData = AveragingData(),
                              QuantLib::Natural hoursPerDay = QuantLib::Null<QuantLib::Natural>(),
                              const boost::optional<OffPeakPowerIndexData>& offPeakPowerIndexData = boost::none,
                              const std::string& indexName = "", const std::string& optionFrequency = "");


    AnchorType anchorType() const;
    QuantLib::Natural dayOfMonth() const;
    QuantLib::Natural nth() const;
    QuantLib::Weekday weekday() const;
    QuantLib::Natural calendarDaysBefore() const;
    QuantLib::Integer businessDaysAfter() const;
    QuantLib::Frequency contractFrequency() const;
    const QuantLib::Calendar& calendar() const;
    const QuantLib::Calendar& expiryCalendar() const;
    QuantLib::Size expiryMonthLag() const;
    QuantLib::Month oneContractMonth() const;
    QuantLib::Integer offsetDays() const;
    QuantLib::BusinessDayConvention businessDayConvention() const;
    bool adjustBeforeOffset() const;
    bool isAveraging() const;
    QuantLib::Natural optionExpiryOffset() const;
    const std::set<ProhibitedExpiry>& prohibitedExpiries() const;
    QuantLib::Size optionExpiryMonthLag() const;
    QuantLib::Natural optionExpiryDay() const;
    QuantLib::BusinessDayConvention optionBusinessDayConvention() const;
    const std::map<QuantLib::Natural, QuantLib::Natural>& futureContinuationMappings() const;
    const std::map<QuantLib::Natural, QuantLib::Natural>& optionContinuationMappings() const;
    const AveragingData& averagingData() const;
    QuantLib::Natural hoursPerDay() const;
    const boost::optional<OffPeakPowerIndexData>& offPeakPowerIndexData() const;
    const std::string& indexName() const;
    QuantLib::Frequency optionContractFrequency() const;
    OptionAnchorType optionAnchorType() const;
    QuantLib::Natural optionNth() const;
    QuantLib::Weekday optionWeekday() const;
    const std::string& savingsTime() const;
    const std::set<QuantLib::Month>& validContractMonths() const;
    bool balanceOfTheMonth() const;
    Calendar balanceOfTheMonthPricingCalendar() const;
    const std::string& optionUnderlyingFutureConvention() const;

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

    %extend {
      static const ext::shared_ptr<CommodityFutureConvention> getFullView(ext::shared_ptr<Convention> baseInput) {
          return ext::dynamic_pointer_cast<ore::data::CommodityFutureConvention>(baseInput);
      }
    }

    void build() override;
};

%rename(compare_prohibitedexpiry) operator<;
bool operator<(const CommodityFutureConvention::ProhibitedExpiry& lhs,
    const CommodityFutureConvention::ProhibitedExpiry& rhs);

%shared_ptr(FxOptionConvention)
class FxOptionConvention : public Convention {
  public:
    FxOptionConvention();
    FxOptionConvention(const std::string& id, const std::string& fxConventionId, const std::string& atmType, const std::string& deltaType,
                    const std::string& switchTenor = "", const std::string& longTermAtmType = "",
                    const std::string& longTermDeltaType = "", const std::string& riskReversalInFavorOf = "Call",
                    const std::string& butterflyStyle = "Broker");

    const std::string& fxConventionID() const;
    const QuantLib::DeltaVolQuote::AtmType& atmType() const;
    const QuantLib::DeltaVolQuote::DeltaType& deltaType() const;
    const QuantLib::Period& switchTenor() const;
    const QuantLib::DeltaVolQuote::AtmType& longTermAtmType() const;
    const QuantLib::DeltaVolQuote::DeltaType& longTermDeltaType() const;
    const QuantLib::Option::Type& riskReversalInFavorOf() const;
    const bool butterflyIsBrokerStyle() const;

    %extend {
      static const ext::shared_ptr<FxOptionConvention> getFullView(ext::shared_ptr<Convention> baseInput) {
          return ext::dynamic_pointer_cast<ore::data::FxOptionConvention>(baseInput);
      }
    }
};

%shared_ptr(FxOptionTimeWeightingConvention)
class FxOptionTimeWeightingConvention : public Convention {
  public:
    struct TradingCenter {
        std::string name;
        std::string calendar;
        double weight;
    };
    struct Event {
        std::string description;
        QuantLib::Date date;
        double weight;
    };

    FxOptionTimeWeightingConvention();
    FxOptionTimeWeightingConvention(const std::string& id, const std::vector<double>& weekdayWeights,
                                    const std::vector<TradingCenter>& tradingCenters = {},
                                    const std::vector<Event>& events = {});

    const std::vector<double>& weekdayWeights() const;
    const std::vector<TradingCenter>& tradingCenters() const;
    const std::vector<Event>& events() const;

    %extend {
      static const ext::shared_ptr<FxOptionTimeWeightingConvention> getFullView(ext::shared_ptr<Convention> baseInput) {
          return ext::dynamic_pointer_cast<ore::data::FxOptionTimeWeightingConvention>(baseInput);
      }
    }
};

%shared_ptr(ZeroInflationIndexConvention)
class ZeroInflationIndexConvention : public Convention {
  public:
    ZeroInflationIndexConvention();
    ZeroInflationIndexConvention(const std::string& id,
        const std::string& regionName,
        const std::string& regionCode,
        bool revised,
        const std::string& frequency,
        const std::string& availabilityLag,
        const std::string& currency);

    QuantLib::Region region() const;
    bool revised() const;
    QuantLib::Frequency frequency() const;
    const QuantLib::Period& availabilityLag() const;
    const QuantLib::Currency& currency() const;

    %extend {
      static const ext::shared_ptr<ZeroInflationIndexConvention> getFullView(ext::shared_ptr<Convention> baseInput) {
          return ext::dynamic_pointer_cast<ore::data::ZeroInflationIndexConvention>(baseInput);
      }
    }
};

%shared_ptr(BondYieldConvention)
class BondYieldConvention : public Convention {
  public:
    BondYieldConvention();
    BondYieldConvention(const std::string& id,
			const std::string& compoundingName,
			const std::string& frequencyName,
			const std::string& priceTypeName,
			QuantLib::Real accuracy,
			QuantLib::Size maxEvaluations,
			QuantLib::Real guess);

    std::string compoundingName() const;
    QuantLib::Compounding compounding() const;
    std::string frequencyName() const;
    QuantLib::Frequency frequency() const;
    QuantLib::Bond::Price::Type priceType() const;
    std::string priceTypeName() const;
    QuantLib::Real accuracy() const;
    QuantLib::Size maxEvaluations() const;
    QuantLib::Real guess() const;

    %extend {
      static const ext::shared_ptr<BondYieldConvention> getFullView(ext::shared_ptr<Convention> baseInput) {
          return ext::dynamic_pointer_cast<ore::data::BondYieldConvention>(baseInput);
      }
    }
};

#endif
