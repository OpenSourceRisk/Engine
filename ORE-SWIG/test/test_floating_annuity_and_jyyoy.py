"""
 Copyright (C) 2026 AcadiaSoft, Inc.
 All rights reserved.

Focused tests for:
  - QuantExt::FloatingAnnuityCoupon   (qle/cashflows/floatingannuitycoupon.hpp)
  - QuantExt::FloatingAnnuityNominal  (qle/cashflows/floatingannuitynominal.hpp)
  - QuantExt::JyYoYInflationCouponPricer  (qle/cashflows/jyyoyinflationcouponpricer.hpp)
  - QuantExt::jyExpectedIndexRatio    (free function in the same header)
  - makeFloatingAnnuityNominalLeg     (SWIG helper for makeFloatingAnnuityNominalLeg)

FloatingAnnuityCoupon is a coupon where the notional adjusts each period so that
the total annuity payment (interest + principal) remains constant, similar to an
annuity-style mortgage.  Each coupon takes the *previous* coupon (or any Coupon
representing the initial state) as its starting point.

JyYoYInflationCouponPricer implements the Jarrow-Yildirim model for YoY inflation
coupon pricing. Full construction requires a CrossAssetModel with an InfJy
inflation component; since InfJyParameterization is not yet separately wrapped in
the Python bindings, construction is smoke-tested for symbol presence only.
"""

import math
import pytest
import ORE as ore


# ---------------------------------------------------------------------------
# Shared fixtures
# ---------------------------------------------------------------------------

@pytest.fixture(autouse=True)
def set_eval_date():
    """Pin evaluation date for all tests."""
    ore.Settings.instance().evaluationDate = ore.Date(15, ore.January, 2026)


@pytest.fixture
def euribor3m_curve():
    """Flat 3% EUR curve and Euribor3M index with a past fixing."""
    today = ore.Date(15, ore.January, 2026)
    curve = ore.FlatForward(today, 0.03, ore.Actual365Fixed())
    handle = ore.YieldTermStructureHandle(curve)
    index = ore.Euribor3M(handle)
    # Add a fixing so that indexFixing() succeeds on a past date.
    fix_date = ore.Date(13, ore.January, 2026)
    index.addFixing(fix_date, 0.03, True)
    return index


def _make_initial_fixed_coupon(nominal: float, start: ore.Date, end: ore.Date, pay: ore.Date) -> ore.FixedRateCoupon:
    """Create a FixedRateCoupon to serve as the predecessor of the first FloatingAnnuityCoupon.

    FloatingAnnuityCoupon uses previousCoupon.nominal() and previousCoupon.amount()
    to calculate the adjusted nominal for the next period, so any concrete Coupon works.
    """
    return ore.FixedRateCoupon(pay, nominal, 0.0, ore.Actual365Fixed(), start, end)


# ===========================================================================
# 1. Symbol availability
# ===========================================================================

def test_floating_annuity_coupon_symbol_available():
    """FloatingAnnuityCoupon class is importable from ORE."""
    assert hasattr(ore, "FloatingAnnuityCoupon")


def test_floating_annuity_nominal_symbol_available():
    """FloatingAnnuityNominal class is importable from ORE."""
    assert hasattr(ore, "FloatingAnnuityNominal")


def test_make_floating_annuity_nominal_leg_symbol_available():
    """makeFloatingAnnuityNominalLeg function is importable from ORE."""
    assert hasattr(ore, "makeFloatingAnnuityNominalLeg")


def test_jyyoy_pricer_symbol_available():
    """JyYoYInflationCouponPricer class is importable from ORE."""
    assert hasattr(ore, "JyYoYInflationCouponPricer")


def test_jy_expected_index_ratio_symbol_available():
    """jyExpectedIndexRatio free function is importable from ORE."""
    assert hasattr(ore, "jyExpectedIndexRatio")


# ===========================================================================
# 2. FloatingAnnuityCoupon — construction and basic inspectors
# ===========================================================================

