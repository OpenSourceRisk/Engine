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

    void computeCurrentExposure();

private:
    boost::shared_ptr<ore::data::Portfolio> portfolio_;
    boost::shared_ptr<NPVCube> cube_;

    std::map<std::string, double> currentExposure_;
};

} // namespace analytics
} // namespace ore
