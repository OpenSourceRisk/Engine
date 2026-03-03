# ORE Python SWIG Bindings — Systematic Expansion Plan

This document is a **concrete, step-by-step implementation plan** for systematically expanding and standardising the Python SWIG bindings for the ORE (Open Source Risk Engine) C++ library. It is designed to be consumed and executed by a coding agent or developer.

## Status Note: Current Implementation (March 2026)

**CRITICAL FINDING:** Skeleton `.i` files exist for Phases 2–7, but they are **incomplete stubs** with the following issues:

1. ❌ **Missing base classes**: Many derived classes are declared without their base classes being wrapped. This causes SWIG warning 401 ("Nothing known about base class") and results in broken inheritance in Python.
   - Example: `SimpleScenario` extends `Scenario`, but `Scenario` is not wrapped
   - Example: Model builders extend `QuantExt::ModelBuilder`, not wrapped

2. ❌ **Incomplete method signatures**: Constructor signatures and public method lists do not match actual C++ headers.

3. ❌ **Test failures**: 93 of 159 tests fail due to missing/incomplete class bindings.

**This plan has been updated to enforce strict validation** (see Section 3.2.1 and Appendix C) to prevent future stubs from being merged without complete, hierarchically-sound implementations.

**Action**: **Do NOT attempt to fill in stubs piece-by-piece.** When re-implementing Phases 2–7, ensure:
- ✅ Full dependency analysis of every class
- ✅ Base classes wrapped BEFORE derived classes
- ✅ All public methods and constructors from C++ headers
- ✅ SWIG warning 401 validation gate (zero warnings for new classes)

---

## Table of Contents

