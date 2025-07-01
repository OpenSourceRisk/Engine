#include "exposures.hpp"
#include <ored/portfolio/trade.hpp>
#include <ored/portfolio/envelope.hpp>
#include <iostream>

namespace ore {
namespace analytics {

Exposures::Exposures(const boost::shared_ptr<ore::data::Portfolio>& portfolio,
                     const boost::shared_ptr<NPVCube>& cube)
    : portfolio_(portfolio), cube_(cube) {}

void Exposures::computeCE() {
  for (const auto& tradePair : portfolio_->trades()) {
    const auto& trade = tradePair.second;
    const std::string tradeId = trade->id();
    const std::string nettingSetId = trade->envelope().nettingSetId();

    // Get NPV from cube at first date (0), first sample (0), depth (0)
    // const std::vector<QuantLib::Date>& dates = cube_->dates();
    // QuantLib::Date date = dates[0];
    double npv = cube_->getT0(tradeId, 0);

    std::cout << "Trade ID: " << tradeId
              << ", Netting Set: " << nettingSetId
              << ", NPV: " << npv << std::endl;

    ce_[nettingSetId] += std::max(npv, 0.0);
  }
}

void Exposures::computeEPE() {
    const std::vector<QuantLib::Date>& dates = cube_->dates();
    Size numSamples = cube_->samples();

    if (dates.empty() || numSamples == 0) {
        QL_FAIL("Cube has no dates or samples.");
    }

    // Reset
    epe_.clear();
    expectedExposure_.clear();

    for (const auto& tradePair : portfolio_->trades()) {
        const std::string& tradeId = tradePair.first;
        const std::string& nettingSetId = tradePair.second->envelope().nettingSetId();

        std::vector<Real> eePerDate;

        expectedExposure_[nettingSetId].resize(dates.size(), 0.0);
        for (Size d = 0; d < dates.size(); ++d) {
            Real sum = 0.0;
            for (Size s = 0; s < numSamples; ++s) {
                try {
                    Real npv = cube_->get(tradeId, dates[d], s, 0); // depth = 0 for base NPV
                    sum += std::max(npv, 0.0);
                } catch (...) {
                    ALOG("Cube missing entry for trade " << tradeId << ", date index " << d << ", sample " << s);
                }
            }
            Real ee = sum / static_cast<Real>(numSamples);
            expectedExposure_[nettingSetId][d] += ee;  // Aggregate by netting set
        }
    }

    // Final EPE: average of expected exposure across all future dates
    for (const auto& kv : expectedExposure_) {
        const std::string& nettingSetId = kv.first;
        const std::vector<Real>& exposures = kv.second;

        Real sum = std::accumulate(exposures.begin(), exposures.end(), 0.0);
        epe_[nettingSetId] = sum / exposures.size();
    }
}

void Exposures::computePFE(QuantLib::Real quantile) {
    const std::vector<QuantLib::Date>& dates = cube_->dates();
    Size numSamples = cube_->samples();

    if (dates.empty() || numSamples == 0) {
        QL_FAIL("Cube has no dates or samples.");
    }

    pfe_.clear();

    // Step 1: For each netting set and date, store one exposure per sample path
    std::map<std::string, std::vector<std::vector<Real>>> exposureSamples;

    for (const auto& tradePair : portfolio_->trades()) {
        const std::string& tradeId = tradePair.first;
        const std::string& nettingSetId = tradePair.second->envelope().nettingSetId();

        // Ensure structure is initialized
        if (exposureSamples[nettingSetId].empty()) {
            exposureSamples[nettingSetId].resize(dates.size(), std::vector<Real>(numSamples, 0.0));
        }

        // Loop over each date and sample path
        for (Size d = 0; d < dates.size(); ++d) {
            for (Size s = 0; s < numSamples; ++s) {
                try {
                    Real npv = cube_->get(tradeId, dates[d], s, 0);
                    exposureSamples[nettingSetId][d][s] += std::max(npv, 0.0);  // Aggregate across trades
                } catch (...) {
                    ALOG("Missing cube value for " << tradeId << " on date index " << d << ", sample " << s);
                }
            }
        }
    }

    // Step 2: For each netting set and date, compute the quantile of the exposures
    for (const auto& kv : exposureSamples) {
        const std::string& nettingSetId = kv.first;
        const auto& samplesByDate = kv.second;

        std::vector<Real> pfeForDates;
        for (const auto& exposures : samplesByDate) {
            if (exposures.empty()) {
                pfeForDates.push_back(0.0);
                continue;
            }

            std::vector<Real> sorted = exposures;
            std::sort(sorted.begin(), sorted.end());

            Size index = static_cast<Size>(std::floor(quantile * sorted.size()));
            if (index >= sorted.size()) index = sorted.size() - 1;

            pfeForDates.push_back(sorted[index]);
        }

        pfe_[nettingSetId] = pfeForDates;
    }

    // Optional: Log last date PFE values
    for (const auto& kv : pfe_) {
        const auto& vec = kv.second;
        ALOG("Netting Set: " << kv.first << ", PFE[" << quantile << "] at final date: " << vec.back());
    }
}

} // namespace analytics
} // namespace ore
