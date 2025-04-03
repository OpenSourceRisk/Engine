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

/*! \file scenario/stressscenariodata.hpp
    \brief A class to hold the parametrisation for building sensitivity scenarios
    \ingroup scenario
*/

#pragma once

#include <orea/scenario/scenario.hpp>

#include <ored/utilities/parsers.hpp>
#include <ored/utilities/xmlutils.hpp>

#include <qle/termstructures/dynamicstype.hpp>

namespace ore {
namespace analytics {
using namespace QuantLib;
using ore::data::XMLNode;
using ore::data::XMLSerializable;
using ore::data::XMLUtils;
using std::map;
using std::pair;
using std::string;
using std::vector;

//! Description of sensitivity shift scenarios
/*! \ingroup scenario
 */
class StressTestScenarioData : public XMLSerializable {
public:
    struct CurveShiftData {
        ShiftType shiftType;
        vector<Real> shifts;
        vector<Period> shiftTenors;
    };

    struct SpotShiftData {
        ShiftType shiftType;
        Real shiftSize;
    };

    struct VolShiftData {
        ShiftType shiftType;
        vector<Period> shiftExpiries;
        vector<Real> shifts; 
    };

    struct FXVolShiftData {

        enum class AtmShiftMode {
            Explicit,   // Shifts are given, if only one shift is given perform parallel shift
            Unadjusted, // Shift only one pillar and leave other unadjusted
            Weighted    // Shift for one pillar given, derive the other shifts from given weights by shift_t = shift_ref *
                        // w_t / w_ref
        };
        ShiftType shiftType;
        vector<Period> shiftExpiries;
        vector<Real> shifts;
        vector<Period> weightTenors;
        vector<Real> weights;
        AtmShiftMode mode = AtmShiftMode::Explicit;
    };
    
    struct CapFloorVolShiftData {
        ShiftType shiftType;
        vector<Period> shiftExpiries;
        vector<double> shiftStrikes;
        std::map<Period, vector<Real>> shifts;
    };
    struct SwaptionVolShiftData {
        ShiftType shiftType;
        Real parallelShiftSize;
        vector<Period> shiftExpiries;
        vector<Period> shiftTerms;
        map<pair<Period, Period>, Real> shifts;
    };
    struct CommodityVolShiftData {
        ShiftType shiftType;
        vector<Period> shiftExpiries;
        vector<Real> shiftMoneyness;
        vector<Real> shifts; 
    };


    struct StressTestData {
        string label;
        map<string, QuantLib::ext::shared_ptr<CurveShiftData>> discountCurveShifts;       // by currency code
        map<string, QuantLib::ext::shared_ptr<CurveShiftData>> indexCurveShifts;    // by index name
        map<string, QuantLib::ext::shared_ptr<CurveShiftData>> yieldCurveShifts;          // by yield curve name
        map<string, QuantLib::ext::shared_ptr<SpotShiftData>> fxShifts;             // by currency pair
        map<string, QuantLib::ext::shared_ptr<FXVolShiftData>> fxVolShifts;               // by currency pair
        map<string, QuantLib::ext::shared_ptr<SpotShiftData>> equityShifts;         // by equity
        map<string, QuantLib::ext::shared_ptr<VolShiftData>> equityVolShifts;             // by equity
        map<string, QuantLib::ext::shared_ptr<CurveShiftData>> commodityCurveShifts;       // by commodity
        map<string, QuantLib::ext::shared_ptr<CommodityVolShiftData>> commodityVolShifts;  // by commodity
        map<string, QuantLib::ext::shared_ptr<CapFloorVolShiftData>> capVolShifts; // by currency
        map<string, QuantLib::ext::shared_ptr<SwaptionVolShiftData>> swaptionVolShifts;  // by currency
        map<string, QuantLib::ext::shared_ptr<SpotShiftData>> securitySpreadShifts;     // by bond/security
        map<string, QuantLib::ext::shared_ptr<SpotShiftData>> recoveryRateShifts;        // by underlying name
        map<string, QuantLib::ext::shared_ptr<CurveShiftData>> survivalProbabilityShifts; // by underlying name
        bool irCurveParShifts = false;
        bool irCapFloorParShifts = false;
        bool creditCurveParShifts = false;

