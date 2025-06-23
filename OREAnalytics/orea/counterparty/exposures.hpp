#pragma once

#include <orea/engine/portfolio.hpp>
#include <orea/cube/npvcube.hpp>

#include <map>
#include <string>
#include <vector>

namespace ore {
namespace analytics {

class Exposures {
public:
  Exposures(const boost::shared_ptr<Portfolio>& portfolio,
            const boost::shared_ptr<NPVCube>& cube);

  // Calculate Current Exposure (CE) as max(MtM, 0) at time 0
  void computeCurrentExposure();

  // Calculate Expected Exposure (EE) as average positive exposure at each time
  //  step
  void computeExpectedExposure();

  // Calculate Expected Positive Exposure (EPE) as time-averaged EE
  Real computeEPE() const;

  // Calculate Positive Future Exposure (PFE) at given quantile (e.g. 90%)
  void computePotentialFutureExposure(Real quantile);

private:
  boost::shared_ptr<Portfolio> portfolio_;
  boost::shared_ptr<NPVCube> cube_;

  // Storage of exposures by counterparty (or netting set key)
  std::map<std::string, Real> currentExposure_; // at t=0
  std::map<std::string, std::vector<Real>> expectedExposure_; // EE(t)
  std::map<std::string, std::vector<Real>> potentialFutureExposure_; // PFE(t)
};

}
}
