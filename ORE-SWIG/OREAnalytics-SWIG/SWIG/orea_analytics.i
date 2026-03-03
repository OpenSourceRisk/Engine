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

%{
using ore::analytics::AnalyticFactory;
using ore::analytics::PricingAnalytic;
using ore::analytics::XvaAnalytic;
using ore::analytics::SimmAnalytic;
using ore::analytics::SaCcrAnalytic;
%}

%shared_ptr(AnalyticFactory)
%shared_ptr(PricingAnalytic)
%shared_ptr(XvaAnalytic)
%shared_ptr(SimmAnalytic)
%shared_ptr(SaCcrAnalytic)

%rename(SaccrAnalytic) SaCcrAnalytic;

class PricingAnalytic : public Analytic {
  public:
    %extend {
        PricingAnalytic() {
            auto inputs = QuantLib::ext::make_shared<InputParameters>();
            return new PricingAnalytic(inputs, QuantLib::ext::weak_ptr<ore::analytics::AnalyticsManager>());
        }
        PricingAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs) {
            return new PricingAnalytic(inputs, QuantLib::ext::weak_ptr<ore::analytics::AnalyticsManager>());
        }
    }
};

class XvaAnalytic : public Analytic {
  public:
    %extend {
        XvaAnalytic() {
            auto inputs = QuantLib::ext::make_shared<InputParameters>();
            return new XvaAnalytic(inputs, QuantLib::ext::weak_ptr<ore::analytics::AnalyticsManager>());
        }
        XvaAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs) {
            return new XvaAnalytic(inputs, QuantLib::ext::weak_ptr<ore::analytics::AnalyticsManager>());
        }
    }
};

class SimmAnalytic : public Analytic {
  public:
    %extend {
        SimmAnalytic() {
            auto inputs = QuantLib::ext::make_shared<InputParameters>();
            return new SimmAnalytic(inputs, QuantLib::ext::weak_ptr<ore::analytics::AnalyticsManager>());
        }
        SimmAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs) {
            return new SimmAnalytic(inputs, QuantLib::ext::weak_ptr<ore::analytics::AnalyticsManager>());
        }
    }

    bool hasNettingSetDetails();
    bool determineWinningRegulations();
};

class SaCcrAnalytic : public Analytic {
  public:
    %extend {
        SaCcrAnalytic() {
            auto inputs = QuantLib::ext::make_shared<InputParameters>();
            return new SaCcrAnalytic(inputs, QuantLib::ext::weak_ptr<ore::analytics::AnalyticsManager>());
        }
        SaCcrAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs) {
            return new SaCcrAnalytic(inputs, QuantLib::ext::weak_ptr<ore::analytics::AnalyticsManager>());
        }
    }
};

%nodefaultctor AnalyticFactory;
class AnalyticFactory {
  public:
    %extend {
        static AnalyticFactory* instance() {
            return &AnalyticFactory::instance();
        }
    }
};

#endif
