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

/*! \file marketdata/todaysmarket.hpp
    \brief An concerte implementation of the Market class that loads todays market and builds the required curves
    \ingroup marketdata
*/

#pragma once

#include <boost/shared_ptr.hpp>
#include <map>
#include <ored/configuration/conventions.hpp>
#include <ored/configuration/curveconfigurations.hpp>
#include <ored/marketdata/curvespec.hpp>
#include <ored/marketdata/loader.hpp>
#include <ored/marketdata/marketimpl.hpp>
#include <ored/marketdata/todaysmarketparameters.hpp>

namespace ore {
namespace data {

class ReferenceDataManager;

// TODO: rename class
//! Today's Market
/*!
  Today's Market differes from MarketImpl in that it actually loads market data
  and builds term structure objects.

  We label this object Today's Market in contrast to the Simulation Market which can
  differ in composition and granularity. The Simulation Market is initialised using a Today's Market
  object.

  Today's market's purpose is t0 pricing, the Simulation Market's purpose is
  pricing under future scenarios.

  \ingroup marketdata
 */
class TodaysMarket : public MarketImpl {
public:
    //! Constructor
    TodaysMarket( //! Valuation date
        const Date& asof,
        //! Description of the market composition
        const TodaysMarketParameters& params,
        //! Market data loader
        const Loader& loader,
        //! Description of curve compositions
        const CurveConfigurations& curveConfigs,
        //! Repository of market conventions
        const Conventions& conventions,
        //! Continue even if build errors occur
        const bool continueOnError = false,
        //! Optional Load Fixings
        bool loadFixings = true,
        //! Optional reference data manager, needed to build fitted bond curves
        const boost::shared_ptr<ReferenceDataManager>& referenceData = nullptr);
};
} // namespace data
} // namespace ore
