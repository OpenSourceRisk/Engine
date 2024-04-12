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

#include <boost/test/unit_test.hpp>
#include <oret/datapaths.hpp>
#include <oret/fileutilities.hpp>
#include <oret/toplevelfixture.hpp>

#include <ored/model/irlgmdata.hpp>

#include <ored/model/calibrationinstruments/cpicapfloor.hpp>
#include <ored/model/crossassetmodeldata.hpp>
#include <ored/model/inflation/infdkdata.hpp>
#include <ored/utilities/correlationmatrix.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace std;
using namespace ore;
using namespace ore::data;

using ore::test::TopLevelFixture;

namespace {

QuantLib::ext::shared_ptr<vector<QuantLib::ext::shared_ptr<IrModelData>>> irConfigsData() {

    // Create three instances
    QuantLib::ext::shared_ptr<IrLgmData> lgmData1(new data::IrLgmData());
    QuantLib::ext::shared_ptr<IrLgmData> lgmData2(new data::IrLgmData());
    QuantLib::ext::shared_ptr<IrLgmData> lgmData3(new data::IrLgmData());

    vector<std::string> expiries = {"1Y", "2Y", "36M"};
    vector<std::string> terms = {"5Y", "2Y", "6M"};
    vector<std::string> strikes = {"ATM", "ATM", "ATM"};

    // First instance
    lgmData1->qualifier() = "EUR";

    lgmData1->calibrationType() = parseCalibrationType("BOOTSTRAP");
    lgmData1->reversionType() = parseReversionType("HULLWHITE");
    lgmData1->volatilityType() = parseVolatilityType("HAGAN");
    std::vector<Time> hTimes = {1.0, 2.0, 3.0, 4.0};
    std::vector<Real> hValues = {1.0, 2.0, 3.0, 4.0};
    std::vector<Time> aTimes = {1.0, 2.0, 3.0, 4.0};
    std::vector<Real> aValues = {1.0, 2.0, 3.0, 4.0};

    lgmData1->calibrateH() = false;
    lgmData1->hParamType() = parseParamType("PIECEWISE");

    lgmData1->hTimes() = hTimes;

    lgmData1->hValues() = hValues;

    lgmData1->calibrateA() = false;
    lgmData1->aParamType() = parseParamType("PIECEWISE");
    lgmData1->aTimes() = aTimes;
    lgmData1->aValues() = aValues;
    lgmData1->shiftHorizon() = 1.0;

    lgmData1->optionExpiries() = expiries;
    lgmData1->optionTerms() = terms;
    lgmData1->optionStrikes() = strikes;

    lgmData1->scaling() = 1.0;

    // Second instance
    lgmData2->qualifier() = "USD";

    lgmData2->calibrationType() = parseCalibrationType("BOOTSTRAP");
    lgmData2->reversionType() = parseReversionType("HULLWHITE");
    lgmData2->volatilityType() = parseVolatilityType("HAGAN");

    lgmData2->calibrateH() = false;
    lgmData2->hParamType() = parseParamType("PIECEWISE");
    lgmData2->hTimes() = hTimes;
    lgmData2->hValues() = hValues;

    lgmData2->calibrateA() = false;
    lgmData2->aParamType() = parseParamType("PIECEWISE");

    lgmData2->aTimes() = aTimes;
    lgmData2->aValues() = aValues;
    lgmData2->shiftHorizon() = 1.0;

    lgmData2->optionExpiries() = expiries;
    lgmData2->optionTerms() = terms;
    lgmData2->optionStrikes() = strikes;

    lgmData2->scaling() = 1.0;

    // Third instance
    lgmData3->qualifier() = "JPY";

    lgmData3->calibrationType() = parseCalibrationType("BOOTSTRAP");
    lgmData3->reversionType() = parseReversionType("HULLWHITE");
    lgmData3->volatilityType() = parseVolatilityType("HAGAN");

    lgmData3->calibrateH() = false;
    lgmData3->hParamType() = parseParamType("PIECEWISE");
    lgmData3->hTimes() = hTimes;
    lgmData3->hValues() = hValues;

    lgmData3->calibrateA() = false;
    lgmData3->aParamType() = parseParamType("PIECEWISE");

    lgmData3->aTimes() = aTimes;
    lgmData3->aValues() = aValues;
    lgmData3->shiftHorizon() = 1.0;

    lgmData3->optionExpiries() = expiries;
    lgmData3->optionTerms() = terms;
    lgmData3->optionStrikes() = strikes;

    lgmData3->scaling() = 1.0;

    QuantLib::ext::shared_ptr<vector<QuantLib::ext::shared_ptr<IrModelData>>> lgmDataVector(new vector<QuantLib::ext::shared_ptr<IrModelData>>);
    *lgmDataVector = {lgmData1, lgmData2, lgmData3};
    return lgmDataVector;
}

vector<QuantLib::ext::shared_ptr<InflationModelData>> infConfigsData() {

    // TODO: Replacing the data that was here for now. It doesn't make sense.

    vector<QuantLib::ext::shared_ptr<CalibrationInstrument>> instruments;
    vector<Period> expiries = { 1 * Years, 2 * Years, 36 * Months };
    auto strike = QuantLib::ext::make_shared<AbsoluteStrike>(0.03);
    for (const Period& expiry : expiries) {
        instruments.push_back(QuantLib::ext::make_shared<CpiCapFloor>(CapFloor::Floor, expiry, strike));
    }
    CalibrationBasket cb(instruments);
    vector<CalibrationBasket> calibrationBaskets = { cb };

    ReversionParameter reversion(LgmData::ReversionType::HullWhite, false, ParamType::Piecewise,
        { 1.0, 2.0, 3.0, 4.0 }, { 1.0, 2.0, 3.0, 4.0, 4.0 });

    VolatilityParameter volatility(LgmData::VolatilityType::Hagan, false, ParamType::Piecewise,
        { 1.0, 2.0, 3.0, 4.0 }, { 1.0, 2.0, 3.0, 4.0, 4.0 });

    LgmReversionTransformation rt(1.0, 1.0);

    QuantLib::ext::shared_ptr<InfDkData> data = QuantLib::ext::make_shared<InfDkData>(CalibrationType::Bootstrap, calibrationBaskets,
        "EUR", "EUHICPXT", reversion, volatility, rt);

    return {data};
}

QuantLib::ext::shared_ptr<vector<QuantLib::ext::shared_ptr<FxBsData>>> fxConfigsData() {

    // Create two instances
    QuantLib::ext::shared_ptr<FxBsData> fxBsData1(new data::FxBsData());
    QuantLib::ext::shared_ptr<FxBsData> fxBsData2(new data::FxBsData());

    vector<std::string> expiries = {"1Y", "2Y", "36M"};
    vector<std::string> strikes = {"ATMF", "ATMF", "ATMF"};
    std::vector<Time> times = {1.0, 2.0, 3.0, 4.0};

    // First instance
    fxBsData1->foreignCcy() = "USD";
    fxBsData1->domesticCcy() = "EUR";
    fxBsData1->calibrationType() = parseCalibrationType("BOOTSTRAP");
    fxBsData1->calibrateSigma() = true;
    fxBsData1->sigmaParamType() = parseParamType("CONSTANT");
    fxBsData1->sigmaTimes() = times;
    fxBsData1->optionExpiries() = expiries;
    fxBsData1->optionStrikes() = strikes;

    // Second instance
    fxBsData2->foreignCcy() = "JPY";
    fxBsData2->domesticCcy() = "EUR";
    fxBsData2->calibrationType() = parseCalibrationType("BOOTSTRAP");
    fxBsData2->calibrateSigma() = true;
    fxBsData2->sigmaParamType() = parseParamType("CONSTANT");
    fxBsData2->sigmaTimes() = times;
    fxBsData2->optionExpiries() = expiries;
    fxBsData2->optionStrikes() = strikes;

    QuantLib::ext::shared_ptr<vector<QuantLib::ext::shared_ptr<FxBsData>>> fxBsDataVector(new vector<QuantLib::ext::shared_ptr<FxBsData>>);
    *fxBsDataVector = {fxBsData1, fxBsData2};
    return fxBsDataVector;
}

QuantLib::ext::shared_ptr<vector<QuantLib::ext::shared_ptr<EqBsData>>> eqConfigsData() {

    // Create two instances
    QuantLib::ext::shared_ptr<EqBsData> eqBsData1(new data::EqBsData());

    vector<std::string> expiries = {"1Y", "2Y", "36M"};
    vector<std::string> strikes = {"ATMF", "ATMF", "ATMF"};
    std::vector<Time> times = {1.0, 2.0, 3.0, 4.0};

    // First instance
    eqBsData1->eqName() = "SP5";
    eqBsData1->currency() = "EUR";
    eqBsData1->calibrationType() = parseCalibrationType("BOOTSTRAP");
    eqBsData1->calibrateSigma() = true;
    eqBsData1->sigmaParamType() = parseParamType("CONSTANT");
    eqBsData1->sigmaTimes() = times;
    eqBsData1->optionExpiries() = expiries;
    eqBsData1->optionStrikes() = strikes;

    QuantLib::ext::shared_ptr<vector<QuantLib::ext::shared_ptr<EqBsData>>> eqBsDataVector(new vector<QuantLib::ext::shared_ptr<EqBsData>>);
    *eqBsDataVector = {eqBsData1};
    return eqBsDataVector;
}

QuantLib::ext::shared_ptr<vector<QuantLib::ext::shared_ptr<CrLgmData>>> crLgmConfigsData() {

    // Create three instances
    QuantLib::ext::shared_ptr<CrLgmData> lgmData(new data::CrLgmData());

    lgmData->name() = "ItraxxEuropeS9V1";

    lgmData->calibrationType() = parseCalibrationType("BOOTSTRAP");
    lgmData->reversionType() = parseReversionType("HULLWHITE");
    lgmData->volatilityType() = parseVolatilityType("HAGAN");
    std::vector<Time> hTimes = {1.0, 2.0, 3.0, 4.0};
    std::vector<Real> hValues = {1.0, 2.0, 3.0, 4.0};
    std::vector<Time> aTimes = {1.0, 2.0, 3.0, 4.0};
    std::vector<Real> aValues = {1.0, 2.0, 3.0, 4.0};

    lgmData->calibrateH() = false;
    lgmData->hParamType() = parseParamType("CONSTANT");

    lgmData->hTimes() = hTimes;

    lgmData->hValues() = hValues;

    lgmData->calibrateA() = false;
    lgmData->aParamType() = parseParamType("CONSTANT");
    lgmData->aTimes() = aTimes;
    lgmData->aValues() = aValues;
    lgmData->shiftHorizon() = 1.0;

    lgmData->scaling() = 1.0;

    QuantLib::ext::shared_ptr<vector<QuantLib::ext::shared_ptr<CrLgmData>>> lgmDataVector(new vector<QuantLib::ext::shared_ptr<CrLgmData>>);
    *lgmDataVector = {lgmData};
    return lgmDataVector;
}

QuantLib::ext::shared_ptr<vector<QuantLib::ext::shared_ptr<CrCirData>>> crCirConfigsData() {

    // Create three instances
    QuantLib::ext::shared_ptr<CrCirData> cirData(new data::CrCirData());

    cirData->name() = "CDX.NA.S33v1";

    cirData->currency() = "USD";
    cirData->calibrationType() = parseCalibrationType("None");
    cirData->calibrationStrategy() = parseCirCalibrationStrategy("None");
    cirData->startValue() = 0.1;
    cirData->reversionValue() = 0.1;
    cirData->longTermValue() = 0.1;
    cirData->volatility() = 0.1;
    cirData->relaxedFeller() = true;
    cirData->fellerFactor() = 1.1;
    cirData->tolerance() = 1e-8;

    QuantLib::ext::shared_ptr<vector<QuantLib::ext::shared_ptr<CrCirData>>> cirDataVector(new vector<QuantLib::ext::shared_ptr<CrCirData>>);
    *cirDataVector = {cirData};
    return cirDataVector;
}

QuantLib::ext::shared_ptr<data::CrossAssetModelData> crossAssetData() {

    QuantLib::ext::shared_ptr<data::CrossAssetModelData> crossAssetData(new data::CrossAssetModelData());

    crossAssetData->domesticCurrency() = "EUR";
    crossAssetData->currencies() = {"EUR", "USD", "JPY"}; // need to check how to set this up
    crossAssetData->equities() = {"SP5"};
    crossAssetData->infIndices() = {"EUHICPXT"};
    crossAssetData->creditNames() = {"ItraxxEuropeS9V1", "CDX.NA.S33v1"};
    crossAssetData->irConfigs() = *irConfigsData();
    crossAssetData->fxConfigs() = *fxConfigsData();
    crossAssetData->eqConfigs() = *eqConfigsData();
    crossAssetData->infConfigs() = infConfigsData();
    crossAssetData->crLgmConfigs() = *crLgmConfigsData();
    crossAssetData->crCirConfigs() = *crCirConfigsData();

    CorrelationMatrixBuilder cmb;
    cmb.addCorrelation("IR:EUR", "IR:USD", 1.0);
    cmb.addCorrelation("IR:EUR", "IR:JPY", 1.0);
    cmb.addCorrelation("IR:USD", "IR:JPY", 1.0);
    cmb.addCorrelation("INF:EUHICPXT", "IR:EUR", 1.0);
    cmb.addCorrelation("IR:EUR", "CR:ItraxxEuropeS9V1", 1.0);
    cmb.addCorrelation("IR:USD", "CR:CDX.NA.S33v1", 1.0);
    cmb.addCorrelation("CR:ItraxxEuropeS9V1", "CR:CDX.NA.S33v1", 1.0);

    crossAssetData->setCorrelations(cmb.correlations());

    crossAssetData->bootstrapTolerance() = 0.001;

    return crossAssetData;
}

// Fixture to remove output files
class F : public TopLevelFixture {
public:
    F() {}
    ~F() { clearOutput(TEST_OUTPUT_PATH); }
};
} // namespace

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, TopLevelFixture)

BOOST_FIXTURE_TEST_SUITE(CrossAssetModelDataTests, F)

BOOST_AUTO_TEST_CASE(testToXMLFromXML) {

    BOOST_TEST_MESSAGE("Testing toXML/fromXML...");

    data::CrossAssetModelData data = *crossAssetData();
    XMLDocument OutDoc;

    XMLNode* simulationNode = OutDoc.allocNode("Simulation");
    OutDoc.appendNode(simulationNode);

    XMLNode* crossAssetModelNode = data.toXML(OutDoc);
    XMLUtils::appendNode(simulationNode, crossAssetModelNode);

    std::string filename = TEST_OUTPUT_FILE("simulationtest.xml");
    OutDoc.toFile(filename);

    data::CrossAssetModelData newData;
    newData.fromFile(filename);

    BOOST_CHECK(data == newData);

    newData.irConfigs() = {};
    BOOST_CHECK(data != newData);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
