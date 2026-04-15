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

/*! \file moneymarketfuture.hpp
    \brief Money Market IR Future
*/

#pragma once

#include <ql/indexes/iborindex.hpp>
#include <ql/instrument.hpp>
#include <ql/quote.hpp>

namespace QuantExt {

/*! Money Market Future */
class MoneyMarketFuture : public QuantLib::Instrument {
public:
    MoneyMarketFuture(QuantLib::ext::shared_ptr<QuantLib::IborIndex> index, const QuantLib::Date& valueDate,
                      QuantLib::Handle<QuantLib::Quote> convexityAdjustment = QuantLib::Handle<QuantLib::Quote>());

    QuantLib::Real convexityAdjustment() const;
    bool isExpired() const override;
    const QuantLib::ext::shared_ptr<QuantLib::IborIndex>& index() const { return index_; }
    QuantLib::Date valueDate() const { return valueDate_; }
    QuantLib::Date maturityDate() const { return maturityDate_; }

private:
    void performCalculations() const override;
    QuantLib::Real rate() const;
    QuantLib::ext::shared_ptr<QuantLib::IborIndex> index_;
    QuantLib::Date valueDate_, maturityDate_;
    QuantLib::Handle<QuantLib::Quote> convexityAdjustment_;
};

} // namespace QuantExt
