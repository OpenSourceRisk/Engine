#include "lossgivendefault.hpp"
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/settings.hpp>
#include <stdexcept>
#include <algorithm>

namespace ore {
namespace analytics {

using namespace QuantLib;

LossGivenDefault::LossGivenDefault(
    const std::map<std::string, double>& ratingLGD,
    const std::map<std::string, std::vector<std::pair<Date, double>>>& marketLGD)
    : ratingLGD_(ratingLGD), marketLGD_(marketLGD) {}

double LossGivenDefault::fromRating(const std::string& creditRating) const {
    auto it = ratingLGD_.find(creditRating);
    if (it == ratingLGD_.end()) {
        throw std::runtime_error("LGD for rating '" + creditRating + "' not found.");
    }
    return it->second;
}

double LossGivenDefault::fromMarket(const std::string& counterpartyId, const Date& date) const {
    auto it = marketLGD_.find(counterpartyId);
    if (it == marketLGD_.end() || it->second.empty()) {
        throw std::runtime_error("No market LGD data for counterparty: " + counterpartyId);
    }

    const auto& points = it->second;
    Date today = Settings::instance().evaluationDate();
    std::vector<Time> times;
    std::vector<double> lgds;

    for (const auto& pt : points) {
        times.push_back(Actual365Fixed().yearFraction(today, pt.first));
        lgds.push_back(pt.second);
    }

    Time t = Actual365Fixed().yearFraction(today, date);
    auto interpolator = LinearInterpolation(times.begin(), times.end(), lgds.begin());

    return interpolator(t);
}

} // namespace analytics
} // namespace ore
