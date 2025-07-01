#pragma once

#include <map>
#include <string>
#include <vector>
#include <ql/time/date.hpp>

namespace ore {
namespace analytics {

class LossGivenDefault {
public:
    // Constructor for static LGD per rating
    LossGivenDefault(const std::map<std::string, double>& ratingLGD,
                     const std::map<std::string, std::vector<std::pair<QuantLib::Date, double>>>& marketLGD = {});

    // Retrieve LGD from rating table
    double fromRating(const std::string& creditRating) const;

    // Interpolate LGD from market data (if available)
    double fromMarket(const std::string& counterpartyId, const QuantLib::Date& date) const;

private:
    std::map<std::string, double> ratingLGD_;
    std::map<std::string, std::vector<std::pair<QuantLib::Date, double>>> marketLGD_;
};

} // namespace analytics
} // namespace ore
