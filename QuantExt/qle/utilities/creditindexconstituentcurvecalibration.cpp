#include <numeric>
#include <ql/time/daycounters/actual360.hpp>
#include <qle/instruments/indexcreditdefaultswap.hpp>
#include <qle/pricingengines/midpointindexcdsengine.hpp>
#include <qle/utilities/creditcurves.hpp>
#include <qle/utilities/creditindexconstituentcurvecalibration.hpp>
#include <qle/termstructures/spreadedsurvivalprobabilitytermstructure.hpp>
#include <ql/quotes/compositequote.hpp>
#include <ql/math/solvers1d/brent.hpp>

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

    if (indexTenors_.empty() || indexCurves_.empty() || indexRecoveryRates_.empty()) {
        results.errorMessage = "Index CDS data missing";
        return results;
    }

    if (indexTenors_.size() != indexCurves_.size() || indexTenors_.size() != indexRecoveryRates_.size()) {
        results.errorMessage = "Index CDS data mismatch";
        return results;
    }

    try {
        // Make CDS and pricing Engine and target NPV
        double totalNotional = std::accumulate(remainingNotionals.begin(), remainingNotionals.end(), 0.0);
        std::vector<ext::shared_ptr<IndexCreditDefaultSwap>> indexCDSs;
        std::vector<QuantLib::ext::shared_ptr<SimpleQuote>> calibrationFactors;
        std::vector<QuantLib::Date> maturities;
        for (const auto& indexTenor : indexTenors_) {
            auto maturity = cdsMaturity(indexStartDate_, indexTenor, DateGeneration::Rule::CDS2015);
            if (maturity > Settings::instance().evaluationDate()) {
                Schedule cdsSchedule(indexStartDate_, maturity, 3 * Months, WeekendsOnly(), Unadjusted, Unadjusted,
                                     DateGeneration::Rule::CDS2015, false);

                auto indexCDS = QuantLib::ext::make_shared<QuantExt::IndexCreditDefaultSwap>(
                    Protection::Buyer, totalNotional, remainingNotionals, 0.0, indexSpread_, cdsSchedule, Following,
                    Actual360(false), true, CreditDefaultSwap::atDefault, Date(), Date(),
                    QuantLib::ext::shared_ptr<Claim>(), Actual360(true), true);
                indexCDSs.push_back(indexCDS);
                calibrationFactors.push_back(ext::make_shared<SimpleQuote>(1.0));
                maturities.push_back(cdsSchedule.dates().back());
            }
        }
        if (indexCDSs.empty()) {
            results.errorMessage = "No index CDS to calibrate";
            return results;
        }

        std::vector<Handle<DefaultProbabilityTermStructure>> calibratedCurves;
        for (const auto& orgCurve : creditCurves) {
            calibratedCurves.push_back(buildShiftedCurve(orgCurve, maturities, calibrationFactors));
        }

        // Iterrate bootstrap calibration
        for (size_t i = 0; i < indexCDSs.size(); ++i) {
            auto indexCDS = indexCDSs[i];
            auto calibrationFactor = calibrationFactors[i];
            double target = targetNpv(indexCDS, i);
            auto cdsPricingEngineUnderlyingCurves = QuantLib::ext::make_shared<QuantExt::MidPointIndexCdsEngine>(
                calibratedCurves, recoveryRates, discountCurve_);
            indexCDS->setPricingEngine(cdsPricingEngineUnderlyingCurves);
            auto targetFunction = [&calibrationFactor, &target, &indexCDS](const double& factor) {
                calibrationFactor->setValue(factor);
                return (target - indexCDS->NPV());
            };
            Brent solver;
            double adjustmentFactor = solver.solve(targetFunction, 1e-8, 0.5, 0.001, 2);

            calibrationFactor->setValue(adjustmentFactor);
            results.calibrationFactor.push_back(adjustmentFactor);
            results.curves = calibratedCurves;
            results.success = true;
            results.marketNpv.push_back(target);
            results.impliedNpv.push_back(indexCDS->NPV());
            results.cdsMaturity.push_back(indexCDS->maturity());
        }
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

double CreditIndexConstituentCurveCalibration::targetNpv(const ext::shared_ptr<Instrument>& indexCDS, size_t i) const {
    auto indexPricingEngine = QuantLib::ext::make_shared<QuantExt::MidPointIndexCdsEngine>(
        indexCurves_[i], indexRecoveryRates_[i]->value(), discountCurve_);
    indexCDS->setPricingEngine(indexPricingEngine);
    return indexCDS->NPV();
}

QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure> CreditIndexConstituentCurveCalibration::buildShiftedCurve(
    const QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>& curve,
    const std::vector<QuantLib::Date>& maturities,
    const std::vector<QuantLib::ext::shared_ptr<SimpleQuote>>& calibrationFactors) const {
    auto curveTimes = getCreditCurveTimes(curve);
    std::vector<double> indexCdsMaturityTimes;
    for (const auto& d : maturities) {
        const auto maturityTime = curve->timeFromReference(d);
        QL_REQUIRE(maturityTime > 0, "Maturity time must be positive");
        curveTimes.push_back(curve->timeFromReference(d - 1 * Days));
        curveTimes.push_back(maturityTime);
        curveTimes.push_back(curve->timeFromReference(d + 1 * Days));
        indexCdsMaturityTimes.push_back(maturityTime);
    }
    std::sort(curveTimes.begin(), curveTimes.end());
    auto it = std::unique(curveTimes.begin(), curveTimes.end());
    curveTimes.erase(it, curveTimes.end());
    if (curveTimes.size() < 2) {
        return curve;
    } else {
        std::vector<Handle<Quote>> spreads;
        size_t i = 0;
        for (size_t timeIdx = 0; timeIdx < curveTimes.size(); ++timeIdx) {
            auto time = curveTimes[timeIdx];
            while (indexCdsMaturityTimes[i] < time && (i + 1) < indexCdsMaturityTimes.size()) {
                i++;
            }

            auto sp = curve->survivalProbability(curveTimes[timeIdx], true);

            auto compQuote = QuantLib::ext::make_shared<CompositeQuote<std::function<double(double, double)>>>(
                Handle<Quote>(calibrationFactors[i]), Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(sp)),
                [](const double q1, const double q2) -> double { return std::exp(-(1 - q1) * std::log(q2)); });
            spreads.push_back(Handle<Quote>(compQuote));
        }
        Handle<DefaultProbabilityTermStructure> targetCurve = Handle<DefaultProbabilityTermStructure>(
            QuantLib::ext::make_shared<SpreadedSurvivalProbabilityTermStructure>(
                curve, curveTimes, spreads));
        if (curve->allowsExtrapolation()) {
            targetCurve->enableExtrapolation();
        }
        return targetCurve;
    }
}

} // namespace QuantExt