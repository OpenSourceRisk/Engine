#include <qle/termstructures/yoyoptionletsurfacestripper.hpp>
#include <qle/termstructures/yoyoptionletsolver.hpp>
#include <qle/termstructures/iterativebootstrap.hpp>
#include <qle/termstructures/kinterpolatedyoyoptionletvolatilitysurface.hpp>

namespace QuantExt {

QuantLib::ext::shared_ptr<QuantLib::YoYOptionletVolatilitySurface> YoYOptionletSurfaceStripper::operator()(
    const QuantLib::ext::shared_ptr<QuantLib::YoYCapFloorTermPriceSurface>& priceSurface,
    const QuantLib::ext::shared_ptr<QuantLib::YoYInflationIndex>& index,
    const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve, double accuracy, double globalAccuracy,
    size_t maxAttemps, double maxFactor, double minFactor, bool dontThrow, size_t dontThrowSteps) const {
    QL_REQUIRE(priceSurface != nullptr, "YoYOptionletSurfaceStripper: missing price surface");
    QL_REQUIRE(index != nullptr, "YoYOptionletSurfaceStripper: missing yoy index");
    QL_REQUIRE(!discountCurve.empty(), "YoYOptionletSurfaceStripper: missing discount curve");
    std::unique_ptr<QuantLib::YoYOptionletBaseSolver> firstCapletSolver;

    if (dontThrow) {
        firstCapletSolver =
            std::make_unique<QuantExt::YoYOptionletStripperSolverWithFallBack>(1e-8, 0.3, dontThrowSteps);
    } else {
        firstCapletSolver = std::make_unique<QuantLib::YoYOptionletSolver>();
    }

    auto yoyStripper = QuantLib::ext::make_shared<
        QuantLib::InterpolatedYoYOptionletStripper<QuantLib::Linear, QuantExt::IterativeBootstrap>>(
        std::move(firstCapletSolver),
        QuantExt::IterativeBootstrap<
            InterpolatedYoYOptionletStripper<Linear, QuantExt::IterativeBootstrap>::optionlet_curve>(
            accuracy, globalAccuracy, dontThrow, maxAttemps, maxFactor, minFactor, dontThrowSteps));

    // Create an empty volatility surface to pass to the engine
    QuantLib::ext::shared_ptr<QuantLib::YoYOptionletVolatilitySurface> ovs =
        QuantLib::ext::dynamic_pointer_cast<QuantLib::YoYOptionletVolatilitySurface>(
            QuantLib::ext::make_shared<QuantLib::ConstantYoYOptionletVolatility>(
                0.0, priceSurface->settlementDays(), priceSurface->calendar(), priceSurface->businessDayConvention(),
                priceSurface->dayCounter(), priceSurface->observationLag(), priceSurface->frequency(),
                priceSurface->indexIsInterpolated()));
    QuantLib::Handle<QuantLib::YoYOptionletVolatilitySurface> hovs(ovs);

    // create a yoy Index from the surfaces termstructure
    auto yoyTs = priceSurface->YoYTS();
    QuantLib::ext::shared_ptr<QuantLib::YoYInflationIndex> yoyIndex =
        index->clone(Handle<QuantLib::YoYInflationTermStructure>(yoyTs));

    QuantLib::ext::shared_ptr<QuantLib::YoYInflationBachelierCapFloorEngine> cfEngine =
        QuantLib::ext::make_shared<QuantLib::YoYInflationBachelierCapFloorEngine>(yoyIndex, hovs, discountCurve);

    auto curve =  QuantLib::ext::make_shared<QuantExt::KInterpolatedYoYOptionletVolatilitySurface<Linear>>(
        priceSurface->settlementDays(), priceSurface->calendar(), priceSurface->businessDayConvention(),
        priceSurface->dayCounter(), priceSurface->observationLag(), priceSurface, cfEngine, yoyStripper, 0, Linear(),
        VolatilityType::Normal);
    curve->enableExtrapolation();
    return curve;
}

} // namespace QuantExt