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
    void computePFE(QuantLib::Real quantile);

    // const std::map<std::string, QuantLib::Real>& epe() const;
    const std::map<std::string, std::vector<QuantLib::Real>>& pfe() const { return pfe_; }
    const std::map<std::string, QuantLib::Real>& epe() const { return epe_; }
    const std::map<std::string, double>& ce() const { return ce_; }

private:
    boost::shared_ptr<ore::data::Portfolio> portfolio_;
    boost::shared_ptr<NPVCube> cube_;

    std::map<std::string, double> ce_;
    std::map<std::string, std::vector<QuantLib::Real>> expectedExposure_;
    std::map<std::string, QuantLib::Real> epe_;
    std::map<std::string, std::vector<QuantLib::Real>> pfe_;
};

} // namespace analytics
} // namespace ore