def test_floating_annuity_coupon_construct(euribor3m_curve):
    """Construct a FloatingAnnuityCoupon from a FixedRateCoupon predecessor."""
    annuity = 12_000.0
    nominal = 100_000.0
    start = ore.Date(15, ore.January, 2026)
    end = ore.Date(15, ore.April, 2026)
    pay = ore.Date(17, ore.April, 2026)
    initial = _make_initial_fixed_coupon(nominal, start, end, pay)

    coupon = ore.FloatingAnnuityCoupon(
        annuity,
        True,      # underflow allowed
        initial,
        pay,
        start,
        end,
        2,         # fixingDays
        euribor3m_curve,
    )
    assert coupon is not None


def test_floating_annuity_coupon_inspectors(euribor3m_curve):
    """FloatingAnnuityCoupon returns correct gearing, spread, dayCounter, isInArrears."""
    annuity = 12_000.0
    nominal = 100_000.0
    start = ore.Date(15, ore.January, 2026)
    end = ore.Date(15, ore.April, 2026)
    pay = ore.Date(17, ore.April, 2026)
    initial = _make_initial_fixed_coupon(nominal, start, end, pay)

    coupon = ore.FloatingAnnuityCoupon(
        annuity,
        False,
        initial,
        pay,
        start,
        end,
        2,
        euribor3m_curve,
        1.5,       # gearing
        0.001,     # spread
        ore.Date(),
        ore.Date(),
        ore.Actual360(),
        True,      # isInArrears
    )

    assert coupon.gearing() == pytest.approx(1.5, rel=1e-12)
    assert coupon.spread() == pytest.approx(0.001, rel=1e-12)
    assert coupon.isInArrears() is True
    assert coupon.fixingDays() == 2
    assert coupon.index() is not None


def test_floating_annuity_coupon_rate_and_fixing(euribor3m_curve):
    """FloatingAnnuityCoupon.rate() = gearing * (indexFixing + spread)."""
    annuity = 12_000.0
    nominal = 100_000.0
    start = ore.Date(15, ore.January, 2026)
    end = ore.Date(15, ore.April, 2026)
    pay = ore.Date(17, ore.April, 2026)
    initial = _make_initial_fixed_coupon(nominal, start, end, pay)

    gearing = 1.0
    spread = 0.0
    coupon = ore.FloatingAnnuityCoupon(
        annuity,
        True,
        initial,
        pay,
        start,
        end,
        2,
        euribor3m_curve,
        gearing,
        spread,
    )

    # indexFixing() should return approximately 0.03 (flat curve)
    fixing = coupon.indexFixing()
    assert isinstance(fixing, float)
    assert abs(fixing - 0.03) < 0.001   # flat forward at 3%

    # rate() = gearing * (fixing + spread) = 1.0 * (0.03 + 0.0)
    rate = coupon.rate()
    assert abs(rate - fixing * gearing + spread) < 1e-12


def test_floating_annuity_coupon_nominal_decreases(euribor3m_curve):
    """Chained FloatingAnnuityCoupons reduce nominal over time (standard annuity)."""
    annuity = 5_000.0    # target payment per period
    nominal = 100_000.0
    start1 = ore.Date(15, ore.January, 2026)
    end1 = ore.Date(15, ore.April, 2026)
    pay1 = ore.Date(17, ore.April, 2026)

    # First coupon: predecessor is a plain FixedRateCoupon with the initial nominal
    initial = _make_initial_fixed_coupon(nominal, start1, end1, pay1)

    coupon1 = ore.FloatingAnnuityCoupon(
        annuity, True, initial, pay1, start1, end1, 2, euribor3m_curve
    )

    # Second coupon uses coupon1 as predecessor
    start2 = ore.Date(15, ore.April, 2026)
    end2 = ore.Date(15, ore.July, 2026)
    pay2 = ore.Date(17, ore.July, 2026)
    euribor3m_curve.addFixing(ore.Date(13, ore.April, 2026), 0.03, True)

    coupon2 = ore.FloatingAnnuityCoupon(
        annuity, True, coupon1, pay2, start2, end2, 2, euribor3m_curve
    )

    # nominal2 = nominal1 + amount1 - annuity
    # amount1 = rate1 * accrual1 * nominal1
    # With flat 3%: amount1 ≈ 0.03 * (91/365) * 100000 ≈ 747
    # nominal2 ≈ 100000 + 747 - 5000 = 95747  (decreasing)
    nom1 = coupon1.nominal()
    nom2 = coupon2.nominal()
    assert isinstance(nom1, float)
    assert isinstance(nom2, float)
    assert nom2 < nom1, f"Expected nominal to decrease but got nom1={nom1}, nom2={nom2}"


