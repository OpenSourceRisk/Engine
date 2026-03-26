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

#ifndef orea_analytics_i
#define orea_analytics_i

%include orea_app.i
%include orea_scenario_ext.i

%shared_ptr(ore::analytics::AnalyticFactory)
%shared_ptr(ore::analytics::PricingAnalytic)
%shared_ptr(ore::analytics::XvaAnalytic)
%shared_ptr(ore::analytics::SimmAnalytic)
%shared_ptr(ore::analytics::SaCcrAnalytic)

%rename(SaccrAnalytic) ore::analytics::SaCcrAnalytic;

namespace ore {
namespace analytics {
class PricingAnalytic : public ore::analytics::Analytic {
  public:
    %extend {
        PricingAnalytic() {
                        auto inputs = QuantLib::ext::make_shared<ore::analytics::InputParameters>();
            return new ore::analytics::PricingAnalytic(
                inputs, QuantLib::ext::weak_ptr<ore::analytics::AnalyticsManager>());
        }
        PricingAnalytic(const QuantLib::ext::shared_ptr<ore::analytics::InputParameters>& inputs) {
            return new ore::analytics::PricingAnalytic(
                inputs, QuantLib::ext::weak_ptr<ore::analytics::AnalyticsManager>());
        }
    }
};
}
}

namespace ore {
namespace analytics {
class XvaAnalytic : public ore::analytics::Analytic {
  public:
    %extend {
        XvaAnalytic() {
                        auto inputs = QuantLib::ext::make_shared<ore::analytics::InputParameters>();
            return new ore::analytics::XvaAnalytic(
                inputs, QuantLib::ext::weak_ptr<ore::analytics::AnalyticsManager>());
        }
        XvaAnalytic(const QuantLib::ext::shared_ptr<ore::analytics::InputParameters>& inputs) {
            return new ore::analytics::XvaAnalytic(
                inputs, QuantLib::ext::weak_ptr<ore::analytics::AnalyticsManager>());
        }
    }
};
}
}

namespace ore {
namespace analytics {
class SimmAnalytic : public ore::analytics::Analytic {
  public:
    %extend {
        SimmAnalytic() {
                        auto inputs = QuantLib::ext::make_shared<ore::analytics::InputParameters>();
            return new ore::analytics::SimmAnalytic(
                inputs, QuantLib::ext::weak_ptr<ore::analytics::AnalyticsManager>());
        }
        SimmAnalytic(const QuantLib::ext::shared_ptr<ore::analytics::InputParameters>& inputs) {
            return new ore::analytics::SimmAnalytic(
                inputs, QuantLib::ext::weak_ptr<ore::analytics::AnalyticsManager>());
        }
    }

    bool hasNettingSetDetails();
    bool determineWinningRegulations();
};
}
}

namespace ore {
namespace analytics {
class SaCcrAnalytic : public ore::analytics::Analytic {
  public:
    %extend {
        SaCcrAnalytic() {
                        auto inputs = QuantLib::ext::make_shared<ore::analytics::InputParameters>();
            return new ore::analytics::SaCcrAnalytic(
                inputs, QuantLib::ext::weak_ptr<ore::analytics::AnalyticsManager>());
        }
        SaCcrAnalytic(const QuantLib::ext::shared_ptr<ore::analytics::InputParameters>& inputs) {
            return new ore::analytics::SaCcrAnalytic(
                inputs, QuantLib::ext::weak_ptr<ore::analytics::AnalyticsManager>());
        }
    }
};
}
}

%nodefaultctor ore::analytics::AnalyticFactory;
namespace ore {
namespace analytics {
class AnalyticFactory {
  public:
    %extend {
        static ore::analytics::AnalyticFactory* instance() {
            return &ore::analytics::AnalyticFactory::instance();
        }
    }
};
}
}

#endif
