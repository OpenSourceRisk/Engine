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

#ifndef orea_aggregation_i
#define orea_aggregation_i

%include stl.i
%include types.i
%include orea_cube.i
%include orea_scenario_ext.i
%include ored_market.i
%include ored_portfolio.i

%shared_ptr(ore::analytics::CollateralAccount)
namespace ore {
namespace analytics {
class CollateralAccount {
public:
    CollateralAccount();

    class MarginCall {
    public:
        MarginCall(const Real& marginFlowAmount, const Date& marginPayDate, const Date& marginRequestDate,
                   const bool& openMarginRequest = true);
        bool openMarginRequest() const;
        Real marginAmount() const;
        Date marginPayDate() const;
        Date marginRequestDate() const;
    };
};
}
}

%shared_ptr(ore::analytics::DynamicInitialMarginCalculator)
%nodefaultctor ore::analytics::DynamicInitialMarginCalculator;
namespace ore {
namespace analytics {
class DynamicInitialMarginCalculator {
public:
    virtual ~DynamicInitialMarginCalculator() {}
    virtual void build() = 0;
    const QuantLib::ext::shared_ptr<ore::analytics::NPVCube>& dimCube() const;
};
}
}

%template(StringDateMap) std::map<std::string, Date>;

%shared_ptr(ore::analytics::ExposureCalculator)
%nodefaultctor ore::analytics::ExposureCalculator;
namespace ore {
namespace analytics {
class ExposureCalculator {
public:
    virtual ~ExposureCalculator() {}
    virtual void build();

    // Metadata
    std::vector<Date> dates();
    Date today();
    std::vector<std::string> nettingSetIds();
    std::map<std::string, Real> nettingSetValueToday();
    std::map<std::string, Date> nettingSetMaturity();
    std::vector<Real> times();
    std::string baseCurrency();
    QuantLib::ext::shared_ptr<ore::data::Portfolio> portfolio();
    QuantLib::ext::shared_ptr<ore::analytics::NPVCube> npvCube();
    QuantLib::ext::shared_ptr<ore::data::Market> market();

    // Cube accessor
    const QuantLib::ext::shared_ptr<ore::analytics::NPVCube>& exposureCube();

    // Per-trade exposure profiles
    std::vector<Real> epe(const std::string& tid);
    std::vector<Real> ene(const std::string& tid);
    std::vector<Real> allocatedEpe(const std::string& tid);
    std::vector<Real> allocatedEne(const std::string& tid);
    std::vector<Real>& ee_b(const std::string& tid);
    std::vector<Real>& eee_b(const std::string& tid);
    std::vector<Real>& pfe(const std::string& tid);
    Real& epe_b(const std::string& tid);
    Real& eepe_b(const std::string& tid);
    std::vector<Real>& epe_b_timeWeighted(const std::string& tid);
    std::vector<Real>& eepe_b_timeWeighted(const std::string& tid);
};
}
}

%shared_ptr(ore::analytics::NettedExposureCalculator)
%nodefaultctor ore::analytics::NettedExposureCalculator;
%feature("flatnested") ore::analytics::NettedExposureCalculator::TimeAveragedExposure;
%rename(NettedExposureCalculatorTimeAveragedExposure) ore::analytics::NettedExposureCalculator::TimeAveragedExposure;
namespace ore {
namespace analytics {
class NettedExposureCalculator {
public:
    virtual ~NettedExposureCalculator() {}
    virtual void build();

    const QuantLib::ext::shared_ptr<ore::analytics::NPVCube>& exposureCube();
    const QuantLib::ext::shared_ptr<ore::analytics::NPVCube>& nettedCube();

    // Per-netting-set profiles
    std::vector<Real> epe(const std::string& nid);
    std::vector<Real> ene(const std::string& nid);
    std::vector<Real>& ee_b(const std::string& nid);
    std::vector<Real>& eee_b(const std::string& nid);
    std::vector<Real>& pfe(const std::string& nid);
    std::vector<Real>& expectedCollateral(const std::string& nid);
    std::vector<Real>& colvaIncrements(const std::string& nid);
    std::vector<Real>& collateralFloorIncrements(const std::string& nid);
    std::vector<Real>& epe_b_timeWeighted(const std::string& nid);
    std::vector<Real>& eepe_b_timeWeighted(const std::string& nid);
    Real& epe_b(const std::string& nid);
    Real& eepe_b(const std::string& nid);
    Real& colva(const std::string& nid);
    Real& collateralFloor(const std::string& nid);

    const std::map<std::string, std::string>& counterpartyMap();

    struct TimeAveragedExposure {
        Real positiveExposureBeforeCollateral;
        Real negativeExposureBeforeCollateral;
        Real positiveExposureAfterCollateral;
        Real negativeExposureAfterCollateral;
    };
};
}
}

%shared_ptr(ore::analytics::ValueAdjustmentCalculator)
%nodefaultctor ore::analytics::ValueAdjustmentCalculator;
%rename(XvaCalculator) ore::analytics::ValueAdjustmentCalculator;
namespace ore {
namespace analytics {
class ValueAdjustmentCalculator {
public:
    virtual ~ValueAdjustmentCalculator() {}
    virtual void build();
};
}
}

%shared_ptr(ore::analytics::PostProcess)
%nodefaultctor ore::analytics::PostProcess;
namespace ore {
namespace analytics {
class PostProcess {
public:
    const QuantLib::ext::shared_ptr<ore::analytics::NPVCube>& cube();
    const QuantLib::ext::shared_ptr<ore::analytics::NPVCube>& netCube();
};
}
}

#endif