- [1. High-Level Architecture of Python Bindings](#1-high-level-architecture-of-python-bindings)
- [2. Coverage Strategy and Prioritisation](#2-coverage-strategy-and-prioritisation)
- [3. SWIG Design and Conventions](#3-swig-design-and-conventions)
- [4. Build and Integration Plan](#4-build-and-integration-plan)
- [5. Testing Strategy for Python Bindings](#5-testing-strategy-for-python-bindings)
- [6. Migration and Refactoring of Existing SWIG Files](#6-migration-and-refactoring-of-existing-swig-files)
- [7. Documentation Plan](#7-documentation-plan)
- [8. Work Breakdown Structure](#8-work-breakdown-structure)

---

## 1. High-Level Architecture of Python Bindings

### 1.1 Current State

The ORE SWIG bindings compile into a **single Python extension module** called `_ORE` (C extension) with a generated Python wrapper `ORE.py`. All four C++ layers — QuantLib, QuantExt, OREData, OREAnalytics — are compiled into this one module. The Python package exposes everything via `from ORE import *`.

**Directory layout (current):**

```
ore/ORE-SWIG/
├── QuantLib-SWIG/SWIG/        # 68 upstream .i files (ql.i master)
├── QuantExt-SWIG/SWIG/        # 21 .i files (qle.i master)
├── OREData-SWIG/SWIG/         # 25 .i files (ored.i master)
├── OREAnalytics-SWIG/SWIG/    # 6 .i files (orea.i master)
├── oreanalytics.i             # Top-level entry: %module ORE, includes all
├── oreanalytics_wrap.cpp      # Generated (DO NOT EDIT)
├── ORE.py                     # Generated (DO NOT EDIT)
├── __init__.py                # from .ORE import *
├── setup.py                   # Build script
├── CMakeLists.txt             # CMake SWIG build
└── test/                      # Python tests (unittest/nose)
```

### 1.2 Target Architecture

Retain the **single-module architecture** (`import ORE`). This is a deliberate design choice:

- SWIG generates one C++ wrapper file (`oreanalytics_wrap.cpp`) from the top-level `oreanalytics.i`.
- A single shared library (`_ORE.pyd` / `_ORE.so`) is produced.
- All classes from all four layers coexist in a single flat Python namespace.
- This matches upstream QuantLib-SWIG conventions and avoids cross-module `shared_ptr` ownership issues.

**Do NOT split into sub-packages** (e.g. `ore.base`, `ore.data`, `ore.analytics`). Cross-module `shared_ptr<>` sharing in SWIG requires careful registration; a single module avoids this entirely. The existing architecture is correct.

### 1.3 Logical Organisation Within the Single Module

Although everything lives in one Python module, the `.i` files are organised by C++ layer:

| SWIG Layer | Master `.i` | Sub-`.i` Files | C++ Layer |
|---|---|---|---|
| QuantLib (upstream) | `ql.i` | 68 files (unchanged from upstream) | `QuantLib/ql/` |
| QuantExt | `qle.i` | `qle_*.i` files | `QuantExt/qle/` |
| OREData | `ored.i` | `ored_*.i` files | `OREData/ored/` |
| OREAnalytics | `orea.i` | `orea_*.i` files | `OREAnalytics/orea/` |

**Inclusion chain:**
```
oreanalytics.i  →  ql.i (QuantLib)
                →  qle.i (QuantExt)   →  qle_*.i
                →  ored.i (OREData)   →  ored_*.i
                →  orea.i (Analytics) →  orea_*.i
```

### 1.4 Public API Surface

After expansion, the user API should support:

```python
import ORE

# QuantLib basics (already works)
today = ORE.Date(27, ORE.February, 2026)
ORE.Settings.instance().evaluationDate = today

# OREData market + portfolio construction (partially works)
loader = ORE.InMemoryLoader()
conventions = ORE.Conventions()
portfolio = ORE.Portfolio()

# NEW: OREAnalytics workflows (Target)
params = ORE.InputParameters()
app = ORE.OREApp(params)
app.run()

# NEW: Direct analytics access (Target)
cube = ORE.DoublePrecisionInMemoryCubeN(...)
sensitivity = ORE.SensitivityAnalysis(...)

# NEW: SIMM calculation (Target)
simm_calc = ORE.SimmCalculator(...)
```

---

## 2. Coverage Strategy and Prioritisation

### 2.0 Overriding Priority Principle

> **The highest priority for new SWIG wrapping is any `XMLSerializable` C++ class not yet covered.**
> OREData **configuration classes**, **reference data**, and the remaining **portfolio trade types
> and leg data** must be wrapped before QuantExt pricing engines or OREAnalytics analytics engines.

**Rationale:**

- The primary practical benefit of Python bindings for ORE is enabling Python-driven construction,
  inspection, and round-trip serialisation of ORE input objects without C++ recompilation.
- `XMLSerializable` classes (inheriting from `ore::data::XMLSerializable`) expose `fromFile()`,
  `fromXMLString()`, `toXMLString()`, and `toFile()` via the already-wrapped base class in
  `ored_xmlutils.i`, providing immediate value to Python users.
- QuantExt pricing engines and OREAnalytics engines are secondary: they are only useful once the
  configuration and portfolio layers are fully accessible from Python.

**Identifying `XMLSerializable` classes (target list):**

- All classes under `OREData/ored/configuration/` inheriting `XMLSerializable`.
- All trade, leg, and data types under `OREData/ored/portfolio/` (trades, leg data, option data).
- Marketdata config objects under `OREData/ored/marketdata/` (curve specs, loaders, vol configs).
- `ReferenceDatum` subclasses in `OREData/ored/portfolio/referencedata.hpp`.
- `ScenarioSimMarketParameters`, `SensitivityScenarioData`, `StressTestScenarioData` (already
  partially wrapped — must be completed).
- `CrossAssetModelData` and all model data structs under `OREData/ored/model/`.

### 2.1 Coverage Inventory — Current State vs Gaps

| C++ Area | Headers | Wrapped | Gap | Priority |
|---|---|---|---|---|
| QuantLib (via upstream SWIG) | ~200+ | ~80% | Low — upstream provides good coverage | Maintenance only |
| QuantExt/calendars | 23 | 16 | 7 missing calendars | Low |
| QuantExt/currencies | 6 | 1 (metals only) | 5 regional currency files | Low |
| QuantExt/indexes | 30 | 11 | 19 indexes (FallbackIbor, composite, etc.) | Medium |
| QuantExt/instruments | 55 | 12 | **43 instruments** (MultiLegOption, BondTRS, ConvertibleBond, etc.) | Medium (Phase 3) |
| QuantExt/pricingengines | 85 | 6 | **79 engines** | Medium (Phase 3) |
| QuantExt/cashflows | 50 | 2 | **48 cashflows** (equity, commodity, CPI, TRS, etc.) | Medium (Phase 3) |
| QuantExt/termstructures | 120 | 12 | **108 term structures** | Medium (Phase 4) |
| QuantExt/models | 90 | 0 | **90 models** (CrossAssetModel, LGM, HW, parametrizations) | Medium (Phase 4) |
| QuantExt/methods | 17 | 0 | All FDM/MC methods | Low |
| QuantExt/math | 25 | 0 | RandomVariable, compute env | Low |
| QuantExt/processes | 9 | 1 | 8 state processes | Low |
| OREData/configuration | 30 | 5 | **25 XMLSerializable config classes** | **Critical — Phase 2 first** |
| OREData/marketdata | 45 | 8 | **37** (curve specs, loaders, vol config — all XMLSerializable) | **Critical — Phase 2** |
| OREData/model | 40 | 1 | **39** (XMLSerializable data structs critical; builders secondary) | **Critical (data) / Phase 5 (builders)** |
| OREData/portfolio | 170 | ~40 | **130** (trade types, legs, reference data — all XMLSerializable) | **Critical — Phase 2 first** |
| OREData/scripting | 20 | 0 | Entire scripting engine | Low |
| OREData/utilities | 30 | 5 | 25 utility classes/functions | Medium |
| OREData/report | 4 | 1 | 3 report classes | Low |
| OREAnalytics/aggregation | 20 | 0 | **All** (PostProcess, XVA, DIM, exposure) | High |
| OREAnalytics/engine | 55 | 0 | **All** (SensitivityAnalysis, ValuationEngine, VaR, StressTest) | High |
| OREAnalytics/scenario | 30 | 3 | **27** (ScenarioSimMarket, generators, etc.) | High |
| OREAnalytics/cube | 15 | 4 | 11 cube types | Medium |
| OREAnalytics/simm | 50 | 0 | **All** (SimmCalculator, CrifRecord, configs) | High |
| OREAnalytics/app | 25 | 5 | 20 (individual analytic types) | Medium |
| OREAnalytics/simulation | 2 | 0 | SimMarket, FixingManager | Medium |

### 2.2 Phased Work Plan

#### Phase 1 — Foundation and Conventions (Weeks 1–2)

> **Note on overall phase ordering:** By reviewer direction, OREData `XMLSerializable` classes
> (configurations, reference data, portfolio trades) are the **highest-priority expansion block**
> and are scheduled in Phase 2, before any QuantExt or OREAnalytics expansion begins.
> QuantExt instruments/engines follow in Phase 3, QuantExt models in Phase 4, model builders in
> Phase 5, and OREAnalytics in Phases 6–7.

**Goal:** Establish shared infrastructure, centralise typemaps, refactor existing files.

- Create `qle_common_typemaps.i` — shared typemaps for all ORE layers.
- Create `ored_common.i` — shared OREData typemaps and template instantiations.
- Create `orea_common.i` — shared OREAnalytics typemaps.
- Audit and refactor existing `.i` files to use new shared infrastructure.
- Add `%feature("docstring")` to all existing wrapped classes.
- Verify all existing tests pass after refactor.

**Deliverables:**
  - 3 new shared `.i` files.
  - All existing `.i` files refactored to new conventions.
  - Full test suite green.

#### Phase 2 — XMLSerializable OREData: Configuration, Reference Data, Portfolio (Weeks 3–6)

**Goal:** Wrap all OREData `XMLSerializable` classes not yet covered — configuration, reference data,
leg data, and portfolio trade types. These are the highest-value additions because they allow
Python-driven construction and round-trip XML serialisation of every ORE input object.

**Key rule:** Every class wrapped in this phase must pass a `fromXMLString` / `toXMLString`
round-trip test. No class is considered done until this test exists and is green.

**Extended configuration** (new `ored_configuration_ext.i`):
  - All remaining `CurveConfig` subclasses: `FxVolatilityCurveConfig`, `InflationCapFloorPriceConfig`,
    `InflationCapFloorVolatilityCurveConfig`, `EquityVolatilityCurveConfig`, `CommodityVolatilityConfig`
  - `SecurityConfig`, `BootstrapConfig` (completion — already partially started)
  - `YieldCurveConfig` extensions (full segment variety)
  - `CurveConfigurationsManager` — allows Python to load and query config files directly

**Reference data** (extend `ored_referencedatamanager.i`):
  - `BondReferenceDatum` and its `LegData` sub-struct
  - `CreditIndexReferenceDatum`
  - `EquityReferenceDatum`
  - `CommodityReferenceDatum`
  - `CurrencyHedgedEquityIndexReferenceDatum`
  - Full `ReferenceDatumRegister` / `BasicReferenceDataManager` — load `.xml` files from Python

**Extended portfolio leg data** (new `ored_portfolio_legs.i`):
  - `CMSSpreadLegData`
  - `EquityLegData`
  - `TRSLegData`
  - `DigitalCMSSpreadLegData`
  - `CommodityFloatingLegData`, `CommodityFixedLegData`
  - `BondBasketData`
  - Full `LegDataFactory` — enables dynamic leg construction from Python

**Extended portfolio trade types** (new `ored_portfolio_trades.i` and `ored_portfolio_ext.i`):
  - `ForwardBond`, `BondOption`
  - `TotalReturnSwap`
  - `FxDoubleBarrierOption`, `FxEuropeanBarrierOption`, `FxTouchOption`
  - `EquitySwap`, `EquityBarrierOption`, `EquityCliquetOption`
  - `CommodityDigitalOption`, `CommoditySpreadOption`, `CommodityAveragePriceOption`
  - `InflationCapFloor`, `CPISwap`, `YoYSwap`
  - `MultiLegOption` (data-layer trade object)
  - `ScriptedTrade` — critical for script-engine workflows

**XMLSerializable model data structs** (extend `ored_crossassetmodeldata.i`):
  - `LgmData` with `OptionBasketData`, `ReversionType`, `VolatilityType` enums
  - `HwData`
  - `FxBsData`, `EqBsData`, `InfDkData`, `InfJyData`, `ComData`, `CrCirData`, `CrLgm1fData`
  - `CalibrationBasket`, `CalibrationInstrument` hierarchy

**Test files to create:**
  - `test/test_configuration.py` — XML round-trip per config class
  - `test/test_referencedata.py` — all `ReferenceDatum` subclasses
  - `test/test_portfolio_extended.py` — XML round-trip per new trade type and leg type
  - `test/test_modeldata.py` — round-trip for each model data struct

**Dependencies:** Phase 1 shared typemaps; `XMLSerializable` base (already in `ored_xmlutils.i`).

#### Phase 3 — QuantExt Core Instruments and Engines (Weeks 7–9)

**Goal:** Wrap the most-used QuantExt instruments with their pricing engines.

**Classes to wrap (in order):**

- **Instruments** (new `.i` files or additions to `qle_instruments.i`):
  - `MultiLegOption` — most general exotic instrument
  - `BondTotalReturnSwap`
  - `ConvertibleBond2`
  - `CallableBond`
  - `RiskParticipationAgreement`
  - `CommoditySwap`, `CommodityAveragePriceOption` (OTC commodity)
  - `InflationSwap`
  - `BalanceGuaranteedSwap`
  - `AscotWrapper`

- **Pricing engines** (new `qle_pricingengines.i`):
  - `DiscountingSwapEngineMultiCurve`
  - `AnalyticLgmSwaptionEngine`
  - `AnalyticHaganCmsCouponPricer`
  - `McMultiLegOptionEngine`
  - `CommodityApoEngine`, `CommoditySwaptionEngine`
  - `NumericLgmMultiLegOptionEngine`
  - `BlackIndexCdsOptionEngine`
  - `DiscountingBondTRSEngine`

- **Cashflows** (new `qle_cashflows_ext.i`):
  - `EquityCoupon`, `EquityCouponPricer`
  - `CommodityCashFlow`, `CommodityIndexedCashFlow`
  - `SubPeriodsCoupon1` (already partial — complete it)
  - `AverageONIndexedCoupon` (already partial — complete it)
  - `TRSCashFlow`
  - `FXLinkedCashFlow` extensions (add missing features)

**Dependencies:** Phase 2 (XMLSerializable OREData) must complete first — pricing engines wrap
QuantExt instruments which are used with OREData-constructed portfolios.

#### Phase 4 — QuantExt Models and Term Structures (Weeks 9–11)

**Goal:** Expose the key stochastic models needed for XVA and simulation.

**Classes to wrap:**

- **Models** (new `qle_models.i`):
  - `CrossAssetModel` — the central multi-asset model
  - `CrossAssetModelData` (already in `ored_crossassetmodeldata.i` — verify consistency)
  - `LGM` (Linear Gaussian Markov model)
  - `HwModel`
  - `IrLgm1fParametrization`, `FxBsParametrization`, `EqBsParametrization`, `InfDkParametrization`
  - `Parametrization` (base class)
  - `TransitionMatrix`
  - `HullWhiteBucketing`

- **Term structures** (new `qle_termstructures_ext.i` or additions):
  - `CreditCurve`, `CreditVolCurve` (already partial — complete)
  - `SpreadedSwaptionVolatility`
  - `DynamicSwaptionVolatilityMatrix`
  - `BlackVolatilitySurfaceBFRR`
  - `YoYInflationTermStructure` extensions
  - `CommodityVolSurface` extensions
  - `CorrelationTermStructure`

- **Processes** (extend `qle_processes.i`):
  - `CrossAssetStateProcess`
  - `IrLgm1fStateProcess`

**Dependencies:** Phase 3 instruments helpful but not blocking.

#### Phase 5 — OREData Model Builders (Weeks 11–13)

**Goal:** Expose model construction classes. Portfolio trades and configuration are already wrapped
in Phase 2; this phase adds the C++ builder objects that construct calibrated models from config.

**Classes to wrap:**

- **Model builders** (new `ored_modelbuilders.i`):
  - `CrossAssetModelBuilder`
  - `LGMBBuilder`, `LGMMultiSwaptionCalibrationBuilder`
  - `HWBuilder`
  - `FXBSBuilder`, `EQBSBuilder`
  - `LocalVolModelBuilder` (already partial — complete)

**Dependencies:** Phase 2 (model data structs) and Phase 4 (QuantExt model parametrizations).

#### Phase 6 — OREAnalytics Core: Scenarios, Simulation, Engines (Weeks 13–16)

**Goal:** Expose the analytics pipeline so Python users can run sensitivities, stress tests, and simulation.

**Classes to wrap:**

- **Scenario framework** (new `orea_scenario_ext.i`):
  - `Scenario` (abstract), `SimpleScenario`
  - `ScenarioSimMarket`
  - `ScenarioGenerator`, `ScenarioGeneratorData`
  - `ScenarioFactory`, `CloneScenarioFactory`
  - `RiskFactorKey` (already partial — extend with all key types)

- **Engine layer** (new `orea_engine.i`):
  - `SensitivityAnalysis`, `ParSensitivityAnalysis`
  - `ValuationEngine`
  - `StressTest`
  - `ParametricVarCalculator`
  - `MarketRiskBacktest`

- **Aggregation** (new `orea_aggregation.i`):
  - `PostProcess`
  - `ExposureCalculator`
  - `XvaCalculator`
  - `CollateralAccount`
  - `DIMCalculator`

- **Simulation** (new `orea_simulation.i`):
  - `FixingManager`

**Dependencies:** Phase 5 model builders.

#### Phase 7 — OREAnalytics SIMM, Cubes, Analytics (Weeks 17–19)

**Goal:** Complete SIMM support and the higher-level analytic orchestration.

**Classes to wrap:**

- **SIMM** (new `orea_simm.i`):
  - `CrifRecord` (and `SlimCrifRecord` if applicable)
  - `SimmCalculator`
  - `SimmConfiguration` (abstract + versioned implementations)
  - `SimmResults`
  - `CrifLoader`

- **Extended cubes** (extend `orea_cube.i`):
  - `SensitivityCube`
  - `CubeWriter`, `CubeReader` (I/O)

- **Analytics manager** (new `orea_analytics.i`):
  - `Analytic` subclasses: `PricingAnalytic`, `XvaAnalytic`, `SimmAnalytic`, `SaccrAnalytic`
  - `AnalyticFactory`
  - `AnalyticsManager` (already partial — extend)

**Dependencies:** Phase 6 engines and scenarios.

#### Phase 8 — Documentation, Examples, Hardening (Weeks 19–21)

**Goal:** Production-quality docs, examples, full test coverage.

- Write docstrings for all wrapped classes.
- Create tutorial Jupyter notebooks for core workflows.
- Add comprehensive error-path tests.
- Performance benchmarks.
- Final review & cleanup.

---

## 3. SWIG Design and Conventions

### 3.1 Type Mappings

#### 3.1.1 Smart Pointer Convention

**Rule:** Always use `ext::shared_ptr` (which maps to `QuantLib::ext::shared_ptr`, itself aliased from `boost::shared_ptr` or `std::shared_ptr`).

```swig
// CORRECT:
%shared_ptr(MyClass)
class MyClass { ... };

// WRONG — never use boost::shared_ptr directly in .i files:
%shared_ptr(boost::shared_ptr<MyClass>)
```

Every class that inherits from `Observable` or is passed via `shared_ptr` in C++ **must** have a `%shared_ptr` declaration **before** the class definition in the `.i` file. Omitting this causes `SwigPyObject has no attribute X` errors at runtime.

**Rule for `Handle<T>`:** QuantLib `Handle<T>` is already wrapped by upstream QuantLib-SWIG. When using a new term structure type `T`, define:

```swig
%shared_ptr(T)
class T : public TermStructure { ... };
%template(THandle) Handle<T>;
%template(RelinkableTHandle) RelinkableHandle<T>;
```

#### 3.1.2 STL Container Mappings

Use `%template` to instantiate STL containers needed in Python:

```swig
// Standard patterns (from QuantLib-SWIG vectors.i):
%template(StringVector) std::vector<std::string>;
%template(DoubleVector) std::vector<double>;
%template(IntVector) std::vector<int>;
%template(SizeTVector) std::vector<std::size_t>;
%template(DateVector) std::vector<Date>;

// Maps:
%template(StringStringMap) std::map<std::string, std::string>;
%template(StringDoubleMap) std::map<std::string, double>;

// ORE-specific (must be added as needed):
%template(TradeVector) std::vector<ext::shared_ptr<Trade>>;
%template(LegDataVector) std::vector<LegData>;
%template(CrifRecordVector) std::vector<CrifRecord>;
```

**Rule:** Only instantiate container templates that appear in **public API signatures**. Do not pre-emptively instantiate containers that are only used internally.

**Rule:** Place all ORE-layer template instantiations in the corresponding shared `.i` file (`qle_common_typemaps.i`, `ored_common.i`, `orea_common.i`), not in individual class `.i` files, to avoid duplicate instantiation errors.

#### 3.1.3 Enum Mappings

Enums are exposed directly to Python. Use `using` declarations to bring them into the SWIG scope:

```swig
%{
using ore::data::MarketObject;
%}

// Expose enum values:
enum class MarketObject {
    DiscountCurve,
    YieldCurve,
    IndexCurve,
    // ...
};
```

For nested enums (inside a class), use `%feature("flatnested")`:

```swig
%feature("flatnested") SensitivityScenarioData::ShiftData;
```

**Rule:** C++ `enum class` values are accessed in Python as `ORE.MarketObject_DiscountCurve` (with underscore joining class and value) or `ORE.MarketObject.DiscountCurve` depending on SWIG version. Test both forms. Prefer explicit `%rename` if disambiguation is needed.

#### 3.1.4 Optional and Nullable Types

The existing `qle_common.i` file provides `%typemap` for `ext::optional<bool>` and `ext::optional<Integer>`, mapping `None` ↔ `ext::nullopt`. Extend this pattern:

```swig
// In qle_common_typemaps.i — add for each optional<T> used in the API:
%typemap(in) ext::optional<Real> %{
    if ($input == Py_None)
        $1 = ext::nullopt;
    else if (PyFloat_Check($input) || PyLong_Check($input))
        $1 = PyFloat_AsDouble($input);
    else
        SWIG_exception(SWIG_TypeError, "float expected");
%}
%typecheck (SWIG_TYPECHECK_DOUBLE) ext::optional<Real> %{
    $1 = (PyFloat_Check($input) || PyLong_Check($input) || $input == Py_None) ? 1 : 0;
%}
%typemap(out) ext::optional<Real> %{
    $result = !$1 ? Py_None : PyFloat_FromDouble(*$1);
    Py_INCREF($result);
%}
```

#### 3.1.5 String Types

`std::string` is automatically converted to/from Python `str` by SWIG. No action needed.

`QuantLib::Date` → mapped by upstream QuantLib-SWIG. The Python `Date` class has `__str__` and comparison operators.

### 3.2 Inheritance and Base Class Wrapping

#### 3.2.1 Critical Rule: Wrap Base Classes First

**CRITICAL RULE:** When wrapping a derived class, **all of its base classes must be wrapped in the same or an earlier-included `.i` file**. Failure to do this results in SWIG warning 401 ("Nothing known about base class"), which causes:

- ✗ Base class methods not exposed to the derived class in Python
- ✗ Inheritance chain broken in Python
- ✗ Objects that appear empty or non-functional
- ✗ Tests fail with `AttributeError: module 'ORE' has no attribute 'ClassName'`

**Example of what fails:**
```swig
// WRONG: IrLgmData extends LgmData, but LgmData is not wrapped
%shared_ptr(IrLgmData)
class IrLgmData : public LgmData {  // SWIG warning: "Nothing known about base class 'LgmData'"
public:
    IrLgmData();
};
```

**Example of what works:**
```swig
// RIGHT: Wrap LgmData first
%shared_ptr(LgmData)
class LgmData : public XMLSerializable {
public:
    LgmData();
};

// Then derive from it
%shared_ptr(IrLgmData)
class IrLgmData : public LgmData {
public:
    IrLgmData();
};
```

**Action items when planning a wrapping task:**

1. **Dependency Analysis**: Before wrapping any class, identify all its base classes (direct and transitive).
2. **Inclusion Order**: Ensure the `.i` file that wraps the base class is `%include`-d BEFORE the `.i` file wrapping the derived class.
3. **Same `.i` File Option**: If base and derived are in the same header, wrap them in the same `.i` file with base first.
4. **Cross-Layer Dependencies**: When derived class is in OREAnalytics and base is in QuantExt:
   - Qle base class must be wrapped in `qle_*.i` and included in `qle.i`
   - `orea.i` then includes `qle.i` → inheritance works

**Transitive Dependencies**: Check the full inheritance hierarchy. Example:
```
CrossAssetModel
  └─ extends QuantExt::ModelBase
      └─ extends QuantLib::Observable
           └─ extends QuantLib::Visitor (may not need wrapping)

// Must wrap in order: Observable → ModelBase → CrossAssetModel
```

#### 3.2.2 Abstract Base Classes and Pure Virtual Methods

**Rule:** For abstract base classes, use `%nodefaultctor ClassName` to prevent direct instantiation:

```swig
%shared_ptr(Scenario)
%nodefaultctor Scenario;
class Scenario {
public:
    virtual ~Scenario() {}
    virtual consts& asof() const = 0;
    // ... other virtual methods
};
```

**Rule:** For concrete derived classes, do NOT use `%nodefaultctor` if they have public constructors:

```swig
%shared_ptr(SimpleScenario)
class SimpleScenario : public Scenario {
public:
    SimpleScenario();  // Public constructor → NO %nodefaultctor
    // ...
};
```

#### 3.2.3 Interface vs. Implementation

**Rule:** If class only serves as a factory or interface (never directly instantiated), use `%nodefaultctor`. Otherwise, only use it if the C++ class truly has no public default constructor.

### 3.3 Memory Management and Ownership

#### 3.3.1 Shared Pointer Semantics

**Rule:** All classes that participate in shared ownership in C++ must use `%shared_ptr(ClassName)` in SWIG. This tells SWIG to wrap them in `shared_ptr` on both sides, ensuring correct reference counting.

**Rule:** When a C++ method returns a `shared_ptr<T>`, SWIG will return a Python proxy that shares ownership. When a C++ method takes `const shared_ptr<T>&`, SWIG will extract the pointer from the Python proxy.

**Rule:** If a C++ class uses `unique_ptr<T>`:
  - Prefer wrapping the factory method that produces the object rather than wrapping the `unique_ptr` directly.
  - If unavoidable, use `%newobject FactoryClass::create` so that SWIG transfers ownership to Python.

#### 3.3.2 Raw Pointer Returns

**Rule:** If a C++ method returns a raw pointer (`T*`), determine ownership:
  - If ownership is **transferred** to the caller: Use `%newobject MethodName`.
  - If ownership is **not transferred** (getter/accessor): Default SWIG behaviour is correct (Python will not delete the object).

**Rule:** Avoid wrapping methods that return `T*` with transfer semantics where possible; prefer wrapping the `shared_ptr`-based alternative.

#### 3.3.3 Observer/Observable Pattern

QuantLib's `Observer`/`Observable` pattern uses weak references internally. SWIG wraps `Observable` objects fine as long as `%shared_ptr` is declared. No special action needed.

### 3.4 Exception Handling

#### 3.4.1 Global Exception Handler

The existing top-level `oreanalytics.i` has a global `%exception` block:

```swig
%exception {
    try {
        $action
    } catch (std::out_of_range& e) {
        SWIG_exception(SWIG_IndexError, const_cast<char*>(e.what()));
    } catch (std::exception& e) {
        SWIG_exception(SWIG_RuntimeError, const_cast<char*>(e.what()));
    } catch (...) {
        SWIG_exception(SWIG_UnknownError, "unknown error");
    }
}
```

**Rule:** Keep this global handler. It catches all QuantLib exceptions (`ql/errors.hpp` throws `std::runtime_error` subclasses) and ORE exceptions.

#### 3.4.2 Specific Exception Mapping

**Rule:** Do NOT create a custom Python exception hierarchy for ORE. The existing mapping to `RuntimeError` / `IndexError` is sufficient and matches QuantLib-SWIG conventions.

**Rule:** If a specific C++ method is known to throw a domain-specific exception that should map to a different Python exception, add a method-level `%exception` block:

```swig
%exception MyClass::parseFromXML {
    try {
        $action
    } catch (ore::data::XMLParseException& e) {
        SWIG_exception(SWIG_ValueError, const_cast<char*>(e.what()));
    } catch (std::exception& e) {
        SWIG_exception(SWIG_RuntimeError, const_cast<char*>(e.what()));
    }
}
```

### 3.5 Naming Conventions

#### 3.5.1 Class Names

**Rule:** Keep C++ class names in Python (CamelCase). Do NOT convert to snake_case.

```python
# Correct:
curve = ORE.YieldCurveConfig()
trade = ORE.FxForward(...)

# Wrong:
curve = ORE.yield_curve_config()
```

**Rationale:** QuantLib-SWIG uses CamelCase throughout. Mixing conventions would confuse users.

#### 3.5.2 Method Names

**Rule:** Keep C++ method names as-is (camelCase for QuantLib, varies for ORE). Do NOT apply Python-style renaming.

```python
# Correct:
trade.npv()
curve.discount(date)
params.setBaseCurrency("USD")

# Do NOT rename:
trade.get_npv()
```

#### 3.5.3 Name Conflicts

When a C++ class name conflicts with an existing QuantLib class name (e.g. `ore::data::Swap` vs `QuantLib::Swap`), use a `using` alias in the `%{ %}` block:

```swig
%{
using ORESwap = ore::data::Swap;  // Avoids conflict with QuantLib::Swap
%}
```

Then expose to Python under the aliased name:

```swig
%shared_ptr(ORESwap)
class ORESwap : public Trade { ... };
```

**Rule:** Document all aliases in a comment at the top of the `.i` file. Maintain a central list in `ored_common.i`.

### 3.6 Overloads and Default Arguments

#### 3.6.1 Overloaded Methods

**Rule:** SWIG handles most C++ overloads automatically. However, when overloads differ only by smart pointer type (e.g. `shared_ptr<YieldTermStructure>` vs `shared_ptr<DefaultProbabilityTermStructure>`), use `%rename` to disambiguate:

```swig
%rename(discountCurve) MarketImpl::yieldCurve(const std::string&, const std::string&);
%rename(defaultCurve) MarketImpl::defaultCurve(const std::string&, const std::string&);
```

#### 3.6.2 Default Arguments

**Rule:** SWIG respects C++ default arguments. No special handling is needed unless the default value references a C++ object not available in Python. In that case, provide a Python-side wrapper with a Python default:

```swig
%extend MyClass {
    void doSomething(int x, const std::string& name = "") {
        $self->doSomething(x, name.empty() ? MyClass::defaultName() : name);
    }
}
```

### 3.7 Nested Classes and Structs

**Rule:** SWIG does not natively support nested classes. Use `%feature("flatnested")` to expose them:

```swig
%feature("flatnested") SensitivityScenarioData::ShiftData;
%feature("flatnested") SensitivityScenarioData::CurveShiftData;

// Then in class definition:
class SensitivityScenarioData {
public:
    struct ShiftData { ... };
    struct CurveShiftData : public ShiftData { ... };
};
```

The nested struct becomes accessible in Python as `ORE.SensitivityScenarioData_ShiftData` (SWIG auto-mangles the name).

**Rule:** Use `%rename` to provide a cleaner Python name:

```swig
%rename(ShiftData) SensitivityScenarioData::ShiftData;
```

### 3.8 Template Classes

**Rule:** Template classes must be explicitly instantiated:

```swig
%shared_ptr(InMemoryCubeOpt<float>)
%shared_ptr(InMemoryCubeOpt<double>)

class InMemoryCubeOptBase : public NPVCube { ... };

template <typename T>
class InMemoryCubeOpt : public InMemoryCubeOptBase { ... };

%template(SinglePrecisionInMemoryCubeN) InMemoryCubeOpt<float>;
%template(DoublePrecisionInMemoryCubeN) InMemoryCubeOpt<double>;
```

**Rule:** Choose meaningful Python template names. Use the pattern `{Purpose}{Type}` (e.g. `DoublePrecisionInMemoryCubeN`).

### 3.9 Ignored and Excluded Items

**Rule:** Use `%ignore` for:
  - Private/internal methods not useful to Python users.
  - Methods that use C++ types not wrappable by SWIG (e.g. function pointers, complex template expressions).
  - Operators that SWIG cannot handle (e.g. `operator<<` for streams).

```swig
%ignore MyClass::internalMethod;
%ignore operator<<;
```

**Rule:** Prefer `%ignore` over omitting the declaration. This makes the exclusion explicit and prevents accidental wrapping if SWIG auto-discovers the symbol.

### 3.10 The `%extend` Directive

**Rule:** Use `%extend` to add Python-friendly methods that do not exist in C++:

```swig
%extend NPVCube {
    // Python-friendly way to get all trade IDs
    std::vector<std::string> tradeIds() {
        std::vector<std::string> ids;
        for (Size i = 0; i < $self->numIds(); ++i)
            ids.push_back($self->id(i));
        return ids;
    }
}
```

**Rule:** Use `%extend` sparingly. Prefer exposing the actual C++ API. Use `%extend` only when:
  - The C++ API returns iterators (not SWIG-friendly).
  - The C++ API uses output parameters (e.g. `void get(T& out)`).
  - A Python-idiomatic convenience method significantly improves usability.

---

## 4. Build and Integration Plan (CMake + SWIG + Python)

### 4.1 Current Build Configuration

There are **two build paths**:

1. **CMake path** (preferred): In `ore/ORE-SWIG/CMakeLists.txt`, the `swig_add_library(OREP ...)` target builds `_ORE.pyd`/`_ORE.so`. Triggered when the parent ORE build has `-DORE_BUILD_SWIG=ON`.

2. **setup.py path** (standalone): The `setup.py` `wrap` command invokes SWIG directly, then `build` compiles the wrapper.

### 4.2 No Changes Needed to Build Architecture

The existing CMake + setup.py dual build is correct and does not need restructuring. The key files remain:

- `ore/ORE-SWIG/CMakeLists.txt` — CMake SWIG target
- `ore/ORE-SWIG/setup.py` — Standalone Python build
- `ore/ORE-SWIG/OREAnalytics-SWIG/SWIG/oreanalytics.i` — Top-level SWIG entry point

### 4.3 Tasks When Adding New `.i` Files

For each new `.i` file created (e.g. `qle_models.i`), the coding agent must:

1. **Create** the new `.i` file in the appropriate directory:
   - QuantExt: `ore/ORE-SWIG/QuantExt-SWIG/SWIG/qle_models.i`
   - OREData: `ore/ORE-SWIG/OREData-SWIG/SWIG/ored_modelbuilders.i`
   - OREAnalytics: `ore/ORE-SWIG/OREAnalytics-SWIG/SWIG/orea_engine.i`

2. **Include** it in the layer's master `.i` file:
   - For QuantExt: Add `%include qle_models.i` to `qle.i`
   - For OREData: Add `%include ored_modelbuilders.i` to `ored.i`
   - For OREAnalytics: Add `%include orea_engine.i` to `orea.i`

3. **No changes to CMakeLists.txt**. SWIG dependency tracking via `USE_SWIG_DEPENDENCIES TRUE` already handles new includes.

4. **No changes to setup.py**. The SWIG command-line includes all four SWIG directories; new `.i` files are found automatically.

### 4.4 Build Verification After Changes

After modifying any `.i` file, **always validate** to ensure no base classes are missing:

```bash
# setup.py path (Windows):
python setup.py wrap 2>&1 | Tee-Object -Variable swig_output

# Check for critical SWIG warnings:
if ($swig_output -match "Warning 401: Nothing known about base class") {
    Write-Error "CRITICAL: Missing base class implementation. Do not proceed to build."
    exit 1
}

if ($swig_output -match "Warning 404: Duplicate template") {
    Write-Warning "Template duplicate warnings detected. Review and consolidate."
}

python setup.py build  # Compiles the extension
python setup.py test   # Runs test suite
```

#### 4.4.1 Validation Checklist After SWIG Wrap

**CRITICAL: Do not skip this.** Before running `build`, verify:

1. ✅ **No "Warning 401" messages**: "Nothing known about base class"
   - If present: **STOP**. Find and wrap the missing base class.

2. ✅ **No "Nested class not currently supported"** (OK if `%feature("flatnested")` is used, minor otherwise)

3. ✅ **No obvious "Redefinition"** warnings for your new classes (existing dupes OK if not new)

4. ✅ **All new class declarations show in SWIG output** without errors (grep for your class names)

5. ✅ **No strange undefined symbol errors** in the SWIG output

**Command to check for base class issues** (bash/PowerShell):
```bash
# Find all Warning 401 about your new classes
python setup.py wrap 2>&1 | grep -E "(Warning 401.*\(MyNewClass|MyNewClass.*Warning 401)"
```

If any found, your base class is not wrapped. Add it to the appropriate `.i` file.

### 4.5 Environment Requirements

| Requirement | Version |
|---|---|
| SWIG | 4.3.0+ (Windows), 3.0.1+ (Linux/macOS) |
| Python | 3.8–3.13 with development headers |
| C++ Standard | C++20 |
| Boost | 1.72+ |
| CMake | 3.20+ (for `USE_SWIG_DEPENDENCIES`) |
| Compiler | MSVC 2022 (Windows), GCC 11+ / Clang 14+ (Linux/macOS) |

### 4.6 Compile Flags

All SWIG `.i` files are compiled with these flags (already configured in `CMakeLists.txt`):

```
-fastdispatch -includeall -small
```

These must be maintained. `-fastdispatch` speeds up method dispatch; `-includeall` processes `%include` directives transitively; `-small` reduces generated code size.

### 4.7 Platform-Specific Notes

**Windows:**
- `QL_ENABLE_SESSIONS`, `QL_USE_STD_ANY`, `QL_FASTER_LAZY_OBJECTS`, `QL_USE_STD_OPTIONAL` must be defined.
- `/bigobj` is required due to the size of `oreanalytics_wrap.cpp`.
- Boost auto-link is used — do not manually specify Boost libraries.

**Linux/macOS:**
- The `oreanalytics-config` script provides compiler/linker flags.
- If not present, set `BOOST_INC`, `BOOST_LIB`, `ORE` environment variables.

---

## 5. Testing Strategy for Python Bindings

### 5.1 Test Framework

Use **`unittest`** (standard library) as the primary framework, matching existing test conventions. Tests are run via **`pynose`** (the `OREAnalyticsTestSuite.py` runner) or directly via **`pytest`**.

### 5.2 Test Organisation

```
ore/ORE-SWIG/test/
├── OREAnalyticsTestSuite.py         # Master runner (keep existing)
├── testrunner.py                    # pytest runner (keep existing)
├── Input/                           # Test data files
│   ├── market_20160205.txt
│   ├── fixings_20160205.txt
│   └── (add new test data here)
│
│ # QuantExt tests:
├── test_instruments.py              # EXISTING — extend
├── test_cashflow.py                 # EXISTING — extend
├── test_crosscurrencyswap.py        # EXISTING
├── test_ratehelpers.py              # EXISTING — extend
├── test_termstructures.py           # EXISTING — extend
├── test_qle_models.py              # NEW — Phase 3
├── test_qle_pricingengines.py      # NEW — Phase 2
│
│ # OREData tests:
├── test_loader.py                   # EXISTING — extend
├── test_marketdatum.py              # EXISTING
├── test_portfolio_extended.py       # NEW — Phase 4
├── test_modelbuilders.py            # NEW — Phase 4
├── test_configuration.py            # NEW — Phase 4
│
│ # OREAnalytics tests:
├── test_sensitivity.py              # NEW — Phase 5
├── test_scenarios.py                # NEW — Phase 5
├── test_aggregation.py              # NEW — Phase 5
├── test_simm.py                     # NEW — Phase 6
├── test_analytics.py                # NEW — Phase 6
│
│ # Cross-cutting tests:
├── test_import_smoke.py             # NEW — Phase 1
└── test_exception_handling.py       # NEW — Phase 1
```

### 5.3 Test Types

#### Smoke Tests (Every Phase)

Verify that newly wrapped classes can be imported and instantiated:

```python
class SmokeTest(unittest.TestCase):
    def test_import_new_classes(self):
        """Verify all new Phase N classes are importable."""
        from ORE import CrossAssetModel, LGM, HwModel
        # Just verify they exist — no crash
        self.assertTrue(hasattr(ORE, 'CrossAssetModel'))

    def test_basic_construction(self):
        """Construct an object with minimal arguments."""
        cube = ORE.DoublePrecisionInMemoryCubeN(ORE.Date(1,1,2026), set(), ...)
```

#### Behavioural Tests (Phases 2–6)

Test that wrapped objects produce correct financial results:

```python
class FxForwardPricingTest(unittest.TestCase):
    def setUp(self):
        # Set evaluation date, build curves, etc.
        ...

    def test_fx_forward_npv(self):
        """Test FxForward NPV against known value."""
        fwd = ORE.FxForward(...)
        engine = ORE.DiscountingFxForwardEngine(...)
        fwd.setPricingEngine(engine)
        npv = fwd.NPV()
        self.assertAlmostEqual(npv, expected_npv, places=2)
```

#### Error-Path Tests (Every Phase)

Verify that C++ exceptions propagate correctly to Python:

```python
class ExceptionTest(unittest.TestCase):
    def test_invalid_date_raises(self):
        with self.assertRaises(RuntimeError):
            ORE.Date(32, ORE.January, 2026)

    def test_null_curve_raises(self):
        with self.assertRaises(RuntimeError):
            engine = ORE.DiscountingFxForwardEngine(
                ORE.YieldTermStructureHandle(),  # empty handle
                ORE.YieldTermStructureHandle()
            )
```

#### XML Round-Trip Tests (Phase 4)

Verify that OREData XML serialization works via SWIG:

```python
class XmlRoundTripTest(unittest.TestCase):
    def test_trade_xml_roundtrip(self):
        trade = ORE.ORESwap()
        trade.fromXMLString(xml_string)
        result = trade.toXMLString()
        # Re-parse and verify key fields match
```

### 5.4 Test Data Management

- Place market data files in `test/Input/`.
- For larger datasets, use CSV loaders: `ORE.CSVLoader(...)` or `ORE.InMemoryLoader()`.
- Do NOT embed large data in Python test files. Reference external input files.

### 5.5 Test Maintenance Rules

- **Every new wrapped class must have at least one smoke test** (construction + basic accessor).
- **Every new wrapped pricing engine must have at least one behavioural test** (NPV check).
- **Every new wrapped enum must have a test** verifying value access.
- Register each new test file in `OREAnalyticsTestSuite.py` so it runs in the suite.

---

## 6. Migration and Refactoring of Existing SWIG Files

### 6.1 Audit Procedure

For each existing `.i` file, perform the following checks:

1. **Header guard:** Verify the file has `#ifndef / #define / #endif` guards.
2. **`%shared_ptr` declarations:** Verify every class that is passed by `shared_ptr` in C++ has a `%shared_ptr(ClassName)` declaration **before** the class definition.
3. **`using` declarations:** Verify that all C++ fully-qualified names are brought in via `using` in the `%{ %}` block.
4. **Naming conflicts:** Check for QuantLib name collisions (comparison list: `Swap`, `Swaption`, `FxForward`, `EquityForward`, `CapFloor`, `Bond`, `CreditDefaultSwap`, `CommodityForward`).
5. **Template instantiations:** Verify no duplicate `%template` across files. Move shared templates to the layer's `_common.i` file.
6. **Docstrings:** Check for presence of `%feature("docstring")`. Add if missing.

### 6.2 Specific Refactoring Tasks

#### 6.2.1 Create Shared Typemap Files

**Task:** Create three new shared infrastructure files.

File: `QuantExt-SWIG/SWIG/qle_common_typemaps.i`
- Move the `optional<bool>`, `optional<Integer>` typemaps from `qle_common.i` (keep `qle_common.i` as a thin include).
- Add `optional<Real>`, `optional<std::string>` typemaps.
- Add shared `%template` instantiations for QuantExt-specific containers:
  - `%template(PriceTermStructureHandle) Handle<PriceTermStructure>;`
  - `%template(CreditCurveHandle) Handle<CreditCurve>;`
  - etc.

File: `OREData-SWIG/SWIG/ored_common.i`
- Centralise all OREData `%template` instantiations currently scattered across `ored_portfolio.i`, `ored_market.i`, `ored_conventions.i`.
- Document the name-conflict alias list:
  ```swig
  // Name conflict resolution (OREData names that collide with QuantLib):
  // ORESwap = ore::data::Swap
  // ORESwaption = ore::data::Swaption
  // OREFxForward = ore::data::FxForward
  // OREBond = ore::data::Bond
  // ORECapFloor = ore::data::CapFloor
  // ORECreditDefaultSwap = ore::data::CreditDefaultSwap
  // ORECommodityForward = ore::data::CommodityForward
  // ORECommoditySwap = ore::data::CommoditySwap
  // ORECommodityOption = ore::data::CommodityOption
  // OREEquityForward = ore::data::EquityForward
  // OREForwardRateAgreement = ore::data::ForwardRateAgreement
  ```

File: `OREAnalytics-SWIG/SWIG/orea_common.i`
- Centralise OREAnalytics `%template` instantiations.
- Common `using` declarations for analytics namespaces.

#### 6.2.2 Refactor `ored_portfolio.i`

This file is 818 lines and growing. Split into:

- `ored_portfolio.i` — Core types: `Trade`, `Portfolio`, `EngineFactory`, `EngineData`, `TradeFactory`, `InstrumentWrapper`, `Envelope`.
- `ored_portfolio_legs.i` — Leg types: `ScheduleRules`, `ScheduleData`, `LegData`, `FixedLegData`, `FloatingLegData`, `CMSLegData`, `CPILegData`, `YoYLegData`, `CommodityFixedLegData`, `CommodityFloatingLegData`, `AmortizationData`, `Indexing`.
- `ored_portfolio_trades.i` — Trade types: `ORESwap`, `ORESwaption`, `OREFxForward`, `FxOption`, `EquityOption`, etc.
- `ored_portfolio_options.i` — Option-related: `OptionData`, `OptionExerciseData`, `OptionPaymentData`, `TradeStrike`, `BarrierData`, `PremiumData`.
- `ored_portfolio_credit.i` — Credit trades: `CreditDefaultSwapData`, `ORECreditDefaultSwap`, `IndexCreditDefaultSwapData`, `SyntheticCDO`.

Update `ored.i` to include all the new sub-files in order.

#### 6.2.3 Refactor `orea_scenario.i`

This file mixes two concerns: stress test data and sensitivity data. Split into:

- `orea_scenario_stress.i` — `StressTestScenarioData` and nested structs.
- `orea_scenario_sensitivity.i` — `SensitivityScenarioData` and nested structs.
- `orea_scenario.i` — Thin wrapper that includes both.

#### 6.2.4 Remove Commented-Out Code

Audit all `.i` files for commented-out `%include` directives and class definitions. Either:
- Uncomment and complete the wrapping, or
- Remove with a comment explaining why (e.g. "Not wrapped: requires feature X").

Specific instances found:
- `qle.i`: commented-out `%include qle_crossccyfixfloatswaphelper.i`.
- `ored_volcurves.i`: commented-out `GenericYieldVolCurve`/`SwaptionVolCurve`.
- `oreanalytics.i`: commented-out `%feature("autodoc")`.

### 6.3 Backwards Compatibility

**Rule:** Because the module exports a flat namespace (`from ORE import *`), renaming or removing any currently wrapped class is a **breaking change**.

**Migration procedure for renames:**
1. Keep the old name as a Python-level alias.
2. Add a deprecation warning if possible (via `__init__.py` or `%pythoncode` block).
3. Remove old name after one release cycle.

```swig
// In the .i file:
%pythoncode %{
# Backwards compatibility alias (deprecated, will remove in v2.0)
OldClassName = NewClassName
%}
```

**Rule:** Splitting files (e.g. splitting `ored_portfolio.i`) does NOT affect the Python API because all symbols end up in the same `ORE` module. This is safe to do at any time.

---

## 7. Documentation Plan

### 7.1 Docstring Strategy

Add SWIG docstrings to all wrapped classes and key methods:

```swig
%feature("docstring") CrossAssetModel
"Multi-asset stochastic model for XVA simulation.

Combines IR, FX, EQ, INF, and CR component models into a
single correlated framework.

Parameters
----------
parametrizations : list
    List of model parametrizations
correlations : Matrix
    Inter-model correlation matrix"

%feature("docstring") CrossAssetModel::dimension
"Return the total dimension of the model state process."
```

**Rule:** Use NumPy-style docstring format (Parameters/Returns/Raises sections).

**Rule:** Place `%feature("docstring")` immediately before the class/method definition in the `.i` file.

### 7.2 Auto-Generated API Documentation

- Use Sphinx with `autodoc` to generate HTML docs from the compiled Python module.
- Place Sphinx configuration in `ore/ORE-SWIG/Docs/`.
- Generate per-module pages:
  - `docs/api/core_types.rst`
  - `docs/api/instruments.rst`
  - `docs/api/pricing_engines.rst`
  - `docs/api/market_data.rst`
  - `docs/api/analytics.rst`
  - `docs/api/simm.rst`

### 7.3 Tutorial Notebooks

Create Jupyter notebooks in `ore/ORE-SWIG/Docs/notebooks/`:

- `01_getting_started.ipynb` — Import, set dates, basic pricing.
- `02_market_data_loading.ipynb` — CSVLoader, InMemoryLoader, market construction.
- `03_portfolio_construction.ipynb` — Build trades programmatically and from XML.
- `04_sensitivity_analysis.ipynb` — Run sensitivities, inspect results.
- `05_xva_workflow.ipynb` — End-to-end XVA calculation.
- `06_simm_calculation.ipynb` — CRIF loading, SIMM computation.

### 7.4 Inline Module Overviews

Add a `%pythoncode` block at the end of each layer's master `.i` file:

```swig
// In orea.i:
%pythoncode %{
"""
OREAnalytics Python Bindings
============================

Key classes for analytics workflows:

- InputParameters: Configure an analytics run
- OREApp: Run the full analytics pipeline
- SensitivityAnalysis: Compute risk sensitivities
- NPVCube: Multi-dimensional NPV storage
- SimmCalculator: Compute SIMM margin
"""
%}
```

---

## 8. Work Breakdown Structure (WBS)

### Task 1 — Setup and Infrastructure (Phase 1)

| ID | Task | Input | Output | Acceptance Criteria |
|---|---|---|---|---|
| 1.1 | Create `QuantExt-SWIG/SWIG/qle_common_typemaps.i` | Existing `qle_common.i` | New shared typemap file | Contains `optional<Real>`, `optional<string>` typemaps; existing tests pass |
| 1.2 | Create `OREData-SWIG/SWIG/ored_common.i` | Existing `ored_*.i` files | Centralised templates/aliases | All `%template` moved; no duplicate instantiation errors |
| 1.3 | Create `OREAnalytics-SWIG/SWIG/orea_common.i` | Existing `orea_*.i` files | Centralised analytics templates | Clean compilation |
| 1.4 | Update `qle.i` to include `qle_common_typemaps.i` | `qle.i` | Updated master include | SWIG wrap succeeds |
| 1.5 | Update `ored.i` to include `ored_common.i` | `ored.i` | Updated master include | SWIG wrap succeeds |
| 1.6 | Update `orea.i` to include `orea_common.i` | `orea.i` | Updated master include | SWIG wrap succeeds |
| 1.7 | Create `test/test_import_smoke.py` | All existing wrappers | Smoke test file | All currently wrapped classes are importable |
| 1.8 | Create `test/test_exception_handling.py` | Exception mapping config | Error-path tests | C++ exceptions map to correct Python exceptions |
| 1.9 | Verify full test suite passes | All files | Green test run | `python setup.py test` passes |

### Task 2 — Refactor Existing SWIG Files (Phase 1)

| ID | Task | Input | Output | Acceptance Criteria |
|---|---|---|---|---|
| 2.1 | Audit all `.i` files for missing `%shared_ptr` | All 52 `.i` files | Audit log + fixes | No `SwigPyObject` runtime errors |
| 2.2 | Split `ored_portfolio.i` into sub-files | `ored_portfolio.i` (818 lines) | 5 new files (see §6.2.2) | Identical Python API; tests pass |
| 2.3 | Split `orea_scenario.i` into sub-files | `orea_scenario.i` | 3 files (see §6.2.3) | Identical Python API; tests pass |
| 2.4 | Move scattered `%template` to common files | All `.i` files | Centralised templates | No duplicate instantiation warnings |
| 2.5 | Remove or complete commented-out code | All `.i` files | Clean `.i` files | No commented-out `%include` or class defs |
| 2.6 | Add header guards to any `.i` files missing them | All `.i` files | Guarded files | All files have `#ifndef / #define / #endif` |
| 2.7 | Verify full test suite passes after refactor | All files | Green test run | `python setup.py wrap && python setup.py build && python setup.py test` |

### Task 3 — XMLSerializable OREData: Configuration, Reference Data, Portfolio (Phase 2)

| ID | Task | Input | Output | Acceptance Criteria |
|---|---|---|---|---|
| 3.1 | Create `ored_configuration_ext.i` | `ored/configuration/` headers | New `.i` file | All remaining `CurveConfig` subclasses + `CurveConfigurationsManager`; XML round-trip passes |
| 3.2 | Extend `ored_referencedatamanager.i` | `ored/portfolio/referencedata.hpp` | Extended `.i` file | All `ReferenceDatum` subclasses; `BasicReferenceDataManager.fromFile()` callable from Python |
| 3.3 | Create `ored_portfolio_legs.i` | `ored/portfolio/legdata.hpp` and sub-headers | New `.i` file | `CMSSpreadLegData`, `EquityLegData`, `TRSLegData`, `CommodityFloatingLegData`; `LegDataFactory` |
| 3.4 | Create `ored_portfolio_trades.i` | `ored/portfolio/` trade headers | New `.i` file | `ForwardBond`, `BondOption`, `TotalReturnSwap`, `FxDoubleBarrierOption`, `FxEuropeanBarrierOption` |
| 3.5 | Create `ored_portfolio_options.i` | `ored/portfolio/` options headers | New `.i` file | `EquitySwap`, `EquityBarrierOption`, `InflationCapFloor`, `CPISwap`, `YoYSwap` |
| 3.6 | Create `ored_portfolio_commodity.i` | `ored/portfolio/` commodity headers | New `.i` file | `CommodityDigitalOption`, `CommoditySpreadOption`, `CommodityAveragePriceOption` |
| 3.7 | Create `ored_portfolio_scripted.i` | `ored/portfolio/scriptedtrade.hpp` | New `.i` file | `ScriptedTrade` fully constructable and XML round-trippable from Python |
| 3.8 | Extend `ored_crossassetmodeldata.i` | `ored/model/` data struct headers | Extended `.i` file | `LgmData`, `HwData`, `FxBsData`, `EqBsData`, `InfDkData`, `CalibrationBasket` |
| 3.9 | Update `ored.i` to include all new files | `ored.i` | Updated master include | SWIG wrap + compile succeed; no duplicate symbol errors |
| 3.10 | Create `test/test_configuration.py` | New wrappers | Test file | `fromXMLString` / `toXMLString` round-trip per config class; no data loss |
| 3.11 | Create `test/test_referencedata.py` | New wrappers | Test file | All `ReferenceDatum` subclasses round-trip; `BasicReferenceDataManager.fromFile()` reads fixture |
| 3.12 | Create `test/test_portfolio_extended.py` | New wrappers | Test file | XML round-trip per new trade type and leg type; `Portfolio.fromFile()` loads fixture with new trades |

### Task 4 — QuantExt Instruments and Engines (Phase 3)

| ID | Task | Input | Output | Acceptance Criteria |
|---|---|---|---|---|
| 4.1 | Create `qle_pricingengines.i` | C++ headers in `qle/pricingengines/` | New `.i` file | 8+ engines wrapped (see Phase 3 list) |
| 4.2 | Extend `qle_instruments.i` with new instruments | C++ headers in `qle/instruments/` | Extended `.i` file | MultiLegOption, BondTRS, ConvertibleBond2, etc. |
| 4.3 | Create `qle_cashflows_ext.i` | C++ headers in `qle/cashflows/` | New `.i` file | EquityCoupon, CommodityCashFlow, TRSCashFlow, etc. |
| 4.4 | Update `qle.i` to include new files | `qle.i` | Updated master | Clean compilation |
| 4.5 | Create `test/test_qle_pricingengines.py` | New wrappers | Test file | One NPV test per engine |
| 4.6 | Extend `test/test_instruments.py` | New instrument wrappers | Extended tests | Construction + basic accessor per instrument |
| 4.7 | Verify full test suite | All | Green | All old + new tests pass |

### Task 5 — QuantExt Models and Term Structures (Phase 4)

| ID | Task | Input | Output | Acceptance Criteria |
|---|---|---|---|---|
| 5.1 | Create `qle_models.i` | C++ headers in `qle/models/` | New `.i` file | CrossAssetModel, LGM, HwModel, parametrizations |
| 5.2 | Create `qle_termstructures_ext.i` | C++ headers in `qle/termstructures/` | New `.i` file | SpreadedSwaption, DynamicSwaptionVol, etc. |
| 5.3 | Extend `qle_processes.i` | C++ headers in `qle/processes/` | Extended `.i` file | CrossAssetStateProcess, IrLgm1f |
| 5.4 | Update `qle.i` to include new files | `qle.i` | Updated master | Clean compilation |
| 5.5 | Create `test/test_qle_models.py` | New wrappers | Test file | Model construction, dimension checks |
| 5.6 | Extend `test/test_termstructures.py` | New TS wrappers | Extended tests | Handle creation, discount/forward queries |
| 5.7 | Verify full test suite | All | Green | All old + new tests pass |

### Task 6 — OREData Model Builders (Phase 5)

| ID | Task | Input | Output | Acceptance Criteria |
|---|---|---|---|---|
| 6.1 | Create `ored_modelbuilders.i` | C++ headers in `ored/model/` | New `.i` file | CrossAssetModelBuilder, LGMBuilder, HWBuilder, FXBSBuilder |
| 6.2 | Update `ored.i` to include new file | `ored.i` | Updated master | Clean compilation |
| 6.3 | Create `test/test_modelbuilders.py` | New wrappers | Test file | Builder construction, output verification |
| 6.4 | Verify full test suite | All | Green | All old + new tests pass |

### Task 7 — OREAnalytics Core (Phase 6)

| ID | Task | Input | Output | Acceptance Criteria |
|---|---|---|---|---|
| 7.1 | Create `orea_scenario_ext.i` | C++ headers in `orea/scenario/` | New `.i` file | ScenarioSimMarket, generators, factories |
| 7.2 | Create `orea_engine.i` | C++ headers in `orea/engine/` | New `.i` file | SensitivityAnalysis, ValuationEngine, StressTest, VaR |
| 7.3 | Create `orea_aggregation.i` | C++ headers in `orea/aggregation/` | New `.i` file | PostProcess, ExposureCalculator, XvaCalculator |
| 7.4 | Create `orea_simulation.i` | C++ headers in `orea/simulation/` | New `.i` file | FixingManager |
| 7.5 | Update `orea.i` to include new files | `orea.i` | Updated master | Clean compilation |
| 7.6 | Create `test/test_sensitivity.py` | New wrappers | Test file | Run sensitivity, verify output shape |
| 7.7 | Create `test/test_scenarios.py` | New wrappers | Test file | Scenario construction, sim market setup |
| 7.8 | Create `test/test_aggregation.py` | New wrappers | Test file | PostProcess result verification |
| 7.9 | Verify full test suite | All | Green | All old + new tests pass |

### Task 8 — OREAnalytics SIMM and Analytics (Phase 7)

| ID | Task | Input | Output | Acceptance Criteria |
|---|---|---|---|---|
| 8.1 | Create `orea_simm.i` | C++ headers in `orea/simm/` | New `.i` file | CrifRecord, SimmCalculator, SimmConfiguration, SimmResults |
| 8.2 | Create `orea_analytics.i` | C++ headers in `orea/app/analytics/` | New `.i` file | PricingAnalytic, XvaAnalytic, SimmAnalytic, SaccrAnalytic |
| 8.3 | Extend `orea_cube.i` | C++ headers in `orea/cube/` | Extended `.i` file | SensitivityCube, CubeWriter, CubeReader |
| 8.4 | Update `orea.i` to include new files | `orea.i` | Updated master | Clean compilation |
| 8.5 | Create `test/test_simm.py` | New wrappers | Test file | CRIF construction, SIMM calculation |
| 8.6 | Create `test/test_analytics.py` | New wrappers | Test file | Analytic construction, run verification |
| 8.7 | Verify full test suite | All | Green | All old + new tests pass |

### Task 9 — Documentation and Examples (Phase 8)

| ID | Task | Input | Output | Acceptance Criteria |
|---|---|---|---|---|
| 9.1 | Add `%feature("docstring")` to all wrapped classes | All `.i` files | Docstrings in Python | `help(ORE.ClassName)` shows description |
| 9.2 | Uncomment `%feature("autodoc")` in `oreanalytics.i` | `oreanalytics.i` | Auto-generated method signatures | Method help shows parameter names/types |
| 9.3 | Create Sphinx config in `ore/ORE-SWIG/Docs/` | Compiled module | `conf.py`, `index.rst` | `make html` produces API docs |
| 9.4 | Write 6 tutorial notebooks | Wrapped module | Jupyter notebooks | Each notebook runs without errors |
| 9.5 | Add module overview `%pythoncode` blocks | Layer `.i` files | Module docstrings | `help(ORE)` shows overview |

### Task 10 — Final Review and Cleanup

| ID | Task | Input | Output | Acceptance Criteria |
|---|---|---|---|---|
| 10.1 | Full audit: every `%shared_ptr` matches C++ | All `.i` files | Audit report | No missing declarations |
| 10.2 | Full audit: no duplicate `%template` | All `.i` files | Audit report | Zero SWIG warnings for duplicates |
| 10.3 | Run `python setup.py wrap && build && test` on Linux | Full module | Green build+test | Cross-platform verification |
| 10.4 | Run `python setup.py wrap && build && test` on Windows | Full module | Green build+test | Cross-platform verification |
| 10.5 | Build wheel: `python -m build --wheel` | Full module | `.whl` file | Installable wheel |
| 10.6 | Verify `import ORE` in clean virtualenv | Wheel | Import success | Module loads with all new classes |
| 10.7 | Update `ore/ORE-SWIG/tutorials.*.md` with new coverage | Tutorials | Updated docs | Reflects new capabilities |
| 10.8 | Update this plan with lessons learned | Experience | Updated plan | Captures deviations and improvements |

---

## Appendix A — File Naming Conventions

| Convention | Example |
|---|---|
| QuantExt `.i` file | `qle_{topic}.i` (e.g. `qle_models.i`, `qle_pricingengines.i`) |
| OREData `.i` file | `ored_{topic}.i` (e.g. `ored_modelbuilders.i`, `ored_portfolio_ext.i`) |
| OREAnalytics `.i` file | `orea_{topic}.i` (e.g. `orea_engine.i`, `orea_simm.i`) |
| Shared typemap/infrastructure | `{layer}_common.i` or `{layer}_common_typemaps.i` |
| Test file | `test_{topic}.py` matching the `.i` file topic |

## Appendix B — Name Conflict Resolution Table

| C++ Fully Qualified Name | Python Name (Alias) | Reason |
|---|---|---|
| `ore::data::Swap` | `ORESwap` | Conflicts with `QuantLib::Swap` |
| `ore::data::Swaption` | `ORESwaption` | Conflicts with `QuantLib::Swaption` |
| `ore::data::FxForward` | `OREFxForward` | Conflicts with `QuantExt::FxForward` |
| `ore::data::EquityForward` | `OREEquityForward` | Conflicts with `QuantExt::EquityForward` |
| `ore::data::CapFloor` | `ORECapFloor` | Conflicts with `QuantLib::CapFloor` |
| `ore::data::Bond` | `OREBond` | Conflicts with `QuantLib::Bond` |
| `ore::data::CreditDefaultSwap` | `ORECreditDefaultSwap` | Conflicts with `QuantLib::CreditDefaultSwap` |
| `ore::data::CommodityForward` | `ORECommodityForward` | Conflicts with `QuantExt::CommodityForward` |
| `ore::data::CommoditySwap` | `ORECommoditySwap` | Conflicts with `QuantExt::CommoditySwap` |
| `ore::data::CommodityOption` | `ORECommodityOption` | Naming consistency |
| `ore::data::ForwardRateAgreement` | `OREForwardRateAgreement` | Conflicts with `QuantLib::ForwardRateAgreement` |
| `QuantExt::CdsOption` | `QLECdsOption` | Conflicts with `QuantLib::CdsOption` |
| `QuantExt::BlackCdsOptionEngine` | `QLEBlackCdsOptionEngine` | Conflicts with QuantLib engine |

## Appendix C — Checklist Template for Each New `.i` File

Use this checklist when creating any new SWIG interface file. **All items are required — do not skip validation.**

```
[ ] File has copyright header (copy from existing .i files)
[ ] File has #ifndef/#define/#endif header guard
[ ] File #includes prerequisite .i files (common.i, cashflows.i, etc.)
[ ] **BASE CLASS AUDIT — CRITICAL:**
    [ ] Identify ALL base classes (direct and transitive) for each wrapped class
    [ ] Verify EVERY base class is wrapped in an earlier-included .i file
    [ ] If base class missing: STOP and wrap it first
    [ ] Test with: python setup.py wrap 2>&1 | grep "Warning 401"
    [ ] If any "Warning 401" found: FIX immediately
[ ] All C++ fully-qualified names have `using` declarations in %{ %} block
[ ] All shared_ptr-managed classes have %shared_ptr(ClassName) BEFORE the class def
[ ] All nested structs/classes use %feature("flatnested")
[ ] All constructor signatures match C++ headers (check .hpp files)
[ ] All public methods from C++ are included (not just getters/setters)
[ ] All needed STL container templates are instantiated via %template
[ ] Name conflicts with QuantLib are resolved with aliases (see Appendix B)
[ ] %feature("docstring") is present for every class and key method
[ ] The file is included in the layer's master .i file (qle.i / ored.i / orea.i)
[ ] NO use of %nodefaultctor on classes should be public-constructible
[ ] At least one smoke test exists in test/
[ ] **SWIG WRAP VALIDATION:**
    [ ] python setup.py wrap completes WITHOUT "Warning 401" for your classes
    [ ] python setup.py wrap output does NOT contain "Nothing known about base class" for your new classes
    [ ] Check: python setup.py wrap 2>&1 | grep -c "Warning 401" — should be 0 or only pre-existing
[ ] python setup.py build succeeds
[ ] python setup.py test passes (all old + new tests)
[ ] New class is actually importable: python -c "from ORE import MyNewClass; print(MyNewClass)"
```

**CRITICAL VALIDATION GATE:** Do not merge or consider a phase complete until SWIG 401 warnings are zero for all new classes.
