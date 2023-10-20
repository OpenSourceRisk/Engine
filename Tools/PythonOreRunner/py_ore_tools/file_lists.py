import os
import xml.etree.ElementTree as ET
import pandas as pd


class FileList(object):
    """ Provides an abstraction to a list of files on the harddrive and functionality to systematically parse them. """

    def __init__(self, key_list, file_list, file_type, parsing_function):
        """
        :param key_list:    A list of keys that will be used to identify the files, e.g. ['A', 'B', 'C']
        :param file_list:   A list of files in that list, e.g. ['C:\A.csv', 'C:\B.csv', 'C:\C.csv']
        :param file_type:   A key to identify the file type , e.g. 'csv'
        :param parsing_function:  A function to parse each file in the file_list, e.g. pd.read_csv
        """

        self._key_list = key_list
        self._file_list = file_list
        self._file_type = file_type
        self._parsing_function = parsing_function
        self.locations = dict(zip(key_list, file_list))

    def _parse(self):
        """
        Applies the parsing_function to each file in the file_list and computes a dictionary that contains for each
        key in the key_list the parsed file in the file_list. This dictionary is then added as a attribute named
        file_type to the object, e.g. self.csv['A'] contains the content of 'C:\A.csv' parsed via pd.read_csv.
        """
        try:
            setattr(self,
                    self._file_type,
                    dict(zip(self._key_list, [self._parsing_function(f) for f in self._file_list])))
        except FileNotFoundError as e:
            print(e)


class InputFiles(FileList):
    """ Provides an abstraction to the input files of ORE. (Currently not all input files are supported). """

    @classmethod
    def from_folder(cls, folder):
        """
        A factory to produce a FileList of input files of ORE.

        :param folder: A folder that contains the config files:
                              ore.xml, portfolio.xml, netting.xml, simulation.xml

        :return: An InputFiles object that assumes that all input files are in the same folder and have
                 the default names.
        """
        return cls(*[os.path.join(folder, f) for f in ('ore.xml', 'portfolio.xml', 'netting.xml', 'simulation.xml')])

    def __init__(self, ore, portfolio, netting, simulation):
        """
        A constructor that allows to construct an InputFiles object even if the input files are not in the same folder
        and the factory cannot be used.

        :param ore:         Location of ore.xml, e.g. 'C:\ore.xml'
        :param portfolio:   Location of portfolio.xml, e.g. 'C:\portfolio.xml'
        :param netting:     Location of netting.xml, e.g. 'C:\netting.xml'
        :param simulation:  Location of simulation.xml, e.g. 'C:\simulation.xml'
        """

        super().__init__(('ore', 'portfolio', 'netting', 'simulation'),
                         (ore, portfolio, netting, simulation),
                         'xml',
                         ET.parse)


class OutputFiles(FileList):
    """ Provides an abstraction to all output files of ORE. It is always assumed that they are in one folder. """

    @classmethod
    def from_folder(cls, folder):
        """
        A factory that constructs an OutpuFiles object with a file_list containing of all files in folder that end
        with 'csv'. It uses the names of the files as the key_list and 'csv' as the file_type.

        :param folder:  A location on the harddrive containing the output files.
        :return:  An OutputFiles object with the parsed files.
        """

        csv_files = [f for f in os.listdir(folder) if f[-4:] == '.csv']
        obj = cls([f[:-4] for f in csv_files],
                  [os.path.join(folder, f) for f in csv_files],
                  'csv', pd.read_csv)
        obj.folder = folder
        return obj
