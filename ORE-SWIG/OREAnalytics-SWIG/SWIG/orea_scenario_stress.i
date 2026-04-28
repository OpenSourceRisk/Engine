/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

#ifndef orea_scenario_stress_i
#define orea_scenario_stress_i

%{
#include <orea/scenario/scenariocurvepillar.hpp>
#include <sstream>
using ore::analytics::ScenarioCurvePillar;
using ore::analytics::parseScenarioCurvePillar;
%}

%template(PeriodVectorRealMap) std::map<Period, std::vector<Real>>;
%template(PeriodPairsRealMap) std::map<pair<Period, Period>, Real>;

%shared_ptr(ore::analytics::StressTestScenarioData);
%shared_ptr(ore::analytics::StressTestScenarioData::CurveShiftData);
%shared_ptr(ore::analytics::StressTestScenarioData::SpotShiftData);
%shared_ptr(ore::analytics::StressTestScenarioData::VolShiftData);
%shared_ptr(ore::analytics::StressTestScenarioData::FXVolShiftData);
%shared_ptr(ore::analytics::StressTestScenarioData::CapFloorVolShiftData);
%shared_ptr(ore::analytics::StressTestScenarioData::SwaptionVolShiftData);
%template()std::pair<std::string, ext::shared_ptr<ore::analytics::StressTestScenarioData::CurveShiftData>>;
%template(StressTestScenarioDataCurveShiftDataVector) std::vector<std::pair<std::string, ext::shared_ptr<ore::analytics::StressTestScenarioData::CurveShiftData>>>;
%template()std::pair<std::string, ext::shared_ptr<ore::analytics::StressTestScenarioData::SpotShiftData>>;
%template(StressTestScenarioDataSpotShiftDataVector) std::vector<std::pair<std::string, ext::shared_ptr<ore::analytics::StressTestScenarioData::SpotShiftData>>>;
%template()std::pair<std::string, ext::shared_ptr<ore::analytics::StressTestScenarioData::FXVolShiftData>>;
%template(StressTestScenarioDataFXVolShiftDataVector) std::vector<std::pair<std::string, ext::shared_ptr<ore::analytics::StressTestScenarioData::FXVolShiftData>>>;
%template()std::pair<std::string, ext::shared_ptr<ore::analytics::StressTestScenarioData::VolShiftData>>;
%template(StressTestScenarioDataVolShiftDataVector) std::vector<std::pair<std::string, ext::shared_ptr<ore::analytics::StressTestScenarioData::VolShiftData>>>;
%template()std::pair<std::string, ext::shared_ptr<ore::analytics::StressTestScenarioData::CapFloorVolShiftData>>;
%template(StressTestScenarioDataCapFloorVolShiftDataVector) std::vector<std::pair<std::string, ext::shared_ptr<ore::analytics::StressTestScenarioData::CapFloorVolShiftData>>>;
%template()std::pair<std::string, ext::shared_ptr<ore::analytics::StressTestScenarioData::SwaptionVolShiftData>>;
%template(StressTestScenarioDataSwaptionShiftDataVector) std::vector<std::pair<std::string, ext::shared_ptr<ore::analytics::StressTestScenarioData::SwaptionVolShiftData>>>;
%template(StressTestDataVector) std::vector<ore::analytics::StressTestScenarioData::StressTestData>;

%rename (StressTestScenarioDataStressTestData) ore::analytics::StressTestScenarioData::StressTestData;
%feature ("flatnested") StressTestData;

%rename (StressTestScenarioDataCurveShiftData) ore::analytics::StressTestScenarioData::CurveShiftData;
%feature ("flatnested") CurveShiftData;

%rename (StressTestScenarioDataSpotShiftData) ore::analytics::StressTestScenarioData::SpotShiftData;
%feature ("flatnested") SpotShiftData;

%rename (StressTestScenarioDataFXVolShiftData) ore::analytics::StressTestScenarioData::FXVolShiftData;
%feature ("flatnested") FXVolShiftData;

%rename (StressTestScenarioDataBaseSwaptionVolShiftData) ore::analytics::StressTestScenarioData::SwaptionVolShiftData;
%feature ("flatnested") SwaptionVolShiftData;

%rename (StressTestScenarioDataVolShiftData) ore::analytics::StressTestScenarioData::VolShiftData;
%feature ("flatnested") VolShiftData;

%rename (StressTestScenarioDataCapFloorVolShiftData) ore::analytics::StressTestScenarioData::CapFloorVolShiftData;
%feature ("flatnested") CapFloorVolShiftData;

namespace ore {
namespace analytics {
class StressTestScenarioData : public ore::data::XMLSerializable {
  public:
    struct CurveShiftData {
        QuantExt::ShiftType shiftType;
        std::vector<Real> shifts;
    };

    struct SpotShiftData {
        QuantExt::ShiftType shiftType;
        Real shiftSize;
    };

    struct VolShiftData {
        QuantExt::ShiftType shiftType;
        std::vector<Period> shiftExpiries;
        std::vector<Real> shifts;
    };

    struct FXVolShiftData {
        enum class AtmShiftMode {
            Explicit,
            Unadjusted,
            Weighted
        };
        QuantExt::ShiftType shiftType;
        std::vector<Period> shiftExpiries;
        std::vector<Real> shifts;
        std::vector<Period> weightTenors;
        std::vector<Real> weights;
        AtmShiftMode mode = AtmShiftMode::Explicit;
    };

    struct CapFloorVolShiftData {
        QuantExt::ShiftType shiftType;
        std::vector<Period> shiftExpiries;
        std::vector<double> shiftStrikes;
        std::map<Period, std::vector<Real>> shifts;
    };

