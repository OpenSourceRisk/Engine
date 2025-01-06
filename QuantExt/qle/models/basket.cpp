/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of ORE, a free-software/open-source library
 for transparent pricing and risk analysis - http://opensourcerisk.org

 ORE is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program.
 The license is also available online at <http://opensourcerisk.org>

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

#include <qle/models/basket.hpp>
//#include <ql/experimental/credit/basket.hpp>
#include <ql/experimental/credit/loss.hpp>
#include <ql/time/daycounters/actualactual.hpp>
//#include <ql/experimental/credit/defaultlossmodel.hpp>
#include <boost/make_shared.hpp>
#include <qle/models/defaultlossmodel.hpp>

#include <qle/instruments/indexcreditdefaultswap.hpp>
#include <qle/pricingengines/midpointindexcdsengine.hpp>
#include <ql/math/solvers1d/brent.hpp>
#include <qle/utilities/creditcurves.hpp>
#include <ql/quotes/compositequote.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <qle/termstructures/spreadedsurvivalprobabilitytermstructure.hpp>
#include <numeric>

using namespace std;

namespace QuantExt {

IndexConstituentDefaultCurveCalibration::CalibrationResults IndexConstituentDefaultCurveCalibration::calibratedCurves(
    const std::vector<double>& remainingNotionals,
    const std::vector<Handle<DefaultProbabilityTermStructure>>& creditCurves,
    const std::vector<double>& recoveryRates) const {
    QuantLib::ext::shared_ptr<SimpleQuote> calibrationFactor = ext::make_shared<SimpleQuote>(1.0);
    CalibrationResults results;
    results.curves = creditCurves;
    results.calibrationFactor = 1;
    try {
        // Make CDS and pricing Engine and target NPV
        double totalNotional = std::accumulate(remainingNotionals.begin(), remainingNotionals.end(), 0.0);
        auto maturity = cdsMaturity(indexStartDate_, indexTenor_, DateGeneration::Rule::CDS2015);
        Schedule cdsSchedule(indexStartDate_, maturity, 3 * Months, WeekendsOnly(), Following, Unadjusted,
                             DateGeneration::Rule::CDS2015, false);

        auto indexCDS = QuantLib::ext::make_shared<QuantExt::IndexCreditDefaultSwap>(
            Protection::Buyer, totalNotional, remainingNotionals, 0.0, indexSpread_, cdsSchedule, Following,
            Actual360(false), true, CreditDefaultSwap::atDefault, Date(), Date(), QuantLib::ext::shared_ptr<Claim>(),
            Actual360(true), true);

        std::vector<Handle<DefaultProbabilityTermStructure>> calibratedCurves;
        for (const auto& orgCurve : creditCurves) {
            calibratedCurves.push_back(buildShiftedCurves(orgCurve, calibrationFactor));
        }

        double target = targetNpv(indexCDS);
        auto cdsPricingEngineUnderlyingCurves = QuantLib::ext::make_shared<QuantExt::MidPointIndexCdsEngine>(
            calibratedCurves, recoveryRates, discountCurve_);

        indexCDS->setPricingEngine(cdsPricingEngineUnderlyingCurves);

        auto targetFunction = [&](const double& factor) {
            calibrationFactor->setValue(factor);
            return (target - indexCDS->NPV());
        };

        Brent solver;
        double adjustmentFactor = solver.solve(targetFunction, 1e-8, 0.5, 0.001, 2);
        calibrationFactor->setValue(adjustmentFactor);
        results.calibrationFactor = adjustmentFactor;
        results.curves = calibratedCurves;
        results.success = true;
        results.marketNpv = target;
        results.impliedNpv = targetFunction(results.calibrationFactor);
        results.cdsMaturity = indexCDS->maturity();

    } catch (const std::exception& e) {
        results.success = false;
        results.curves = creditCurves;
        results.calibrationFactor = 1;
        results.errorMessage = e.what();
    }
    return results;
}

double IndexConstituentDefaultCurveCalibration::targetNpv(const ext::shared_ptr<Instrument>& indexCDS) const {
    auto indexPricingEngine = QuantLib::ext::make_shared<QuantExt::MidPointIndexCdsEngine>(
        indexCurve_, indexRecoveryRate_->value(), discountCurve_);
    indexCDS->setPricingEngine(indexPricingEngine);
    return indexCDS->NPV();
}

QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure> IndexConstituentDefaultCurveCalibration::buildShiftedCurves(
    const QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>& curve,
    const QuantLib::ext::shared_ptr<SimpleQuote>& calibrationFactor) const {
    if (calibrationFactor == nullptr) {
        return curve;
    }
    auto curveTimes = getCreditCurveTimes(curve);
    if (curveTimes.size() < 2) {
        return curve;
    } else {
        std::vector<Handle<Quote>> spreads;
        for (size_t timeIdx = 0; timeIdx < curveTimes.size(); ++timeIdx) {
            auto sp = curve->survivalProbability(curveTimes[timeIdx]);
            auto compQuote = QuantLib::ext::make_shared<CompositeQuote<std::function<double(double, double)>>>(
                Handle<Quote>(calibrationFactor), Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(sp)),
                [](const double q1, const double q2) -> double { return std::exp(-(1 - q1) * std::log(q2)); });
            spreads.push_back(Handle<Quote>(compQuote));
        }
        Handle<DefaultProbabilityTermStructure> targetCurve = Handle<DefaultProbabilityTermStructure>(
            QuantLib::ext::make_shared<SpreadedSurvivalProbabilityTermStructure>(curve, curveTimes, spreads));
        if (curve->allowsExtrapolation()) {
            targetCurve->enableExtrapolation();
        }
        return targetCurve;
    }
}

