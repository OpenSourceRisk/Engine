#pragma once

#include <ored/portfolio/portfolio.hpp>
#include <orea/cube/npvcube.hpp>
#include <ql/time/date.hpp>
#include <map>
#include <string>
#include <vector>

namespace ore {
namespace analytics {

class WrongWayRisk {
public:
    WrongWayRisk(const boost::shared_ptr<ore::data::Portfolio>& portfolio,
                 const boost::shared_ptr<NPVCube>& cube,
                 const std::map<std::string, std::vector<double>>& creditFactorsPerScenario);

    // Correlation between exposure and credit factor per netting set
    std::map<std::string, double> computeCorrelationBasedWWR();
    const std::map<std::string, double>& correlation() const { return correlation_; }

private:
    boost::shared_ptr<ore::data::Portfolio> portfolio_;
    boost::shared_ptr<NPVCube> cube_;
    std::map<std::string, std::vector<double>> creditFactorsPerScenario_;
    std::map<std::string, double> correlation_;
};

} // namespace analytics
} // namespace ore
