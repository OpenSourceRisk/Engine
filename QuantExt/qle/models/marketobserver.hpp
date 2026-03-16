/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file qle/models/marketobserver.hpp
    \brief helper class for model builders that observes market ts
    \ingroup models
*/

#pragma once

#include <ql/patterns/observable.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Observer class for Model Builders
/*!
  This class holds all observables of a builder, except special ones like vol surfaces
  that should be handled separately in the builders to determine whether
  a recalibration of the model is required.

  \ingroup models
*/
class MarketObserver : public Observer, public Observable {
public:
    MarketObserver() : updated_(true){};

    //! Add an observable
    void addObservable(QuantLib::ext::shared_ptr<Observable> observable);
    //! Observer interface
    void update() override;
    //! Returns true if has been updated, reset updated flag if required
    bool hasUpdated(const bool reset);

private:
    //! Flag to indicate if updated
    bool updated_;
};

// implementation

inline void MarketObserver::addObservable(QuantLib::ext::shared_ptr<Observable> observable) {
    registerWith(observable);
    updated_ = true;
}

inline void MarketObserver::update() {
    updated_ = true;
    notifyObservers();
};

inline bool MarketObserver::hasUpdated(const bool reset) {
    if (!reset)
        return updated_;
    else {
        bool upd = updated_;
        updated_ = false;
        return upd;
    }
}

} // namespace QuantExt
