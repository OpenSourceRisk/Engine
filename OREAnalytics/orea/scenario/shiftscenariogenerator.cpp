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

#include <orea/scenario/shiftscenariogenerator.hpp>
#include <orea/scenario/simplescenariofactory.hpp>
#include <ored/utilities/log.hpp>
#include <boost/algorithm/string/split.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace ore {
namespace analytics {

string ShiftScenarioGenerator::ScenarioDescription::keyName( RiskFactorKey key) const {
    string keyName;
    RiskFactorKey::KeyType keyType = key.keytype;
    if (keyType != RiskFactorKey::KeyType::IndexCurve)
        keyName = key.name;
    else {
        std::vector<string> tokens;
        boost::split(tokens, key.name, boost::is_any_of("-"));
        keyName = tokens[0];
    }

    std::ostringstream o;
    o << keyType << "/" << keyName;
    return o.str();
}
string ShiftScenarioGenerator::ScenarioDescription::typeString() const {
    if (type_ == ScenarioDescription::Type::Base)
        return "Base";
    else if (type_ == ScenarioDescription::Type::Up)
        return "Up";
    else if (type_ == ScenarioDescription::Type::Down)
        return "Down";
    else if (type_ == ScenarioDescription::Type::Cross)
        return "Cross";
    else
        QL_FAIL("ScenarioDescription::Type not covered");
}
string ShiftScenarioGenerator::ScenarioDescription::factor1() const {
    ostringstream o;
    if (key1_ != RiskFactorKey()) {
        o << key1_;
        // if (indexDesc1_ != "")
        o << "/" << indexDesc1_;
        return o.str();
    }
    return "";
}
string ShiftScenarioGenerator::ScenarioDescription::factor2() const {
    ostringstream o;
    if (key2_ != RiskFactorKey()) {
        o << key2_;
        // if (indexDesc2_ != "")
        o << "/" << indexDesc2_;
        return o.str();
    }
    return "";
}
string ShiftScenarioGenerator::ScenarioDescription::text() const {
    string t = typeString();
    string f1 = factor1();
    string f2 = factor2();
    string ret = t;
    if (f1 != "")
        ret += ":" + f1;
    if (f2 != "")
        ret += ":" + f2;
    return ret;
}

ShiftScenarioGenerator::ShiftScenarioGenerator(const boost::shared_ptr<Scenario>& baseScenario,
                                               const boost::shared_ptr<ScenarioSimMarketParameters> simMarketData)
    : baseScenario_(baseScenario), simMarketData_(simMarketData), counter_(0) {
    QL_REQUIRE(baseScenario_ != NULL, "ShiftScenarioGenerator: baseScenario is null");
    QL_REQUIRE(simMarketData_ != NULL, "ShiftScenarioGenerator: simMarketData is null");
    scenarios_.push_back(baseScenario_);
    scenarioDescriptions_.push_back(ScenarioDescription(ScenarioDescription::Type::Base));
}

boost::shared_ptr<Scenario> ShiftScenarioGenerator::next(const Date& d) {
    QL_REQUIRE(counter_ < scenarios_.size(), "scenario vector size " << scenarios_.size() << " exceeded");
    return scenarios_[counter_++];
}

ShiftScenarioGenerator::ShiftType parseShiftType(const std::string& s) {
    static map<string, ShiftScenarioGenerator::ShiftType> m = {
        {"Absolute", ShiftScenarioGenerator::ShiftType::Absolute},
        {"Relative", ShiftScenarioGenerator::ShiftType::Relative}};
    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("Cannot convert shift type " << s << " to ShiftScenarioGenerator::ShiftType");
    }
}

