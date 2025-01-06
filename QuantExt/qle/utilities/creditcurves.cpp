#include <ql/math/interpolations/backwardflatinterpolation.hpp>
#include <ql/math/interpolations/loginterpolation.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <qle/termstructures/interpolatedhazardratecurve.hpp>
#include <qle/termstructures/interpolatedsurvivalprobabilitycurve.hpp>
#include <qle/termstructures/multisectiondefaultcurve.hpp>
#include <qle/termstructures/spreadedsurvivalprobabilitytermstructure.hpp>
#include <qle/termstructures/survivalprobabilitycurve.hpp>
namespace QuantExt {

std::vector<Time> getCreditCurveTimes(const QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>& dpts) {
    if (auto c = QuantLib::ext::dynamic_pointer_cast<SpreadedSurvivalProbabilityTermStructure>(*dpts)) {
        return getCreditCurveTimes(c->referenceCurve());
    } else if (auto c = QuantLib::ext::dynamic_pointer_cast<QuantExt::InterpolatedSurvivalProbabilityCurve<LogLinear>>(*dpts)) {
        return c->times();
    } else if (auto c = QuantLib::ext::dynamic_pointer_cast<QuantExt::InterpolatedHazardRateCurve<BackwardFlat>>(*dpts)) {
        return c->times();
    } else if (auto c = QuantLib::ext::dynamic_pointer_cast<QuantExt::SurvivalProbabilityCurve<LogLinear>>(*dpts)) {
        return c->times();
    } else {
        return std::vector<double>();
    }
}

} // namespace QuantExt