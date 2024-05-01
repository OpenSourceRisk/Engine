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

#ifndef quantext_defaultlossmodel_hpp
#define quantext_defaultlossmodel_hpp

#include <ql/experimental/credit/defaultprobabilitykey.hpp>
#include <ql/instruments/claim.hpp>
#include <qle/models/basket.hpp>
//#include <ql/experimental/credit/basket.hpp>

#include <ql/utilities/null_deleter.hpp>

/* Intended to replace LossDistribution in
    ql/experimental/credit/lossdistribution, not sure its covering all the
    functionality (see mthod below)
*/

namespace QuantExt {
using namespace QuantLib;

/*! Default loss model interface definition.
Allows communication between the basket and specific algorithms. Intended to
hold any kind of portfolio joint loss, latent models, top-down,....

An inconvenience of this design as opposed to the full arguments/results
is that when pricing several derivatives instruments on the same basket
not all the pricing engines would point to the same loss model; thus when
pricing a set of such instruments there might be some switching on the
basket loss models, which might require recalculations (of the basket) or
not depending on the pricing order.
*/
class DefaultLossModel : public Observable { // joint-? basket?-defaultLoss
                                             /* Protection together with frienship to avoid the need of checking the
                                             basket-argument pointer integrity. It is the responsibility of the basket
                                             now; our only caller.
                                             */
    // RL: QuantExt version
    friend class QuantExt::Basket;

protected:
    // argument basket:
    // RL: QuantExt version
    mutable RelinkableHandle<QuantExt::Basket> basket_;

    DefaultLossModel() {}
    //! \name Statistics
    //@{
    /* Non mandatory implementations, fails if client is not providing what
    requested. */

    /* Default implementation using the expectedLoss(Date) method.
      Typically this method is called repeatedly with the same
      date parameter which makes it innefficient. */
    // RL: additional flag
    virtual Real expectedTrancheLoss(const Date& d, Real recoveryRate = Null<Real>()) const {
        QL_FAIL("expectedTrancheLoss Not implemented for this model.");
    }
    /*! Probability of the tranche losing the same or more than the
        fractional amount given.

        The passed lossFraction is a fraction of losses over the
        tranche notional (not the portfolio).
    */
    virtual Probability probOverLoss(const Date& d, Real lossFraction) const {
        QL_FAIL("probOverLoss Not implemented for this model.");
    }
    //! Value at Risk given a default loss percentile.
    virtual Real percentile(const Date& d, Real percentile) const {
        QL_FAIL("percentile Not implemented for this model.");
    }
    //! Expected shortfall given a default loss percentile.
    virtual Real expectedShortfall(const Date& d, Real percentile) const {
        QL_FAIL("eSF Not implemented for this model.");
    }
    //! Associated VaR fraction to each counterparty.
    virtual std::vector<Real> splitVaRLevel(const Date& d, Real loss) const {
        QL_FAIL("splitVaRLevel Not implemented for this model.");
    }
    //! Associated ESF fraction to each counterparty.
    virtual std::vector<Real> splitESFLevel(const Date& d, Real loss) const {
        QL_FAIL("splitESFLevel Not implemented for this model.");
    }

    // \todo Add splits by instrument position.

    //! Full loss distribution.
    virtual std::map<Real, Probability> lossDistribution(const Date&) const {
        QL_FAIL("lossDistribution Not implemented for this model.");
    }
    //! Probability density of a given loss fraction of the basket notional.
    virtual Real densityTrancheLoss(const Date& d, Real lossFraction) const {
        QL_FAIL("densityTrancheLoss Not implemented for this model.");
    }
    /*! Probabilities for each of the (remaining) basket elements in the
    pool to have defaulted by time d and at the same time be the Nth
    defaulting name to default in the basket. This method is oriented to
    default order dependent portfolio pricing (e.g. NTDs)
        The probabilities ordering in the vector coincides with the
        pool order.
    */
    virtual std::vector<Probability> probsBeingNthEvent(Size n, const Date& d) const {
        QL_FAIL("probsBeingNthEvent Not implemented for this model.");
    }
    //! Pearsons' default probability correlation.
    virtual Real defaultCorrelation(const Date& d, Size iName, Size jName) const {
        QL_FAIL("defaultCorrelation Not implemented for this model.");
    }
    /*! Returns the probaility of having a given or larger number of
    defaults in the basket portfolio at a given time.
    */
    virtual Probability probAtLeastNEvents(Size n, const Date& d) const {
        QL_FAIL("probAtLeastNEvents Not implemented for this model.");
    }
    /*! Expected RR for name conditinal to default by that date.
     */
    virtual Real expectedRecovery(const Date&, Size iName, const DefaultProbKey&) const {
        QL_FAIL("expected recovery Not implemented for this model.");
    }
    //@}

    /*! Return single correlation number for one factor models. If not implemented or not applicable,
        returns a \c Null<Real>().
    */
    virtual QuantLib::Real correlation() const {
        return QuantLib::Null<QuantLib::Real>();
    }

    /*! Send a reference to the basket to allow the model to read the
    problem arguments (contained in the basket)
    */
private: // can only be called from Basket
    // RL: QuantExt version
    void setBasket(QuantExt::Basket* bskt) {
        /* After this; if the model modifies its internal status/caches (if
        any) it should notify the  prior basket to recognise that basket is
        not in a calculated=true state. Since we dont know at this level if
        the model keeps caches it is the children responsibility. Typically
        this is done at the first call to calculate to the loss model, there
        it notifies the basket. The old basket is still registered with us
        until the basket takes in a new model....
        ..alternatively both old basket and model could be forced reset here
        */
        // RL: QuantExt version
        basket_.linkTo(QuantLib::ext::shared_ptr<QuantExt::Basket>(bskt, null_deleter()), false);
        resetModel(); // or rename to setBasketImpl(...)
    }
    // the call order matters, which is the reason for the parent to be the
    //   sole caller.
    //! Concrete models do now any updates/inits they need on basket reset
    virtual void resetModel() = 0;
};

} // namespace QuantExt

#endif
