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

/*! \file qle/models/yoyinflationmodeltermstructure.hpp
    \brief year-on-year inflation term structure implied by a cross asset model
    \ingroup models
*/

#ifndef quantext_yoy_inflation_model_term_structure_hpp
#define quantext_yoy_inflation_model_term_structure_hpp

#include <qle/models/crossassetmodel.hpp>
#include <ql/termstructures/inflationtermstructure.hpp>

namespace QuantExt {

/*! Base class for cross asset model implied year on year inflation term structures.
    
    The termstructure has the reference date of the model's term structure at construction, but you can vary this as 
    well as the state. Note that this term structure does not implement the full YoYInflationTermStructure interface. 
    It is questionable whether it should derive from YoYInflationTermStructure at all.
    
    \ingroup models
*/

class YoYInflationModelTermStructure : public QuantLib::YoYInflationTermStructure {
public:
    /*! Constructor taking the cross asset model, \p model, and the index of the relevant inflation component within 
        the model, \p index.
    */
    YoYInflationModelTermStructure(const QuantLib::ext::shared_ptr<CrossAssetModel>& model, QuantLib::Size index, bool indexIsInterpolated);

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

    /*! Hides the YoYInflationTermStructure::yoyRate method. The parameters \p forceLinearInterpolation and 
        \p extrapolate are ignored.
    */
    QuantLib::Real yoyRate(const QuantLib::Date& d, const QuantLib::Period& obsLag = -1 * QuantLib::Days,
        bool forceLinearInterpolation = false, bool extrapolate = false) const;

    /*! Return the year-on-year rates for the maturities associated with \p dates. If an \p obsLag is explicitly 
        provided and not set to <code>-1 * QuantLib::Days</code>, it is used as the observation lag. Otherwise, the 
        term structure's observation lag is used.
    */
    virtual std::map<QuantLib::Date, QuantLib::Real> yoyRates(const std::vector<QuantLib::Date>& dates,
        const QuantLib::Period& obsLag = -1 * QuantLib::Days) const = 0;

protected:
    QuantLib::ext::shared_ptr<CrossAssetModel> model_;
    QuantLib::Size index_;
    bool indexIsInterpolated_;
    // Hides referenceDate_ in TermStructure.
    QuantLib::Date referenceDate_;
    QuantLib::Time relativeTime_;
    QuantLib::Array state_;

    //! \name YoYInflationTermStructure interface
    //@{
    //! This cannot be called. The implementation is set to throw an exception.
    QuantLib::Real yoyRateImpl(QuantLib::Time t) const override;
    //@}

    /*! Override this method to perform checks on the state variable array when the \c state and \c move methods are 
        called.
    */
    virtual void checkState() const {}
};

}

#endif
