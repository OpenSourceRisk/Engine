#pragma once

#include <string>
#include <map>
#include <ql/types.hpp>
#include <ql/time/date.hpp>

namespace ore {
namespace analytics {

class ProbabilityOfDefault {
public:
    // Construct with lookup tables for ratings and CDS curves
    ProbabilityOfDefault(const std::map<std::string, QuantLib::Real>& ratingPDs,
                         const std::map<std::string, std::vector<std::pair<QuantLib::Date, QuantLib::Real>>>& cdsSpreads);

    // Estimate based on external credit rating
    QuantLib::Real fromRating(const std::string& rating) const;

    // Estimate based on market CDS spreads (flat or at a given date horizon)
    QuantLib::Real fromCDS(const std::string& counterparty, QuantLib::Date horizon) const;

private:
    std::map<std::string, QuantLib::Real> ratingPDs_;
    std::map<std::string, std::vector<std::pair<QuantLib::Date, QuantLib::Real>>> cdsSpreads_;
};

} // namespace analytics
} // namespace ore
