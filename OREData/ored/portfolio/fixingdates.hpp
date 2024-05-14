/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/fixingdates.hpp
    \brief Logic for calculating required fixing dates on legs
*/

#pragma once

#include <ored/marketdata/todaysmarketparameters.hpp>
#include <qle/indexes/fxindex.hpp>

#include <ql/patterns/visitor.hpp>
#include <ql/time/date.hpp>

#include <map>
#include <ostream>
#include <set>
#include <string>

namespace QuantLib {
class CashFlow;
class FloatingRateCoupon;
class IborCoupon;
class CappedFlooredCoupon;
class IndexedCashFlow;
class CPICashFlow;
class CPICoupon;
class YoYInflationCoupon;
class OvernightIndexedCoupon;
class AverageBMACoupon;
class CmsSpreadCoupon;
class DigitalCoupon;
class StrippedCappedFlooredCoupon;
} // namespace QuantLib

namespace QuantExt {
class CappedFlooredAverageBMACoupon;
class AverageONIndexedCoupon;
class OvernightIndexedCoupon;
class BondTRSCashFlow;
class CappedFlooredAverageONIndexedCoupon;
class CappedFlooredOvernightIndexedCoupon;
class EquityCoupon;
class FloatingRateFXLinkedNotionalCoupon;
class FXLinkedCashFlow;
class AverageFXLinkedCashFlow;
class SubPeriodsCoupon1;
class IndexedCoupon;
class IndexWrappedCashFlow;
class NonStandardYoYInflationCoupon;
class CmbCoupon;
class CommodityIndex;
class CommodityIndexedAverageCashFlow;
class CommodityIndexedCashFlow;
class EquityMarginCoupon;
class TRSCashFlow;
} // namespace QuantExt

namespace ore {
namespace data {

class FixingDateGetter;

/*! Class holding the information on the fixings required to price a trade (or a portfolio of trades). */
class RequiredFixings {
public:    
    class FixingDates {
    public:
        FixingDates() = default;

        FixingDates(const std::set<QuantLib::Date>& dates, const bool mandatory) { addDates(dates, mandatory); }

        FixingDates(const std::map<QuantLib::Date, bool>& dates) : data_(dates){}

        void clear() { data_.clear(); }

        void addDate(const QuantLib::Date& date, const bool mandatory) {
            auto exits = data_.find(date);
            if (exits == data_.end() || exits->second == false) {
                data_[date] = mandatory;
            }
        }

        void addDates(const FixingDates& dates) {
            for (const auto& [d, man] : dates) {
                addDate(d, man);
            }
        }

        void addDates(const FixingDates& dates, bool mandatory) {
            for (const auto& [d, _] : dates) {
                addDate(d, mandatory);
            }
        }

        void addDates(const std::set<QuantLib::Date>& dates, bool mandatory) {
            for (const QuantLib::Date& d : dates) {
                addDate(d, mandatory);
            }
        }

        FixingDates filterByDate(const QuantLib::Date& before) const {
            std::map<QuantLib::Date, bool> results;
            for (const auto& [d, mandatory] : data_) {
                if (d < before) {
                    results.insert({d,mandatory});
                }
            }
            return FixingDates(results);
        }
        
        //! Iterrator for range-base forloop
        std::map<QuantLib::Date, bool>::const_iterator begin() const {return data_.begin();}
        std::map<QuantLib::Date, bool>::const_iterator end() const {return data_.end();}
       
        size_t size() const { return data_.size(); }

        bool empty() const { return data_.empty(); }

    private:
        std::map<QuantLib::Date, bool> data_;
    };


    // FixingEntry = indexName, fixingDate, payDate, alwaysAddIfPaysOnSettlement
    struct FixingEntry {
        std::string indexName;
        QuantLib::Date fixingDate;
        QuantLib::Date payDate;
        bool alwaysAddIfPaysOnSettlement;
        bool mandatory = true;
        friend bool operator<(const FixingEntry& lhs, const FixingEntry& rhs);
    };

    struct InflationFixingEntry : public FixingEntry {
        bool indexInterpolated;
        QuantLib::Frequency indexFreq;
        QuantLib::Period availabilityLeg;
        friend bool operator<(const InflationFixingEntry& lhs, const InflationFixingEntry& rhs);
    };

    struct ZeroInflationFixingEntry : InflationFixingEntry {
        CPI::InterpolationType couponInterpolation;
        QuantLib::Frequency couponFrequency;
        friend bool operator<(const ZeroInflationFixingEntry& lhs, const ZeroInflationFixingEntry& rhs);
    };

