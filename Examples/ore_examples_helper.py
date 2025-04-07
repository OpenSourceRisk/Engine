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
#                   if e == 'Example_39'], key=lambda e: int(e.split('_')[1]))

def get_list_ore_academy():
    return sorted([e for e in os.listdir(os.getcwd()) if e[:11] == 'OREAcademy_'])


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
        if 'ORE_EXAMPLES_USE_PYTHON' in os.environ.keys():
            self.use_python = os.environ['ORE_EXAMPLES_USE_PYTHON']=="1"
            self.ore_exe = ""
        else:
            self.use_python = False
            self._locate_ore_exe()

    def _locate_ore_exe(self):
        if os.name == 'nt':
            if platform.machine()[-2:] == "64":
                if os.path.isfile("..\\..\\App\\bin\\x64\\Release\\ore.exe"):
                    self.ore_exe = "..\\..\\App\\bin\\x64\\Release\\ore.exe"
                elif os.path.isfile("..\\..\\build\\App\\ore.exe"):
                    self.ore_exe = "..\\..\\build\\App\\ore.exe"
                elif os.path.isfile("..\\..\\..\\build\\ore\\App\\ore.exe"):
                    self.ore_exe = "..\..\\..\\build\\ore\\App\\ore.exe"
                elif os.path.isfile("..\\..\\..\\build\\ore\\App\\RelWithDebInfo\\ore.exe"):
                    self.ore_exe = "..\\..\\..\\build\\ore\\App\\RelWithDebInfo\\ore.exe"
                elif os.path.isfile("..\\..\\build\\App\\Release\\ore.exe"):
                    self.ore_exe = "..\\..\\build\\App\\Release\\ore.exe"
                else:
                    print_on_console("ORE executable not found.")
                    quit()
            else:
                if os.path.isfile("..\\..\\App\\bin\\Win32\\Release\\ore.exe"):
                    self.ore_exe = "..\\..\\App\\bin\\Win32\\Release\\ore.exe"
                elif os.path.isfile("..\\..\\build\\App\\ore.exe"):
                    self.ore_exe = "..\\..\\build\\App\\ore.exe"
                else:
                    print_on_console("ORE executable not found.")
                    quit()
        else:
            if os.path.isfile("../../App/build/ore"):
                self.ore_exe = "../../App/build/ore"
            elif os.path.isfile("../../build/App/ore"):
                self.ore_exe = "../../build/App/ore"
            elif os.path.isfile("../../App/ore"):
                self.ore_exe = "../../App/ore"
            elif os.path.isfile("../../../build/ore/App/ore"):
                self.ore_exe = "../../../build/ore/App/ore"
                self.ore_plus_exe = "../../../build/AppPlus/ore_plus"
            elif shutil.which("ore"):
                # Look on the PATH to see if the executable is there
                self.ore_exe = shutil.which("ore")
                print_on_console("Using ORE executable found on PATH")
            else:
                print_on_console("ORE executable not found.")
                quit()
        print_on_console("Using ORE executable " + (os.path.abspath(self.ore_exe)))

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

    def get_output_data_from_column(self, csv_name, colidx, offset=1, filter='', filterCol=0):
        f = open(os.path.join(os.path.join(os.getcwd(), "Output"), csv_name))
        data = []
        for line in f:
            tokens = line.split(',')
            if colidx < len(line.split(',')):
                if (filter == '' or tokens[filterCol] == filter):
                    data.append(line.split(',')[colidx])
            else:
                data.append("Error")
        return [float(i) for i in data[offset:]]

    def save_output_to_subdir(self, subdir, files):
        if not os.path.exists(os.path.join("Output", subdir)):
            os.makedirs(os.path.join("Output", subdir))
        for file in files:
            shutil.copy(os.path.join("Output", file), os.path.join("Output", subdir))

    def plot(self, filename, colIdxTime, colIdxVal, color, label, offset=1, marker='', linestyle='-', filter='', filterCol=0):
        self.ax.plot(self.get_output_data_from_column(filename, colIdxTime, offset, filter, filterCol),
                     self.get_output_data_from_column(filename, colIdxVal, offset, filter, filterCol),
                     linewidth=2,
                     linestyle=linestyle,
                     color=color,
                     label=label,
                     marker=marker)

    def plotScaled(self, filename, colIdxTime, colIdxVal, color, label, offset=1, marker='', linestyle='-', title='', xlabel='', ylabel='', rescale=False, zoom=1, legendLocation='upper right', xScale=1.0, yScale=1.0, exponent=1.0):
        xTmp = self.get_output_data_from_column(filename, colIdxTime, offset)
        yTmp = self.get_output_data_from_column(filename, colIdxVal, offset)
        x = []
        y = []
        yMax = pow(float(yTmp[0]), exponent) / yScale
        yMin = pow(float(yTmp[0]), exponent) / yScale
        for i in range(0, len(xTmp)-1):
            try :
                tmp = pow(float(yTmp[i]), exponent) / yScale;
                y.append(tmp)
                yMax = max(tmp, yMax)
                yMin = min(tmp, yMin)
                x.append(float(xTmp[i]) / xScale)
            except TypeError:
                pass
        if (yMax != 0.0):
            yn = [ u / yMax for u in y ]
        self.ax.plot(x,
                     y,
                     linewidth=2,
                     linestyle=linestyle,
                     color=color,
                     label=label,
                     marker=marker)
        if rescale:            
            self.ax.set_ylim([yMin/zoom, yMax/zoom])
        self.ax.set_title(title)
        self.ax.set_xlabel(xlabel)
        self.ax.set_ylabel(ylabel)
        self.ax.legend(loc=legendLocation, shadow=True)        

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

    def decorate_plot(self, title, ylabel="Exposure", xlabel="Time / Years", legend_loc="upper right", y_format_as_int = True, display_grid = False):
        self.ax.set_title(title)
        self.ax.set_xlabel(xlabel)
        self.ax.set_ylabel(ylabel)
        self.ax.legend(loc=legend_loc, shadow=True)
        if y_format_as_int:
            self.ax.get_yaxis().set_major_formatter(
                matplotlib.ticker.FuncFormatter(lambda x, p: format(int(x), ',')))
        if display_grid:
            self.ax.grid()

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
            if(self.use_python):
                res = subprocess.call([sys.executable, os.path.join(os.pardir, "ore_wrapper.py"), xml])
            else:
                res = subprocess.call([self.ore_exe, xml])
            if res != 0:
                raise Exception("Return Code was not Null.")

    def run_plus(self, xml):
        if not self.dry:
            if subprocess.call([self.ore_plus_exe, xml]) != 0:
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
    for example in (get_list_of_examples() + get_list_ore_academy()):
        run_example(example)
