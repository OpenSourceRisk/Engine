"""
 Copyright (C) 2026 AcadiaSoft, Inc.
 All rights reserved.
"""
# noinspection PyPep8Naming
import ORE as ore
import pytest

def create_discount_curve(eval_date):
    disc_data = {
        0: 1.000000000,
        1: 0.999969863,
        2: 0.999937353,
        3: 0.999903139,
        4: 0.999864017,
        5: 0.999824880,
        6: 0.999784156,
        7: 0.999739477,
        8: 0.999688193,
        9: 0.999632828,
        10: 0.999576202,
        11: 0.999519599,
        12: 0.999462613,
        13: 0.999405507,
        14: 0.999335271,
        15: 0.999266198,
        16: 0.999203719,
        17: 0.999134111,
        18: 0.999065005,
        19: 0.998996331,
        20: 0.998915909,
        21: 0.998823996,
        22: 0.998726216,
        23: 0.998637407,
        24: 0.998545094,
        25: 0.998455406,
        26: 0.998367992,
        27: 0.998267186,
        28: 0.998175836,
        29: 0.998065381,
        30: 0.997956526,
        31: 0.997860354,
        32: 0.997758343,
        33: 0.997643778,
        34: 0.997544354,
        35: 0.997435933,
        36: 0.997333220,
        37: 0.997189585,
        38: 0.996938409,
        39: 0.996571583,
        40: 0.996081047
    }

    dates = [eval_date + num_days for num_days in disc_data.keys()]
    discounts = list(disc_data.values())

    return ore.DiscountCurve(dates, discounts, ore.Actual365Fixed())

def load_fixings_up_to(eval_date, ir_index):
    sofr_fixings = {
        ore.Date(24,  ore.October, 2025): 0.0424,
        ore.Date(24,  ore.October, 2025): 0.0424,
        ore.Date(27,  ore.October, 2025): 0.0427,
        ore.Date(28,  ore.October, 2025): 0.0431,
        ore.Date(29,  ore.October, 2025): 0.0427,
        ore.Date(30,  ore.October, 2025): 0.0404,
        ore.Date(31,  ore.October, 2025): 0.0422,
        ore.Date( 3, ore.November, 2025): 0.0413,
        ore.Date( 4, ore.November, 2025): 0.0400,
        ore.Date( 5, ore.November, 2025): 0.0391,
        ore.Date( 6, ore.November, 2025): 0.0392,
        ore.Date( 7, ore.November, 2025): 0.0393,
        ore.Date(10, ore.November, 2025): 0.0395,
        ore.Date(12, ore.November, 2025): 0.0398,
        ore.Date(13, ore.November, 2025): 0.0400,
        ore.Date(14, ore.November, 2025): 0.0395,
        ore.Date(17, ore.November, 2025): 0.0400,
        ore.Date(18, ore.November, 2025): 0.0394,
        ore.Date(19, ore.November, 2025): 0.0391,
        ore.Date(20, ore.November, 2025): 0.0391,
        ore.Date(21, ore.November, 2025): 0.0393,
        ore.Date(24, ore.November, 2025): 0.0396,
        ore.Date(25, ore.November, 2025): 0.0401,
        ore.Date(26, ore.November, 2025): 0.0405,
        ore.Date(28, ore.November, 2025): 0.0412,
        ore.Date( 1, ore.December, 2025): 0.0412,
        ore.Date( 2, ore.December, 2025): 0.0401
    }

    for fixing_date, fixing_value in sofr_fixings.items():
        if fixing_date >= eval_date:
            break
        ir_index.addFixing(fixing_date, fixing_value)

