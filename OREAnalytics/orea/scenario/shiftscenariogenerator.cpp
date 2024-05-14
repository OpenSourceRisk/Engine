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

#include <boost/algorithm/string/split.hpp>
#include <orea/scenario/shiftscenariogenerator.hpp>
#include <orea/scenario/simplescenariofactory.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

using ore::data::to_string;

namespace ore {
namespace analytics {

string ShiftScenarioGenerator::ScenarioDescription::keyName(RiskFactorKey key) const {
    string keyName;
    RiskFactorKey::KeyType keyType = key.keytype;
    if (keyType != RiskFactorKey::KeyType::IndexCurve)
        keyName = key.name;
    else {
        std::vector<string> tokens;
        boost::split(tokens, key.name, boost::is_any_of("-"));
        keyName = tokens[0];
    }

    boost::replace_all(keyName, "/", "\\/");

    std::ostringstream o;
    o << keyType << "/" << keyName;
    return o.str();
}
ShiftScenarioGenerator::ScenarioDescription::ScenarioDescription(const string& description) {

    // Expect string to be possibly three pieces delimited by ":"
    vector<string> tokens;
    boost::split(tokens, description, boost::is_any_of(":"));

    if (tokens.size() == 1 && tokens[0] == "Base") {
        type_ = ScenarioDescription::Type::Base;

        key1_ = RiskFactorKey();
        indexDesc1_ = "";

        key2_ = RiskFactorKey();
        indexDesc2_ = "";

    } else if (tokens.size() == 2 && (tokens[0] == "Up" || tokens[0] == "Down")) {
        type_ = tokens[0] == "Up" ? ScenarioDescription::Type::Up : ScenarioDescription::Type::Down;

        auto temp = deconstructFactor(tokens[1]);
        key1_ = temp.first;
        indexDesc1_ = temp.second;

        key2_ = RiskFactorKey();
        indexDesc2_ = "";

    } else if (tokens.size() == 3 && tokens[0] == "Cross") {
        type_ = ScenarioDescription::Type::Cross;

        auto temp = deconstructFactor(tokens[1]);
        key1_ = temp.first;
        indexDesc1_ = temp.second;

        temp = deconstructFactor(tokens[2]);
        key2_ = temp.first;
        indexDesc2_ = temp.second;

    } else {
        QL_FAIL("Could not construct ScenarioDescription from string '" << description << "'");
    }
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
        o << "/" << indexDesc1_;
        return o.str();
    }
    return "";
}
string ShiftScenarioGenerator::ScenarioDescription::factor2() const {
    ostringstream o;
    if (key2_ != RiskFactorKey()) {
        o << key2_;
        o << "/" << indexDesc2_;
        return o.str();
    }
    return "";
}

string ShiftScenarioGenerator::ScenarioDescription::factors() const {
    string result = factor1();
    if (factor2() != "")
        result += ":" + factor2();
    return result;
}

ShiftScenarioGenerator::ShiftScenarioGenerator(const QuantLib::ext::shared_ptr<Scenario>& baseScenario,
                                               const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& simMarketData,
					       const QuantLib::ext::weak_ptr<ScenarioSimMarket>& simMarket)
  : baseScenario_(baseScenario), simMarketData_(simMarketData), simMarket_(simMarket), counter_(0) {
    QL_REQUIRE(baseScenario_ != NULL, "ShiftScenarioGenerator: baseScenario is null");
    QL_REQUIRE(simMarketData_ != NULL, "ShiftScenarioGenerator: simMarketData is null");
    scenarios_.push_back(baseScenario_);
    scenarioDescriptions_.push_back(ScenarioDescription(ScenarioDescription::Type::Base));
}

QuantLib::ext::shared_ptr<Scenario> ShiftScenarioGenerator::next(const Date& d) {
    QL_REQUIRE(counter_ < scenarios_.size(), "scenario vector size " << scenarios_.size() << " exceeded");
    return scenarios_[counter_++];
}

ostream& operator<<(ostream& out, const ShiftScenarioGenerator::ScenarioDescription& scenarioDescription) {
    out << scenarioDescription.typeString();
    if (scenarioDescription.factor1() != "")
        out << ":" << scenarioDescription.factor1();
    if (scenarioDescription.factor2() != "")
        out << ":" << scenarioDescription.factor2();
    return out;
}

pair<RiskFactorKey, string> deconstructFactor(const string& factor) {

    // If string is empty
    if (factor.empty()) {
        return make_pair(RiskFactorKey(), "");
    }

    boost::escaped_list_separator<char> sep('\\', '/', '\"');
    boost::tokenizer<boost::escaped_list_separator<char> > tokenSplit(factor, sep);

    vector<string> tokens(tokenSplit.begin(), tokenSplit.end());

    // first 3 tokens are the risk factor key, the remainder are the description
    ostringstream o;
    if (tokens.size() > 3) {
        o << tokens[3];
        Size i = 4;
        while (i < tokens.size()) {
            o << "/" << tokens[i];
            i++;
        }
    }

    return make_pair(RiskFactorKey(parseRiskFactorKeyType(tokens[0]), tokens[1], parseInteger(tokens[2])), o.str());
}

string reconstructFactor(const RiskFactorKey& key, const string& desc) {
    // If risk factor is empty
    if (key == RiskFactorKey()) {
        return "";
    }

    // If valid risk factor
    return to_string(key) + "/" + desc;
}

QuantLib::ext::shared_ptr<RiskFactorKey> parseRiskFactorKey(const string& str, vector<string>& addTokens) {
    // Use deconstructFactor to split in to pair: [key, additional token str]
    auto p = deconstructFactor(str);

    // The additional tokens
    boost::escaped_list_separator<char> sep('\\', '/', '\"');
    boost::tokenizer<boost::escaped_list_separator<char> > tokenSplit(p.second, sep);

    vector<string> tokens(tokenSplit.begin(), tokenSplit.end());
    addTokens = tokens;

    // Return the key value
    return QuantLib::ext::make_shared<RiskFactorKey>(p.first.keytype, p.first.name, p.first.index);
}

void ShiftScenarioGenerator::applyShift(Size j, Real shiftSize, bool up, ShiftType shiftType,
                                        const vector<Time>& tenors, const vector<Real>& values,
                                        const vector<Real>& times, vector<Real>& shiftedValues, bool initialise) {

    QL_REQUIRE(j < tenors.size(), "index j out of range");
    QL_REQUIRE(times.size() == values.size(), "vector size mismatch");
    QL_REQUIRE(shiftedValues.size() == values.size(), "shifted values vector size does not match input");

    Time t1 = tenors[j];

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
    QL_REQUIRE(shiftX.size() >= 1 && shiftY.size() >= 1, "shift vector size >= 1 required");
    QL_REQUIRE(i < shiftX.size(), "index i out of range");
    QL_REQUIRE(j < shiftY.size(), "index j out of range");

    // initialise the shifted data
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
