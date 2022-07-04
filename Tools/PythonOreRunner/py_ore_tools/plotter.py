import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.ticker
import os


class OutputPlotter(dict):
    """ A class to provide plotting functionality of the output file. """

    def __init__(self, output_files, **kwargs):
        """

        :param output_files:  An OutputFile object representing the output files.
        """
        super().__init__(kwargs)
        self.output_files = output_files

    def _plot_exposure(self, data_file):
        df = self.output_files.csv[data_file]
        fig = plt.figure()
        ax = fig.add_subplot(111)

        dates = pd.to_datetime(df['Date']).unique()
        dates = dates - dates.min()
        years = dates.astype('timedelta64[D]') / np.timedelta64(1, 'D') / 365
        ax.plot(years, df['EPE'], label='EPE')
        ax.plot(years, -df['ENE'], label='ENE')
        ax.get_xaxis().set_major_formatter(matplotlib.ticker.FuncFormatter(lambda x, p: "{:,.1f}".format(x)))
        ax.set_xlabel('years')
        ax.set_ylabel("Exposure")
        ax.get_yaxis().set_major_formatter(matplotlib.ticker.FuncFormatter(lambda x, p: format(int(x), ',')))
        ax.legend(loc='upper right')
        ax.set_title(data_file)

        self[data_file] = fig
        plt.savefig(os.path.join(self.output_files.folder, data_file + '.pdf'))
        plt.close(fig)

    def plot_nettingset_exposures(self):
        """
         Produces plots of all output files starting with 'exposure_nettingset'. The plot is saved as a pdf file
         with the same name in the same folder and also added as an item to self. It can be accessed like
         self['exposure_nettingset_CPTY_A'].
        """

        for ns in [f for f in self.output_files.csv.keys() if "exposure_nettingset" in f]:
            self._plot_exposure(ns)

    def plot_trade_exposures(self):
        """
         Produces plots of all output files starting with 'exposure_trade'. The plot is saved as a pdf file
         with the same name in the same folder and also added as an item to self. It can be accessed like
         self['exposure_trade_CPTY_A'].
        """
        for ns in [f for f in self.output_files.csv.keys() if "exposure_trade" in f]:
            self._plot_exposure(ns)
