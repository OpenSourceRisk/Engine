#include <numeric>
#include <ql/math/solvers1d/brent.hpp>
#include <ql/quotes/compositequote.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <qle/instruments/indexcreditdefaultswap.hpp>
#include <qle/pricingengines/midpointindexcdsengine.hpp>
#include <qle/termstructures/spreadedsurvivalprobabilitytermstructure.hpp>
#include <qle/utilities/creditcurves.hpp>
#include <qle/utilities/creditindexconstituentcurvecalibration.hpp>

namespace QuantExt {

CreditIndexConstituentCurveCalibration::CalibrationResults CreditIndexConstituentCurveCalibration::calibratedCurves(
    const std::vector<std::string>& names, const std::vector<double>& remainingNotionals,
    const std::vector<Handle<DefaultProbabilityTermStructure>>& creditCurves,
    const std::vector<double>& recoveryRates) const {

    CalibrationResults results;
    results.success = false;
    results.curves = creditCurves;

    if (names.size() != creditCurves.size() || names.size() != recoveryRates.size()) {
        results.errorMessage = "Number of names, credit curves and recovery rates do not match";
        return results;
    }

    try {
        // Make CDS and pricing Engine and target NPV
        double totalNotional = std::accumulate(remainingNotionals.begin(), remainingNotionals.end(), 0.0);
        ext::shared_ptr<IndexCreditDefaultSwap> indexCDS;
        QuantLib::ext::shared_ptr<SimpleQuote> calibrationFactor;

        auto maturity = cdsMaturity(startDate_, indexTerm_, DateGeneration::Rule::CDS2015);
        if (maturity > Settings::instance().evaluationDate()) {
            Schedule cdsSchedule(startDate_, maturity, tenor_, calendar_, convention_, termConvention_, rule_, endOfMonth_);
            indexCDS = QuantLib::ext::make_shared<QuantExt::IndexCreditDefaultSwap>(
                Protection::Buyer, totalNotional, remainingNotionals, 0.0, runningSpread_, cdsSchedule, payConvention_,
                dayCounter_, true, CreditDefaultSwap::atDefault, Date(), Date(),
                QuantLib::ext::shared_ptr<Claim>(), lastPeriodDayCounter_, true, Date(), cashSettlementDays_);
            calibrationFactor = ext::make_shared<SimpleQuote>(1.0);
        }

        if (indexCDS == nullptr) {
            results.errorMessage = "No index CDS to calibrate";
            return results;
        }

        std::vector<Handle<DefaultProbabilityTermStructure>> calibratedCurves;
        for (const auto& orgCurve : creditCurves) {
            calibratedCurves.push_back(buildShiftedCurve(orgCurve, maturity, calibrationFactor));
        }

        double target = targetNpv(indexCDS);
        auto cdsPricingEngineUnderlyingCurves = QuantLib::ext::make_shared<QuantExt::MidPointIndexCdsEngine>(
            calibratedCurves, recoveryRates, discountCurve_);
        indexCDS->setPricingEngine(cdsPricingEngineUnderlyingCurves);
        auto targetFunction = [&calibrationFactor, &target, &indexCDS](const double& factor) {
            calibrationFactor->setValue(factor);
            return (target - indexCDS->NPV());
        };
        Brent solver;
        double adjustmentFactor = solver.solve(targetFunction, 1e-8, 1.0, 0.001, 2);

        calibrationFactor->setValue(adjustmentFactor);
        results.calibrationFactor.push_back(adjustmentFactor);
        results.curves = calibratedCurves;
        results.success = true;
        results.marketNpv.push_back(target);
        results.impliedNpv.push_back(indexCDS->NPV());
        results.cdsMaturity.push_back(indexCDS->maturity());

    } catch (const std::exception& e) {
        results.success = false;
        results.curves = creditCurves;
        results.errorMessage = e.what();
        results.calibrationFactor.clear();
        results.marketNpv.clear();
        results.impliedNpv.clear();
        results.cdsMaturity.clear();
    }
    return results;
}

double CreditIndexConstituentCurveCalibration::targetNpv(const ext::shared_ptr<Instrument>& indexCDS) const {
    auto indexPricingEngine = QuantLib::ext::make_shared<QuantExt::MidPointIndexCdsEngine>(
        indexCurve_, indexRecoveryRate_->value(), discountCurve_);
    indexCDS->setPricingEngine(indexPricingEngine);
    return indexCDS->NPV();
}

QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure> CreditIndexConstituentCurveCalibration::buildShiftedCurve(
    const QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>& curve,
    const QuantLib::Date& maturity,
    const QuantLib::ext::shared_ptr<SimpleQuote>& calibrationFactor) const {
    auto curveTimes = getCreditCurveTimes(curve);
    if (curveTimes.size() <= 2){
        curveTimes.push_back(curve->timeFromReference(cdsMaturity(Settings::instance().evaluationDate(), Period(1, Years), DateGeneration::Rule::CDS2015)));
        curveTimes.push_back(curve->timeFromReference(cdsMaturity(Settings::instance().evaluationDate(), Period(2, Years), DateGeneration::Rule::CDS2015)));
        curveTimes.push_back(curve->timeFromReference(cdsMaturity(Settings::instance().evaluationDate(), Period(3, Years), DateGeneration::Rule::CDS2015)));
        curveTimes.push_back(curve->timeFromReference(cdsMaturity(Settings::instance().evaluationDate(), Period(5, Years), DateGeneration::Rule::CDS2015)));
        curveTimes.push_back(curve->timeFromReference(cdsMaturity(Settings::instance().evaluationDate(), Period(7, Years), DateGeneration::Rule::CDS2015)));
        curveTimes.push_back(curve->timeFromReference(cdsMaturity(Settings::instance().evaluationDate(), Period(10, Years), DateGeneration::Rule::CDS2015)));
    }
    std::sort(curveTimes.begin(), curveTimes.end());
    auto it = std::unique(curveTimes.begin(), curveTimes.end());
    curveTimes.erase(it, curveTimes.end());
    if (curveTimes.size() < 2) {
        return curve;
    } else {
        std::vector<Handle<Quote>> spreads;
        for (size_t timeIdx = 0; timeIdx < curveTimes.size(); ++timeIdx) {
            auto time = curveTimes[timeIdx];
            auto sp = curve->survivalProbability(time, true);
            Handle<Quote> calibrationFactorSpread;
            if (QuantLib::close_enough(sp, 0.0)) {
                calibrationFactorSpread = spreads.empty()
                                              ? Handle<Quote>(QuantLib::ext::make_shared<QuantLib::SimpleQuote>(1.0))
                                              : spreads.back();
            } else {
                calibrationFactorSpread =
                    Handle<Quote>(QuantLib::ext::make_shared<CompositeQuote<std::function<double(double, double)>>>(
                        Handle<Quote>(calibrationFactor),
                        Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(sp)),
                        [](const double q1, const double q2) -> double { return std::exp(-(1 - q1) * std::log(q2)); }));
            }
            spreads.push_back(calibrationFactorSpread);
        }
        Handle<DefaultProbabilityTermStructure> targetCurve = Handle<DefaultProbabilityTermStructure>(
            QuantLib::ext::make_shared<SpreadedSurvivalProbabilityTermStructure>(curve, curveTimes, spreads));
        if (curve->allowsExtrapolation()) {
            targetCurve->enableExtrapolation();
        }
        return targetCurve;
    }
}

} // namespace QuantExt