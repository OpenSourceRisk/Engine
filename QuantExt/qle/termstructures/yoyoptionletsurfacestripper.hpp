#include <ql/experimental/inflation/yoycapfloortermpricesurface.hpp>
#include <ql/termstructures/volatility/inflation/yoyinflationoptionletvolatilitystructure.hpp>

namespace QuantExt {

class YoYOptionletSurfaceStripper {
public:
    QuantLib::ext::shared_ptr<QuantLib::YoYOptionletVolatilitySurface>
    operator()(const QuantLib::ext::shared_ptr<QuantLib::YoYCapFloorTermPriceSurface>& priceSurface,
               const QuantLib::ext::shared_ptr<QuantLib::YoYInflationIndex>& index,
               const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve, double accuracy,
               double globalAccuracy, size_t maxAttemps, double maxFactor, double minFactor, bool dontThrow,
               size_t dontThrowSteps) const;
};
} // namespace QuantExt