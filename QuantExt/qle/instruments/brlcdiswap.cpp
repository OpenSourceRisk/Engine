/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
 All rights reserved.
*/

#include <qle/instruments/brlcdiswap.hpp>

#include <qle/cashflows/brlcdicouponpricer.hpp>
#include <qle/cashflows/couponpricer.hpp>
#include <ql/cashflows/overnightindexedcoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/time/schedule.hpp>
#include <ql/time/daycounters/business252.hpp>
#include <boost/assign/list_of.hpp>

using namespace QuantLib;
using boost::assign::list_of;
using std::vector;
using std::pow;

namespace QuantExt {

// Reason for use of convert_to_container:
// https://stackoverflow.com/a/17805923/1771882
BRLCdiSwap::BRLCdiSwap(Type type, Real nominal, const Date& startDate, const Date& endDate, Rate fixedRate, 
    const boost::shared_ptr<BRLCdi>& overnightIndex, Spread spread, bool telescopicValueDates) 
    : OvernightIndexedSwap(type, nominal, Schedule(list_of(startDate)(endDate).convert_to_container<vector<Date> >(), 
        NullCalendar(), QuantLib::Unadjusted, QuantLib::Unadjusted, 100 * Years), fixedRate, 
        overnightIndex->dayCounter(), overnightIndex, spread, 0, ModifiedFollowing, overnightIndex->fixingCalendar(), 
        telescopicValueDates), startDate_(startDate), endDate_(endDate), index_(overnightIndex) {

    // Need to overwrite the fixed leg with the correct fixed leg for a standard BRL CDI swap
    // Fixed leg is of the form: N [(1 + k) ^ \delta - 1]
    // where \delta is the number of BRL business days in the period divided by 252 i.e. 
    // the day count fraction for the period on a Business252 basis.
    Time dcf = index_->dayCounter().yearFraction(startDate_, endDate_);
    Real fixedLegPayment = nominal * (pow(1.0 + fixedRate, dcf) - 1.0);
    Date paymentDate = legs_[0].back()->date();
    boost::shared_ptr<CashFlow> fixedCashflow = boost::make_shared<SimpleCashFlow>(fixedLegPayment, paymentDate);
    legs_[0].clear();
    legs_[0].push_back(fixedCashflow);
    registerWith(fixedCashflow);

    // Set the pricer on the BRL CDI coupon
    QL_REQUIRE(legs_[1].size() == 1, "BRLCdiSwap expected exactly one overnight coupon");
    boost::shared_ptr<OvernightIndexedCoupon> coupon = boost::dynamic_pointer_cast<OvernightIndexedCoupon>(legs_[1][0]);
    coupon->setPricer(boost::make_shared<BRLCdiCouponPricer>());
}

Real BRLCdiSwap::fixedLegBPS() const {
    
    calculate();

    static Spread basisPoint = 1.0e-4;
    if (!close(endDiscounts_[0], 0.0) && endDiscounts_[0] != Null<DiscountFactor>()) {
        DiscountFactor df = endDiscounts_[0];
        Time dcf = index_->dayCounter().yearFraction(startDate_, endDate_);
        legBPS_[0] = df * nominal() * (pow(1.0 + fixedRate() + basisPoint, dcf) - pow(1.0 + fixedRate(), dcf));
        return legBPS_[0];
    }

    QL_FAIL("BRLCdiSwap cannot calculate fixed leg BPS because end discount is not populated");
}

Real BRLCdiSwap::fairRate() const {
    
    calculate();
    
    static Spread basisPoint = 1.0e-4;
    if (!close(endDiscounts_[0], 0.0) && endDiscounts_[0] != Null<DiscountFactor>()) {
        DiscountFactor df = endDiscounts_[0];
        Time dcf = index_->dayCounter().yearFraction(startDate_, endDate_);
        return pow(overnightLegNPV() / (nominal() * df) + 1.0, 1.0 / dcf) - 1.0;
    }

    QL_FAIL("BRLCdiSwap cannot calculate fair rate because end discount is not populated");
}

}
