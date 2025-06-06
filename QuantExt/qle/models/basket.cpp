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

#include <boost/make_shared.hpp>
#include <ql/experimental/credit/loss.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <qle/models/basket.hpp>
#include <qle/models/defaultlossmodel.hpp>

using namespace std;

namespace QuantExt {

Basket::Basket(const Date& refDate, const vector<string>& names, const vector<Real>& notionals,
               const QuantLib::ext::shared_ptr<Pool> pool, Real attachment, Real detachment,
               const QuantLib::ext::shared_ptr<Claim>& claim)
    : notionals_(notionals), pool_(pool), claim_(claim), attachmentRatio_(attachment), detachmentRatio_(detachment),
      basketNotional_(0.0), attachmentAmount_(0.0), detachmentAmount_(0.0), trancheNotional_(0.0), refDate_(refDate) {
    QL_REQUIRE(!notionals_.empty(), "notionals empty");
    QL_REQUIRE(attachmentRatio_ >= 0 && attachmentRatio_ <= detachmentRatio_ &&
                   (detachmentRatio_ < 1 || close_enough(detachmentRatio_, 1.0)),
               "invalid attachment/detachment ratio");

    QL_REQUIRE(pool_, "Empty pool pointer.");
    QL_REQUIRE(notionals_.size() == pool_->size(), "unmatched data entry sizes in basket, "
                                                       << notionals_.size() << " notionals, " << pool_->size()
                                                       << " pool size");
    basketNotional_ = attachmentAmount_ = detachmentAmount_ = 0.0;
    for (Size i = 0; i < notionals_.size(); i++) {
        basketNotional_ += notionals_[i];
        attachmentAmount_ += notionals_[i] * attachmentRatio_;
        detachmentAmount_ += notionals_[i] * detachmentRatio_;
    }
    trancheNotional_ = detachmentAmount_ - attachmentAmount_;
    // registrations relevant to the loss status, not to the expected
    // loss values; those are through models.
    registerWith(Settings::instance().evaluationDate());
    registerWith(claim_);
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

vector<Real> Basket::probabilities(const Date& d) const {
    calculate();
    vector<Real> prob(size());
    vector<DefaultProbKey> defKeys = defaultKeys();
    for (Size j = 0; j < size(); j++)
        prob[j] = pool_->get(pool_->names()[j]).defaultProbability(defKeys[j])->defaultProbability(d);
    return prob;
}

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

    const std::vector<Size>& alive = liveList();
    for (Size i = 0; i < alive.size(); i++)
        prob.push_back(
            pool_->get(pool_->names()[i]).defaultProbability(pool_->defaultKeys()[i])->defaultProbability(d, true));
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
    QL_REQUIRE(endDate >= refDate_, "remainingAttchementAmount: Target date " << io::iso_date(endDate)
                                                                              << " lies before basket inception "
                                                                              << io::iso_date(refDate_));
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
