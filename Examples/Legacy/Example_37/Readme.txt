Run Example
===========

python run.py

Description
===========

This example generates scenarios under a Hull-White model. The model is driven by two independent Brownian motions and
has four states. The diffusion matrix sigma is therefore 2 x 4. The reversion matrix is a 4 x 4 diagonal matrix and
entered as an array. Both diffusion and reversion are constant in time. Their values are not calibrated to the option
market, but hardcoded in simulation.xml.

The values for the diffusion and reversion matrices were fitted to the first two principal components of a
(hypothetical) analyis of absolute rate curve movements. These input principal components can be found in
inputeigenvectors.csv in the input folder. The tenor is given in years, and the two components are given as column
vectors:

+-----+--------------+---------------+
|tenor|eigenvector_1 |eigenvector_2  |
+-----+--------------+---------------+
|1    |0.353553390593|-0.537955502871|
+-----+--------------+---------------+
|2    |0.353553390593|-0.374924478795|
+-----+--------------+---------------+
|3    |0.353553390593|-0.252916811525|
+-----+--------------+---------------+
|5    |0.353553390593|-0.087587539893|
+-----+--------------+---------------+
|10   |0.353553390593|0.12267800393  |
+-----+--------------+---------------+
|15   |0.353553390593|0.240659435416 |
+-----+--------------+---------------+
|20   |0.353553390593|0.339148675322 |
+-----+--------------+---------------+
|30   |0.353553390593|0.552478951238 |
+-----+--------------+---------------+

The first eigenvector represent perfectly parallel movements. The second eigenvector represent a rotation around the 7y
point of the curve. Furthermore we prescribe an annual volatility of 0.0070 for the first components and 0.0030 for the
second one. The values can be compared to normal (bp) volatilities.

We follow Andersen, L., and Piterbarg, V. (2010): Interest Rate Modeling, Volume I-III, chapter 12.1.5 ``Multi-Factor
Statistical Gaussian Model'' to calibrate the diffusion and reversion matrices to the prescribed components and
volatilities. We do not detail the procedure here and refer the interested reader to the given reference.

The example generates a single monte carlo path with 5000 daily steps and outputs the generated scenarios in
scenariodump.csv. The python script pca.py performs a principal component analysis on this output. The model implied
eigenvalues are

+------+----------------------+
|number|value                 |
+------+----------------------+
|1     |4.9144936649319346e-05|
+------+----------------------+
|2     |8.846877641067412e-06 |
+------+----------------------+
|3     |5.82566039467854e-10  |
+------+----------------------+
|4     |2.1298948225571415e-10|
+------+----------------------+
|5     |9.254913949332787e-11 |
+------+----------------------+
|6     |1.0861256211767673e-11|
+------+----------------------+
|7     |8.478795662698618e-14 |
+------+----------------------+
|8     |9.74468069377584e-13  |
+------+----------------------+

Only the first two values are relevant, the following are all close to zero. The square root of the first two
eigenvalues is given by

+------+---------------------------+
|number|sqrt(value)                |
+------+---------------------------+
|1     |0.007010344973631422       |
+------+---------------------------+
|2     |0.0029743701250966414      |
+------+---------------------------+

matching the prescribed input values of 0.0070 and 0.0030 quite well. The corresponding eigenvectors are given by

+-----+-------------------+--------------------+
|tenor|eigenvector_1      |eigenvector_2       |
+-----+-------------------+--------------------+
|1    |0.34688826736335926|0.5441204725042812  |
+-----+-------------------+--------------------+
|2    |0.3489303472083185 |0.380259707350115   |
+-----+-------------------+--------------------+
|3    |0.350362134519783  |0.2581408080614405  |
+-----+-------------------+--------------------+
|5    |0.3523983915961889 |0.09230899007104967 |
+-----+-------------------+--------------------+
|10   |0.3550169593982022 |-0.11856777284904292|
+-----+-------------------+--------------------+
|15   |0.35647835947136625|-0.23676104168229614|
+-----+-------------------+--------------------+
|20   |0.3577146190751303 |-0.3354486339442275 |
+-----+-------------------+--------------------+
|30   |0.36042236352102563|-0.549124709243042  |
+-----+-------------------+--------------------+

again matching the input principal components quite well. The second eigenvector is the negative of the input vector
here (the principal component analysis can not distinguish these of course).

The example also produces a plot comparing the input eigenvectors and the model implied eigenvectors.
