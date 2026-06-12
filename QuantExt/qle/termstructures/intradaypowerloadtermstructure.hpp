/*
Copyright (C) 2026 AcadiaSoft Inc
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

/*! \file qle/termstructures/intradayshapetermstructure.hpp
    \brief Term structure of intraday shape factors
*/

#pragma once

#include <ql/currency.hpp>
#include <ql/math/comparison.hpp>
#include <ql/quote.hpp>
#include <ql/termstructure.hpp>

namespace QuantExt {

using LoadFactors = std::vector<std::tuple<int,int, double>>;
class IntradayLoadProfile {
public:
    IntradayLoadProfile(const LoadFactors& load,
                        const LoadFactors& loadDST)
        : loadProfile_(std::move(load)), loadProfileDST_(std::move(loadDST)) {}

    const LoadFactors& loadProfile() const { return loadProfile_; }
    const LoadFactors& loadProfileDST() const { return loadProfileDST_; }

    QuantLib::Real totalMWh() const {
        QuantLib::Real total = 0.0;
        for (const auto& [start, end, load] : loadProfile_) {
            total += load * (end - start) / 3600.0;
        }
        for (const auto& [start, end, load] : loadProfileDST_) {
            total += load * (end - start) / 3600.0;
        }
        return total;
    }

    QuantLib::Real totalDeliveryHours() const {
        QuantLib::Real total = 0.0;
        for (const auto& [start, end, load] : loadProfile_) {
            total += (end - start) / 3600.0;
        }
        for (const auto& [start, end, load] : loadProfileDST_) {
            total += (end - start) / 3600.0;
        }
        return total;
    }

private:
    LoadFactors loadProfile_;
    LoadFactors loadProfileDST_;
};

//! Intraday Price term structure
/*! This abstract class defines the interface of concrete
    price term structures which will be derived from this one.

    \ingroup termstructures
*/
class IntradayPowerLoadTermStructure {

public:
    //! \name Constructors
    //@{
    IntradayPowerLoadTermStructure(std::map<QuantLib::Date, QuantLib::ext::shared_ptr<IntradayLoadProfile>> loadingShapes)
        : loadingShapes_(std::move(loadingShapes)) {}
    //@}

    //! \name Find the loading shapes on a given date or the last day before the given date, return nullptr if no date before is found
    //@{
    QuantLib::ext::shared_ptr<IntradayLoadProfile> loadProfile(const QuantLib::Date& d) const {
        auto it = loadingShapes_.upper_bound(d);
        if (it == loadingShapes_.begin()) {
            return nullptr;
        }
        --it;
        return it->second;
    }

private:
    std::map<QuantLib::Date, QuantLib::ext::shared_ptr<IntradayLoadProfile>> loadingShapes_;
};

} // namespace QuantExt