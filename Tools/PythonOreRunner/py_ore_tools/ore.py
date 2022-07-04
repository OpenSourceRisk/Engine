import os
import subprocess
import shutil

from py_ore_tools.file_lists import InputFiles, OutputFiles
from py_ore_tools.plotter import OutputPlotter


class OreBasic(object):
    """
    Provides an abstraction to an ORE configuration and basic functionality to work with it. A functionality is basic
    if it is technically possible and conceptually meaningful to have it on all ORE configurations, i.e. it is
    independent of a concrete application or project. This includes a consistent parsing of input and output files
    and an interface to execute a run.
    """

    @classmethod
    def from_folders(cls, input_folder, output_folder, execution_folder):
        """
        A factory to produce an OreBasic object.

        :param input_folder:        A location consumable by InputFiles.from_folder containing the input files.
        :param output_folder:       A location where the output files will be stored.
        :param execution_folder:    A location from where ore.exe should be run on that configuration.
        :return: An OreBasic object.
        """

        return cls(InputFiles.from_folder(input_folder),
                   output_folder,
                   execution_folder)

    def __init__(self, input_files, output_folder, execution_folder):
        """
        A constructor to produce an OreBasic object manually in case the factory is not applicable.

        :param input_files:         An InputFiles object representing the input files.
        :param output_folder:       A location where the output files will be stored.
        :param execution_folder:    A location from where ore.exe should be run on that configuration.
        """

        self.input = input_files
        self.output_folder = output_folder
        self.execution_folder = execution_folder
        self.output = None
        self.plots = None
        self.parse_input()

    def parse_input(self):
        """ Parses the input files. This function is invoked in the constructor. """
        self.input._parse()

    def parse_output(self):
        """
        Parses the output files. This function is not invoked in the constructor, because in case this object is
        constructed to execute the first run represented by this ORE configuration, the output files do not exist yet.
        """

        self.output = OutputFiles.from_folder(self.output_folder)
        self.output._parse()
        self.plots = OutputPlotter(self.output)

    def run(self, ore_exe, delete_output_folder_before_run=False):
        """
        Changes the working directory to the execution_folder, executes a run on the ore configuration represented
        by this object and changes the directory back to where it was before.

        :param ore_exe: Location of the ore.exe on your machine, e.g. 'C:\Program Files\ORE\ore.exe'
        :param delete_output_folder_before_run: A flag whether or not to delete the output folder (if present)
                                                before executing the run. This can be helpful to guarantee that a
                                                re-run is unaffected by a previous run.
        """

        if os.path.exists(self.output_folder):
            if delete_output_folder_before_run:
                shutil.rmtree(self.output_folder)
        self.parse_input()
        command = [ore_exe, self.input.locations['ore']]
        cwd = os.getcwd()
        os.chdir(self.execution_folder)
        subprocess.call(command)
        os.chdir(cwd)

    def backup_outputfolder_to(self, subfolder):
        if not os.path.exists(subfolder):
            os.makedirs(subfolder)
        for item in os.listdir(self.output_folder):
            if os.path.isfile(os.path.join(self.output_folder, item)):
                shutil.copy(os.path.join(self.output_folder, item), os.path.join(subfolder, item))

    def backup_inputfiles_to(self, subfolder):
        if not os.path.exists(subfolder):
            os.makedirs(subfolder)
        for key, value in self.input.locations.items():
            shutil.copy(value, os.path.join(subfolder, os.path.basename(value)))
