import subprocess
from ipywidgets import Text, Button, HBox, widgets
from IPython.display import display


class OreLauncher(object):

    def __init__(self):
        self.stdout = ''
        self.stdin = ''
        self.ore_path_selector = Text(description='ORE Path:', value='C:\\CppDev\\openxva\\App\\bin\\x64\\Debug\\', width=200)
        self.xml_selector = Text(description='XML File:', value='C:\\CppDev\\openxva\\Input\\Example_1\\ore.xml', width=200)
        self.launch_button = Button(description='Launch')
        self.launch_button.on_click(self.run)

    def run(self, b):
        command = [
            self.ore_path_selector.value+'ore.exe',
            self.xml_selector.value]
        print(command)
        p = subprocess.Popen(command,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE)
        self.stdout, self.stderr = p.communicate()
        print(self.stdout)

    def show(self):
        display(self.ore_path_selector)
        display(self.xml_selector)
        display(self.launch_button)