Basket::Basket(const Date& refDate, const vector<string>& names, const vector<Real>& notionals,
               const QuantLib::ext::shared_ptr<Pool> pool, Real attachment, Real detachment,
               const QuantLib::ext::shared_ptr<Claim>& claim,
               const QuantLib::ext::shared_ptr<IndexConstituentDefaultCurveCalibration>& calibration,
               const std::vector<double>& recoveryRates)
    : notionals_(notionals), pool_(pool), claim_(claim), attachmentRatio_(attachment), detachmentRatio_(detachment),
      basketNotional_(0.0), attachmentAmount_(0.0), detachmentAmount_(0.0), trancheNotional_(0.0), refDate_(refDate),
      calibration_(calibration), recoveryRates_(recoveryRates) {
    QL_REQUIRE(!notionals_.empty(), "notionals empty");
    QL_REQUIRE(attachmentRatio_ >= 0 && attachmentRatio_ <= detachmentRatio_ &&
                   (detachmentRatio_ < 1 || close_enough(detachmentRatio_, 1.0)),
               "invalid attachment/detachment ratio");

    QL_REQUIRE(pool_, "Empty pool pointer.");
    QL_REQUIRE(notionals_.size() == pool_->size(), "unmatched data entry sizes in basket, "
                                                       << notionals_.size() << " notionals, " << pool_->size()
                                                       << " pool size");

    // registrations relevant to the loss status, not to the expected
    // loss values; those are through models.
    registerWith(Settings::instance().evaluationDate());
    registerWith(claim_);
    vector<DefaultProbKey> defKeys = defaultKeys();
    for (Size j = 0; j < size(); j++)
        registerWith(pool_->get(pool_->names()[j]).defaultProbability(defKeys[j]));
    if(calibration_){
        QL_REQUIRE(!calibration_->indexCurve().empty(), "Can not use calibration without index curve");
        QL_REQUIRE(!calibration_->discountCurve().empty(), "Can not use calibration without discount curve");
        registerWith(calibration_->indexCurve());
        //registerWith(calibration_->discountCurve());
    }
}

/*\todo Alternatively send a relinkable handle so it can be changed from
the outside. In that case reconsider the observability chain.
*/
void Basket::setLossModel(const QuantLib::ext::shared_ptr<DefaultLossModel>& lossModel) {
    if (lossModel_)
        unregisterWith(lossModel_);
    lossModel_ = lossModel;
    if (lossModel_) {
        // recovery quotes, defaults(once Issuer is observable)etc might
        //  trigger us:
        registerWith(lossModel_);
    }
    LazyObject::update(); //<- just set calc=false
}

