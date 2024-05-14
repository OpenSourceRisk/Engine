Example using experimental xva cg engine.

Differences between AD and bump-and-revalue cva-sensis are driven by:

- Indicator derivatives (from the calculation step EPE = max( E, 0 ) = 1_{E>0} x E) – I think these should be turned off in our context here, since we are interested in T0 – expectations (CVA) ultimately. The calculation of indicator derivatives (when necessary) is notoriously difficult.

- Approximation error of conditional expectations E( | F) calculated by polynomial regression models. The regression models are trained under the base and bump scenario and generate different errors under each. With AAD we only look at the base scenario. This effect is hard to eliminate in the testing.

- Number of paths, i.e. the usual MC error (I looked at 232k in addition to the original 8k samples)

- Bump shift size, 1E-7 for IR curve and normal swaption vol actually gives better match than the original 1E-4
