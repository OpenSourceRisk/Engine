/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file qle/models/jyimpliedzeroinflationtermstructure.hpp
    \brief zero inflation term structure implied by a Jarrow Yildrim (JY) model
    \ingroup models
*/

#ifndef quantext_jy_implied_zero_inflation_term_structure_hpp
#define quantext_jy_implied_zero_inflation_term_structure_hpp

#include <qle/models/crossassetmodel.hpp>
#include <qle/models/zeroinflationmodeltermstructure.hpp>

namespace QuantExt {

/*! Jarrow Yildrim (JY) implied zero inflation term structure
    \ingroup models
*/
class JyImpliedZeroInflationTermStructure : public ZeroInflationModelTermStructure {
public:
    /*! Constructor taking the cross asset model, \p model, and the index of the relevant inflation component within
        the model, \p index.
    */
    JyImpliedZeroInflationTermStructure(const QuantLib::ext::shared_ptr<CrossAssetModel>& model, QuantLib::Size index);

    QL_DEPRECATED
    JyImpliedZeroInflationTermStructure(const QuantLib::ext::shared_ptr<CrossAssetModel>& model, QuantLib::Size index,
        bool indexIsInterpolated);

protected:
    //! \name ZeroInflationTermStructure interface
    //@{
    QuantLib::Real zeroRateImpl(QuantLib::Time t) const override;
    //@}

    //! \name ZeroInflationModelTermStructure interface
    //@{
    void checkState() const override;
    //@}
};

/*! Calculation of inflation growth between two times given the Jarrow Yildrim (JY) real rate state, \p rrState, and 
    the nominal interest rate state, \p irState.
*/
QuantLib::Real inflationGrowth(const QuantLib::ext::shared_ptr<CrossAssetModel>& model, QuantLib::Size index,
    QuantLib::Time S, QuantLib::Time T, QuantLib::Real irState, QuantLib::Real rrState, bool indexIsInterpolated);

}

#endif
