#include "probdefault.hpp"
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/utilities/dataparsers.hpp>
#include <stdexcept>
#include <ql/settings.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

namespace ore {
namespace analytics {

ProbabilityOfDefault::ProbabilityOfDefault(
    const std::map<std::string, QuantLib::Real>& ratingPDs,
    const std::map<std::string, std::vector<std::pair<QuantLib::Date, QuantLib::Real>>>& cdsSpreads)
    : ratingPDs_(ratingPDs), cdsSpreads_(cdsSpreads) {}

QuantLib::Real ProbabilityOfDefault::fromRating(const std::string& rating) const {
    auto it = ratingPDs_.find(rating);
    if (it == ratingPDs_.end())
        throw std::runtime_error("Unknown credit rating: " + rating);
    return it->second;
}

QuantLib::Real ProbabilityOfDefault::fromCDS(const std::string& counterparty, QuantLib::Date horizon) const {
    auto it = cdsSpreads_.find(counterparty);
    if (it == cdsSpreads_.end())
        throw std::runtime_error("No CDS spreads available for counterparty: " + counterparty);

    const auto& curve = it->second;
    if (curve.empty())
        throw std::runtime_error("Empty CDS spread curve for counterparty: " + counterparty);

    // Extract times and spreads
    std::vector<QuantLib::Time> times;
    std::vector<QuantLib::Real> spreads;

    QuantLib::Date today = QuantLib::Settings::instance().evaluationDate();
    for (const auto& point : curve) {
        times.push_back(QuantLib::Actual365Fixed().yearFraction(today, point.first));
        spreads.push_back(point.second);
    }

    QuantLib::LinearInterpolation interp(times.begin(), times.end(), spreads.begin());

    QuantLib::Time t = QuantLib::Actual365Fixed().yearFraction(today, horizon);
    QuantLib::Real spread = interp(t); // Interpolated flat spread

    // Very rough PD approximation: PD ≈ 1 - exp(-spread × time / (1 - R))
    QuantLib::Real recovery = 0.4;
    return 1.0 - std::exp(-spread * t / (1.0 - recovery));
}

} // namespace analytics
} // namespace ore