def test_qle_overnightindexedcoupon():
    # Create SOFR
    sofr_termstructure = ore.RelinkableYieldTermStructureHandle()
    sofr_index = ore.Sofr(sofr_termstructure)

    # Create the coupon
    cpn = ore.QLEOvernightIndexedCoupon(
        ore.Date(1, ore.December, 2025),  # payment date
        100000000,  # notional
        ore.Date(1, ore.November, 2025),  # start date
        ore.Date(30, ore.November, 2025),  # end date
        sofr_index,
        1.0,  # gearing
        0.0,  # spread
        ore.Date(),  # reference period start date
        ore.Date(),  # reference period end date
        ore.Actual360(),  # can't pass DayCounter() so pass the SOFR Act/360 day counter
        True,  # telescopic value dates
        False,  # include spread, doesn't matter here as spread is 0
        ore.Period(0, ore.Days),  # lookback period
        0,  # rate cut-off
        ore.nullInt(),  # fixing days, don't set to allow use of index fixing days
        ore.Date(),  # rate computation start date
        ore.Date(),  # rate computation end date
        False # observation shift
    )

    # For a selection of evaluation dates, check the accrual amount for a few dates and the full coupon amount against
    # known values taken from QuantExt/test/overnightindexedcoupon.cpp. A full grid of checks is performed there and
    # not repeated here. It is performed as one test to make sure the evaluation date movement works correctly.

    # Expected results. Dict key is the evaluation date and the inner accruals Dict key is the accrual date to check.
    exp_results = {
        ore.Date(30, ore.October, 2025) : {
            "full_coupon" : 207835.93,
            "accruals" : {
                ore.Date( 2, ore.November, 2025):   3528.68,
                ore.Date( 9, ore.November, 2025):  35894.26,
                ore.Date(13, ore.November, 2025):  59970.86,
                ore.Date(21, ore.November, 2025): 120990.62,
            }
        },
        ore.Date(18, ore.November, 2025): {
            "full_coupon": 242715.08,
            "accruals": {
                ore.Date( 6, ore.November, 2025):  56900.46,
                ore.Date(21, ore.November, 2025): 198529.81,
                ore.Date(24, ore.November, 2025): 210454.30,
                ore.Date(28, ore.November, 2025): 231302.30,
            }
        }
    }

    # Perform the evaluation date updates and checks.
    for eval_date, exp_res_eval_date in exp_results.items():
        ore.Settings.instance().evaluationDate = eval_date
        sofr_termstructure.linkTo(create_discount_curve(eval_date))
        load_fixings_up_to(eval_date, sofr_index)
        calc_full_coupon = cpn.amount()
        assert calc_full_coupon == pytest.approx(exp_res_eval_date["full_coupon"], abs=1e-2)
        for accrual_date, exp_accrual_amount in exp_res_eval_date["accruals"].items():
            calc_accrual_amount = cpn.accruedAmount(accrual_date)
            assert calc_accrual_amount == pytest.approx(exp_accrual_amount, abs=1e-2)

def test_qle_averageovernightindexedcoupon():
    # Create SOFR
    sofr_termstructure = ore.RelinkableYieldTermStructureHandle()
    sofr_index = ore.Sofr(sofr_termstructure)

    # Create the coupon
    cpn = ore.AverageONIndexedCoupon(
        ore.Date(1, ore.December, 2025),  # payment date
        100000000,  # notional
        ore.Date(1, ore.November, 2025),  # start date
        ore.Date(30, ore.November, 2025),  # end date
        sofr_index,
        1.0,  # gearing
        0.0,  # spread
        0,  # rate cut-off
        ore.Actual360(),  # can't pass DayCounter() so pass the SOFR Act/360 day counter
        ore.Period(0, ore.Days),  # lookback period
        ore.nullInt(),  # fixing days, don't set to allow use of index fixing days
        ore.Date(),  # rate computation start date
        ore.Date(),  # rate computation end date
        True,  # telescopic value dates
        False # observation shift
    )

    # For a selection of evaluation dates, check the accrual amount for a few dates and the full coupon amount against
    # known values taken from QuantExt/test/overnightindexedcoupon.cpp. A full grid of checks is performed there and
    # not repeated here. It is performed as one test to make sure the evaluation date movement works correctly.

    # Expected results. Dict key is the evaluation date and the inner accruals Dict key is the accrual date to check.
    exp_results = {
        ore.Date(30, ore.October, 2025) : {
            "full_coupon" : 207622.60,
            "accruals" : {
                ore.Date( 2, ore.November, 2025):   3528.68,
                ore.Date( 9, ore.November, 2025):  35888.70,
                ore.Date(13, ore.November, 2025):  59953.14,
                ore.Date(21, ore.November, 2025): 120917.73,
            }
        },
        ore.Date(18, ore.November, 2025): {
            "full_coupon": 242441.93,
            "accruals": {
                ore.Date( 6, ore.November, 2025):  56888.89,
                ore.Date(21, ore.November, 2025): 198353.28,
                ore.Date(24, ore.November, 2025): 210253.44,
                ore.Date(28, ore.November, 2025): 231055.49,
            }
        }
    }

    # Perform the evaluation date updates and checks.
    for eval_date, exp_res_eval_date in exp_results.items():
        ore.Settings.instance().evaluationDate = eval_date
        sofr_termstructure.linkTo(create_discount_curve(eval_date))
        load_fixings_up_to(eval_date, sofr_index)
        calc_full_coupon = cpn.amount()
        assert calc_full_coupon == pytest.approx(exp_res_eval_date["full_coupon"], abs=1e-2)
        for accrual_date, exp_accrual_amount in exp_res_eval_date["accruals"].items():
            calc_accrual_amount = cpn.accruedAmount(accrual_date)
            assert calc_accrual_amount == pytest.approx(exp_accrual_amount, abs=1e-2)