import os
import platform
import sys
import subprocess
import shutil
import matplotlib.pyplot as plt
import matplotlib.ticker

class OreExample(object):

    def __init__(self, dry=False):
        self.ore_exe = ""
        self.headlinecounter = 0
        self.dry = dry
        self.ax = None
        self.plot_name=""
        self._locate_ore_exe()

    def _locate_ore_exe(self):
        if os.name == 'nt':
            if sys.platform == "win32":
                self.ore_exe = "..\\..\\App\\bin\\Win32\\Release\\ore.exe"
            else:
                self.ore_exe = "..\\..\\App\\bin\\x64\\Release\\ore.exe"
        else:
            self.ore_exe = "../../App/ore"

    def print_headline(self, headline):
        self.headlinecounter += 1
        print()
        print(str(self.headlinecounter) + ") " + headline)

    def get_times(self, output):
        print("Get times from the log file:")
        logfile = open(output)
        for line in logfile.readlines():
            if "ValuationEngine completed" in line:
                times = line.split(":")[-1].strip().split(",")
                for time in times:
                    print("\t" + time.split()[0] + ": " + time.split()[1])

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

    def plot_npv(self, filename, colIdx, color, label, marker=''):
        data = self.get_output_data_from_column(filename, colIdx)
        self.ax.plot(range(1, len(data) + 1),
                data,
                color=color,
                label=label,
                linewidth=2,
                marker=marker)

    def decorate_plot(self, title, ylabel="Exposure", xlabel="Time / Years"):
        self.ax.set_title(title)
        self.ax.set_xlabel(xlabel)
        self.ax.set_ylabel(ylabel)
        self.ax.legend(loc='upper right', shadow=True)
        self.ax.get_yaxis().set_major_formatter(
            matplotlib.ticker.FuncFormatter(lambda x, p: format(int(x), ',')))

    def plot_line(self, xvals, yvals, color, label):
        self.ax.plot(xvals, yvals, color=color, label=label, linewidth=2)

    def setup_plot(self, filename):
        self.fig = plt.figure(figsize=plt.figaspect(0.4))
        self.ax = self.fig.add_subplot(111)
        self.plot_name = "mpl_" + filename

    def save_plot_to_file(self, subdir="Output"):
        file = os.path.join(subdir,self.plot_name+".pdf")
        plt.savefig(file)
        print("Saving plot...." + file)
        plt.close()

    def run(self, xml):
        if not self.dry:
            subprocess.call([self.ore_exe, xml])


if __name__ == "__main__":
    examples = [
        "Example_1",
        "Example_2",
        "Example_3",
        "Example_4",
        "Example_5",
        "Example_6",
        "Example_7",
        "Example_8",
        "Example_9",
        "Example_10",
        "Example_11",
        "Example_12",
    ]

    for example in examples:
        try:
            print("Running: " + example)
            os.chdir(os.path.join(os.getcwd(), example))
            filename = os.path.join("run.py")
            sys.argv = ["run.py", 0]
            exec(compile(open(filename, "rb").read(), filename, 'exec'))
            os.chdir(os.path.dirname(os.getcwd()))
        except:
            print("Error running " + example)
