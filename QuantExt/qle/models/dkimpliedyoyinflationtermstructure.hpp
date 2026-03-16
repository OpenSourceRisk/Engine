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

/*! \file qle/models/dkimpliedyoyinflationtermstructure.hpp
    \brief year on year inflation term structure implied by a Dodgson Kainth (DK) model
    \ingroup models
*/

#ifndef quantext_dk_implied_yoy_inflation_term_structure_hpp
#define quantext_dk_implied_yoy_inflation_term_structure_hpp

#include <qle/models/crossassetmodel.hpp>
#include <qle/models/yoyinflationmodeltermstructure.hpp>

namespace QuantExt {

/*! Dodgson Kainth (DK) implied year on year inflation term structure
    \ingroup models
*/
class DkImpliedYoYInflationTermStructure : public YoYInflationModelTermStructure {
public:
    /*! Constructor taking the cross asset model, \p model, and the index of the relevant inflation component within
        the model, \p index.
    */
    DkImpliedYoYInflationTermStructure(const QuantLib::ext::shared_ptr<CrossAssetModel>& model, QuantLib::Size index, bool indexIsInterpolated);

    //! \name YoYInflationModelTermStructure interface
    //@{
    std::map<QuantLib::Date, QuantLib::Real> yoyRates(const std::vector<QuantLib::Date>& dates,
        const QuantLib::Period& obsLag = -1 * QuantLib::Days) const override;
    //@}

protected:
    QuantLib::Real yoySwapletRate(QuantLib::Time S, QuantLib::Time T) const;

    //! \name YoYInflationModelTermStructure interface
    //@{
    void checkState() const override;
    //@}
};

}

#endif
