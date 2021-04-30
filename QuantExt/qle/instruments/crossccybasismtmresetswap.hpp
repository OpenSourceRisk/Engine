/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file crossccybasismtmresetswap.hpp
    \brief Cross currency basis swap instrument with MTM reset
    \ingroup instruments
*/

#ifndef quantext_cross_ccy_basis_mtmreset_swap_hpp
#define quantext_cross_ccy_basis_mtmreset_swap_hpp

#include <ql/indexes/iborindex.hpp>
#include <ql/time/schedule.hpp>

#include <qle/indexes/fxindex.hpp>
#include <qle/instruments/crossccyswap.hpp>

namespace QuantExt {

//! Cross currency basis MtM resettable swap
/*! The foreign leg holds the pay currency cashflows and domestic leg holds
    the receive currency cashflows. The notional resets are applied to the
    domestic leg.

            \ingroup instruments
*/
class CrossCcyBasisMtMResetSwap : public CrossCcySwap {
public:
    class arguments;
    class results;
    //! \name Constructors
    //@{
    /*! First leg holds the pay currency cashflows and the second leg
        holds the receive currency cashflows.
    */
    CrossCcyBasisMtMResetSwap(Real foreignNominal, const Currency& foreignCurrency, const Schedule& foreignSchedule,
                              const boost::shared_ptr<IborIndex>& foreignIndex, Spread foreignSpread,
                              const Currency& domesticCurrency, const Schedule& domesticSchedule,
                              const boost::shared_ptr<IborIndex>& domesticIndex, Spread domesticSpread,
                              const boost::shared_ptr<FxIndex>& fxIdx, bool resetOnFloatLeg = true, bool receiveDomestic = true);
    //@}
    //! \name Instrument interface
    //@{
    void setupArguments(PricingEngine::arguments* args) const;
    void fetchResults(const PricingEngine::results*) const;
    //@}
    //! \name Inspectors
    //@{
    Real foreignNominal() const { return foreignNominal_; }
    const Currency& foreignCurrency() const { return foreignCurrency_; }
    const Schedule& foreignSchedule() const { return foreignSchedule_; }
    const boost::shared_ptr<IborIndex>& foreignIndex() const { return foreignIndex_; }
    Spread foreignSpread() const { return foreignSpread_; }

    const Currency& domesticCurrency() const { return domesticCurrency_; }
    const Schedule& domesticSchedule() const { return domesticSchedule_; }
    const boost::shared_ptr<IborIndex>& domesticIndex() const { return domesticIndex_; }
    Spread domesticSpread() const { return domesticSpread_; }
    //@}

    //! \name Additional interface
    //@{
    Spread fairForeignSpread() const {
        calculate();
        QL_REQUIRE(fairForeignSpread_ != Null<Real>(), "Fair foreign spread is not available");
        return fairForeignSpread_;
    }
    Spread fairDomesticSpread() const {
        calculate();
        QL_REQUIRE(fairDomesticSpread_ != Null<Real>(), "Fair domestic spread is not available");
        return fairDomesticSpread_;
    }
    //@}

protected:
    //! \name Instrument interface
    //@{
    void setupExpired() const;
    //@}

private:
    void initialize();

    Real foreignNominal_;
    Currency foreignCurrency_;
    Schedule foreignSchedule_;
    boost::shared_ptr<IborIndex> foreignIndex_;
    Spread foreignSpread_;

    Currency domesticCurrency_;
    Schedule domesticSchedule_;
    boost::shared_ptr<IborIndex> domesticIndex_;
    Spread domesticSpread_;

    boost::shared_ptr<FxIndex> fxIndex_;
    bool resetOnFloatLeg_;
    bool receiveDomestic_;

    mutable Spread fairForeignSpread_;
    mutable Spread fairDomesticSpread_;
};

//! \ingroup instruments
class CrossCcyBasisMtMResetSwap::arguments : public CrossCcySwap::arguments {
public:
    Spread foreignSpread;
    Spread domesticSpread;
    void validate() const;
};

//! \ingroup instruments
class CrossCcyBasisMtMResetSwap::results : public CrossCcySwap::results {
public:
    Spread fairForeignSpread;
    Spread fairDomesticSpread;
    void reset();
};
} // namespace QuantExt

#endif
