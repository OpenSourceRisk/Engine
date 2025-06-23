#include "exposures.hpp"
#include <orea/aggregation/nettingsetmanager.hpp>
#include <iostream>
#include <algorithm>
#include <cmath>

namespace ore {
namespace analytics {

Exposures::Exposures(const boost::shared_ptr<Portfolio>& portfolio,
                     const boost::shared_ptr<NPVCube>& cube)
    : portfolio_(portfolio), cube_(cube) {}

void Exposures::computeCurrentExposure() {
    std::cout << "[Exposures] Computing Current Exposure (CE) at t=0..." << std::endl;

    // At t=0, get NPVs by counterparty/netting set from NPVCube layer 0
    // Use NettingSetManager to group trades per counterparty/netting set

    auto nettingSetManager = boost::make_shared<NettingSetManager>(portfolio_);
    currentExposure_.clear();

    const Size nNettingSets = nettingSetManager->nettingSetCount();
    for (Size i = 0; i < nNettingSets; ++i) {
        auto nettingSetId = nettingSetManager->nettingSetId(i);

        Real sumNpv = 0.0;
        auto trades = nettingSetManager->trades(i);
        for (auto& trade : trades) {
            // NPV at time 0 (initial date), path=0, scenario=0
            Real npv = cube_->npv(trade->id(), 0, 0, 0);
            sumNpv += npv;
        }

        currentExposure_[nettingSetId] = std::max(sumNpv, 0.0);
        std::cout << "NettingSet " << nettingSetId << " CE = " << currentExposure_[nettingSetId] << std::endl;
    }
}

void Exposures::computeExpectedExposure() {
    std::cout << "[Exposures] Computing Expected Exposure (EE) over time..." << std::endl;

    auto nettingSetManager = boost::make_shared<NettingSetManager>(portfolio_);
    expectedExposure_.clear();

    const Size nNettingSets = nettingSetManager->nettingSetCount();
    const Size nDates = cube_->samples();
    const Size nPaths = cube_->paths();

    for (Size i = 0; i < nNettingSets; ++i) {
        auto nettingSetId = nettingSetManager->nettingSetId(i);
        std::vector<Real> eeProfile(nDates, 0.0);

        auto trades = nettingSetManager->trades(i);

        for (Size t = 0; t < nDates; ++t) {
            Real sumExposure = 0.0;

            // average positive exposure over all paths and scenarios for this date
            // Here assuming scenarios=1 for simplicity, adapt if needed

            for (Size p = 0; p < nPaths; ++p) {
                Real pathSum = 0.0;
                for (auto& trade : trades) {
                    Real npv = cube_->npv(trade->id(), t, p, 0);
                    pathSum += npv;
                }
                sumExposure += std::max(pathSum, 0.0);
            }

            eeProfile[t] = sumExposure / nPaths;
        }

        expectedExposure_[nettingSetId] = eeProfile;
    }
}

Real Exposures::computeEPE() const {
    std::cout << "[Exposures] Computing Expected Positive Exposure (EPE)..." << std::endl;

    Real totalEPE = 0.0;
    Size count = 0;

    for (auto& kv : expectedExposure_) {
        const auto& eeProfile = kv.second;

        // Simple average over time points for now, weighted average can be added later
        Real sum = 0.0;
        for (auto e : eeProfile)
            sum += e;

        if (!eeProfile.empty()) {
            totalEPE += sum / eeProfile.size();
            ++count;
        }
    }

    return count > 0 ? totalEPE / count : 0.0;
}

void Exposures::computePotentialFutureExposure(Real quantile) {
    std::cout << "[Exposures] Computing Potential Future Exposure (PFE) at quantile " << quantile << " ..." << std::endl;

    auto nettingSetManager = boost::make_shared<NettingSetManager>(portfolio_);
    potentialFutureExposure_.clear();

    const Size nNettingSets = nettingSetManager->nettingSetCount();
    const Size nDates = cube_->samples();
    const Size nPaths = cube_->paths();

    for (Size i = 0; i < nNettingSets; ++i) {
        auto nettingSetId = nettingSetManager->nettingSetId(i);
        std::vector<Real> pfeProfile(nDates, 0.0);

        auto trades = nettingSetManager->trades(i);

        for (Size t = 0; t < nDates; ++t) {
            std::vector<Real> exposuresAtT;

            for (Size p = 0; p < nPaths; ++p) {
                Real pathSum = 0.0;
                for (auto& trade : trades) {
                    Real npv = cube_->npv(trade->id(), t, p, 0);
                    pathSum += npv;
                }
                Real positiveExposure = std::max(pathSum, 0.0);
                exposuresAtT.push_back(positiveExposure);
            }

            // Compute the quantile of exposuresAtT
            std::sort(exposuresAtT.begin(), exposuresAtT.end());
            Size index = static_cast<Size>(quantile * (exposuresAtT.size() - 1));
            pfeProfile[t] = exposuresAtT[index];
        }

        potentialFutureExposure_[nettingSetId] = pfeProfile;
    }
}

} // namespace analytics
} // namespace ore
