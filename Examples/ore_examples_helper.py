import os
import sys
import subprocess


class OreExample(object):

    def __init__(self):
        self.ore_exe = ""
        self.headlinecounter = 0
        self._locate_ore_exe()

    def _locate_ore_exe(self):
        ore_exe = os.getcwd()
        ore_exe = os.path.dirname(ore_exe)  # one level up
        ore_exe = os.path.dirname(ore_exe)  # one level up
        ore_exe = os.path.join(ore_exe, "App")
        ore_exe = os.path.join(ore_exe, "bin")
        ore_exe = os.path.join(ore_exe, "Win32" if sys.platform == "win32" else "x64")
        ore_exe = os.path.join(ore_exe, "Release")
        ore_exe = os.path.join(ore_exe, "ore.exe")
        self.ore_exe = ore_exe

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

    def get_output_data_from_column(self, csv_name, colidx):
        f = open(os.path.join(os.path.join(os.getcwd(), "Output"), csv_name))
        data = []
        for line in f:
            data.append(line.split(',')[colidx])
        return data[1:]

    def run(self, xml):
        subprocess.call([self.ore_exe, xml])