void ShiftScenarioGenerator::applyShift(Size j, Real shiftSize, bool up, ShiftType shiftType,
                                        const vector<Time>& tenors, const vector<Real>& values,
                                        const vector<Real>& times, vector<Real>& shiftedValues, bool initialise) {

    QL_REQUIRE(j < tenors.size(), "index j out of range");
    QL_REQUIRE(times.size() == values.size(), "vector size mismatch");
    QL_REQUIRE(shiftedValues.size() == values.size(), "shifted values vector size does not match input");

    Time t1 = tenors[j];
    // FIXME: Handle case where the shift curve is more granular than the original
    // ostringstream o_tenors;
    // o_tenors << "{";
    // for (auto it_tenor : tenors)
    //     o_tenors << it_tenor << ",";
    // o_tenors << "}";
    // ostringstream o_times;
    // o_times << "{";
    // for (auto it_time : times)
    //     o_times << it_time << ",";
    // o_times << "}";
    // QL_REQUIRE(tenors.size() <= times.size(),
    //            "shifted tenor vector " << o_tenors.str() << " cannot be more granular than the base curve tenor
    //            vector "
    //                                    << o_times.str());
    // auto it = std::find(times.begin(), times.end(), t1);
    // QL_REQUIRE(it != times.end(),
    //            "shifted tenor node (" << t1 << ") not found in base curve tenor vector " << o_times.str());

    if (initialise) {
        for (Size i = 0; i < values.size(); ++i)
            shiftedValues[i] = values[i];
    }

    if (tenors.size() == 1) { // single shift tenor means parallel shift
        Real w = up ? 1.0 : -1.0;
        for (Size k = 0; k < times.size(); k++) {
            if (shiftType == ShiftType::Absolute)
                shiftedValues[k] += w * shiftSize;
            else
                shiftedValues[k] *= (1.0 + w * shiftSize);
        }
    } else if (j == 0) { // first shift tenor, flat extrapolation to the left
        Time t2 = tenors[j + 1];
        for (Size k = 0; k < times.size(); k++) {
            Real w = 0.0;
            if (times[k] <= t1) // full shift
                w = 1.0;
            else if (times[k] <= t2) // linear interpolation in t1 < times[k] < t2
                w = (t2 - times[k]) / (t2 - t1);
            if (!up)
                w *= -1.0;
            if (shiftType == ShiftType::Absolute)
                shiftedValues[k] += w * shiftSize;
            else
                shiftedValues[k] *= (1.0 + w * shiftSize);
        }
    } else if (j == tenors.size() - 1) { // last shift tenor, flat extrapolation to the right
        Time t0 = tenors[j - 1];
        for (Size k = 0; k < times.size(); k++) {
            Real w = 0.0;
            if (times[k] >= t0 && times[k] <= t1) // linear interpolation in t0 < times[k] < t1
                w = (times[k] - t0) / (t1 - t0);
            else if (times[k] > t1) // full shift
                w = 1.0;
            if (!up)
                w *= -1.0;
            if (shiftType == ShiftType::Absolute)
                shiftedValues[k] += w * shiftSize;
            else
                shiftedValues[k] *= (1.0 + w * shiftSize);
        }
    } else { // intermediate shift tenor
        Time t0 = tenors[j - 1];
        Time t2 = tenors[j + 1];
        for (Size k = 0; k < times.size(); k++) {
            Real w = 0.0;
            if (times[k] >= t0 && times[k] <= t1) // linear interpolation in t0 < times[k] < t1
                w = (times[k] - t0) / (t1 - t0);
            else if (times[k] > t1 && times[k] <= t2) // linear interpolation in t1 < times[k] < t2
                w = (t2 - times[k]) / (t2 - t1);
            if (!up)
                w *= -1.0;
            if (shiftType == ShiftType::Absolute)
                shiftedValues[k] += w * shiftSize;
            else
                shiftedValues[k] *= (1.0 + w * shiftSize);
        }
    }
}

