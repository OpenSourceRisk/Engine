#include <qle/utilities/commodity.hpp>

namespace QuantExt {
namespace Commodity {
namespace Utilities {

//! Get the contract date from an expiry date
QuantLib::Date getContractDate(const QuantLib::Date& expiry, const boost::shared_ptr<FutureExpiryCalculator>& fec) {
    using QuantLib::Date;
    using QuantLib::io::iso_date;

    // Try the expiry date's associated contract month and year. Start with the expiry month and year itself.
    // Assume that expiry is within 12 months of the contract month and year.
    Date result(1, expiry.month(), expiry.year());
    Date calcExpiry = fec->expiryDate(result, 0);
    Size maxIncrement = 12;
    while (calcExpiry != expiry && maxIncrement > 0) {
        if (calcExpiry < expiry)
            result += 1 * Months;
        else
            result -= 1 * Months;
        calcExpiry = fec->expiryDate(result, 0);
        maxIncrement--;
    }

    QL_REQUIRE(calcExpiry == expiry, "Expected the calculated expiry, " << iso_date(calcExpiry)
                                                                        << ", to equal the expiry, " << iso_date(expiry)
                                                                        << ".");

    return result;
}

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
            QuantLib::NullCalendar(), 0.0, 1.0, CommodityIndexedAverageCashFlow::PaymentTiming::InArrears, true, 0, 0,
            baseFec);

    } else {
        return boost::make_shared<CommodityIndexedCashFlow>(
            1.0, start, end, baseIndex, paymentLag, QuantLib::NullCalendar(), QuantLib::Unadjusted, 0,
            QuantLib::NullCalendar(), 0.0, 1.0, CommodityIndexedCashFlow::PaymentTiming::InArrears, true, true, true, 0,
            baseFec);
    }
}

} // namespace Utilities
} // namespace Commodity
} // namespace QuantExt