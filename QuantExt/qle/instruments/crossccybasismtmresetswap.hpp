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

/*! \file crossccybasisswap.hpp
    \brief Cross currency basis swap instrument

        \ingroup instruments
*/

#ifndef quantext_cross_ccy_basis_mtmreset_swap_hpp
#define quantext_cross_ccy_basis_mtmreset_swap_hpp

#include <ql/indexes/iborindex.hpp>
#include <ql/time/schedule.hpp>

#include <qle/instruments/crossccyswap.hpp>
#include <qle/indexes/fxindex.hpp>

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
    CrossCcyBasisMtMResetSwap(Real fgnNominal, const Currency& fgnCurrency, const Schedule& fgnSchedule,
                      const boost::shared_ptr<IborIndex>& fgnIndex, Spread fgnSpread,
                      const Currency& domCurrency, const Schedule& domSchedule,
                      const boost::shared_ptr<IborIndex>& domIndex, Spread domSpread, 
                      const boost::shared_ptr<FxIndex>& fxIdx, bool invertFxIdx = false);
    //@}
    //! \name Instrument interface
    //@{
    void setupArguments(PricingEngine::arguments* args) const;
    void fetchResults(const PricingEngine::results*) const;
    //@}
    //! \name Inspectors
    //@{
    Real fgnNominal() const { return fgnNominal_; }
    const Currency& fgnCurrency() const { return fgnCurrency_; }
    const Schedule& fgnSchedule() const { return fgnSchedule_; }
    const boost::shared_ptr<IborIndex>& fgnIndex() const { return fgnIndex_; }
    Spread fgnSpread() const { return fgnSpread_; }

    const Currency& domCurrency() const { return domCurrency_; }
    const Schedule& domSchedule() const { return domSchedule_; }
    const boost::shared_ptr<IborIndex>& domIndex() const { return domIndex_; }
    Spread domSpread() const { return domSpread_; }
    //@}

    //! \name Additional interface
    //@{
    Spread fairFgnSpread() const {
        calculate();
        QL_REQUIRE(fairFgnSpread_ != Null<Real>(), "Fair foreign spread is not available");
        return fairFgnSpread_;
    }
    Spread fairDomSpread() const {
        calculate();
        QL_REQUIRE(fairDomSpread_ != Null<Real>(), "Fair domestic spread is not available");
        return fairDomSpread_;
    }
    //@}

protected:
    //! \name Instrument interface
    //@{
    void setupExpired() const;
    //@}

private:
    void initialize();

    Real fgnNominal_;
    Currency fgnCurrency_;
    Schedule fgnSchedule_;
    boost::shared_ptr<IborIndex> fgnIndex_;
    Spread fgnSpread_;

    Currency domCurrency_;
    Schedule domSchedule_;
    boost::shared_ptr<IborIndex> domIndex_;
    Spread domSpread_;

    boost::shared_ptr<FxIndex> fxIndex_;
    bool invertFxIndex_;

    mutable Spread fairFgnSpread_;
    mutable Spread fairDomSpread_;
};

//! \ingroup instruments
class CrossCcyBasisMtMResetSwap::arguments : public CrossCcySwap::arguments {
public:
    Spread fgnSpread;
    Spread domSpread;
    void validate() const;
};

//! \ingroup instruments
class CrossCcyBasisMtMResetSwap::results : public CrossCcySwap::results {
public:
    Spread fairFgnSpread;
    Spread fairDomSpread;
    void reset();
};
} // namespace QuantExt

#endif
