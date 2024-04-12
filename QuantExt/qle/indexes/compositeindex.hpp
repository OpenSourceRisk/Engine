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

/*! \file qle/indexes/compositeindex.hpp
    \brief index representing a weighted sum of underlying indices
    \ingroup indexes
*/

#pragma once

#include <qle/indexes/fxindex.hpp>

#include <ql/currency.hpp>
#include <ql/handle.hpp>
#include <ql/index.hpp>

namespace QuantExt {
using namespace QuantLib;

class CompositeIndex : public Index, public Observer {
public:
    /*! fxConversion can be an empty vector or its length should match indices. For components that
      do not require a conversion, a nullptr should be given, otherwise a FxIndex with domestic ccy
      equal to the target currency of the index */
    CompositeIndex(const std::string& name, const std::vector<QuantLib::ext::shared_ptr<QuantLib::Index>>& indices,
                   const std::vector<Real>& weights, const std::vector<QuantLib::ext::shared_ptr<FxIndex>>& fxConversion = {});

    //! Index interface
    std::string name() const override;
    Calendar fixingCalendar() const override;
    bool isValidFixingDate(const Date& fixingDate) const override;
    Real fixing(const Date& fixingDate, bool forecastTodaysFixing = false) const override;
    bool allowsNativeFixings() override { return false; }

    //! Observer interface
    void update() override { notifyObservers(); }

    //! Inspectors
    const std::vector<QuantLib::ext::shared_ptr<QuantLib::Index>>& indices() const { return indices_; }
    const std::vector<Real>& weights() const { return weights_; }
    const std::vector<QuantLib::ext::shared_ptr<FxIndex>>& fxConversion() const { return fxConversion_; }

    /*! Collect dividends from equity underlying indices, apply weighting, fx conversion (if any) and return
      the sum. Notice that the endDate is capped at today, as in EquityIndex::dividendsBetweenDates.
      This only applies to underlying equity indices, for other index types zero dividends are returned */
    Real dividendsBetweenDates(const Date& startDate, const Date& endDate = Date::maxDate()) const;
    std::vector<std::pair<QuantLib::Date, std::string>> dividendFixingDates(const Date& startDate, const Date& endDate = Date::maxDate());

private:
    std::string name_;
    std::vector<QuantLib::ext::shared_ptr<QuantLib::Index>> indices_;
    std::vector<Real> weights_;
    std::vector<QuantLib::ext::shared_ptr<FxIndex>> fxConversion_;
    //
    Calendar fixingCalendar_;
};

} // namespace QuantExt
