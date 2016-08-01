# Copyright (C) 2016 Quaternion Risk Management Ltd.
# All rights reserved.

import matplotlib
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from matplotlib import cm
from numpy.linalg.linalg import LinAlgError
from scipy.stats.kde import gaussian_kde
from scipy.integrate import cumtrapz


class NpvCube(object):

    def __init__(self, csv_file):
        self.df = pd.read_csv(csv_file)
        self.df.columns = ['TradeID', 'DateIndex', 'Date', 'Sample', 'Value']
        self.nd = np.array([])
        self.datedict = {}
        self.tradedict = {}
        self.dates = []
        self.trades = []
        self.calc_nd()

    def calc_nd(self):
        """ Converts DataFrame into 3D array. """
        self.dates = self.df.Date.unique()
        self.trades = self.df.TradeID.unique()
        self.datedict = {val: num for num, val in enumerate(self.dates)}
        self.tradedict = {val: num for num, val in enumerate(self.trades)}
        self.nd = np.zeros((len(self.dates), len(self.trades), self.df.Sample.max() + 1))
        for row in self.df.values:
            i = self.datedict[row[2]]
            j = self.tradedict[row[0]]
            k = row[3]
            self.nd[i][j][k] = row[4]

    def plot_trade(self, trade, ax):
        """ Plots the dates, samples and values.
            Visualises the Monte Carlo Paths (maybe not too insightful).
        """
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

    def plot_density(self, trade, ax, exposure='NPV'):
        """
        Plots the NPV surface for the trade 'trade' on axis 'ax'.
        The optional parameter 'exposure' can be 'NPV', 'NPV+', 'NPV-'.

        X-axis: dates
        Y-axis: npv
        Z-axis: value of the estimated pdf
        """
        # remove last (erroneuous) date
        dates = self.df.Date.unique()
        # configure x-axis to be labeld with dates
        date_step = 5
        ax.set_xticks(np.arange(0, len(dates), date_step))
        ax.set_xticklabels(dates[0::date_step])
        ax.set_xlabel('Dates')
        # set thousands separator for y-axis
        ax.get_yaxis().set_major_formatter(matplotlib.ticker.FuncFormatter(
            lambda x, p: format(int(x), ',')))
        ax.set_ylabel(exposure)
        # estimate the probability density for each date
        data = np.array([self.nd[self.datedict[date], self.tradedict[trade], :]
                         for date in dates])
        if exposure == 'NPV+':
            data = np.fmax(data, np.zeros(data.shape))
        elif exposure == 'NPV-':
            data = np.fmax(-data, np.zeros(data.shape))
        grid_size = 50
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
        X, Y = np.meshgrid(np.arange(len(dates)), dist_space)
        # ax.plot_surface(X, Y, values.T, cmap=cm.jet)
        ax.plot_trisurf(X.flatten(),
                        Y.flatten(),
                        values.T.flatten(),
                        cmap=cm.jet,
                        # color='r',
                        linewidth=0)

    def plot_exposure_statistics(self, trade, ax):
        data = np.array([self.nd[self.datedict[date], self.tradedict[trade], :]
                         for date in self.dates])
        data_plus = np.fmax(data, np.zeros(data.shape))
        data_minus = np.fmax(-data, np.zeros(data.shape))
        npv = np.average(data, axis=1)
        npv_plus = np.average(data_plus, axis=1)
        npv_minus = np.average(data_minus, axis=1)
        perc = np.percentile(data, 95, axis=1)
        ax.plot(range(len(self.dates)), npv, label='NPV')
        ax.plot(range(len(self.dates)), npv_plus, label='NPV+')
        ax.plot(range(len(self.dates)), npv_minus, label='NPV-')
        ax.plot(range(len(self.dates)), perc, label='95p')
        ax.get_yaxis().set_major_formatter(matplotlib.ticker.FuncFormatter(
        lambda x, p: format(int(x), ',')))
        date_step = 2
        ax.set_xticks(np.arange(0, len(self.dates), date_step))
        ax.set_xticklabels(self.dates[0::date_step], rotation=90)
        ax.legend(loc='upper right', shadow=True)

    def plot_distribution(self, trade, ax):
        """
        Plots the NPV surface for trade.
        X-axis: dates
        Y-axis: npv
        Z-axis: histogram for each date

        depcrecated
        """
        dates = self.df.Date.unique()[1:-1]
        data = np.array([self.nd[self.datedict[date], self.tradedict[trade], :] for date in dates])
        dist_space = np.linspace(np.min(data), np.max(data), 100)
        d = data[0]
        values = np.array([np.histogram(d, 100, density=True)[0] for d in data])
        dates, dist_space = np.meshgrid(np.arange(len(dates)), dist_space)
        ax.plot_surface(dates, dist_space, values.T)

    def plot_density_date(self, trade, ax, date, exposure='NPV'):
        """
        Plots the NPV density for the trade 'trade' on axis 'ax' at 'date'
        The optional parameter 'exposure' can be 'NPV', 'NPV+', 'NPV-'.

        Y-axis: npv
        X-axis: value of the estimated pdf
        """
        data = self.nd[self.datedict[date], self.tradedict[trade], :]
        if exposure == 'NPV+':
            data = np.fmax(data, np.zeros(data.shape))
        elif exposure == 'NPV-':
            data = np.fmax(-data, np.zeros(data.shape))
        xmin = np.min(self.nd[:, self.tradedict[trade], :])
        xmax = np.max(self.nd[:, self.tradedict[trade], :])
        grid_size = 50
        dist_space = np.linspace(xmin, xmax, grid_size)
        try:
            density = gaussian_kde(data)
            values = density(dist_space)
        except LinAlgError:
            values = np.zeros(grid_size)

        ax.plot(dist_space, values, color='k', label='NPV')
        np.max(self.nd[:, self.tradedict[trade], :])
        ax.set_xlim(xmin,xmax)
        ax.set_ylim(0, 10e-7)
        #ax.set_xticks(np.arange(0, len(dates), date_step))
        #ax.set_xticklabels(dates[0::date_step])
        ax.set_xlabel(exposure)


        # # set thousands separator for y-axis
        # ax.get_yaxis().set_major_formatter(matplotlib.ticker.FuncFormatter(
        #     lambda x, p: format(int(x), ',')))
        # ax.set_ylabel(exposure)
        # # estimate the probability density for each date
        # # create X and Y data for plotting
        # X, Y = np.meshgrid(np.arange(len(dates)), dist_space)
        # # ax.plot_surface(X, Y, values.T, cmap=cm.jet)
        # ax.plot_trisurf(X.flatten(),
        #                 Y.flatten(),
        #                 values.T.flatten(),
        #                 cmap=cm.jet,
        #                 # color='r',
        #                 linewidth=0)
