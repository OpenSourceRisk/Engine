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
%template(StringSizeMap) std::map<std::string, QuantLib::Size>;

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

%rename(tradeCvaMap)         ore::analytics::ValueAdjustmentCalculator::tradeCva();
%rename(tradeDvaMap)         ore::analytics::ValueAdjustmentCalculator::tradeDva();
%rename(nettingSetCvaMap)    ore::analytics::ValueAdjustmentCalculator::nettingSetCva();
%rename(nettingSetDvaMap)    ore::analytics::ValueAdjustmentCalculator::nettingSetDva();
%rename(nettingSetSumCvaMap) ore::analytics::ValueAdjustmentCalculator::nettingSetSumCva();
%rename(nettingSetSumDvaMap) ore::analytics::ValueAdjustmentCalculator::nettingSetSumDva();

%shared_ptr(ore::analytics::ValueAdjustmentCalculator)
%nodefaultctor ore::analytics::ValueAdjustmentCalculator;
%rename(XvaCalculator) ore::analytics::ValueAdjustmentCalculator;
namespace ore {
namespace analytics {
class ValueAdjustmentCalculator {
public:
    virtual ~ValueAdjustmentCalculator() {}
    virtual void build();

    virtual const std::vector<QuantLib::Date>& dates();
    virtual const QuantLib::Date asof();

    // No-arg overloads return full result maps (renamed to avoid collision)
    const std::map<std::string, QuantLib::Real>& tradeCva();
    const std::map<std::string, QuantLib::Real>& tradeDva();
    const std::map<std::string, QuantLib::Real>& nettingSetCva();
    const std::map<std::string, QuantLib::Real>& nettingSetDva();
    const std::map<std::string, QuantLib::Real>& nettingSetSumCva();
    const std::map<std::string, QuantLib::Real>& nettingSetSumDva();

    // Per-trade scalar accessors
    const QuantLib::Real& tradeCva(const std::string& trade);
    const QuantLib::Real& tradeDva(const std::string& trade);
    const QuantLib::Real& tradeFba(const std::string& trade);
    const QuantLib::Real& tradeFba_exOwnSp(const std::string& trade);
    const QuantLib::Real& tradeFba_exAllSp(const std::string& trade);
    const QuantLib::Real& tradeFca(const std::string& trade);
    const QuantLib::Real& tradeFca_exOwnSp(const std::string& trade);
    const QuantLib::Real& tradeFca_exAllSp(const std::string& trade);
    const QuantLib::Real& tradeMva(const std::string& trade);

    // Per-netting-set scalar accessors
    const QuantLib::Real& nettingSetCva(const std::string& nettingSet);
    const QuantLib::Real& nettingSetDva(const std::string& nettingSet);
    const QuantLib::Real& nettingSetFba(const std::string& nettingSet);
    const QuantLib::Real& nettingSetFba_exOwnSp(const std::string& nettingSet);
    const QuantLib::Real& nettingSetFba_exAllSp(const std::string& nettingSet);
    const QuantLib::Real& nettingSetFca(const std::string& nettingSet);
    const QuantLib::Real& nettingSetFca_exOwnSp(const std::string& nettingSet);
    const QuantLib::Real& nettingSetFca_exAllSp(const std::string& nettingSet);
    const QuantLib::Real& nettingSetMva(const std::string& nettingSet);
    const QuantLib::Real& nettingSetSumCva(const std::string& nettingSet);
    const QuantLib::Real& nettingSetSumDva(const std::string& nettingSet);
};
}
}

%shared_ptr(ore::analytics::StaticCreditXvaCalculator)
%nodefaultctor ore::analytics::StaticCreditXvaCalculator;
namespace ore {
namespace analytics {
class StaticCreditXvaCalculator : public ValueAdjustmentCalculator {
public:
    virtual ~StaticCreditXvaCalculator() {}
};
}
}

%shared_ptr(ore::analytics::DynamicCreditXvaCalculator)
%nodefaultctor ore::analytics::DynamicCreditXvaCalculator;
namespace ore {
namespace analytics {
class DynamicCreditXvaCalculator : public ValueAdjustmentCalculator {
public:
    virtual ~DynamicCreditXvaCalculator() {}
};
}
}

// --- CVASpreadSensitivityCalculator ---

