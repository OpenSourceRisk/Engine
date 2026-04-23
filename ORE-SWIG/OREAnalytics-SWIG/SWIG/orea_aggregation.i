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

%shared_ptr(ore::analytics::ExposureCalculator)
%nodefaultctor ore::analytics::ExposureCalculator;
namespace ore {
namespace analytics {
class ExposureCalculator {
public:
    virtual ~ExposureCalculator() {}
    virtual void build();
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
