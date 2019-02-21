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

/*! \file ored/marketdata/yieldvolcurve.hpp
\brief Wrapper class for building yield volatility structures
\ingroup curves
*/

#pragma once

#include <ored/configuration/conventions.hpp>
#include <ored/configuration/curveconfigurations.hpp>
#include <ored/marketdata/curvespec.hpp>
#include <ored/marketdata/loader.hpp>
#include <ql/termstructures/volatility/swaption/swaptionvolmatrix.hpp>


namespace ore {
    namespace data {
        using QuantLib::Date;
        using QuantLib::SwaptionVolatilityMatrix;
        using ore::data::CurveConfigurations;

        //! Wrapper class for building yield volatility structures
        /*!
        \ingroup curves
        */
        class YieldVolCurve {
        public:
            //! \name Constructors
            //@{
            //! Default constructor
            YieldVolCurve() {}
            //! Detailed constructor
            YieldVolCurve(Date asof, YieldVolatilityCurveSpec spec, const Loader& loader,
                const CurveConfigurations& curveConfigs,
                const map<string, boost::shared_ptr<SwapIndex>>& requiredSwapIndices =
                map<string, boost::shared_ptr<SwapIndex>>());
            //@}

            //! \name Inspectors
            //@{
            const YieldVolatilityCurveSpec& spec() const { return spec_; }

            const boost::shared_ptr<SwaptionVolatilityStructure>& volTermStructure() { return vol_; }
            //@}
        private:
            YieldVolatilityCurveSpec spec_;
            boost::shared_ptr<SwaptionVolatilityStructure> vol_;
        };
    } // namespace data
} // namespace ore
