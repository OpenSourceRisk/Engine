import platform
import subprocess
import shutil

import matplotlib
import os
import sys

matplotlib.use('Agg')

import matplotlib.pyplot as plt
import matplotlib.ticker
import pandas as pd
from datetime import datetime
from math import log


def get_list_of_examples():
    return sorted([e for e in os.listdir(os.getcwd())
                   if e[:8] == 'Example_'], key=lambda e: int(e.split('_')[1]))


def print_on_console(line):
    print(line)
    sys.stdout.flush()


class OreExample(object):
    def __init__(self, dry=False):
        self.ore_exe = ""
        self.headlinecounter = 0
        self.dry = dry
        self.ax = None
        self.plot_name = ""
        self._locate_ore_exe()

    def _locate_ore_exe(self):
        if os.name == 'nt':
            if platform.machine()[-2:] == "64":
                self.ore_exe = "..\\..\\App\\bin\\x64\\Release\\ore.exe"
            else:
                self.ore_exe = "..\\..\\App\\bin\\Win32\\Release\\ore.exe"
        else:
            self.ore_exe = "../../App/ore"


    def print_headline(self, headline):
        self.headlinecounter += 1
        print_on_console('')
        print_on_console(str(self.headlinecounter) + ") " + headline)

    def get_times(self, output):
        print_on_console("Get times from the log file:")
        logfile = open(output)
        for line in logfile.readlines():
            if "ValuationEngine completed" in line:
                times = line.split(":")[-1].strip().split(",")
                for time in times:
                    print_on_console("\t" + time.split()[0] + ": " + time.split()[1])

    def get_output_data_from_column(self, csv_name, colidx, offset=1):
        f = open(os.path.join(os.path.join(os.getcwd(), "Output"), csv_name))
        data = []
        for line in f:
            if colidx < len(line.split(',')):
                data.append(line.split(',')[colidx])
            else:
                data.append("Error")
        return data[offset:]

    def save_output_to_subdir(self, subdir, files):
        if not os.path.exists(os.path.join("Output", subdir)):
            os.makedirs(os.path.join("Output", subdir))
        for file in files:
            shutil.copy(os.path.join("Output", file), os.path.join("Output", subdir))

    def plot(self, filename, colIdxTime, colIdxVal, color, label, offset=1, marker='', linestyle='-'):
        self.ax.plot(self.get_output_data_from_column(filename, colIdxTime, offset),
                     self.get_output_data_from_column(filename, colIdxVal, offset),
                     linewidth=2,
                     linestyle=linestyle,
                     color=color,
                     label=label,
                     marker=marker)

    def plotSq(self, filename, colIdxTime, colIdxVal, color, label, offset=1, marker='', linestyle='-', title='',
               xlabel='', ylabel='', rescale=False, zoom=1):
        xTmp = self.get_output_data_from_column(filename, colIdxTime, offset)
        yTmp = self.get_output_data_from_column(filename, colIdxVal, offset)
        x = []
        y2 = []
        yMax = 0.0
        for i in range(0, len(xTmp) - 1):
            try:
                tmp = float(yTmp[i]) * float(yTmp[i])
                y2.append(tmp)
                yMax = max(tmp, yMax)
                x.append(float(xTmp[i]))
            except TypeError:
                pass
        y2n = [u / yMax for u in y2]
        self.ax.plot(x,
                     y2,
                     linewidth=2,
                     linestyle=linestyle,
                     color=color,
                     label=label,
                     marker=marker)
        if rescale:
            self.ax.set_ylim([0, yMax / zoom])
        self.ax.set_title(title)
        self.ax.set_xlabel(xlabel)
        self.ax.set_ylabel(ylabel)
        self.ax.legend(loc="upper right", shadow=True)
        self.ax.get_yaxis().set_major_formatter(
            matplotlib.ticker.FuncFormatter(lambda x, p: '{:1.2e}'.format(float(x))))

    def plot_npv(self, filename, colIdx, color, label, marker=''):
        data = self.get_output_data_from_column(filename, colIdx)
        self.ax.plot(range(1, len(data) + 1),
                     data,
                     color=color,
                     label=label,
                     linewidth=2,
                     marker=marker)

    def plot_zeroratedist(self, filename, colIdxTime, colIdxVal, maturity, color, label,
                          title='Zero Rate Distribution'):
        f = open(os.path.join(os.path.join(os.getcwd(), "Output"), filename))
        xdata = []
        ydata = []
        for line in f:
            try:
                xtmp = datetime.strptime(line.split(',')[colIdxTime], '%Y-%m-%d')
                ytmp = -log(float(line.split(',')[colIdxVal])) / float(maturity)
                xdata.append(xtmp)
                ydata.append(ytmp)
            except ValueError:
                pass
            except TypeError:
                pass
        d = pd.DataFrame({'x': xdata, 'y': ydata})
        grouped = d.groupby('x')
        mdata = grouped.mean()['y']
        sdata = grouped.std()['y']
        self.ax.plot(list(mdata.index.values),
                     list(mdata),
                     linewidth=3,
                     linestyle='-',
                     color=color,
                     label=label + ' (mean)')
        self.ax.plot(list(mdata.index.values),
                     list(mdata - sdata),
                     linewidth=1,
                     linestyle='-',
                     color=color,
                     label=label + ' (mean +/- std)')
        self.ax.plot(list(mdata.index.values),
                     list(mdata + sdata),
                     linewidth=1,
                     linestyle='-',
                     color=color,
                     label='')
        self.ax.set_xlabel("Time")
        self.ax.set_ylabel("Zero Rate")
        self.ax.get_yaxis().set_major_formatter(
            matplotlib.ticker.FuncFormatter(lambda x, p: '{:1.4f}'.format(float(x))))
        self.ax.legend(loc="upper left", shadow=True)
        self.ax.set_title(title)

    def decorate_plot(self, title, ylabel="Exposure", xlabel="Time / Years", legend_loc="upper right"):
        self.ax.set_title(title)
        self.ax.set_xlabel(xlabel)
        self.ax.set_ylabel(ylabel)
        self.ax.legend(loc=legend_loc, shadow=True)
        self.ax.get_yaxis().set_major_formatter(
            matplotlib.ticker.FuncFormatter(lambda x, p: format(int(x), ',')))

    def plot_line(self, xvals, yvals, color, label):
        self.ax.plot(xvals, yvals, color=color, label=label, linewidth=2)

    def plot_hline(self, yval, color, label):
        plt.axhline(yval, xmin=0, xmax=1, color=color, label=label, linewidth=2)

    def setup_plot(self, filename):
        self.fig = plt.figure(figsize=plt.figaspect(0.4))
        self.ax = self.fig.add_subplot(111)
        self.plot_name = "mpl_" + filename

    def save_plot_to_file(self, subdir="Output"):
        file = os.path.join(subdir, self.plot_name + ".pdf")
        plt.savefig(file)
        print_on_console("Saving plot...." + file)
        plt.close()

    def run(self, xml):
        if not self.dry:
            if subprocess.call([self.ore_exe, xml]) != 0:
                raise Exception("Return Code was not Null.")


def run_example(example):
    current_dir = os.getcwd()
    print_on_console("Running: " + example)
    try:
        os.chdir(os.path.join(os.getcwd(), example))
        filename = "run.py"
        sys.argv = [filename, 0]
        exit_code = subprocess.call([sys.executable, filename])
        os.chdir(os.path.dirname(os.getcwd()))
        print_on_console('-' * 50)
        print_on_console('')
    except:
        print_on_console("Error running " + example)
    finally:
        os.chdir(current_dir)
    return exit_code


if __name__ == "__main__":
    for example in get_list_of_examples():
        run_example(example)