    /*! Gives back the dates for which fixings will be required to price the trade assuming a given \p settlementDate.
        If the \p settlementDate is not provided or is set equal to \c QuantLib::Date(), the settlement date in the
        implementation is assumed to be the \c Settings::instance().evaluationDate().

        If a cashflow payment is deemed to have already occurred relative to the settlement date, then no fixing is
        needed. The determination of whether a cashflow has or has not occurred will in general rely on a call to \c
        CashFlow::hasOccurred which is important in cases where the cash flow payment date falls on the settlement date.

        Another important case is where a cash flow fixing date occurs on the settlement date. In this case, we should
        always add the fixing date to the set of fixing dates regardless of
        \c Settings::instance().enforcesTodaysHistoricFixings(). */
    std::map<std::string, FixingDates>
    fixingDatesIndices(const QuantLib::Date& settlementDate = QuantLib::Date()) const;

    /*! Adds a single fixing date \p fixingDate for an index given by its ORE index name \p indexName arising from a
       coupon with payment date \p payDate. If \p alwaysAddIfPaysOnSettlement is true the fixing date will be added
       if the coupon pays on the settlement date even if the cashflow returns hasOccured(settlementDate) as true. This
       is conservative and necessary in some cases since some pricing engines in QL (e.g. CapFloor) do not respect
       hasOccured() and ask for the fixing regardless. If the payDate is not given, it defaults to Date::maxDate()
       meaning that the added fixing is relevant unconditional on a pay date */
    void addFixingDate(const QuantLib::Date& fixingDate, const std::string& indexName,
                       const QuantLib::Date& payDate = Date::maxDate(), const bool alwaysAddIfPaysOnSettlement = false,
                       const bool mandatoryFixing = true);
    
    void addFixingDate(const FixingEntry& fixingEntry);

    /*! adds a vector of fixings dates \p fixingDates and mandatory flags for an index given by is ORE index name \p indexName arising from
       a coupon with payment date \p payDate */
    void addFixingDates(const std::vector<std::pair<QuantLib::Date, bool>>& fixingDates, const std::string& indexName,
                        const QuantLib::Date& payDate = Date::maxDate(),
                        const bool alwaysAddIfPaysOnSettlement = false);

    /*! adds a vector of fixings dates \p fixingDates for an index given by is ORE index name \p indexName arising from
       a coupon with payment date \p payDate */
    void addFixingDates(const std::vector<QuantLib::Date>& fixingDates, const std::string& indexName,
                        const QuantLib::Date& payDate = Date::maxDate(), const bool alwaysAddIfPaysOnSettlement = false,
                        const bool mandatory = true);

    /*! add a single fixing date \p fixingDate for a coupon based on a zero inflation index given by its ORE index name
        \p indexName with payment date \p payDate */
    void addZeroInflationFixingDate(const QuantLib::Date& fixingDate, const std::string& indexName,
                                    const bool indexInterpolated, const Frequency indexFrequency,
                                    const Period& indexAvailabilityLag,
                                    const CPI::InterpolationType coupopnInterpolation, const Frequency couponFrequency,
                                    const QuantLib::Date& payDate = Date::maxDate(),
                                    const bool alwaysAddIfPaysOnSettlement = false,
                                    const bool mandatoryFixing = true);
    void addZeroInflationFixingDate(const ZeroInflationFixingEntry& fixingEntry);

    /*! add a single fixing date \p fixingDate for a coupon based on a yoy inflation index given by its ORE index name
        \p indexName with payment date \p payDate */
    void addYoYInflationFixingDate(const QuantLib::Date& fixingDate, const std::string& indexName,
                                   const bool indexInterpolated, const Frequency indexFrequency,
                                   const Period& indexAvailabilityLag, const QuantLib::Date& payDate = Date::maxDate(),
                                   const bool alwaysAddIfPaysOnSettlement = false,
                                   const bool mandatoryFixing = true);
    void addYoYInflationFixingDate(const InflationFixingEntry& fixingEntry);

    /*! clear all data */
    void clear();

    /*! add data from another RequiredFixings instance */
    void addData(const RequiredFixings& requiredFixings);

    /*! Set all pay dates to Date::maxDate(), fixingDatesIndices() will then not filter the required fixings by the
      given settlement date any more. Needed by total return swaps on bonds for example, where a cashflow in a bond with
      past payment date can still be relevant for the payment of the current return period. */
    void unsetPayDates();


    /*! Create a copy and set mandatory flag to mandatory for all fixing entries */
    RequiredFixings makeCopyWithMandatoryOverride(bool mandatory);

    RequiredFixings filteredFixingDates(const QuantLib::Date& settlementDate = QuantLib::Date());

private:
    // maps an ORE index name to a pair (fixing date, pay date, alwaysAddIfPaysOnSettlment)
    std::set<FixingEntry> fixingDates_;
    // same as above, but for zero inflation index based coupons
    std::set<ZeroInflationFixingEntry> zeroInflationFixingDates_;
    // same as above, but for yoy inflation index based coupons
    std::set<InflationFixingEntry> yoyInflationFixingDates_;