        bool containsParShifts() const { return irCurveParShifts || irCapFloorParShifts || creditCurveParShifts; };

        void setDiscountCurveShift(std::string s, const QuantLib::ext::shared_ptr<CurveShiftData>& csd) {
            discountCurveShifts[s] = csd;
        }
        void setIndexCurveShift(std::string s, const QuantLib::ext::shared_ptr<CurveShiftData>& csd) {
            indexCurveShifts[s] = csd;
        }
        void setYieldCurveShifts(std::string s, const QuantLib::ext::shared_ptr<CurveShiftData>& csd) {
            yieldCurveShifts[s] = csd;
        }
        void setFxShift(std::string s, const QuantLib::ext::shared_ptr<StressTestScenarioData::SpotShiftData>& csd) { 
            fxShifts[s] = csd;
        }
        void setFxVolShift(std::string s, const QuantLib::ext::shared_ptr<StressTestScenarioData::FXVolShiftData>& csd) { 
            fxVolShifts[s] = csd;
        }
        void setEquityShift(std::string s,
                            const QuantLib::ext::shared_ptr<StressTestScenarioData::SpotShiftData>& csd) { 
            equityShifts[s] = csd;
        }
        void setEquityVolShift(std::string s,
                               const QuantLib::ext::shared_ptr<StressTestScenarioData::VolShiftData>& csd) { 
            equityVolShifts[s] = csd;
        }
        void setCommodityCurveShift(std::string s, const QuantLib::ext::shared_ptr<CurveShiftData>& ccsd) {
            commodityCurveShifts[s] = ccsd;
        }
        void setCommodityVolShift(std::string s,
                               const QuantLib::ext::shared_ptr<StressTestScenarioData::CommodityVolShiftData>& csd) { 
            commodityVolShifts[s] = csd;
        }
        void setCapVolShift(std::string s,
                            const QuantLib::ext::shared_ptr<StressTestScenarioData::CapFloorVolShiftData>& csd) {
            capVolShifts[s] = csd;
        }
        void setSwaptionVolShift(std::string s,
                                 const QuantLib::ext::shared_ptr<StressTestScenarioData::SwaptionVolShiftData>& csd) {
            swaptionVolShifts[s] = csd;
        }
        void setSecuritySpreadShift(std::string s,
                                    const QuantLib::ext::shared_ptr<StressTestScenarioData::SpotShiftData>& csd) {
            securitySpreadShifts[s] = csd;
        }
        void setRecoveryRateShift(std::string s,
                                  const QuantLib::ext::shared_ptr<StressTestScenarioData::SpotShiftData>& csd) {
            recoveryRateShifts[s] = csd;
        }
        void setSurvivalProbabilityShift(std::string s,
                                         const QuantLib::ext::shared_ptr<CurveShiftData>& csd) {
            survivalProbabilityShifts[s] = csd;
        }
        void setLabel(const std::string& l) { label = l; }

