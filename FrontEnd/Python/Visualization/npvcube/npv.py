# Copyright (C) 2016 Quaternion Risk Management Ltd.
# All rights reserved.

import matplotlib
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from matplotlib import cm
from mpl_toolkits.mplot3d import Axes3D
from numpy.linalg.linalg import LinAlgError
from scipy.stats.kde import gaussian_kde


class NpvCube(object):

    def __init__(self, csv_file):
        self.df = pd.read_csv(csv_file)
        self.df.columns = ['TradeID', 'DateIndex', 'Date', 'Sample', 'Value']
        self.nd = np.array([])
        self.datedict = {}
        self.tradedict = {}
        self.fig = plt.figure()
        self.calc_nd()

    def calc_nd(self):
        """ Converts DataFrame into 3D array. """
        dates = self.df.Date.unique()
        trades = self.df.TradeID.unique()
        self.datedict = {val: num for num, val in enumerate(dates)}
        self.tradedict = {val: num for num, val in enumerate(trades)}
        self.nd = np.zeros((len(dates), len(trades), self.df.Sample.max() + 1))
        for row in self.df.values:
            i = self.datedict[row[2]]
            j = self.tradedict[row[0]]
            k = row[3]
            self.nd[i][j][k] = row[4]

    def plot_trade(self, trade):
        ax = self.fig.gca(projection='3d')
        dates = np.arange(len(self.datedict))
        samples = np.arange(self.df.Sample.max() + 1)
        dates, samples = np.meshgrid(dates, samples)
        ax.plot_surface(dates, samples, self.nd[:, self.tradedict[trade], :].T)
        ax.set_xlabel('dates')
        ax.set_xlim3d(0, len(self.df.Date.unique()))
        ax.set_xticklabels = self.df.Date.unique()
        ax.set_ylabel('samples')
        ax.set_yticklabels = self.df.Sample.unique()
        ax.set_zlabel('value')

    def plot_density(self, trade):
        """
        Plots the NPV surface for trade.
        X-axis: dates
        Y-axis: npv
        Z-axis: value of the estimated pdf
        """
        ax = self.fig.gca(projection='3d')
        # remove last (erroneuous) date
        dates = self.df.Date.unique()[1:-1]
        # configure x-axis to be labeld with dates
        date_step = 5
        ax.set_xticks(np.arange(0, len(dates), date_step))
        ax.set_xticklabels(dates[0::date_step])
        ax.set_xlabel('Dates')
        # set thousands separator for y-axis
        ax.get_yaxis().set_major_formatter(matplotlib.ticker.FuncFormatter(
            lambda x, p: format(int(x), ',')))
        ax.set_ylabel('NPV')
        # estimate the probability density for each date
        data = np.array([self.nd[self.datedict[date], self.tradedict[trade], :]
                         for date in dates])
        grid_size = 100
        dist_space = np.linspace(np.min(data), np.max(data), grid_size)
        values = np.zeros((len(dates), grid_size))
        for k in range(len(data)):
            row = data[k]
            try:
                density = gaussian_kde(row)
                values[k] = density(dist_space)
            except LinAlgError:
                values[k] = np.zeros(grid_size)
        # create X and Y data for plotting
        dates, dist_space = np.meshgrid(np.arange(len(dates)), dist_space)
        ax.plot_surface(dates, dist_space, values.T, cmap=cm.jet)

    def plot_distribution(self, trade):
        """
        Plots the NPV surface for trade.
        X-axis: dates
        Y-axis: npv
        Z-axis: histogram for each date
        """
        ax = self.fig.gca(projection='3d')
        dates = self.df.date.unique()[:-1]
        data = np.array([self.nd[self.datedict[date], self.tradedict[trade], :] for date in dates])
        dist_space = np.linspace(np.min(data), np.max(data), 100)
        d = data[0]
        values = np.array([np.histogram(d, 100, density=True)[0] for d in data])
        dates, dist_space = np.meshgrid(np.arange(len(dates)), dist_space)
        print(dates.shape, dist_space.shape, values.T.shape)
        ax.plot_surface(dates, dist_space, values.T)


csv_file = 'cube.csv'
npv = NpvCube(csv_file)
trade = '833397AI'
npv.plot_distribution(trade)
plt.show()
