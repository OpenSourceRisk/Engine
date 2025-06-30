/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

/*! \file orea/aggregation/simmhelper.hpp
    \brief Dynamic initial margin helper returning Simple SIMM

    \ingroup dim
*/

#pragma once

#include <orea/cube/npvcube.hpp>
#include <orea/engine/simmsensitivitystoragemanager.hpp>
#include <orea/engine/simpledynamicsimm.hpp>
#include <orea/scenario/aggregationscenariodata.hpp>

#include <ored/portfolio/trade.hpp>
#include <ored/utilities/log.hpp>
#include <ored/marketdata/market.hpp>

#include <qle/methods/irdeltaparconverter.hpp>

#include <ql/math/matrix.hpp>

namespace ore {
namespace analytics {
using namespace ore::data;

using QuantExt::CrossAssetModel;
using QuantLib::Matrix;
using QuantLib::Time;
using QuantExt::IrDeltaParConverter;

//! Helper for cam model implied SImple SIMM calculation
/*! using deltas and vegas stored in the NPV cube */
class SimmHelper {
public:
    /*! Construct the dim helper based on
        - the currencies covered in the simulation
        - the cube which stores the sensitivities on a netting set level
        - the sensitivity storage manager
        Note that we assume that the model has the simulation provides IR/FX deltas and vegas */
    SimmHelper(const std::vector<std::string>& currencies,
	       const QuantLib::ext::shared_ptr<NPVCube>& cube,
	       const QuantLib::ext::shared_ptr<AggregationScenarioData>& marketCube,
	       const QuantLib::ext::shared_ptr<SensitivityStorageManager>& sensitivityStorageManager,
	       const QuantLib::ext::shared_ptr<ore::data::Market>& market);

    /*! Returns the Simple SIMM for
        - a netting set id
        - a date and sample index, if both are null, SIMM computed for the T0 slice of the cube */
    Real initialMargin(const std::string& nettingSetId, const Size dateIndex = Null<Size>(),
                       const Size sampleIndex = Null<Size>(), bool deltaMargin = true, bool vegaMargin = true,
                       bool curvatureMargin = true, bool IR = true, bool FX = true);

    Real deltaMargin() { return deltaMargin_; }
    Real vegaMargin() { return vegaMargin_; }
    Real curvatureMargin() { return curvatureMargin_; }
    Real irDeltaMargin() { return irDeltaMargin_; }
    Real fxDeltaMargin() { return fxDeltaMargin_; }

private:
    Real timeFromReference(const Date& d) const;

    Date referenceDate_;
    DayCounter dc_;
    std::vector<std::string> currencies_;
    QuantLib::ext::shared_ptr<NPVCube> cube_;
    QuantLib::ext::shared_ptr<AggregationScenarioData> marketCube_;
    QuantLib::ext::shared_ptr<SimmSensitivityStorageManager> ssm_;
    QuantLib::ext::shared_ptr<ore::data::Market> market_;
    std::vector<IrDeltaParConverter::InstrumentType> irDeltaInstruments_;
    QuantLib::ext::shared_ptr<NPVCube> imCube_;
    QuantLib::ext::shared_ptr<SimpleDynamicSimm> imCalculator_;
    std::vector<IrDeltaParConverter> irDeltaConverter_;
    Real totalMargin_;
    Real deltaMargin_;
    Real vegaMargin_;
    Real curvatureMargin_;
    Real irDeltaMargin_;
    Real fxDeltaMargin_;
};

} // namespace analytics
} // namespace ore
