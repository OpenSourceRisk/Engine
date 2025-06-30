#pragma once

#include <ored/portfolio/portfolio.hpp>
#include <orea/cube/npvcube.hpp>

#include <map>
#include <string>
#include <vector>

namespace ore {
namespace analytics {

class Exposures {
public:
    Exposures(const boost::shared_ptr<ore::data::Portfolio>& portfolio,
              const boost::shared_ptr<NPVCube>& cube);

    void computeCE();
    void computeEPE();
    void computePFE();

    const std::map<std::string, QuantLib::Real>& epe() const;

private:
    boost::shared_ptr<ore::data::Portfolio> portfolio_;
    boost::shared_ptr<NPVCube> cube_;

    std::map<std::string, double> currentExposure_;
    std::map<std::string, std::vector<QuantLib::Real>> expectedExposure_;
    std::map<std::string, QuantLib::Real> epe_;
    std::map<std::string, std::vector<QuantLib::Real>> pfe_;
};

} // namespace analytics
} // namespace ore
