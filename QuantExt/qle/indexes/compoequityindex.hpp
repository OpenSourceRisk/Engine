/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file qle/indexes/compoequityindex.hpp
    \brief equity index converting the original equity currency to another currency
    \ingroup indexes
*/

#pragma once

#include <qle/indexes/equityindex.hpp>
#include <qle/indexes/fxindex.hpp>

#include <ql/patterns/lazyobject.hpp>

namespace QuantExt {
using namespace QuantLib;

class CompoEquityIndex : public QuantExt::EquityIndex2, public LazyObject {
public:
    /*! - fxIndex source ccy must be the equity ccy, fxIndex target ccy is the new equity ccy
        - dividends before the divCutoffDate are ignored, this is useful since there have to be
          fixings for the fx index on all dividend dates which might not be available */
    CompoEquityIndex(const QuantLib::ext::shared_ptr<QuantExt::EquityIndex2>& source, const QuantLib::ext::shared_ptr<FxIndex>& fxIndex,
                     const Date& dividendCutoffDate = Date());

    QuantLib::ext::shared_ptr<QuantExt::EquityIndex2> source() const;

    void addDividend(const Dividend& dividend, bool forceOverwrite = false) override;
    const std::set<Dividend>& dividendFixings() const override;
    Real pastFixing(const Date& fixingDate) const override;
    QuantLib::ext::shared_ptr<QuantExt::EquityIndex2> clone(const Handle<Quote> spotQuote, const Handle<YieldTermStructure>& rate,
                                         const Handle<YieldTermStructure>& dividend) const override;

private:
    void performCalculations() const override;

    QuantLib::ext::shared_ptr<QuantExt::EquityIndex2> source_;
    QuantLib::ext::shared_ptr<FxIndex> fxIndex_;
    Date dividendCutoffDate_;

    mutable std::set<Dividend> dividendFixings_;
};

} // namespace QuantExt
