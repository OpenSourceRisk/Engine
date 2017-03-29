#pragma once

#include <ored/portfolio/portfolio.hpp>

using QuantLib::Date;
using QuantLib::Real;
using QuantLib::Index;
using ore::data::Portfolio;

namespace ore {
namespace analytics {

//! Pseudo Fixings Manager
/*!
  A Pseudo Fixing is a future historical fixing. When pricing on T0 but asof T and we require a fixing on t with
  T0 < t < T then the QuantLib pricing engines will look to the IndexManager for a fixing at t.

  When moving between dates and simulation paths then the Fixings can change and should be populated in a path
  consistant manager

  The FixingManager controls this updating and reset of the QuantLib::IndexManager for the required set of fixings

  When stepping between simulation dated t_(n-1) and t_(n) and update a fixing t with t_(n-1) < t < t(n) than the fixing
  from t(n) will be backfilled. There is currently no interpolation of fixings.

  \ingroup simulation
 */
class FixingManager {
public:
    FixingManager(Date today) : today_(today), fixingsEnd_(today) {}

    //! Initialise the manager with these flows and indices from the given portfolio
    void initialise(const boost::shared_ptr<Portfolio>& portfolio);

    //! Update fixings to date d
    void update(Date d);

    //! Reset fixings to t0 (today)
    void reset();

private:
    void applyFixings(Date start, Date end);

    Date today_, fixingsEnd_;
    std::map<std::string, TimeSeries<Real>> fixingCache_;
    std::vector<boost::shared_ptr<Index>> indices_;
    std::map<std::string, std::vector<Date>> fixingMap_;
};
}
}
