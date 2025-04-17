/*
Copyright (C) 2016 Quaternion Risk Management Ltd
All rights reserved.
*/
/*

// clang-format off
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
// clang-format on

#include "oredtestmarket.hpp"

#include <ored/scripting/scriptengine.hpp>
#include <ored/scripting/models/blackscholes.hpp>
#include <ored/scripting/models/gaussiancam.hpp>
#include <ored/scripting/staticanalyser.hpp>
#include <ored/scripting/scriptparser.hpp>
#include <ored/scripting/astprinter.hpp>
#include <ored/scripting/models/dummymodel.hpp>

#include <oret/toplevelfixture.hpp>

#include <ored/model/crossassetmodelbuilder.hpp>
#include <ored/model/irlgmdata.hpp>
#include <ored/utilities/to_string.hpp>

#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>
#include <ql/pricingengines/swaption/blackswaptionengine.hpp>
#include <ql/models/shortrate/calibrationhelpers/swaptionhelper.hpp>

#include <ored/model/lgmbuilder.hpp>

using namespace ore::data;
using namespace QuantLib;
using namespace QuantExt;


BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(LGMBuilderTest)

BOOST_AUTO_TEST_CASE(testSetup) {

    BOOST_TEST_MESSAGE("Testing LGM Builder");

    Date asof(7, July, 2019);
    Settings::instance().evaluationDate() = asof;
    QuantLib::ext::shared_ptr<Market> testMarket = QuantLib::ext::make_shared<OredTestMarket>(asof);

    std::vector<Date> calibrationExpiries;
    std::vector<std::string> calibrationExpiriesStr;
    std::vector<Real> calibrationTimes;
    for (Size i = 1; i <= 9; ++i) {
        Date d = TARGET().advance(asof, i * Years);
        calibrationExpiries.push_back(d);
        calibrationExpiriesStr.push_back(ore::data::to_string(asof + i * Years));
        calibrationTimes.push_back(testMarket->discountCurve("EUR")->timeFromReference(d));
    }

    std::vector<std::string> calibrationTermsStr(calibrationExpiriesStr.size(), "2029-07-07");

    auto configEUR = QuantLib::ext::make_shared<IrLgmData>(); 
    configEUR->qualifier() = "EUR";
    configEUR->reversionType() = LgmData::ReversionType::HullWhite;
    configEUR->volatilityType() = LgmData::VolatilityType::Hagan;
    configEUR->calibrateH() = false;
    configEUR->hParamType() = ParamType::Constant;
    configEUR->hTimes() = std::vector<Real>();
    configEUR->calibrationType() = CalibrationType::Bootstrap;
    //configEUR->calibrationStrategy() = CalibrationStrategy::;
    
    configEUR->scaling() = 1.0;
    configEUR->shiftHorizon() = 0.0;
    configEUR->hValues() = {0.0050};
    configEUR->calibrateA() = true;
    configEUR->aParamType() = ParamType::Piecewise;
    configEUR->aTimes() = calibrationTimes;
    configEUR->aValues() = std::vector<Real>(calibrationTimes.size() + 1, 0.0030);
    configEUR->optionExpiries() = calibrationExpiriesStr;
    configEUR->optionTerms() = calibrationTermsStr;
    configEUR->optionStrikes() = std::vector<std::string>(calibrationExpiriesStr.size(), "ATM");

    //data->modelParameters("BermudanSwaption")["CalibrationStrategy"] = "CoterminalATM";

    QuantLib::ext::shared_ptr<ore::data::Market> market=testMarket;
    QuantLib::ext::shared_ptr<IrLgmData> data=configEUR;
    std::string configuration="";
    Real bootstrapTolerance=1e-8;
    bool continueOnError=true;
    std::string referenceCalibrationGrid;
    bool setCalibrationInfo=false;
    std::string id="";

    LgmBuilder builder(market, data, configuration, bootstrapTolerance, continueOnError, referenceCalibrationGrid, setCalibrationInfo, id);

    // TODO 
    //OREData:Swaption. Build, baut den Trade in QuantlibFOrmat zusammen

    // TODO Swaption gegen den Market builden
    //SwaptionEngineBuilder 

    QuantLib::ext::shared_ptr<QuantExt::IrLgm1fParametrization> parametrization=builder.parametrization();
    std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>> swaptionBasket=builder.swaptionBasket();
    QuantLib::ext::shared_ptr<QuantExt::LGM> model = builder.model();

    BOOST_CHECK(1==1);

    //OREData::Swaption sw=;
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()*/
