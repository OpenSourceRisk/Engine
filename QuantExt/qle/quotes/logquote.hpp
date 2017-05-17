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

/*! \file quotes/logquote.hpp
    \brief stores log of quote for log-linear interpolation
    \ingroup quotes
*/

#ifndef quantext_logquote_hpp
#define quantext_logquote_hpp

#include <ql/quote.hpp>

using namespace QuantLib;

namespace QuantExt {

//! Class for storing logs of quotes for log-linear interpolation.
/*! \test the correctness of the returned values is tested by
        checking them against the log of the returned values of q_

    \ingroup quotes
*/
class LogQuote : public Quote, public Observer {
public:
    LogQuote(Handle<Quote> q) : q_(q) {
        registerWith(q);
        update();
    }
    //! \name Inspectors
    //@{
    Real quote() const;
    //@}
    //! \name Quote interface
    //@{
    Real value() const;
    bool isValid() const;
    //@}
    //! \name Observer interface
    //@{
    void update();
    //@}
protected:
    Handle<Quote> q_;
    Real logValue_;
};
} // namespace QuantExt

#endif
