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

/*! \file qle/models/zeroinflationmodeltermstructure.hpp
    \brief zero inflation term structure implied by a cross asset model
    \ingroup models
*/

#ifndef quantext_zero_inflation_model_term_structure_hpp
#define quantext_zero_inflation_model_term_structure_hpp

#include <qle/models/crossassetmodel.hpp>
#include <ql/termstructures/inflationtermstructure.hpp>

namespace QuantExt {

/*! Base class for cross asset model implied zero inflation term structures.
    
    The termstructure has the reference date of the model's term structure at construction, but you can vary this as 
    well as the state. This purely time based variant is mainly here for performance reasons. Note that it does not 
    provide the full term structure interface and does not send notifications on reference time updates.
    
    \ingroup models
*/

class ZeroInflationModelTermStructure : public QuantLib::ZeroInflationTermStructure {
public:
    /*! Constructor taking the cross asset model, \p model, and the index of the relevant inflation component within 
        the model, \p index.
    */
    ZeroInflationModelTermStructure(const QuantLib::ext::shared_ptr<CrossAssetModel>& model, QuantLib::Size index);

    // Deprecated constructor with interpolated index, index is always flat and the coupon is responsible for interpolation
    QL_DEPRECATED
    ZeroInflationModelTermStructure(const QuantLib::ext::shared_ptr<CrossAssetModel>& model, QuantLib::Size index, bool indexIsInterpolated);

    //! \name Observer interface
    //@{
    void update() override;
    //@}

    //! \name TermStructure interface
    //@{
    QuantLib::Date maxDate() const override;
    QuantLib::Time maxTime() const override;
    const QuantLib::Date& referenceDate() const override;
    //@}

    //! \name InflationTermStructure interface
    //@{
    QuantLib::Date baseDate() const override;
    //@}

    //! Set the reference date
    virtual void referenceDate(const QuantLib::Date& d);

    //! Set the current state variables
    void state(const QuantLib::Array& s);

    //! Set the current state and move the reference date to date \p d
    void move(const QuantLib::Date& d, const QuantLib::Array& s);

protected:
    QuantLib::ext::shared_ptr<CrossAssetModel> model_;
    QuantLib::Size index_;
    QL_DEPRECATED bool indexIsInterpolated_;
    // Hides referenceDate_ in TermStructure.
    QuantLib::Date referenceDate_;
    QuantLib::Time relativeTime_;
    QuantLib::Array state_;

    /*! Override this method to perform checks on the state variable array when the \c state and \c move methods are 
        called.
    */
    virtual void checkState() const {}
};

}

#endif