void ShiftScenarioGenerator::applyShift(Size i, Size j, Real shiftSize, bool up, ShiftType shiftType,
                                        const vector<Time>& shiftX, const vector<Time>& shiftY,
                                        const vector<Time>& dataX, const vector<Time>& dataY,
                                        const vector<vector<Real>>& data, vector<vector<Real>>& shiftedData,
                                        bool initialise) {
    QL_REQUIRE(shiftX.size() >= 1 && shiftY.size() >= 1, "shift vector size >= 1 reqired");
    QL_REQUIRE(i < shiftX.size(), "index i out of range");
    QL_REQUIRE(j < shiftY.size(), "index j out of range");

    // initalise the shifted data
    if (initialise) {
        for (Size k = 0; k < dataX.size(); ++k) {
            for (Size l = 0; l < dataY.size(); ++l)
                shiftedData[k][l] = data[k][l];
        }
    }

    // single shift point means parallel shift
    if (shiftX.size() == 1 && shiftY.size() == 1) {
        Real w = up ? 1.0 : -1.0;
        for (Size k = 0; k < dataX.size(); ++k) {
            for (Size l = 0; l < dataY.size(); ++l) {
                if (shiftType == ShiftType::Absolute)
                    shiftedData[k][l] += w * shiftSize;
                else
                    shiftedData[k][l] *= (1.0 + w * shiftSize);
            }
        }
        return;
    }

    Size iMax = shiftX.size() - 1;
    Size jMax = shiftY.size() - 1;
    Real tx = shiftX[i];
    Real ty = shiftY[j];
    Real tx1 = i > 0 ? shiftX[i - 1] : QL_MAX_REAL;
    Real ty1 = j > 0 ? shiftY[j - 1] : QL_MAX_REAL;
    Real tx2 = i < iMax ? shiftX[i + 1] : -QL_MAX_REAL;
    Real ty2 = j < jMax ? shiftY[j + 1] : -QL_MAX_REAL;

    for (Size ix = 0; ix < dataX.size(); ++ix) {
        Real x = dataX[ix];
        for (Size iy = 0; iy < dataY.size(); ++iy) {
            Real y = dataY[iy];
            Real wx = 0.0;
            Real wy = 0.0;
            if (x >= tx && x <= tx2 && y >= ty && y <= ty2) {
                wx = (tx2 - x) / (tx2 - tx);
                wy = (ty2 - y) / (ty2 - ty);
            } else if (x >= tx && x <= tx2 && y >= ty1 && y <= ty) {
                wx = (tx2 - x) / (tx2 - tx);
                wy = (y - ty1) / (ty - ty1);
            } else if (x >= tx1 && x <= tx && y >= ty1 && y <= ty) {
                wx = (x - tx1) / (tx - tx1);
                wy = (y - ty1) / (ty - ty1);
            } else if (x >= tx1 && x <= tx && y >= ty && y <= ty2) {
                wx = (x - tx1) / (tx - tx1);
                wy = (ty2 - y) / (ty2 - ty);
            } else if ((x <= tx && i == 0 && y < ty && j == 0) || (x <= tx && i == 0 && y >= ty && j == jMax) ||
                       (x >= tx && i == iMax && y >= ty && j == jMax) || (x >= tx && i == iMax && y < ty && j == 0)) {
                wx = 1.0;
                wy = 1.0;
            } else if (((x <= tx && i == 0) || (x >= tx && i == iMax)) && y >= ty1 && y <= ty) {
                wx = 1.0;
                wy = (y - ty1) / (ty - ty1);
            } else if (((x <= tx && i == 0) || (x >= tx && i == iMax)) && y >= ty && y <= ty2) {
                wx = 1.0;
                wy = (ty2 - y) / (ty2 - ty);
            } else if (x >= tx1 && x <= tx && ((y < ty && j == 0) || (y >= ty && j == jMax))) {
                wx = (x - tx1) / (tx - tx1);
                wy = 1.0;
            } else if (x >= tx && x <= tx2 && ((y < ty && j == 0) || (y >= ty && j == jMax))) {
                wx = (tx2 - x) / (tx2 - tx);
                wy = 1.0;
            }
            QL_REQUIRE(wx >= 0.0 && wx <= 1.0, "wx out of range");
            QL_REQUIRE(wy >= 0.0 && wy <= 1.0, "wy out of range");

            Real w = up ? 1.0 : -1.0;
            if (shiftType == ShiftType::Absolute)
                shiftedData[ix][iy] += w * wx * wy * shiftSize;
            else
                shiftedData[ix][iy] *= (1.0 + w * wx * wy * shiftSize);
        }
    }
}
} // namespace analytics
} // namespace ore