void Basket::performCalculations() const {
    basketNotional_ = attachmentAmount_ = detachmentAmount_ = 0.0;
    for (Size i = 0; i < notionals_.size(); i++) {
        basketNotional_ += notionals_[i];
        attachmentAmount_ += notionals_[i] * attachmentRatio_;
        detachmentAmount_ += notionals_[i] * detachmentRatio_;
    }
    trancheNotional_ = detachmentAmount_ - attachmentAmount_;
    // Calculations for status
    computeBasket(); // or we might be called from an statistic member
                     // without being intialized yet (first called)
    QL_REQUIRE(lossModel_, "Basket has no default loss model assigned.");

    /* The model must notify us if the another basket calls it for
    reasignment. The basket works as an argument to the deafult loss models
    so, even if the models dont cache anything, they will be using the wrong
    defautl TS. \todo: This has a possible optimization: the basket
    incorporates trancheability and many models do their compuations
    independently of that (some do but do it inefficiently when asked for
    two tranches on the same basket; e,g, recursive model) so it might be
    more efficient sending the pool only; however the modtionals and other
    basket info are still used.*/
    lossModel_->setBasket(const_cast<Basket*>(this));
}
/*
vector<Real> Basket::probabilities(const Date& d) const {
    calculate();
    vector<Real> prob(size());
    vector<DefaultProbKey> defKeys = defaultKeys();
    for (Size j = 0; j < size(); j++)
        prob[j] = pool_->get(pool_->names()[j]).defaultProbability(defKeys[j])->defaultProbability(d);
    return prob;
}
*/

Real Basket::cumulatedLoss(const Date& endDate) const {
    calculate();
    QL_REQUIRE(endDate >= refDate_, "cumulatedLoss: Target date " << io::iso_date(endDate)
                                                                  << " lies before basket inception "
                                                                  << io::iso_date(refDate_));
    Real loss = 0.0;
    for (Size i = 0; i < size(); i++) {
        QuantLib::ext::shared_ptr<DefaultEvent> credEvent =
            pool_->get(pool_->names()[i]).defaultedBetween(refDate_, endDate, pool_->defaultKeys()[i]);
        if (credEvent) {
            /* \todo If the event has not settled one would need to
            introduce some model recovery rate (independently of a loss
            model) This remains to be done.
            */
            if (credEvent->hasSettled())
                loss += claim_->amount(credEvent->date(),
                                       // notionals_[i],
                                       exposure(pool_->names()[i], credEvent->date()),
                                       credEvent->settlement().recoveryRate(pool_->defaultKeys()[i].seniority()));
        }
    }
    return loss;
}

Real Basket::settledLoss(const Date& endDate) const {
    calculate();
    QL_REQUIRE(endDate >= refDate_, "settledLoss: Target date " << io::iso_date(endDate)
                                                                << " lies before basket inception "
                                                                << io::iso_date(refDate_));

    Real loss = 0.0;
    for (Size i = 0; i < size(); i++) {
        QuantLib::ext::shared_ptr<DefaultEvent> credEvent =
            pool_->get(pool_->names()[i]).defaultedBetween(refDate_, endDate, pool_->defaultKeys()[i]);
        if (credEvent) {
            if (credEvent->hasSettled()) {
                loss += claim_->amount(credEvent->date(),
                                       // notionals_[i],
                                       exposure(pool_->names()[i], credEvent->date()),
                                       // NOtice I am requesting an exposure in the past...
                                       /* also the seniority does not belong to the
                                       counterparty anymore but to the position.....*/
                                       credEvent->settlement().recoveryRate(pool_->defaultKeys()[i].seniority()));
            }
        }
    }
    return loss;
}

Real Basket::remainingNotional() const {
    calculate();
    return evalDateRemainingNot_;
}

std::vector<Size> Basket::liveList(const Date& endDate) const {
    calculate();
    std::vector<Size> calcBufferLiveList;
    for (Size i = 0; i < size(); i++)
        if (!pool_->get(pool_->names()[i]).defaultedBetween(refDate_, endDate, pool_->defaultKeys()[i]))
            calcBufferLiveList.push_back(i);

    return calcBufferLiveList;
}

