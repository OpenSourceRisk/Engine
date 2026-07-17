"""
 Copyright (C) 2026 AcadiaSoft, Inc.
 All rights reserved.

Focused tests for the three concrete QuantExt NonStandard YoY inflation coupon
pricer wrappers:
  - NonStandardBlackYoYInflationCouponPricer
  - NonStandardUnitDisplacedBlackYoYInflationCouponPricer
  - NonStandardBachelierYoYInflationCouponPricer

All three extend NonStandardYoYInflationCouponPricer and are used together with
NonStandardYoYInflationCoupon / NonStandardCappedFlooredYoYInflationCoupon.
"""

import pytest
import ORE as ore


# ---------------------------------------------------------------------------
# Parametrize over all three concrete pricer class names
# ---------------------------------------------------------------------------
PRICER_NAMES = [
    "NonStandardBlackYoYInflationCouponPricer",
    "NonStandardUnitDisplacedBlackYoYInflationCouponPricer",
    "NonStandardBachelierYoYInflationCouponPricer",
]


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------

@pytest.fixture()
def nominal_handle():
    today = ore.Date(15, ore.January, 2026)
    curve = ore.FlatForward(today, 0.03, ore.Actual365Fixed())
    return ore.YieldTermStructureHandle(curve)


@pytest.fixture()
def caplet_vol_handle():
    return ore.YoYOptionletVolatilitySurfaceHandle()


@pytest.fixture(autouse=True)
def set_eval_date():
    ore.Settings.instance().evaluationDate = ore.Date(15, ore.January, 2026)


@pytest.fixture()
def plain_nonstd_yoy_coupon_with_fixings():
    """NonStandardYoYInflationCoupon with fully-observed (past) fixings.

    Observation lag = 3 months, so:
      denominator: startDate(Jan-2025) - 3M -> Oct-2024 -> UKRPI Oct-1-2024
      numerator:   endDate  (Jan-2026) - 3M -> Oct-2025 -> UKRPI Oct-1-2025
    Fixings: 300 and 312 → YoY rate = 312/300 - 1 = 4%.
    """
    index = ore.UKRPI()
    index.addFixing(ore.Date(1, ore.October, 2024), 300.0, True)
    index.addFixing(ore.Date(1, ore.October, 2025), 312.0, True)

    obs_lag = ore.Period(3, ore.Months)
    start_date = ore.Date(15, ore.January, 2025)
    end_date = ore.Date(15, ore.January, 2026)
    pay_date = ore.Date(17, ore.February, 2026)

    underlying = ore.NonStandardYoYInflationCoupon(
        pay_date,
        1_000_000.0,
        start_date,
        end_date,
        0,           # fixingDays
        index,
        obs_lag,
        ore.Actual365Fixed(),
        1.0,         # gearing
        0.0,         # spread
        start_date,  # refPeriodStart (required to set fixing dates)
        end_date,    # refPeriodEnd
    )
    # Wrap in NonStandardCappedFlooredYoYInflationCoupon without cap/floor
    # so setPricer accepts NonStandardYoYInflationCouponPricer directly.
    return ore.NonStandardCappedFlooredYoYInflationCoupon(underlying)


# ---------------------------------------------------------------------------
# 1. Symbol availability
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("name", PRICER_NAMES)
def test_symbol_available_in_ore_module(name):
    """Each concrete pricer class is importable from ORE."""
    assert hasattr(ore, name), f"Missing symbol in ORE module: {name}"


# ---------------------------------------------------------------------------
# 2. Construction — nominal-only constructor
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("name", PRICER_NAMES)
def test_construct_nominal_only(name, nominal_handle):
    """Construct each pricer with just a nominal yield term structure."""
    cls = getattr(ore, name)
    pricer = cls(nominal_handle)
    assert pricer is not None


# ---------------------------------------------------------------------------
# 3. Construction — caplet vol + nominal constructor
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("name", PRICER_NAMES)
def test_construct_caplet_vol_and_nominal(name, caplet_vol_handle, nominal_handle):
    """Construct each pricer with an empty caplet vol surface and nominal curve."""
    cls = getattr(ore, name)
    pricer = cls(caplet_vol_handle, nominal_handle)
    assert pricer is not None


# ---------------------------------------------------------------------------
# 4. Inheritance — each pricer is-a NonStandardYoYInflationCouponPricer
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("name", PRICER_NAMES)
def test_isinstance_base_pricer(name, nominal_handle):
    """Each concrete pricer is-a NonStandardYoYInflationCouponPricer."""
    cls = getattr(ore, name)
    pricer = cls(nominal_handle)
    assert isinstance(pricer, ore.NonStandardYoYInflationCouponPricer)


