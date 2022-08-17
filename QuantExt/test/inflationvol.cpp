#include "toplevelfixture.hpp"
#include <boost/test/unit_test.hpp>

#include <ql/termstructures/inflationtermstructure.hpp>
#include <qle/termstructures/inflation/piecewisezeroinflationcurve.hpp>


#include <ql/indexes/inflation/euhicp.hpp>
#include <ql/termstructures/inflation/inflationhelpers.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <qle/utilities/inflation.hpp>
#include <ql/math/matrix.hpp>

using namespace boost::unit_test_framework;
using namespace QuantLib;
using namespace std;

namespace {

struct CommonData {

    Date today;
    Real tolerance;
    DayCounter dayCounter;
    std::vector<Period> zeroCouponPillars;
    std::vector<Rate> zeroCouponQuotes;
    std::map<Date, Rate> cpiFixings;

    std::vector<Rate> strikes{0.01, 0.02, 0.03};
    std::vector<Period> tenors{1 * Years, 2 * Years, 5 * Years};

    QuantLib::Matrix cPrices;
    QuantLib::Matrix fPrices;

    Period obsLag;

    CommonData()
        : today(15, Aug, 2022), tolerance(1e-6), dayCounter(Actual365Fixed()),
          zeroCouponPillars({1 * Years, 2 * Years, 3 * Years, 5 * Years}), zeroCouponQuotes({0.06, 0.04, 0.03, 0.02}),
          cpiFixings({{Date(1, May, 2022), 98.}, {Date(1, June, 2022), 100.}, {Date(1, July, 2022), 104.}}),
          obsLag(2, Months){};
};