%shared_ptr(ore::analytics::CVASpreadSensitivityCalculator)
namespace ore {
namespace analytics {
class CVASpreadSensitivityCalculator {
public:
    CVASpreadSensitivityCalculator(const std::string& key,
                                   const QuantLib::Date& asof,
                                   const std::vector<QuantLib::Real>& epe,
                                   const std::vector<QuantLib::Date>& dates,
                                   const QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>& dts,
                                   const QuantLib::Real& recovery,
                                   const QuantLib::Handle<QuantLib::YieldTermStructure>& yts,
                                   const std::vector<QuantLib::Period>& shiftTenors,
                                   QuantLib::Real shiftSize = 0.0001);

    const std::string key();
    QuantLib::Date asof();
    const std::vector<QuantLib::Real>& exposureProfile();
    const std::vector<QuantLib::Date>& exposureDateGrid();
    QuantLib::Real recoveryRate();
    const std::vector<QuantLib::Period> shiftTenors();

    const std::vector<QuantLib::Real> shiftTimes();
    QuantLib::Real shiftSize();
    const std::vector<QuantLib::Real> hazardRateSensitivities();
    const std::vector<QuantLib::Real> cdsSpreadSensitivities();
};
}
}

// --- ExposureAllocator and subclasses ---

%shared_ptr(ore::analytics::ExposureAllocator)
%nodefaultctor ore::analytics::ExposureAllocator;
namespace ore {
namespace analytics {
class ExposureAllocator {
public:
    enum class AllocationMethod {
        None,
        Marginal,
        RelativeFairValueGross,
        RelativeFairValueNet,
        RelativeXVA
    };

    virtual ~ExposureAllocator() {}
    const QuantLib::ext::shared_ptr<ore::analytics::NPVCube>& exposureCube();
    virtual void build();
};

ExposureAllocator::AllocationMethod parseAllocationMethod(const std::string& s);
}
}

%shared_ptr(ore::analytics::RelativeFairValueNetExposureAllocator)
%nodefaultctor ore::analytics::RelativeFairValueNetExposureAllocator;
namespace ore {
namespace analytics {
class RelativeFairValueNetExposureAllocator : public ExposureAllocator {
public:
    virtual ~RelativeFairValueNetExposureAllocator() {}
};
}
}

%shared_ptr(ore::analytics::RelativeFairValueGrossExposureAllocator)
%nodefaultctor ore::analytics::RelativeFairValueGrossExposureAllocator;
namespace ore {
namespace analytics {
class RelativeFairValueGrossExposureAllocator : public ExposureAllocator {
public:
    virtual ~RelativeFairValueGrossExposureAllocator() {}
};
}
}

%shared_ptr(ore::analytics::RelativeXvaExposureAllocator)
%nodefaultctor ore::analytics::RelativeXvaExposureAllocator;
namespace ore {
namespace analytics {
class RelativeXvaExposureAllocator : public ExposureAllocator {
public:
    virtual ~RelativeXvaExposureAllocator() {}
};
}
}

%shared_ptr(ore::analytics::NoneExposureAllocator)
%nodefaultctor ore::analytics::NoneExposureAllocator;
namespace ore {
namespace analytics {
class NoneExposureAllocator : public ExposureAllocator {
public:
    virtual ~NoneExposureAllocator() {}
};
}
}

// --- PostProcess ---

%shared_ptr(ore::analytics::PostProcess)
%nodefaultctor ore::analytics::PostProcess;
namespace ore {
namespace analytics {
class PostProcess {
public:
    // Cube accessors
    const QuantLib::ext::shared_ptr<ore::analytics::NPVCube>& cube();
    const QuantLib::ext::shared_ptr<ore::analytics::NPVCube>& netCube();
    const QuantLib::ext::shared_ptr<ore::analytics::NPVCube>& cptyCube();

    // Trade-level exposure profiles
    const std::vector<QuantLib::Real>& tradeEPE(const std::string& tradeId);
    const std::vector<QuantLib::Real>& tradeENE(const std::string& tradeId);
    const std::vector<QuantLib::Real>& tradeEE_B(const std::string& tradeId);
    const QuantLib::Real& tradeEPE_B(const std::string& tradeId);
    const std::vector<QuantLib::Real>& tradeEEE_B(const std::string& tradeId);
    const QuantLib::Real& tradeEEPE_B(const std::string& tradeId);
    const std::vector<QuantLib::Real>& tradePFE(const std::string& tradeId);
    const std::vector<QuantLib::Real>& tradeEPE_B_timeWeighted(const std::string& tradeId);
    const std::vector<QuantLib::Real>& tradeEEPE_B_timeWeighted(const std::string& tradeId);