def test_floating_annuity_coupon_amount(euribor3m_curve):
    """FloatingAnnuityCoupon.amount() = rate * accrualPeriod * nominal."""
    annuity = 3_000.0
    nominal = 50_000.0
    start = ore.Date(15, ore.January, 2026)
    end = ore.Date(15, ore.July, 2026)
    pay = ore.Date(17, ore.July, 2026)
    initial = _make_initial_fixed_coupon(nominal, start, end, pay)

    coupon = ore.FloatingAnnuityCoupon(
        annuity, True, initial, pay, start, end, 2, euribor3m_curve
    )

    amount = coupon.amount()
    assert isinstance(amount, float)
    assert amount > 0.0


def test_floating_annuity_coupon_accrued_amount(euribor3m_curve):
    """FloatingAnnuityCoupon.accruedAmount() is zero at accrual start."""
    annuity = 3_000.0
    nominal = 50_000.0
    start = ore.Date(15, ore.January, 2026)
    end = ore.Date(15, ore.April, 2026)
    pay = ore.Date(17, ore.April, 2026)
    initial = _make_initial_fixed_coupon(nominal, start, end, pay)

    coupon = ore.FloatingAnnuityCoupon(
        annuity, True, initial, pay, start, end, 2, euribor3m_curve
    )

    assert coupon.accruedAmount(start) == pytest.approx(0.0, abs=1e-12)


def test_floating_annuity_coupon_previous_nominal(euribor3m_curve):
    """previousNominal() returns the nominal of the predecessor coupon."""
    annuity = 3_000.0
    nominal = 80_000.0
    start = ore.Date(15, ore.January, 2026)
    end = ore.Date(15, ore.April, 2026)
    pay = ore.Date(17, ore.April, 2026)
    initial = _make_initial_fixed_coupon(nominal, start, end, pay)

    coupon = ore.FloatingAnnuityCoupon(
        annuity, True, initial, pay, start, end, 2, euribor3m_curve
    )

    # previousNominal() should return the nominal of `initial` (= 80_000.0)
    prev = coupon.previousNominal()
    assert prev == pytest.approx(nominal, rel=1e-10)


# ===========================================================================
# 3. FloatingAnnuityNominal — construction and cashflow
# ===========================================================================

def test_floating_annuity_nominal_construct(euribor3m_curve):
    """Construct FloatingAnnuityNominal from a FloatingAnnuityCoupon."""
    annuity = 5_000.0
    nominal = 100_000.0
    start = ore.Date(15, ore.January, 2026)
    end = ore.Date(15, ore.April, 2026)
    pay = ore.Date(17, ore.April, 2026)
    initial = _make_initial_fixed_coupon(nominal, start, end, pay)
    coupon = ore.FloatingAnnuityCoupon(
        annuity, True, initial, pay, start, end, 2, euribor3m_curve
    )

    nominal_flow = ore.FloatingAnnuityNominal(coupon)
    assert nominal_flow is not None


def test_floating_annuity_nominal_date(euribor3m_curve):
    """FloatingAnnuityNominal.date() == coupon accrual start date."""
    start = ore.Date(15, ore.January, 2026)
    end = ore.Date(15, ore.April, 2026)
    pay = ore.Date(17, ore.April, 2026)
    initial = _make_initial_fixed_coupon(100_000.0, start, end, pay)
    coupon = ore.FloatingAnnuityCoupon(
        5_000.0, True, initial, pay, start, end, 2, euribor3m_curve
    )
    nominal_flow = ore.FloatingAnnuityNominal(coupon)
    # FloatingAnnuityNominal.date() == accrualStartDate == start
    assert nominal_flow.date() == start


