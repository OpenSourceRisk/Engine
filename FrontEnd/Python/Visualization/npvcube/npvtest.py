# Copyright (C) 2016 Quaternion Risk Management Ltd.
# All rights reserved.

from npvlib import NpvCube
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
# import seaborn # slightly improves look of the plots

csv_file = 'cube_full.csv'
npv = NpvCube(csv_file)
trade = '833397AI'

fig = plt.figure()
ax = fig.add_subplot(111, projection='3d')
#npv.plot_density(trade, ax, 'NPV-')
npv.plot_trade(trade, ax)
plt.show()

# if percentile:
#     for k in range(len(dates)):
#         areas = cumtrapz(values[k], dist_space, initial=0)
#         values[k] = np.array([values[k][j]
#                               if areas[j] <= percentile else 0
#                               for j in range(len(values[k]))])
#     ax.plot_trisurf(X.flatten(),
#                     Y.flatten(),
#                     values.T.flatten(),
#                     cmap=cm.jet,
#                     # color='r',
#                     linewidth=0)