    struct SwaptionVolShiftData {
        QuantExt::ShiftType shiftType;
        Real parallelShiftSize;
        std::vector<Period> shiftExpiries;
        std::vector<Period> shiftTerms;
        std::map<std::pair<Period, Period>, Real> shifts;
    };

    struct CommodityVolShiftData {
        QuantExt::ShiftType shiftType;
        std::vector<Period> shiftExpiries;
        std::vector<Real> shiftMoneyness;
        std::vector<Real> shifts;
    };

    struct StressTestData {
        ~StressTestData() {}
        std::string label;
        bool irCurveParShifts;
        bool irCapFloorParShifts;
        bool creditCurveParShifts;

        bool containsParShifts() const { return irCurveParShifts || irCapFloorParShifts || creditCurveParShifts; };

        void setDiscountCurveShift(std::string s, const ext::shared_ptr<CurveShiftData>& csd);
        void setIndexCurveShift(std::string s, const ext::shared_ptr<CurveShiftData>& csd);
        void setYieldCurveShifts(std::string s, const ext::shared_ptr<CurveShiftData>& csd);
        void setFxShift(std::string s, const ext::shared_ptr<StressTestScenarioData::SpotShiftData>& csd);
        void setFxVolShift(std::string s, const ext::shared_ptr<StressTestScenarioData::FXVolShiftData>& csd);
        void setEquityShift(std::string s, const ext::shared_ptr<StressTestScenarioData::SpotShiftData>& csd);
        void setEquityVolShift(std::string s, const ext::shared_ptr<StressTestScenarioData::VolShiftData>& csd);
        void setCapVolShift(std::string s, const ext::shared_ptr<StressTestScenarioData::CapFloorVolShiftData>& csd);
        void setSwaptionVolShift(std::string s, const ext::shared_ptr<StressTestScenarioData::SwaptionVolShiftData>& csd);
        void setSecuritySpreadShift(std::string s, const ext::shared_ptr<StressTestScenarioData::SpotShiftData>& csd);
        void setRecoveryRateShift(std::string s, const ext::shared_ptr<StressTestScenarioData::SpotShiftData>& csd);
        void setSurvivalProbabilityShift(std::string s, const ext::shared_ptr<CurveShiftData>& csd);
        void setLabel(const std::string& l);

        std::vector<std::pair<std::string, ext::shared_ptr<CurveShiftData>>> getDiscountCurveShifts() const;
        std::vector<std::pair<std::string, ext::shared_ptr<CurveShiftData>>> getIndexCurveShifts() const;
        std::vector<std::pair<std::string, ext::shared_ptr<CurveShiftData>>> getYieldCurveShifts() const;
        std::vector<std::pair<std::string, ext::shared_ptr<SpotShiftData>>> getFxShifts() const;
        std::vector<std::pair<std::string, ext::shared_ptr<FXVolShiftData>>> getFxVolShifts() const;
        std::vector<std::pair<std::string, ext::shared_ptr<SpotShiftData>>> getEquityShifts() const;
        std::vector<std::pair<std::string, ext::shared_ptr<VolShiftData>>> getEquityVolShifts() const;
        std::vector<std::pair<std::string, ext::shared_ptr<CapFloorVolShiftData>>> getCapVolShifts() const;
        std::vector<std::pair<std::string, ext::shared_ptr<SwaptionVolShiftData>>> getSwaptionVolShifts() const;
        std::vector<std::pair<std::string, ext::shared_ptr<SpotShiftData>>> getSecuritySpreadShifts() const;
        std::vector<std::pair<std::string, ext::shared_ptr<SpotShiftData>>> getRecoveryRateShifts() const;
        std::vector<std::pair<std::string, ext::shared_ptr<CurveShiftData>>> getSurvivalProbabilityShifts() const;
        const std::string& getLabel() const;
    };

    StressTestScenarioData();

    const std::vector<StressTestData>& data() const;
    const bool useSpreadedTermStructures() const;
    const bool hasScenarioWithParShifts() const;
    const bool withIrCurveParShifts() const;
    const bool withCreditCurveParShifts() const;
    const bool withIrCapFloorParShifts() const;

    void setData(const std::vector<StressTestData>& data);
    void setData(const StressTestData& data);
    void clearData();
    bool& useSpreadedTermStructures();
    const std::vector<StressTestData>& getData() const;

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

} // namespace analytics
} // namespace ore

%extend ore::analytics::StressTestScenarioData::CurveShiftData {
    void setShiftTenors(const std::vector<Period>& tenors) {
        $self->shiftTenors.clear();
        for (const auto& t : tenors)
            $self->shiftTenors.push_back(ScenarioCurvePillar(t));
    }
    void setShiftTenorPillars(const std::vector<std::string>& tenors) {
        $self->shiftTenors.clear();
        for (const auto& t : tenors)
            $self->shiftTenors.push_back(parseScenarioCurvePillar(t));
    }
    std::vector<std::string> getShiftTenorStrings() const {
        std::vector<std::string> result;
        for (const auto& p : $self->shiftTenors) {
            std::ostringstream os;
            os << p;
            result.push_back(os.str());
        }
        return result;
    }
    %pythoncode %{
        @property
        def shiftTenors(self):
            return self.getShiftTenorStrings()
        @shiftTenors.setter
        def shiftTenors(self, value):
            if not value:
                self.setShiftTenors([])
            elif all(isinstance(v, str) for v in value):
                self.setShiftTenorPillars(list(value))
            else:
                self.setShiftTenors(list(value))
    %}
}

#endif
