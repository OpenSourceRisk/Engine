/*
 Copyright (C) 2026 AcadiaSoft Inc.
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

/*! \file qle/quotes/bondfuturequote.hpp
    \brief Bond future quote that relies on a bond future index
    \ingroup quotes
*/
#pragma once

#include <qle/indexes/bondindex.hpp>

namespace QuantExt {

//! A quote for a bond future price that takes its value from a bond future index.
class BondFutureQuote : public Quote, public Observer {
public:
    BondFutureQuote(QuantLib::ext::shared_ptr<BondFuturesIndex> index);
    //! \name Quote interface
    //@{
    QuantLib::Real value() const override;
    bool isValid() const override;
    //@}
    void update() override;

private:
    QuantLib::ext::shared_ptr<BondFuturesIndex> index_;
};

}
