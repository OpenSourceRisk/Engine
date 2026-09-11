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

/*! \file orea/simulation/fixingmanager.hpp
 \brief Controls the updating/reset of the QuantLib::IndexManager
 \ingroup simulation
 */
#pragma once

#include <ored/marketdata/market.hpp>
#include <ored/portfolio/portfolio.hpp>

namespace ore {
namespace analytics {
using namespace QuantLib;
using ore::data::Market;
using ore::data::Portfolio;

namespace detail {
struct IndexComparator {
    bool operator()(const QuantLib::ext::shared_ptr<Index>& a, const QuantLib::ext::shared_ptr<Index>& b) const {
        return a->name() < b->name();
    }
};
} // namespace detail

//! Pseudo Fixings Manager
/*!
  A Pseudo Fixing is a future historical fixing. When pricing on T0 but asof T and we require a fixing on t with
  T0 < t < T then the QuantLib pricing engines will look to the IndexManager for a fixing at t.

  When moving between dates and simulation paths then the Fixings can change and should be populated in a path
  consistent manner

  The FixingManager controls this updating and reset of the QuantLib::IndexManager for the required set of fixings

  The Mode controls how required fixings are determined:

  When stepping between simulation dated t_(n-1) and t_(n) and update a fixing t with t_(n-1) < t < t(n) then the fixing
  from t(n) will be

  - backfilled                                                                     if Mode is BackwardFlat
  - set to the projected fixing as seen from t_(n-1),
    assuming a zero volatiliy dynamics for all underlyings                         if Mode is Projected

  Note: The different modes require a different orchestration of the simulation:

  - BackwardFlat: fixing manager must be updated _after_ the simulation market was updated to the new date
  - Projected   : fixing manager must be updated _before_ the simulation market is updated to the new date

  The valuation engine takes care of this difference. If the fixing manager is used manually, the order of
  updates must be taken care of in the user code. The fixing manager will raise an error if the mode is
  not consistent with the orchestration.

  Note: Projected does not work with disabled observability (observation mode 'Disabled').

  \ingroup simulation
*/
class FixingManager {
public:
    enum class Mode { BackwardFlat, Projected };

    explicit FixingManager(Date anchor, Mode mode = Mode::BackwardFlat);
    virtual ~FixingManager();

    //! Initialise the manager with these flows and indices from the given portfolio
    void initialise(const QuantLib::ext::shared_ptr<Portfolio>& portfolio,
                    const QuantLib::ext::shared_ptr<Market>& market,
                    const std::string& configuration = Market::defaultConfiguration);

    Mode mode() const { return mode_; }
    const Date& fixingsEnd() const { return fixingsEnd_; }
    void update(const Date& d);
    void reset();

private:
    using FixingCache = std::map<QuantLib::ext::shared_ptr<Index>, TimeSeries<Real>, detail::IndexComparator>;
    using FixingMap = std::map<QuantLib::ext::shared_ptr<Index>, std::set<Date>, detail::IndexComparator>;

    void applyFixings(const Date& start, const Date& end);
    void applyFixingsBackwardFlat(const Date& start, const Date& end);
    void applyFixingsProjected(const Date& start, const Date& end);

    // inputs
    Date anchor_;
    Mode mode_;

    // calculated from inputs
    FixingMap fixingMap_;
    FixingCache fixingCache_;

    // state
    Date fixingsEnd_;
    bool modifiedFixingHistory_;

};

} // namespace analytics
} // namespace ore
