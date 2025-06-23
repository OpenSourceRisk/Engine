#include "exposures.hpp"
#include <ored/portfolio/trade.hpp>
#include <ored/portfolio/envelope.hpp>
#include <iostream>

namespace ore {
namespace analytics {

Exposures::Exposures(const boost::shared_ptr<ore::data::Portfolio>& portfolio,
                     const boost::shared_ptr<NPVCube>& cube)
    : portfolio_(portfolio), cube_(cube) {}

void Exposures::computeCurrentExposure() {
  for (const auto& tradePair : portfolio_->trades()) {
    const auto& trade = tradePair.second;
    const std::string tradeId = trade->id();
    const std::string nettingSetId = trade->envelope().nettingSetId();

    // Get NPV from cube at first date (0), first sample (0), depth (0)
    const std::vector<QuantLib::Date>& dates = cube_->dates();
    QuantLib::Date date = dates[0];
    double npv = cube_->get(tradeId, date, 0, 0);

    std::cout << "Trade ID: " << tradeId
              << ", Netting Set: " << nettingSetId
              << ", NPV: " << npv << std::endl;

    currentExposure_[nettingSetId] += std::max(npv, 0.0);
  }
}

} // namespace analytics
} // namespace ore
