/*
Copyright (C) 2016 Quaternion Risk Management Ltd
All rights reserved.

This file is part of ORE, a free-software/open-source library
for transparent pricing and risk analysis - http://opensourcerisk.org

ORE is free software: you can redistribute it and/or modify it
under the terms of the Modified BSD License.  You should have received a
copy of the license along with this program; if not, please email
<users@opensourcerisk.org>. The license is also available online at
<http://opensourcerisk.org/license.shtml>.

This program is distributed on the basis that it will form a useful
contribution to risk analytics and model standardisation, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

#include "crossassetmodeldata.hpp"
#include <ored/model/crossassetmodeldata.hpp>
#include <ored/utilities/correlationmatrix.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace std;
using namespace ore;
using namespace ore::data;

namespace {

boost::shared_ptr<vector<boost::shared_ptr<LgmData>>> irConfigsData() {

    // Create three instances
    boost::shared_ptr<LgmData> lgmData1(new data::LgmData());
    boost::shared_ptr<LgmData> lgmData2(new data::LgmData());
    boost::shared_ptr<LgmData> lgmData3(new data::LgmData());

    vector<std::string> expiries = {"1Y", "2Y", "36M"};
    vector<std::string> terms = {"5Y", "2Y", "6M"};
    vector<std::string> strikes = {"ATM", "ATM", "ATM"};

    // First instance
    lgmData1->ccy() = "EUR";

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

    lgmData1->swaptionExpiries() = expiries;
    lgmData1->swaptionTerms() = terms;
    lgmData1->swaptionStrikes() = strikes;

    lgmData1->calibrationStrategy() = parseCalibrationStrategy("COTERMINALATM");
    lgmData1->scaling() = 1.0;

    // Second instance
    lgmData2->ccy() = "USD";

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

    lgmData2->swaptionExpiries() = expiries;
    lgmData2->swaptionTerms() = terms;
    lgmData2->swaptionStrikes() = strikes;

    lgmData2->calibrationStrategy() = parseCalibrationStrategy("COTERMINALATM");
    lgmData2->scaling() = 1.0;

    // Third instance
    lgmData3->ccy() = "JPY";

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

    lgmData3->swaptionExpiries() = expiries;
    lgmData3->swaptionTerms() = terms;
    lgmData3->swaptionStrikes() = strikes;

    lgmData3->calibrationStrategy() = parseCalibrationStrategy("COTERMINALATM");
    lgmData3->scaling() = 1.0;

    boost::shared_ptr<vector<boost::shared_ptr<LgmData>>> lgmDataVector(new vector<boost::shared_ptr<LgmData>>);
    *lgmDataVector = {lgmData1, lgmData2, lgmData3};
    return lgmDataVector;
}

boost::shared_ptr<vector<boost::shared_ptr<FxBsData>>> fxConfigsData() {

    // Create two instances
    boost::shared_ptr<FxBsData> fxBsData1(new data::FxBsData());
    boost::shared_ptr<FxBsData> fxBsData2(new data::FxBsData());

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

    boost::shared_ptr<vector<boost::shared_ptr<FxBsData>>> fxBsDataVector(new vector<boost::shared_ptr<FxBsData>>);
    *fxBsDataVector = {fxBsData1, fxBsData2};
    return fxBsDataVector;
}

boost::shared_ptr<data::CrossAssetModelData> crossAssetData() {

    boost::shared_ptr<data::CrossAssetModelData> crossAssetData(new data::CrossAssetModelData());

    crossAssetData->domesticCurrency() = "EUR";
    crossAssetData->currencies() = {"EUR", "USD", "JPY"}; // need to check how to set this up
    crossAssetData->irConfigs() = *irConfigsData();
    crossAssetData->fxConfigs() = *fxConfigsData();

    CorrelationMatrixBuilder cmb;
    cmb.addCorrelation("IR:EUR", "IR:USD", 1.0);
    cmb.addCorrelation("IR:EUR", "IR:JPY", 1.0);
    cmb.addCorrelation("IR:USD", "IR:JPY", 1.0);
    crossAssetData->correlations() = cmb.data();

    crossAssetData->bootstrapTolerance() = 0.001;

    return crossAssetData;
}
} // namespace

namespace testsuite {

void CrossAssetModelDataTest::testToXMLFromXML() {
    BOOST_TEST_MESSAGE("Testing toXML/fromXML...");

    data::CrossAssetModelData data = *crossAssetData();
    XMLDocument OutDoc;

    XMLNode* simulationNode = OutDoc.allocNode("Simulation");
    OutDoc.appendNode(simulationNode);

    XMLNode* crossAssetModelNode = data.toXML(OutDoc);
    XMLUtils::appendNode(simulationNode, crossAssetModelNode);

    std::string filename = "simulationtest.xml";
    OutDoc.toFile(filename);

    data::CrossAssetModelData newData;
    newData.fromFile(filename);

    BOOST_CHECK(data == newData);

    newData.irConfigs() = {};
    BOOST_CHECK(data != newData);
}

test_suite* CrossAssetModelDataTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("CrossAssetModelDataTests");
    suite->add(BOOST_TEST_CASE(&CrossAssetModelDataTest::testToXMLFromXML));
    return suite;
}
} // namespace testsuite
