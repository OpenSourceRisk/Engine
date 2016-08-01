# Copyright (C) 2016 Quaternion Risk Management Ltd.
# All rights reserved.


import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from matplotlib import cm
from matplotlib.font_manager import FontProperties
from numpy.linalg.linalg import LinAlgError
from scipy.stats.kde import gaussian_kde
from scipy.integrate import cumtrapz
import matplotlib
from matplotlib import cm
from mpl_toolkits.mplot3d import Axes3D
from scipy.stats.kde import gaussian_kde
from ipywidgets import Dropdown, RadioButtons, Button, IntSlider, SelectMultiple, HBox
from IPython.display import display
from enum import Enum


class ExposureTypes(Enum):
    npv = 'NPV'
    npvp = 'NPV+'
    npvm = 'NPV-'


class PlotTypes(Enum):
    surface = 'Surface'
    exposure_statistics = 'Exposure Statistics'
    time_slider = 'Time Slider'


class NpvCube(object):

    def __init__(self, csv_file):
        self.df = pd.read_csv(csv_file)
        self.df.columns = ['TradeID', 'DateIndex', 'Date', 'Sample', 'Value']
        self.dates = self.df.Date.unique()
        self.trades = self.df.TradeID.unique()
        self.datedict = {val: num for num, val in enumerate(self.dates)}
        self.tradedict = {val: num for num, val in enumerate(self.trades)}
        self.nd = np.array([])
        self.density_values = np.array([])
        self.dist_space = np.array([])
        self.exposure_statistics ={}
        self.npv_min = None
        self.npv_max = None
        self.density_max = None
        self.grid_size = 50
        self.calc_nd()

    def calc_nd(self):
        """ Converts DataFrame into 3D array. """
        self.nd = np.zeros((len(self.dates), len(self.trades), self.df.Sample.max() + 1))
        for row in self.df.values:
            i = self.datedict[row[2]]
            j = self.tradedict[row[0]]
            k = row[3]
            self.nd[i][j][k] = row[4]

    def trades_samples_per_date(self, trades):
        # ToDo: this can probably be improved
        sum = np.array([self.nd[self.datedict[date], trades[0], :] for date in self.dates])
        for i in range(1, len(trades)):
            sum += np.array([self.nd[self.datedict[date], trades[i], :] for date in self.dates])
        return sum

    def calc_density_data(self, trades, exposure):
        data = self.trades_samples_per_date(trades)
        if exposure == ExposureTypes.npvp:
            data = np.fmax(data, np.zeros(data.shape))
        elif exposure == ExposureTypes.npvm:
            data = np.fmax(-data, np.zeros(data.shape))
        self.dist_space = np.linspace(np.min(data), np.max(data), self.grid_size)
        self.density_values = np.zeros((len(self.dates), self.grid_size))
        for k in range(len(data)):
            row = data[k]
            try:
                density = gaussian_kde(row)
                self.density_values[k] = density(self.dist_space)
            except LinAlgError:
                self.density_values[k] = np.zeros(self.grid_size)
        self.density_max = np.max(self.density_values)
        self.npv_min = np.amin(data)
        self.npv_max = np.amax(data)
        if exposure == ExposureTypes.npvp:
            self.npv_min = max(self.npv_min, 0)
            self.npv_max = max(self.npv_max, 0)
        elif exposure == ExposureTypes.npvm:
            self.npv_min = min(self.npv_min, 0)
            self.npv_max = min(self.npv_max, 0)

    def calc_exposure_statistics(self, trade):
        data = self.trades_samples_per_date(trade)
        data_plus = np.fmax(data, np.zeros(data.shape))
        data_minus = -np.fmax(-data, np.zeros(data.shape))
        npv = np.average(data, axis=1)
        npv_plus = np.average(data_plus, axis=1)
        npv_minus = np.average(data_minus, axis=1)
        perc = np.percentile(data, 95 , axis=1)
        self.exposure_statistics = {
            'npv': npv,
            'npv+': npv_plus,
            'npv-': npv_minus,
            'perc': perc}


