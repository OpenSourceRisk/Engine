"""
Created on Mon Oct 7 18:33:39 2019

@author: cantaro86

source: https://github.com/cantaro86/Financial-Models-Numerical-Methods
"""

import numpy as np
from scipy.integrate import quad
from functools import partial

def Q1(k, cf, right_lim):
    """
    P(X<k) - Probability to be in the money under the stock numeraire.
    cf: characteristic function
    right_lim: right limit of integration
    """

    def integrand(u):
        return np.real((np.exp(-u * k * 1j) / (u * 1j)) * cf(u - 1j) / cf(-1.0000000000001j))

    return 1 / 2 + 1 / np.pi * quad(integrand, 1e-15, right_lim, limit=2000)[0]


def Q2(k, cf, right_lim):
    """
    P(X<k) - Probability to be in the money under the money market numeraire
    cf: characteristic function
    right_lim: right limit of integration
    """

    def integrand(u):
        return np.real(np.exp(-u * k * 1j) / (u * 1j) * cf(u))

    return 1 / 2 + 1 / np.pi * quad(integrand, 1e-15, right_lim, limit=2000)[0]


def Gil_Pelaez_pdf(x, cf, right_lim):
    """
    Gil Pelaez formula for the inversion of the characteristic function
    INPUT
    - x: is a number
    - right_lim: is the right extreme of integration
    - cf: is the characteristic function
    OUTPUT
    - the value of the density at x.
    """

    def integrand(u):
        return np.real(np.exp(-u * x * 1j) * cf(u))

    return 1 / np.pi * quad(integrand, 1e-15, right_lim)[0]


def hestonCF(u, t, v0, mu, kappa, theta, sigma, rho):
    """
    Stable Heston characteristic function as proposed by Schoutens (2004)
    """
    xi = kappa - sigma * rho * u * 1j
    d = np.sqrt(xi**2 + sigma**2 * (u**2 + 1j * u))
    g1 = (xi + d) / (xi - d)
    g2 = 1 / g1
    cf = np.exp(
        1j * u * mu * t
        + (kappa * theta) / (sigma**2) * ((xi - d) * t - 2 * np.log((1 - g2 * np.exp(-d * t)) / (1 - g2)))
        + (v0 / sigma**2) * (xi - d) * (1 - np.exp(-d * t)) / (1 - g2 * np.exp(-d * t))
    )
    return cf

def Heston_pdf(i, t, v0, mu, theta, sigma, kappa, rho):
    """
    Heston density by Fourier inversion.
    """
    cf_H_b_good = partial(
        hestonCF,
        t=t,
        v0=v0,
        mu=mu,
        theta=theta,
        sigma=sigma,
        kappa=kappa,
        rho=rho,
    )
    return Gil_Pelaez_pdf(i, cf_H_b_good, np.inf)

def hestonCallPrice(S, K, T, r, cf):
    limit_max = 1000 # right limit in the integration
    k = np.log(K / S) # the cumulative probability function Q1 takes log moneyness
    call = S * Q1(k, cf, limit_max) - K * np.exp(-r * T) * Q2(k, cf, limit_max)
    return call

def hestonPutPrice(S, K, T, r, cf):
    limit_max = 1000 # right limit in the integration
    k = np.log(K / S) # the cumulative probability function Q1 takes log moneyness
    put = K * np.exp(-r * T) * (1 - Q2(k, cf, limit_max)) - S * (1 - Q1(k, cf, limit_max))
    return put

