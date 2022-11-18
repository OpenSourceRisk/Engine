Description
===========

This example is similar to Example 8 (EPE, ENE for xccy swap), but uses a multifactor HW model for EUR and USD to
generate scenarios. The parametrization of the HW models is taken from Example 37.

Each of the two factors of each HW model is correlated with each of the two factors of the other currency's HW model and
with the FX factors. Remember that the factors represent principal components of interest rate movements and so the
correlations can be interpreted as correlations of these principal components with each other and the fx rate processes.

Run Example
============

python run.py