Real Basket::remainingNotional(const Date& endDate) const {
    calculate();
    Real notional = 0;
    vector<DefaultProbKey> defKeys = defaultKeys();
    for (Size i = 0; i < size(); i++) {
        if (!pool_->get(pool_->names()[i]).defaultedBetween(refDate_, endDate, defKeys[i]))
            notional += notionals_[i];
    }
    return notional;
}

vector<Real> Basket::remainingNotionals(const Date& endDate) const {
    calculate();
    QL_REQUIRE(endDate >= refDate_, "remainingNotionals: Target date " << io::iso_date(endDate)
                                                                       << " lies before basket inception "
                                                                       << io::iso_date(refDate_));

    std::vector<Real> calcBufferNotionals;
    const std::vector<Size>& alive = liveList(endDate);
    for (Size i = 0; i < alive.size(); i++)
        calcBufferNotionals.push_back(exposure(pool_->names()[i], endDate)); // some better way to trim it?
    return calcBufferNotionals;
}

std::vector<Probability> Basket::remainingProbabilities(const Date& d) const {
    calculate();
    QL_REQUIRE(d >= refDate_, "remainingProbabilities: Target date "
                                  << io::iso_date(d) << " lies before basket inception " << io::iso_date(refDate_));
    vector<Real> prob;
    const vector<double> remainingNtls = this->remainingNotionals(d);
    const std::vector<Size>& alive = liveList();
    vector<Handle<DefaultProbabilityTermStructure>> curves;
    for (Size i = 0; i < alive.size(); i++)
        curves.push_back(pool_->get(pool_->names()[i]).defaultProbability(pool_->defaultKeys()[i]));
    if(calibration_ != nullptr){
        std::cout << "Calibrate consitutent curves";
        QL_REQUIRE(recoveryRates_.size() == remainingNtls.size(), "Mismatch between recovery rates and ");
        auto res = calibration_->calibratedCurves(remainingNtls, curves, recoveryRates_);
        if(res.success){
            std::cout << res.cdsMaturity << " " << std::fixed << std::setprecision(6) << res.calibrationFactor << " " << res.marketNpv << " "
                      << res.impliedNpv << " " << res.marketNpv - res.impliedNpv << std::endl;
            curves = res.curves;
        }
    }
    for (const auto& curve : curves) {
        prob.push_back(curve->defaultProbability(d, true));
    }
    return prob;
}

/* It is supossed to return the addition of ALL notionals from the
requested ctpty......*/
Real Basket::exposure(const std::string& name, const Date& d) const {
    calculate();
    //'this->names_' contains duplicates, contrary to 'pool->names'
    std::vector<std::string>::const_iterator match = std::find(pool_->names().begin(), pool_->names().end(), name);
    QL_REQUIRE(match != pool_->names().end(), "Name not in basket.");
    Real totalNotional = 0.;
    do {
        totalNotional +=
            // NOT IMPLEMENTED YET:
            // positions_[std::distance(names_.begin(), match)]->expectedExposure(d);
            notionals_[std::distance(pool_->names().begin(), match)];
        ++match;
        match = std::find(match, pool_->names().end(), name);
    } while (match != pool_->names().end());

    return totalNotional;
    // Size position = std::distance(poolNames.begin(),
    //    std::find(poolNames.begin(), poolNames.end(), name));
    // QL_REQUIRE(position < pool_->size(), "Name not in pool list");

    // return positions_[position]->expectedExposure(d);
}

std::vector<std::string> Basket::remainingNames(const Date& endDate) const {
    calculate();
    // maybe return zero directly instead?:
    QL_REQUIRE(endDate >= refDate_, "remainingNames: Target date " << io::iso_date(endDate)
                                                                   << " lies before basket inception "
                                                                   << io::iso_date(refDate_));

    const std::vector<Size>& alive = liveList(endDate);
    std::vector<std::string> calcBufferNames;
    for (Size i = 0; i < alive.size(); i++)
        calcBufferNames.push_back(pool_->names()[alive[i]]);
    return calcBufferNames;
}

