#include "wrongwayrisk.hpp"
#include <ql/errors.hpp>
#include <algorithm>
#include <iostream>
#include <numeric>
#include <cmath>
#include <ored/portfolio/trade.hpp>

namespace ore {
namespace analytics {

WrongWayRisk::WrongWayRisk(const boost::shared_ptr<ore::data::Portfolio>& portfolio,
                           const boost::shared_ptr<NPVCube>& cube,
                           const std::map<std::string, std::vector<double>>& creditFactorsPerScenario)
    : portfolio_(portfolio), cube_(cube), creditFactorsPerScenario_(creditFactorsPerScenario) {
      
    correlation_ = computeCorrelationBasedWWR();
    }

std::map<std::string, double> WrongWayRisk::computeCorrelationBasedWWR() {
    std::map<std::string, std::vector<double>> exposurePerNettingSet;

    Size numSamples = cube_->samples();
    const auto& dates = cube_->dates();
    if (dates.empty())
        QL_FAIL("NPVCube has no simulation dates");

    // Use only the final horizon date for WWR
    QuantLib::Date finalDate = dates.back();

    for (const auto& tradePair : portfolio_->trades()) {
        const std::string& tradeId = tradePair.first;
        const std::string& nettingSetId = tradePair.second->envelope().nettingSetId();

        if (exposurePerNettingSet[nettingSetId].empty())
            exposurePerNettingSet[nettingSetId].resize(numSamples, 0.0);

        for (Size s = 0; s < numSamples; ++s) {
            double npv = cube_->get(tradeId, finalDate, s, 0);
            exposurePerNettingSet[nettingSetId][s] += std::max(npv, 0.0); // EE
        }
    }

    std::map<std::string, double> result;

    for (const auto& kv : exposurePerNettingSet) {
        const std::string& nettingSetId = kv.first;
        const std::vector<double>& exposures = kv.second;

        auto it = creditFactorsPerScenario_.find(nettingSetId);
        if (it == creditFactorsPerScenario_.end())
            QL_FAIL("No credit factor data for netting set " << nettingSetId);

        const std::vector<double>& creditFactors = it->second;
        if (creditFactors.size() != exposures.size())
            QL_FAIL("Mismatch in sample size for netting set " << nettingSetId);

        Size n = exposures.size();
        double meanX = std::accumulate(exposures.begin(), exposures.end(), 0.0) / n;
        double meanY = std::accumulate(creditFactors.begin(), creditFactors.end(), 0.0) / n;

        double num = 0.0, denomX = 0.0, denomY = 0.0;
        for (Size i = 0; i < n; ++i) {
            double dx = exposures[i] - meanX;
            double dy = creditFactors[i] - meanY;
            num += dx * dy;
            denomX += dx * dx;
            denomY += dy * dy;
        }

        double corr = (denomX > 0 && denomY > 0) ? num / std::sqrt(denomX * denomY) : 0.0;

        result[nettingSetId] = corr;
    }

    return result;
}

} // namespace analytics
} // namespace ore