    // grant access to stream output operator
    friend std::ostream& operator<<(std::ostream&, const RequiredFixings&);
};



/*! allow output of required fixings data via streams */
std::ostream& operator<<(std::ostream& out, const RequiredFixings& f);

/*! Helper Class that gets relevant fixing dates from coupons and add them to a RequiredFixings instance.

    Each type of FloatingRateCoupon that we wish to cover should be added here
    and a \c visit method implemented against it. */
class FixingDateGetter : public QuantLib::AcyclicVisitor,
                         public QuantLib::Visitor<QuantLib::CashFlow>,
                         public QuantLib::Visitor<QuantLib::FloatingRateCoupon>,
                         public QuantLib::Visitor<QuantLib::IborCoupon>,
                         public QuantLib::Visitor<QuantLib::CappedFlooredCoupon>,
                         public QuantLib::Visitor<QuantLib::IndexedCashFlow>,
                         public QuantLib::Visitor<QuantLib::CPICashFlow>,
                         public QuantLib::Visitor<QuantLib::CPICoupon>,
                         public QuantLib::Visitor<QuantLib::YoYInflationCoupon>,
                         public QuantLib::Visitor<QuantLib::OvernightIndexedCoupon>,
                         public QuantLib::Visitor<QuantExt::OvernightIndexedCoupon>,
                         public QuantLib::Visitor<QuantExt::CappedFlooredOvernightIndexedCoupon>,
                         public QuantLib::Visitor<QuantLib::AverageBMACoupon>,
                         public QuantLib::Visitor<QuantExt::CappedFlooredAverageBMACoupon>,
                         public QuantLib::Visitor<QuantLib::CmsSpreadCoupon>,
                         public QuantLib::Visitor<QuantLib::DigitalCoupon>,
                         public QuantLib::Visitor<QuantLib::StrippedCappedFlooredCoupon>,
                         public QuantLib::Visitor<QuantExt::AverageONIndexedCoupon>,
                         public QuantLib::Visitor<QuantExt::CappedFlooredAverageONIndexedCoupon>,
                         public QuantLib::Visitor<QuantExt::EquityCoupon>,
                         public QuantLib::Visitor<QuantExt::FloatingRateFXLinkedNotionalCoupon>,
                         public QuantLib::Visitor<QuantExt::FXLinkedCashFlow>,
                         public QuantLib::Visitor<QuantExt::AverageFXLinkedCashFlow>,
                         public QuantLib::Visitor<QuantExt::SubPeriodsCoupon1>,
                         public QuantLib::Visitor<QuantExt::IndexedCoupon>,
                         public QuantLib::Visitor<QuantExt::IndexWrappedCashFlow>,
                         public QuantLib::Visitor<QuantExt::NonStandardYoYInflationCoupon>,
                         public QuantLib::Visitor<QuantExt::CmbCoupon>,
                         public QuantLib::Visitor<QuantExt::EquityMarginCoupon>,
                         public QuantLib::Visitor<QuantExt::CommodityCashFlow>,
                         public QuantLib::Visitor<QuantExt::BondTRSCashFlow>,
                         public QuantLib::Visitor<QuantExt::TRSCashFlow> {

public:
    //! Constructor
    FixingDateGetter(RequiredFixings& requiredFixings) : requiredFixings_(requiredFixings) {}

    //! \name Visitor interface
    //@{
    void visit(QuantLib::CashFlow& c) override;
    void visit(QuantLib::FloatingRateCoupon& c) override;
    void visit(QuantLib::IborCoupon& c) override;
    void visit(QuantLib::CappedFlooredCoupon& c) override;
    void visit(QuantLib::IndexedCashFlow& c) override;
    /*! Not added in QuantLib so will never be hit automatically!
        Managed by passing off from IndexedCashFlow.
    */
    void visit(QuantLib::CPICashFlow& c) override;
    void visit(QuantLib::CPICoupon& c) override;
    void visit(QuantLib::YoYInflationCoupon& c) override;
    void visit(QuantExt::NonStandardYoYInflationCoupon& c) override;
    void visit(QuantLib::OvernightIndexedCoupon& c) override;
    void visit(QuantExt::OvernightIndexedCoupon& c) override;
    void visit(QuantExt::CappedFlooredOvernightIndexedCoupon& c) override;
    void visit(QuantLib::AverageBMACoupon& c) override;
    void visit(QuantExt::CappedFlooredAverageBMACoupon& c) override;
    void visit(QuantLib::CmsSpreadCoupon& c) override;
    void visit(QuantLib::DigitalCoupon& c) override;
    void visit(QuantLib::StrippedCappedFlooredCoupon& c) override;
    void visit(QuantExt::AverageONIndexedCoupon& c) override;
    void visit(QuantExt::CappedFlooredAverageONIndexedCoupon& c) override;
    void visit(QuantExt::EquityCoupon& c) override;
    void visit(QuantExt::FloatingRateFXLinkedNotionalCoupon& c) override;
    void visit(QuantExt::FXLinkedCashFlow& c) override;
    void visit(QuantExt::AverageFXLinkedCashFlow& c) override;
    void visit(QuantExt::SubPeriodsCoupon1& c) override;
    void visit(QuantExt::IndexedCoupon& c) override;
    void visit(QuantExt::IndexWrappedCashFlow& c) override;
    void visit(QuantExt::CmbCoupon& c) override;
    void visit(QuantExt::EquityMarginCoupon& c) override;
    void visit(QuantExt::CommodityCashFlow& c) override;
    void visit(QuantExt::BondTRSCashFlow& c) override;
    void visit(QuantExt::TRSCashFlow& c) override;
    //@}
        
    void setRequireFixingStartDates(const bool b) { requireFixingStartDates_ = b; }
    void setAdditionalFxIndex(const QuantLib::ext::shared_ptr<QuantExt::FxIndex>& i) { additionalFxIndex_ = i; }

protected:
    std::string oreIndexName(const std::string& qlIndexName) const;
    RequiredFixings& requiredFixings_;

private:
    // flag to indicate if coupon start date fixings are always required, even if initial prices provided
    bool requireFixingStartDates_ = false;
    // We may need fixings for an additional FX Index at every fixing date
    QuantLib::ext::shared_ptr<QuantExt::FxIndex> additionalFxIndex_;
};

/*! Populates a RequiredFixings instance based on a given QuantLib::Leg */
void addToRequiredFixings(const QuantLib::Leg& leg, const QuantLib::ext::shared_ptr<FixingDateGetter>& fixingDateGetter);

/*! Inflation fixings are generally available on a monthly, or coarser, frequency. When a portfolio is asked for its
    fixings, and it contains inflation fixings, ORE will by convention put the fixing date as the 1st day of the
    applicable inflation period. Some market data providers by convention supply the inflation fixings with the date 
    as the last date of the applicable inflation period. This function scans the \p fixings map, and moves any 
    inflation fixing dates from the 1st day of the inflation period to the last day of the inflation period. The key 
    in the \p fixings map is the index name and the value is the set of dates for which we require the fixings.

    If inflation indices have been set up via ZeroInflationIndex entries in the Conventions, the \p conventions 
    should be passed here. If not, the default \c nullptr parameter will be sufficient.
*/
void amendInflationFixingDates(std::map<std::string, RequiredFixings::FixingDates>& fixings);

/*! Add index and fixing date pairs to \p fixings that will be potentially needed to build a TodaysMarket.

    These additional index and fixing date pairs are found by scanning the \p mktParams and:
    - for MarketObject::IndexCurve, take the ibor index name and add the dates for each weekday between settlement
      date minus \p iborLookback period or \p oisLookback period and settlement date. The distinction between 
      Ibor and OIS is made here to cover the fixings necessary for OIS futures. The default value of 4 months 
      covers OIS futures with a contract period of up to 3 months. It would need to be configured differently 
      if OIS futures with a longer contract period are possible.
    - for MarketObject::ZeroInflationCurve, take the inflation index and add the first of each month between
      settlement date minus \p inflationLookback period and settlement date
    - for MarketObject::YoYInflationCurve, take the inflation index and add the first of each month between
      settlement date minus \p inflationLookback period and settlement date
    - for MarketObject::CommodityCurve, add \e fixings for future contracts expiring 2 months either side of the
      settlement date. The fixing dates are added for each weekday going back to the first day of the month that
      precedes the settlement date by 2 months. The approach here will give rise to some spot commodities being
      given a future contract name and dates added against them - this should not be a problem as there will be
      no fixings found for them in any case.

    The original \p fixings map may be empty.
*/
void addMarketFixingDates(const QuantLib::Date& asof, std::map<std::string, RequiredFixings::FixingDates>& fixings,
                          const TodaysMarketParameters& mktParams,
                          const QuantLib::Period& iborLookback = 5 * QuantLib::Days,
                          const QuantLib::Period& oisLookback = 4 * QuantLib::Months,
                          const QuantLib::Period& bmaLookback = 2 * QuantLib::Weeks,
                          const QuantLib::Period& inflationLookback = 1 * QuantLib::Years);

} // namespace data
} // namespace ore