vector<DefaultProbKey> Basket::remainingDefaultKeys(const Date& endDate) const {
    calculate();
    QL_REQUIRE(endDate >= refDate_, "remainingDefaultKeys: Target date " << io::iso_date(endDate)
                                                                         << " lies before basket inception "
                                                                         << io::iso_date(refDate_));

    const std::vector<Size>& alive = liveList(endDate);
    vector<DefaultProbKey> defKeys;
    for (Size i = 0; i < alive.size(); i++)
        defKeys.push_back(pool_->defaultKeys()[alive[i]]);
    return defKeys;
}

Size Basket::remainingSize() const {
    calculate();
    return evalDateLiveList_.size();
}

/* computed on the inception values, notice the positions might have
amortized or changed in value and the total outstanding notional might
differ from the inception one.*/
Real Basket::remainingDetachmentAmount(const Date& endDate) const {
    calculate();
    return detachmentAmount_;
}

Real Basket::remainingAttachmentAmount(const Date& endDate) const {
    calculate();
    // maybe return zero directly instead?:
    QL_REQUIRE(endDate >= refDate_, "remainingAttchementAmount: Target date " << io::iso_date(endDate) << " lies before basket inception " << io::iso_date(refDate_));
    Real loss = settledLoss(endDate);
    return std::min(detachmentAmount_, attachmentAmount_ + std::max(0.0, loss - attachmentAmount_));
}

Probability Basket::probOverLoss(const Date& d, Real lossFraction) const {
    // convert initial basket fraction to remaining basket fraction
    calculate();
    // if eaten up all the tranche the prob of losing any amount is 1
    //  (we have already lost it)
    if (evalDateRemainingNot_ == 0.)
        return 1.;

    // Turn to live (remaining) tranche units to feed into the model request
    Real xPtfl = attachmentAmount_ + (detachmentAmount_ - attachmentAmount_) * lossFraction;
    Real xPrim = (xPtfl - evalDateAttachAmount_) / (detachmentAmount_ - evalDateAttachAmount_);
    // in live tranche fractional units
    // if the level falls within realized losses the prob is 1.
    if (xPtfl < 0.)
        return 1.;

    return lossModel_->probOverLoss(d, xPrim);
}

Real Basket::percentile(const Date& d, Probability prob) const {
    calculate();
    return lossModel_->percentile(d, prob);

    // Real percLiveFract = lossModel_->percentile(d, prob);
    // return (percLiveFract * (detachmentAmount_ - evalDateAttachAmount_) + attachmentAmount_ - evalDateAttachAmount_)
    // /
    //       (detachmentAmount_ - attachmentAmount_);
}

// RL: additional flag
Real Basket::expectedTrancheLoss(const Date& d, Real recoveryRate) const {
    calculate();
    return cumulatedLoss() + lossModel_->expectedTrancheLoss(d, recoveryRate);
}

std::vector<Real> Basket::splitVaRLevel(const Date& date, Real loss) const {
    calculate();
    return lossModel_->splitVaRLevel(date, loss);
}

Real Basket::expectedShortfall(const Date& d, Probability prob) const {
    calculate();
    return lossModel_->expectedShortfall(d, prob);
}

std::map<Real, Probability> Basket::lossDistribution(const Date& d) const {
    calculate();
    return lossModel_->lossDistribution(d);
}

std::vector<Probability> Basket::probsBeingNthEvent(Size n, const Date& d) const {
    Size alreadyDefaulted = pool_->size() - remainingNames().size();
    if (alreadyDefaulted >= n)
        return std::vector<Probability>(remainingNames().size(), 0.);

    calculate();
    return lossModel_->probsBeingNthEvent(n - alreadyDefaulted, d);
}

Real Basket::defaultCorrelation(const Date& d, Size iName, Size jName) const {
    calculate();
    return lossModel_->defaultCorrelation(d, iName, jName);
}

/*! Returns the probaility of having a given or larger number of
defaults in the basket portfolio at a given time.
*/
Probability Basket::probAtLeastNEvents(Size n, const Date& d) const {
    calculate();
    return lossModel_->probAtLeastNEvents(n, d);
}

Real Basket::recoveryRate(const Date& d, Size iName) const {
    calculate();
    return lossModel_->expectedRecovery(d, iName, pool_->defaultKeys()[iName]);
}

Real Basket::correlation() const {
    calculate();
    if (lossModel_)
        return lossModel_->correlation();
    else
        return Null<Real>();
}

} // namespace QuantExt
