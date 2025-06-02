#include <ql/termstructures/defaulttermstructure.hpp>
#include <vector>

namespace QuantExt {

//! get the times of a credit curve, in case of spreaded curves it returns the time grid of the reference curve
// and not the the timegrid of the spreads
// returns empty vector on error
std::vector<double> getCreditCurveTimes(const QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>& dpts);

} // namespace QuantExt