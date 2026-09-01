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
%shared_ptr(ore::analytics::PnlAnalytic)
%shared_ptr(ore::analytics::PnlExplainAnalytic)
%shared_ptr(ore::analytics::HistoricalSimulationVarAnalytic)

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
        PricingAnalytic(const ext::shared_ptr<ore::analytics::InputParameters>& inputs) {
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
        XvaAnalytic(const ext::shared_ptr<ore::analytics::InputParameters>& inputs) {
            return new ore::analytics::XvaAnalytic(
                inputs, QuantLib::ext::weak_ptr<ore::analytics::AnalyticsManager>());
        }
        ext::shared_ptr<ore::analytics::PostProcess> postProcess() {
            auto* impl = dynamic_cast<ore::analytics::XvaAnalyticImpl*>($self->impl().get());
            if (!impl) return nullptr;
            return impl->postProcess();
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
        SimmAnalytic(const ext::shared_ptr<ore::analytics::InputParameters>& inputs) {
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
        SaCcrAnalytic(const ext::shared_ptr<ore::analytics::InputParameters>& inputs) {
            return new ore::analytics::SaCcrAnalytic(
                inputs, QuantLib::ext::weak_ptr<ore::analytics::AnalyticsManager>());
        }
    }

    const ext::shared_ptr<ore::analytics::SaccrCalculator> saccrCalculator() const;
    const ext::shared_ptr<ore::analytics::SaccrTradeData> saccrTradeData() const;
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

// Helper to downcast Analytic to XvaAnalytic
%inline %{
ext::shared_ptr<ore::analytics::XvaAnalytic> asXvaAnalytic(
    ext::shared_ptr<ore::analytics::Analytic> analytic) {
    return QuantLib::ext::dynamic_pointer_cast<ore::analytics::XvaAnalytic>(analytic);
}
%}

namespace ore {
namespace analytics {
class PnlAnalytic : public ore::analytics::Analytic {
  public:
    %extend {
        PnlAnalytic() {
            auto inputs = QuantLib::ext::make_shared<ore::analytics::InputParameters>();
            return new ore::analytics::PnlAnalytic(
                inputs, QuantLib::ext::weak_ptr<ore::analytics::AnalyticsManager>());
        }
        PnlAnalytic(const ext::shared_ptr<ore::analytics::InputParameters>& inputs) {
            return new ore::analytics::PnlAnalytic(
                inputs, QuantLib::ext::weak_ptr<ore::analytics::AnalyticsManager>());
        }
    }
};
}
}

namespace ore {
namespace analytics {
class PnlExplainAnalytic : public ore::analytics::Analytic {
  public:
    %extend {
        PnlExplainAnalytic() {
            auto inputs = QuantLib::ext::make_shared<ore::analytics::InputParameters>();
            return new ore::analytics::PnlExplainAnalytic(
                inputs, QuantLib::ext::weak_ptr<ore::analytics::AnalyticsManager>());
        }
        PnlExplainAnalytic(const ext::shared_ptr<ore::analytics::InputParameters>& inputs) {
            return new ore::analytics::PnlExplainAnalytic(
                inputs, QuantLib::ext::weak_ptr<ore::analytics::AnalyticsManager>());
        }
    }
};
}
}

namespace ore {
namespace analytics {
class HistoricalSimulationVarAnalytic : public ore::analytics::Analytic {
  public:
    %extend {
        HistoricalSimulationVarAnalytic() {
            auto inputs = ext::make_shared<ore::analytics::InputParameters>();
            return new ore::analytics::HistoricalSimulationVarAnalytic(
                inputs, ext::weak_ptr<ore::analytics::AnalyticsManager>());
        }
        HistoricalSimulationVarAnalytic(
            const ext::shared_ptr<ore::analytics::InputParameters>& inputs) {
            return new ore::analytics::HistoricalSimulationVarAnalytic(
                inputs, ext::weak_ptr<ore::analytics::AnalyticsManager>());
        }
    }
};

class HistoricalSimulationVarCalculator {
  public:
    HistoricalSimulationVarCalculator(const std::vector<QuantLib::Real>& pnls);
    QuantLib::Real var(QuantLib::Real confidence, bool isCall = true) const;
    QuantLib::Real expectedShortfall(QuantLib::Real confidence, bool isCall = true) const;
};
}
}

// Helper to downcast Analytic to PnlAnalytic
%inline %{
ext::shared_ptr<ore::analytics::PnlAnalytic> asPnlAnalytic(
    ext::shared_ptr<ore::analytics::Analytic> analytic) {
    return QuantLib::ext::dynamic_pointer_cast<ore::analytics::PnlAnalytic>(analytic);
}
%}

// Helper to downcast Analytic to PnlExplainAnalytic
%inline %{
ext::shared_ptr<ore::analytics::PnlExplainAnalytic> asPnlExplainAnalytic(
    ext::shared_ptr<ore::analytics::Analytic> analytic) {
    return QuantLib::ext::dynamic_pointer_cast<ore::analytics::PnlExplainAnalytic>(analytic);
}
%}

// Helper to downcast Analytic to HistoricalSimulationVarAnalytic
%inline %{
ext::shared_ptr<ore::analytics::HistoricalSimulationVarAnalytic>
asHistoricalSimulationVarAnalytic(
    ext::shared_ptr<ore::analytics::Analytic> analytic) {
    return ext::dynamic_pointer_cast<ore::analytics::HistoricalSimulationVarAnalytic>(analytic);
}
%}

#if defined(SWIGPYTHON)
%pythoncode %{
_HistoricalSimulationVarCalculator_init = HistoricalSimulationVarCalculator.__init__
_HistoricalSimulationVarCalculator_var = HistoricalSimulationVarCalculator.var
_HistoricalSimulationVarCalculator_expectedShortfall = (
    HistoricalSimulationVarCalculator.expectedShortfall
)

def _historicalSimulationVarCalculatorInit(self, pnls):
    copied_pnls = DoubleVector()
    for pnl in pnls:
        copied_pnls.append(pnl)
    self._pnls = copied_pnls
    _HistoricalSimulationVarCalculator_init(self, copied_pnls)

def _historicalSimulationVarCalculatorVar(self, confidence, isCall=True, tradeIds=None):
    return _HistoricalSimulationVarCalculator_var(self, confidence, isCall)

def _historicalSimulationVarCalculatorExpectedShortfall(
    self, confidence, isCall=True, tradeIds=None
):
    return _HistoricalSimulationVarCalculator_expectedShortfall(self, confidence, isCall)

HistoricalSimulationVarCalculator.__init__ = _historicalSimulationVarCalculatorInit
HistoricalSimulationVarCalculator.var = _historicalSimulationVarCalculatorVar
HistoricalSimulationVarCalculator.expectedShortfall = (
    _historicalSimulationVarCalculatorExpectedShortfall
)
%}
#endif

#endif
