"""
 Copyright (C) 2026 AcadiaSoft, Inc.
 All rights reserved.
"""

import ORE as ore


def test_bma_cashflow_wrappers_construct():
    """Construct the BMA coupon and Black pricer wrappers."""
    payment_date = ore.Date(15, ore.January, 2027)
    start_date = ore.Date(15, ore.January, 2026)
    end_date = ore.Date(15, ore.January, 2027)
    bma_index = ore.BMAIndex()
    underlying = ore.AverageBMACoupon(
        payment_date, 1_000_000.0, start_date, end_date, bma_index)
    coupon = ore.CappedFlooredAverageBMACoupon(
        underlying, 0.05, 0.0)
    pricer = ore.BlackAverageBMACouponPricer(
        ore.OptionletVolatilityStructureHandle())

    assert coupon.underlying() is not None
    assert coupon.isCapped()
    assert coupon.isFloored()
    assert pricer is not None


def test_cmb_and_zero_fixed_coupon_wrappers_construct():
    """Construct and inspect CMB and zero-fixed coupon wrappers."""
    payment_date = ore.Date(15, ore.January, 2027)
    start_date = ore.Date(15, ore.January, 2026)
    end_date = ore.Date(15, ore.January, 2027)
    cmb_index = ore.ConstantMaturityBondIndex(
        "Test CMB", ore.Period(10, ore.Years))
    cmb_coupon = ore.CmbCoupon(
        payment_date, 1_000_000.0, start_date, end_date, 2, cmb_index)
    zero_coupon = ore.ZeroFixedCoupon(
        payment_date, 1_000_000.0, 0.02, ore.Actual365Fixed(),
        [start_date, end_date], ore.Compounded, True)

    assert cmb_coupon.bondIndex() is not None
    assert zero_coupon.nominal() == 1_000_000.0
    assert zero_coupon.compounding() == ore.Compounded
