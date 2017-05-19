/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file oiccbasisswaphelper.hpp
    \brief Overnight Indexed Cross Currency Basis Swap helpers
    \ingroup termstructures
*/

#ifndef quantlib_oiccbasisswap_helper_hpp
#define quantlib_oiccbasisswap_helper_hpp

#include <ql/instruments/overnightindexedswap.hpp>
#include <ql/termstructures/yield/ratehelpers.hpp>
#include <qle/instruments/oiccbasisswap.hpp>

namespace QuantExt {

//! Rate helper for bootstrapping over Overnight Indexed CC Basis Swap Spreads
/*
  The bootstrap affects the receive leg's discount curve only.

      \ingroup termstructures
*/
class OICCBSHelper : public RelativeDateRateHelper {
public:
    OICCBSHelper(Natural settlementDays,
                 const Period& term, // swap maturity
                 const boost::shared_ptr<OvernightIndex>& payIndex, const Period& payTenor,
                 const boost::shared_ptr<OvernightIndex>& recIndex, const Period& recTenor,
                 const Handle<Quote>& spreadQuote, const Handle<YieldTermStructure>& fixedDiscountCurve,
                 bool spreadQuoteOnPayLeg, bool fixedDiscountOnPayLeg);
    //! \name RateHelper interface
    //@{
    Real impliedQuote() const;
    void setTermStructure(YieldTermStructure*);
    //@}
    //! \name inspectors
    //@{
    boost::shared_ptr<OvernightIndexedCrossCcyBasisSwap> swap() const { return swap_; }
    //@}
    //! \name Visitability
    //@{
    void accept(AcyclicVisitor&);
    //@}
protected:
    void initializeDates();

    Natural settlementDays_;
    Period term_;
    boost::shared_ptr<OvernightIndex> payIndex_;
    Period payTenor_;
    boost::shared_ptr<OvernightIndex> recIndex_;
    Period recTenor_;
    Handle<YieldTermStructure> fixedDiscountCurve_;
    bool spreadQuoteOnPayLeg_;
    bool fixedDiscountOnPayLeg_;

    boost::shared_ptr<OvernightIndexedCrossCcyBasisSwap> swap_;
    RelinkableHandle<YieldTermStructure> termStructureHandle_;
};
} // namespace QuantExt

#endif