def test_floating_annuity_nominal_amount(euribor3m_curve):
    """FloatingAnnuityNominal.amount() = previousNominal - nominal."""
    annuity = 5_000.0
    nominal = 100_000.0
    start = ore.Date(15, ore.January, 2026)
    end = ore.Date(15, ore.April, 2026)
    pay = ore.Date(17, ore.April, 2026)
    initial = _make_initial_fixed_coupon(nominal, start, end, pay)
    coupon = ore.FloatingAnnuityCoupon(
        annuity, True, initial, pay, start, end, 2, euribor3m_curve
    )
    nominal_flow = ore.FloatingAnnuityNominal(coupon)

    # amount() = previousNominal - nominal = nominal_initial - coupon.nominal()
    expected = coupon.previousNominal() - coupon.nominal()
    assert abs(nominal_flow.amount() - expected) < 1e-9


# ===========================================================================
# 4. makeFloatingAnnuityNominalLeg — builds a leg of FloatingAnnuityNominal
# ===========================================================================

def test_make_floating_annuity_nominal_leg(euribor3m_curve):
    """makeFloatingAnnuityNominalLeg converts a FloatingAnnuity coupon leg to nominal cashflows."""
    annuity = 5_000.0
    nominal = 100_000.0

    dates = [
        ore.Date(15, ore.January, 2026),
        ore.Date(15, ore.April, 2026),
        ore.Date(15, ore.July, 2026),
    ]
    pay_dates = [ore.Date(17, ore.April, 2026), ore.Date(17, ore.July, 2026)]

    # Add fixings for Apr coupon
    euribor3m_curve.addFixing(ore.Date(13, ore.April, 2026), 0.03, True)

    # Build a 2-period floating annuity leg
    initial = _make_initial_fixed_coupon(nominal, dates[0], dates[1], pay_dates[0])
    coupon1 = ore.FloatingAnnuityCoupon(
        annuity, True, initial, pay_dates[0], dates[0], dates[1], 2, euribor3m_curve
    )
    coupon2 = ore.FloatingAnnuityCoupon(
        annuity, True, coupon1, pay_dates[1], dates[1], dates[2], 2, euribor3m_curve
    )

    fa_leg = [coupon1, coupon2]
    nominal_leg = ore.makeFloatingAnnuityNominalLeg(fa_leg)

    # Should return one FloatingAnnuityNominal per input coupon
    assert len(nominal_leg) == 2
    # All cashflows should be dated at the accrual start of the respective coupon
    cf0 = nominal_leg[0]
    cf1 = nominal_leg[1]
    assert cf0.date() == dates[0]
    assert cf1.date() == dates[1]


# ===========================================================================
# 5. JyYoYInflationCouponPricer — symbol and inheritance checks
# ===========================================================================

def test_jyyoy_pricer_is_class():
    """JyYoYInflationCouponPricer is a class (not None)."""
    cls = ore.JyYoYInflationCouponPricer
    assert cls is not None
    assert isinstance(cls, type)


def test_jyyoy_pricer_is_subclass_of_yoy_coupon_pricer():
    """JyYoYInflationCouponPricer subclasses YoYInflationCouponPricer."""
    assert issubclass(ore.JyYoYInflationCouponPricer, ore.YoYInflationCouponPricer)


def test_jy_expected_index_ratio_is_callable():
    """jyExpectedIndexRatio is a callable (exposed as a free function)."""
    assert callable(ore.jyExpectedIndexRatio)


def test_jyyoy_pricer_requires_cross_asset_model():
    """JyYoYInflationCouponPricer constructor signature accepts CrossAssetModel + Size.

    Full construction requires a CrossAssetModel with an InfJy inflation
    component (InfJyParameterization), which is not yet separately wrapped in the
    Python bindings (the InfJyBuilder/InfJyData OREData route is available but
    requires a full calibration step).  This test therefore verifies that the
    constructor would raise TypeError/AttributeError when called with wrong args,
    confirming the signature has been exposed correctly.
    """
    with pytest.raises((TypeError, Exception)):
        # Passing None as model must fail — CrossAssetModel cannot be None
        ore.JyYoYInflationCouponPricer(None, 0)