class NpvDashBoard(object):

    def __init__(self, npv):
        self.npv = npv
        self.current_trades = [0]
        self.current_exposure = ExposureTypes.npv
        self.current_plot = PlotTypes.surface
        self.figure = None
        self.ax = None
        self.trades_selector = SelectMultiple(options=self.npv.tradedict, description='TradeIDs:')
        self.exposure_selector = RadioButtons(description='Exposure:', options=[member.value for name,member in ExposureTypes.__members__.items()])
        self.plot_selector = Dropdown(options=[member.value for name,member in PlotTypes.__members__.items()], description='Plot Type:')
        self.plot_button = Button(description='Plot')
        self.plot_button.on_click(self.show_plot)
        self.time_slider = IntSlider(min=0, max=len(npv.dates)-1, value=0, description='DateIndex:')
        self.time_slider.observe(self.plot_time_slider, names='value')
        self.time_slider.disabled = True
        self.plot_selector.observe(self.set_current_plot, names='value')
        self.trades_selector.observe(self.set_current_trades, names='value')
        self.exposure_selector.observe(self.set_current_exposure, names='value')

    def set_current_trades(self, trades):
        self.current_trades = trades['new']

    def set_current_exposure(self, exposure):
        self.current_exposure = ExposureTypes(exposure['new'])

    def set_current_plot(self, plot):
        value = PlotTypes(plot['new'])
        self.current_plot = value
        if value == PlotTypes.surface:
            self.exposure_selector.disabled = False
            self.time_slider.disabled = True
        elif value == PlotTypes.exposure_statistics:
            self.exposure_selector.disabled = True
            self.time_slider.disabled = True
        elif value == PlotTypes.time_slider:
            self.exposure_selector.disabled = False
            self.time_slider.disabled = False

    def show_plot(self, b):
        if self.figure is None:
            self.figure = plt.figure()
        else:
            self.figure.clear()
        if self.current_plot == PlotTypes.surface:
            self.figure.canvas.set_window_title('Density Surface')
            self.npv.calc_density_data(self.current_trades, self.current_exposure)
            self.plot_npv_surface()
        elif self.current_plot == PlotTypes.exposure_statistics:
            self.npv.calc_exposure_statistics(self.current_trades)
            self.plot_exposure_statistics()
            self.figure.canvas.set_window_title('Exposure Statistics')
        elif self.current_plot == PlotTypes.time_slider:
            self.npv.calc_density_data(self.current_trades, self.current_exposure)
            self.ax = self.figure.add_subplot(111)
            self.figure.canvas.set_window_title('Time Slider')

    def show_config(self):
        display(HBox([
            self.plot_selector,
            self.trades_selector,
            self.exposure_selector]))
        display(self.time_slider)
        display(self.plot_button)

    def plot_npv_surface(self):
        """
        Plots the NPV surface for the current_trade.

        X-axis: dates
        Y-axis: npv
        Z-axis: value of the estimated pdf
        """

        # if self.figure_surface:
        #     self.figure_surface.clf()
        # else:
        #     self.figure_surface = plt.figure()

        self.ax = self.figure.add_subplot(111, projection='3d')
        #self.figure_surface.canvas.set_window_title(self.npv.trades[self.current_trade] + ' ' + self.current_exposure + ' Density Surface')
        #self.figure.suptitle(self.npv.trades[self.current_trades] + ' ' + self.current_exposure.value + ' Density Surface')

        # configure x-axis to be labeld with dates
        date_step = 5
        self.ax.set_xticks(np.arange(0, len(self.npv.dates), date_step))
        self.ax.set_xticklabels(self.npv.dates[0::date_step])
        self.ax.set_xlabel('Dates')
        # set thousands separator for y-axis
        self.ax.get_yaxis().set_major_formatter(matplotlib.ticker.FuncFormatter(
            lambda x, p: format(int(x), ',')))
        self.ax.set_ylabel(self.current_exposure.value)
        # estimate the probability density for each date
        # create X and Y data for plotting
        X, Y = np.meshgrid(np.arange(len(self.npv.dates)), self.npv.dist_space)
        # ax.plot_surface(X, Y, values.T, cmap=cm.jet)
        self.ax.plot_trisurf(
            X.flatten(),
            Y.flatten(),
            self.npv.density_values.T.flatten(),
            cmap=cm.jet,
            linewidth=0)
        plt.show()

    def plot_exposure_statistics(self):
        self.ax = self.figure.add_subplot(111)
        self.ax.plot(range(len(self.npv.dates)), self.npv.exposure_statistics['npv'], label='NPV')
        self.ax.plot(range(len(self.npv.dates)), self.npv.exposure_statistics['npv+'], label='NPV+')
        self.ax.plot(range(len(self.npv.dates)), self.npv.exposure_statistics['npv-'], label='NPV-')
        self.ax.plot(range(len(self.npv.dates)), self.npv.exposure_statistics['perc'], label='perc')
        self.ax.get_yaxis().set_major_formatter(matplotlib.ticker.FuncFormatter(
            lambda x, p: format(int(x), ',')))
        date_step = 2
        self.ax.set_xticks(np.arange(0, len(self.npv.dates), date_step))
        self.ax.set_xticklabels(self.npv.dates[0::date_step], rotation=90)
        fontP = FontProperties() # set legend outside plot
        fontP.set_size('small')
        self.ax.legend(loc='upper right', shadow=True, prop=fontP)
        plt.show()

    def plot_time_slider(self, change):
        """
        Plots the NPV density for the current trade and date.

        Y-axis: npv
        X-axis: value of the estimated pdf
        """
        date = change['new']
        plt.cla()
        self.ax.plot(self.npv.dist_space, self.npv.density_values[date], color='k', label=self.current_exposure.value)
        self.ax.set_xlim(self.npv.npv_min, self.npv.npv_max)
        self.ax.set_ylim(0, self.npv.density_max)
        # ax.set_xticks(np.arange(0, len(dates), date_step))
        # ax.set_xticklabels(dates[0::date_step])
        #self.ax.set_xlabel(exposure)