        std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<CurveShiftData>>> getDiscountCurveShifts() const {
            std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<CurveShiftData>>> discounts;
            for (const auto& [k, v] : discountCurveShifts) {
                discounts.push_back(std::make_pair(k, v));
            }
            return discounts;
        };
        std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<CurveShiftData>>> getIndexCurveShifts() const {
            std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<CurveShiftData>>> csd;
            for (const auto& [k, v] : indexCurveShifts) {
                csd.push_back(std::make_pair(k, v));
            }
            return csd;
        };
        std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<CurveShiftData>>> getYieldCurveShifts() const {
            std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<CurveShiftData>>> csd;
            for (const auto& [k, v] : yieldCurveShifts) {
                csd.push_back(std::make_pair(k, v));
            }
            return csd;
        };
        std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<SpotShiftData>>> getFxShifts() const {
            std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<SpotShiftData>>> ssd;
            for (const auto& [k, v] : fxShifts) {
                ssd.push_back(std::make_pair(k, v));
            }
            return ssd;
        };
        std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<FXVolShiftData>>> getFxVolShifts() const {
            std::vector < std::pair<std::string, QuantLib::ext::shared_ptr<FXVolShiftData>>> fvsd;
            for (const auto& [k, v] : fxVolShifts) {
                fvsd.push_back(std::make_pair(k, v));
            }
            return fvsd;
        };
        std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<SpotShiftData>>> getEquityShifts() const {
            std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<SpotShiftData>>> ssd;
            for (const auto& [k, v] : equityShifts) {
                ssd.push_back(std::make_pair(k, v));
            }
            return ssd;
        };
        std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<VolShiftData>>> getEquityVolShifts() const {
            std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<VolShiftData>>> vsd;
            for (const auto& [k, v] : equityVolShifts) {
                vsd.push_back(std::make_pair(k, v));
            }
            return vsd;
        };
        std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<CurveShiftData>>> getCommodityCurveShifts() const {
            std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<CurveShiftData>>> csd;
            for (const auto& [k, v] : commodityCurveShifts) {
                csd.push_back(std::make_pair(k, v));
            }
            return csd;
        };
        std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<CommodityVolShiftData>>> getCommodityVolShifts() const {
            std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<CommodityVolShiftData>>> vsd;
            for (const auto& [k, v] : commodityVolShifts) {
                vsd.push_back(std::make_pair(k, v));
            }
            return vsd;
        };
        std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<CapFloorVolShiftData>>>
        getCapVolShifts() const {
            std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<CapFloorVolShiftData>>> cfvsd;
            for (const auto& [k, v] : capVolShifts) {
                cfvsd.push_back(std::make_pair(k, v));
            }
            return cfvsd;
        };
        std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<SwaptionVolShiftData>>>
        getSwaptionVolShifts() const {
            std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<SwaptionVolShiftData>>> svsd;
            for (const auto& [k, v] : swaptionVolShifts) {
                svsd.push_back(std::make_pair(k, v));
            }
            return svsd;
        };
        std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<SpotShiftData>>> getSecuritySpreadShifts() const {
            std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<SpotShiftData>>> ssd;
            for (const auto& [k, v] : securitySpreadShifts) {
                ssd.push_back(std::make_pair(k, v));
            }
            return ssd;
        };
        std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<SpotShiftData>>> getRecoveryRateShifts() const {
            std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<SpotShiftData>>> ssd;
            for (const auto& [k, v] : recoveryRateShifts) {
                ssd.push_back(std::make_pair(k, v));
            }
            return ssd;
        };
        std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<CurveShiftData>>>
        getSurvivalProbabilityShifts() const {
            std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<CurveShiftData>>> sps;
            for (const auto& [k, v] : survivalProbabilityShifts) {
                sps.push_back(std::make_pair(k, v));
            }
            return sps;
        };
        const std::string& getLabel() const { return label; };
    };

    //! Default constructor
    StressTestScenarioData(){};

    //! \name Inspectors
    //@{
    const vector<StressTestData>& data() const { return data_; }
    const bool useSpreadedTermStructures() const { return useSpreadedTermStructures_; }
    
    const bool hasScenarioWithParShifts() const {
        for (const auto& scenario : data_) {
            if (scenario.containsParShifts())
                return true;
        }
        return false;
    }

    const bool withIrCurveParShifts() const {
        for (const auto& scenario : data_) {
            if (scenario.irCurveParShifts)
                return true;
        }
        return false;
    }

    const bool withCreditCurveParShifts() const {
        for (const auto& scenario : data_) {
            if (scenario.creditCurveParShifts)
                return true;
        }
        return false;
    }

    const bool withIrCapFloorParShifts() const {
        for (const auto& scenario : data_) {
            if (scenario.irCapFloorParShifts)
                return true;
        }
        return false;
    }
    //@}

    //! \name Setters
    //@{
    void setData(const vector<StressTestData>& data) { data_ = data; }
    void setData(const StressTestData& data) { data_.push_back(data); }
    void clearData() { data_.clear(); }
    bool& useSpreadedTermStructures() { return useSpreadedTermStructures_; }
    //@}

    //! \name Setters
    //@{
    const vector<StressTestData>& getData() const { return data_; }
    //@}
    // 
    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    //@}

    //! \name Equality Operators
    //@{
    // bool operator==(const StressTestScenarioData& rhs);
    // bool operator!=(const StressTestScenarioData& rhs);
    //@}

private:
    vector<StressTestData> data_;
    bool useSpreadedTermStructures_ = false;
};
} // namespace analytics
} // namespace ore