# ---------------------------------------------------------------------------
# 5. Accessors
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("name", PRICER_NAMES)
def test_nominal_term_structure_accessor(name, nominal_handle):
    """nominalTermStructure() returns a non-None handle."""
    cls = getattr(ore, name)
    pricer = cls(nominal_handle)
    assert pricer.nominalTermStructure() is not None


@pytest.mark.parametrize("name", PRICER_NAMES)
def test_caplet_volatility_accessor_empty(name, nominal_handle):
    """capletVolatility() returns a non-None handle (empty but valid)."""
    cls = getattr(ore, name)
    pricer = cls(nominal_handle)
    assert pricer.capletVolatility() is not None


# ---------------------------------------------------------------------------
# 6. Behavioral — attach to coupon and verify swaplet rate
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("name", PRICER_NAMES)
def test_swaplet_rate_with_past_fixings(
    name, nominal_handle, plain_nonstd_yoy_coupon_with_fixings
):
    """After attaching pricer, rate() equals the observed YoY ratio (4%).

    For a plain (uncapped/unfloored) coupon with gearing=1, spread=0:
      rate = I_t/I_s - 1 = 312/300 - 1 = 0.04
    This must hold for all three pricer variants.
    """
    cls = getattr(ore, name)
    pricer = cls(nominal_handle)
    coupon = plain_nonstd_yoy_coupon_with_fixings
    coupon.setPricer(pricer)

    rate = coupon.rate()
    assert isinstance(rate, float), f"{name}: rate() did not return float"
    assert abs(rate - 0.04) < 1e-9, (
        f"{name}: expected rate ≈ 0.04, got {rate}"
    )


# ---------------------------------------------------------------------------
# 7. Behavioral — capped coupon clamps rate below cap
# ---------------------------------------------------------------------------

def test_black_pricer_cap_clamps_rate(nominal_handle):
    """A capped NonStandardCappedFlooredYoYInflationCoupon returns at most cap."""
    index = ore.UKRPI()
    index.addFixing(ore.Date(1, ore.October, 2024), 300.0, True)
    index.addFixing(ore.Date(1, ore.October, 2025), 312.0, True)  # 4% — above cap

    obs_lag = ore.Period(3, ore.Months)
    start_date = ore.Date(15, ore.January, 2025)
    end_date = ore.Date(15, ore.January, 2026)
    pay_date = ore.Date(17, ore.February, 2026)

    underlying = ore.NonStandardYoYInflationCoupon(
        pay_date, 1_000_000.0, start_date, end_date,
        0, index, obs_lag, ore.Actual365Fixed(), 1.0, 0.0,
        start_date, end_date,
    )
    cap = 0.03          # 3% cap — observed rate is 4% so cap bites
    floor = ore.nullDouble()
    capped = ore.NonStandardCappedFlooredYoYInflationCoupon(underlying, cap, floor)

    pricer = ore.NonStandardBlackYoYInflationCouponPricer(nominal_handle)
    capped.setPricer(pricer)

    assert capped.isCapped()
    assert not capped.isFloored()
    rate = capped.rate()
    assert rate <= cap + 1e-9, f"Capped rate {rate} exceeds cap {cap}"


# ---------------------------------------------------------------------------
# 8. Behavioral — floored coupon raises rate above floor
# ---------------------------------------------------------------------------

def test_bachelier_pricer_floor_raises_rate(nominal_handle):
    """A floored NonStandardCappedFlooredYoYInflationCoupon returns at least floor."""
    index = ore.UKRPI()
    index.addFixing(ore.Date(1, ore.October, 2024), 300.0, True)
    index.addFixing(ore.Date(1, ore.October, 2025), 303.0, True)  # 1% — below floor

    obs_lag = ore.Period(3, ore.Months)
    start_date = ore.Date(15, ore.January, 2025)
    end_date = ore.Date(15, ore.January, 2026)
    pay_date = ore.Date(17, ore.February, 2026)

    underlying = ore.NonStandardYoYInflationCoupon(
        pay_date, 1_000_000.0, start_date, end_date,
        0, index, obs_lag, ore.Actual365Fixed(), 1.0, 0.0,
        start_date, end_date,
    )
    cap = ore.nullDouble()
    floor = 0.02        # 2% floor — observed rate is 1% so floor bites

    floored = ore.NonStandardCappedFlooredYoYInflationCoupon(underlying, cap, floor)

    pricer = ore.NonStandardBachelierYoYInflationCouponPricer(nominal_handle)
    floored.setPricer(pricer)

    assert not floored.isCapped()
    assert floored.isFloored()
    rate = floored.rate()
    assert rate >= floor - 1e-9, f"Floored rate {rate} is below floor {floor}"
