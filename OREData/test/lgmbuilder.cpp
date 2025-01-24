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

// clang-format off
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
// clang-format on

#include <ored/scripting/scriptengine.hpp>
#include <ored/scripting/models/localvol.hpp>
#include <ored/scripting/staticanalyser.hpp>
#include <ored/scripting/scriptparser.hpp>
#include <ored/scripting/astprinter.hpp>
#include <ored/scripting/models/blackscholes.hpp>
#include <ored/scripting/models/dummymodel.hpp>

#include <oret/toplevelfixture.hpp>

#include <ored/model/localvolmodelbuilder.hpp>

#include <qle/termstructures/flatcorrelation.hpp>
#include <qle/methods/multipathgeneratorbase.hpp>

#include <ql/exercise.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>
#include <ql/processes/stochasticprocessarray.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/termstructures/volatility/sabr.hpp>
#include <ored/model/lgmbuilder.hpp>
#include <iomanip>

using namespace ore::data;
using namespace QuantLib;
using namespace QuantExt;

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(LgmBuilderTest)

namespace {

    BOOST_AUTO_TEST_CASE(testLgmBuilderCalibrationError) {
        BOOST_TEST_MESSAGE("Testing LGM builder calibration error types ...");

        const QuantLib::ext::shared_ptr<ore::data::Market> market; //TODO fill data        
        const QuantLib::ext::shared_ptr<IrLgmData> data;//TODO fill data
        const std::string& configuration = Market::defaultConfiguration;
        Real bootstrapTolerance = 0.001;
        const bool continueOnError = false; 
        const std::string& referenceCalibrationGrid = "";
        const bool setCalibrationInfo = false; 
        const std::string& id = "unknwon";

        // Absolute Error
        auto error1 = BlackCalibrationHelper::PriceError;
        LgmBuilder lgmb1(market, data, configuration, bootstrapTolerance, continueOnError, referenceCalibrationGrid, setCalibrationInfo, id, error1);
        lgmb1.recalibrate(); // Calibration should be run automatically, just to be sure
        QuantLib::ext::shared_ptr<QuantExt::IrLgm1fParametrization> p1 = lgmb1.parametrization(); // Read calibrated parameters
        p1->printParameters(0.0); // Read calibrated parameters

        // Relative Error
        auto error2 = BlackCalibrationHelper::RelativePriceError;
        LgmBuilder lgmb2(market, data, configuration, bootstrapTolerance, continueOnError, referenceCalibrationGrid, setCalibrationInfo, id, error2);
        lgmb2.recalibrate(); // Calibration should be run automatically, just to be sure
        QuantLib::ext::shared_ptr<QuantExt::IrLgm1fParametrization> p2 = lgmb2.parametrization(); // Read calibrated parameters
        p2->printParameters(0.0); // Read calibrated parameters

        // Comparison
        // TODO
    }

    BOOST_AUTO_TEST_CASE(testCreateSwaptionHelper) {
        BOOST_TEST_MESSAGE("Testing ability of LGM builder to clean degenerate input values  ...");

        const QuantLib::ext::shared_ptr<ore::data::Market> market; //TODO fill data        
        const QuantLib::ext::shared_ptr<IrLgmData> data;//TODO fill data
        const std::string& configuration = Market::defaultConfiguration;
        Real bootstrapTolerance = 0.001;
        const bool continueOnError = false; 
        const std::string& referenceCalibrationGrid = "";
        const bool setCalibrationInfo = false; 
        const std::string& id = "unknwon";

        // Absolute Error
        auto error1 = BlackCalibrationHelper::PriceError;
        LgmBuilder lgmb1(market, data, configuration, bootstrapTolerance, continueOnError, referenceCalibrationGrid, setCalibrationInfo, id, error1);
        lgmb1.recalibrate(); // Calibration should be run automatically, just to be sure
        QuantLib::ext::shared_ptr<QuantExt::IrLgm1fParametrization> p1 = lgmb1.parametrization(); // Read calibrated parameters
        p1->printParameters(0.0); // Read calibrated parameters

        // TODO
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