    // Netting-set-level exposure profiles
    const std::vector<QuantLib::Real>& netEPE(const std::string& nettingSetId);
    const std::vector<QuantLib::Real>& netENE(const std::string& nettingSetId);
    const std::vector<QuantLib::Real>& netEE_B(const std::string& nettingSetId);
    const QuantLib::Real& netEPE_B(const std::string& nettingSetId);
    const std::vector<QuantLib::Real>& netEEE_B(const std::string& nettingSetId);
    const QuantLib::Real& netEEPE_B(const std::string& nettingSetId);
    const std::vector<QuantLib::Real>& netPFE(const std::string& nettingSetId);
    const std::vector<QuantLib::Real>& netEPE_B_timeWeighted(const std::string& nettingSetId);
    const std::vector<QuantLib::Real>& netEEPE_B_timeWeighted(const std::string& nettingSetId);
    const std::vector<QuantLib::Real>& expectedCollateral(const std::string& nettingSetId);
    const std::vector<QuantLib::Real>& colvaIncrements(const std::string& nettingSetId);
    const std::vector<QuantLib::Real>& collateralFloorIncrements(const std::string& nettingSetId);

    // Allocated exposure profiles
    const std::vector<QuantLib::Real>& allocatedTradeEPE(const std::string& tradeId);
    const std::vector<QuantLib::Real>& allocatedTradeENE(const std::string& tradeId);

    // Trade-level XVA scalars
    QuantLib::Real tradeCVA(const std::string& tradeId);
    QuantLib::Real tradeDVA(const std::string& tradeId);
    QuantLib::Real tradeMVA(const std::string& tradeId);
    QuantLib::Real tradeFBA(const std::string& tradeId);
    QuantLib::Real tradeFCA(const std::string& tradeId);
    QuantLib::Real tradeFBA_exOwnSP(const std::string& tradeId);
    QuantLib::Real tradeFCA_exOwnSP(const std::string& tradeId);
    QuantLib::Real tradeFBA_exAllSP(const std::string& tradeId);
    QuantLib::Real tradeFCA_exAllSP(const std::string& tradeId);
    QuantLib::Real allocatedTradeCVA(const std::string& tradeId);
    QuantLib::Real allocatedTradeDVA(const std::string& tradeId);

    // Netting-set-level XVA scalars
    QuantLib::Real nettingSetCVA(const std::string& nettingSetId);
    QuantLib::Real nettingSetDVA(const std::string& nettingSetId);
    QuantLib::Real nettingSetMVA(const std::string& nettingSetId);
    QuantLib::Real nettingSetFBA(const std::string& nettingSetId);
    QuantLib::Real nettingSetFCA(const std::string& nettingSetId);
    QuantLib::Real nettingSetOurKVACCR(const std::string& nettingSetId);
    QuantLib::Real nettingSetTheirKVACCR(const std::string& nettingSetId);
    QuantLib::Real nettingSetOurKVACVA(const std::string& nettingSetId);
    QuantLib::Real nettingSetTheirKVACVA(const std::string& nettingSetId);
    QuantLib::Real nettingSetFBA_exOwnSP(const std::string& nettingSetId);
    QuantLib::Real nettingSetFCA_exOwnSP(const std::string& nettingSetId);
    QuantLib::Real nettingSetFBA_exAllSP(const std::string& nettingSetId);
    QuantLib::Real nettingSetFCA_exAllSP(const std::string& nettingSetId);
    QuantLib::Real nettingSetCOLVA(const std::string& nettingSetId);
    QuantLib::Real nettingSetCollateralFloor(const std::string& nettingSetId);

    // CVA spread sensitivity inspectors
    std::vector<QuantLib::Real> netCvaHazardRateSensitivity(const std::string& nettingSetId);
    std::vector<QuantLib::Real> netCvaSpreadSensitivity(const std::string& nettingSetId);
    const std::vector<QuantLib::Real>& spreadSensitivityTimes();
    const std::vector<QuantLib::Period>& spreadSensitivityGrid();
    QuantLib::Real cvaSpreadSensiShiftSize();

    // Other inspectors
    const std::map<std::string, QuantLib::Size> tradeIds();
    const std::map<std::string, QuantLib::Size> nettingSetIds();
    const std::map<std::string, std::string>& counterpartyId();
    const QuantLib::ext::shared_ptr<ore::data::Portfolio> portfolio();
};
}
}

#endif
