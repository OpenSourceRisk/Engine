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

/*! \file quotes/exceptionquote.hpp
    \brief throws exception when called
    \ingroup quotes
*/

#ifndef quantext_exceptionquote_hpp
#define quantext_exceptionquote_hpp

#include <ql/quote.hpp>

namespace QuantExt {
using namespace QuantLib;

//! A dummy quote class that throws an exception when value is called.
/*!
    \ingroup quotes
*/
class ExceptionQuote : public Quote, public Observer {
public:
    ExceptionQuote(std::string exception = "you've hit an 'ExceptionQuote'") : exception_(exception) {}
    //! \name Inspectors
    //@{
    Real quote() const {
        QL_FAIL(exception_);
        return 0;
    }
    //@}
    //! \name Quote interface
    //@{
    Real value() const override {
        QL_FAIL(exception_);
        return 0;
    }
    bool isValid() const override {
        QL_FAIL(exception_);
        return false;
    }
    //@}
    //! \name Observer interface
    //@{
    void update() override { QL_FAIL(exception_); };
    //@}
protected:
    std::string exception_;
};
} // namespace QuantExt

#endif
