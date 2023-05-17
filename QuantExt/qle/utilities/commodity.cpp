#include <qle/utilities/commodity.hpp>

namespace QuantExt {

//! Make a commodity indexed cashflow
boost::shared_ptr<CashFlow>
makeCommodityCashflowForBasisFuture(const QuantLib::Date& start, const QuantLib::Date& end,
                                    const boost::shared_ptr<CommodityIndex>& baseIndex,
                                    const boost::shared_ptr<FutureExpiryCalculator>& baseFec, bool baseIsAveraging,
                                    const QuantLib::Date& paymentDate) {
    Natural paymentLag = 0;
    if (paymentDate != Date() && paymentDate >= end) {
        paymentLag = paymentDate - end;
    }
    if (baseIsAveraging == true) {
        return boost::make_shared<CommodityIndexedAverageCashFlow>(
            1.0, start, end, paymentLag, QuantLib::NullCalendar(), QuantLib::Unadjusted, baseIndex,
            QuantLib::Calendar(), 0.0, 1.0, CommodityIndexedAverageCashFlow::PaymentTiming::InArrears, true, 0, 0,
            baseFec);

    } else {
        return boost::make_shared<CommodityIndexedCashFlow>(
            1.0, start, end, baseIndex, paymentLag, QuantLib::NullCalendar(), QuantLib::Unadjusted, 0,
            QuantLib::NullCalendar(), 0.0, 1.0, CommodityIndexedCashFlow::PaymentTiming::InArrears, true, true, true, 0,
            baseFec);
    }
}

} // namespace QuantExt