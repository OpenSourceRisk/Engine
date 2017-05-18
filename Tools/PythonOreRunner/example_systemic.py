import os
import pandas as pd
import networkx as nx
import numpy as np
import matplotlib.pyplot as plt

from py_ore_tools import OreBasic


class FinancialSystem(OreBasic):

    def __init__(self, input_files, output_folder, execution_folder):
        super().__init__(input_files, output_folder, execution_folder)
        self.counterparties = None
        self.graph = None
        self.EEPE = None

    def _set_counterparties(self):
        self.parse_output()
        self.counterparties = sorted(list({c.split(".")[0] for c in self.get_nettingset_eepe().to_dict().keys()}))

    def _set_graph(self):
        self.graph = nx.DiGraph()
        eepe_dict = self.get_nettingset_eepe().to_dict()
        for key in self.get_nettingset_eepe().to_dict().keys():
            self.graph.add_edge(key.split(".")[0], key.split(".")[1], weight=eepe_dict[key])

    def compute_systemic_balancedness(self):
        self.EEPE = np.sum(np.array([e[2]['weight'] for e in self.graph.edges(data=True)]))
        for node_name in self.graph.nodes():
            n = self.graph.node[node_name]
            n['EEPE+'] = np.sum(np.array([e[2]['weight'] for e in self.graph.out_edges(node_name, data=True)]))
            n['rho+'] = n['EEPE+'] / self.EEPE
            n['EEPE-'] = np.sum(np.array([e[2]['weight'] for e in self.graph.in_edges(node_name, data=True)]))
            n['rho-'] = n['EEPE-'] / self.EEPE

    def print(self):
        print(list(nx.generate_edgelist(self.graph)))

    def plot(self):
        pos = nx.circular_layout(self.graph)
        nx.draw(self.graph, pos, with_labels=True)
        nx.draw_networkx_edge_labels(self.graph, pos)
        plt.show()

    def get_nettingset_eepe(self):
        return self.output.csv['xva'][self.output.csv['xva']['#TradeId'].isnull()][['NettingSetId', 'BaselEEPE']].set_index(
            'NettingSetId')['BaselEEPE']

    def set_simulation_samples(self, num_samples):
        self.parse_input()
        self.input.xml['simulation'].getroot().findall('./Parameters/Samples')[0].text = str(num_samples)
        self.input.xml['simulation'].write(self.input.locations['simulation'], xml_declaration=True)

    def consistent_input(self):
        trades = self.input.xml['portfolio'].getroot().findall('./Trade')
        # check that every trade has a correctly mirrored trade
        trade_ids = [t.attrib['id'] for t in trades]
        for k, trade_id in enumerate(trade_ids):
            # check if mirrored trade exists
            id_components = trade_id.split('.')
            id_mirror_trade = '.'.join([id_components[1], id_components[0], id_components[2]])
            l = trade_ids.index(id_mirror_trade)
            # check if mirrored trade is indeed mirrored
            trade_type = trades[k].findall('./TradeType')[0].text
            assert trades[l].findall('./TradeType')[0].text == trade_type
            if trade_type == 'FxForward':
                assert trades[k].findall('./FxForwardData/BoughtCurrency')[0].text == \
                       trades[l].findall('./FxForwardData/SoldCurrency')[0].text
                assert trades[k].findall('./FxForwardData/BoughtAmount')[0].text == \
                       trades[l].findall('./FxForwardData/SoldAmount')[0].text
            elif trade_type == 'Swap':
                # both swap have payer and receiver
                to_bool = lambda x : True if x == 'true' else False
                assert to_bool(trades[k].findall('./SwapData/LegData/Payer')[0].text) != \
                       to_bool(trades[k].findall('./SwapData/LegData/Payer')[1].text)
                assert to_bool(trades[l].findall('./SwapData/LegData/Payer')[0].text) != \
                       to_bool(trades[l].findall('./SwapData/LegData/Payer')[1].text)
                # payer leg of trade is receiver leg of mirrored trade and vice versa
                assert to_bool(trades[k].findall('./SwapData/LegData/Payer')[0].text) != \
                       to_bool(trades[l].findall('./SwapData/LegData/Payer')[0].text)
                assert to_bool(trades[k].findall('./SwapData/LegData/Payer')[1].text) != \
                       to_bool(trades[l].findall('./SwapData/LegData/Payer')[1].text)
            else:
                return False
            # check if trades and netting sets are setup correctly
            netting_set_ids = [ns.text for ns in self.input.xml['netting'].getroot().findall('./NettingSet/NettingSetId') ]
            for trade in trades:
                assert trade.findall('./Envelope/NettingSetId')[0].text in netting_set_ids
        return True


# configure folders
fin_system_folder = r"D:\repos\PyCUSystemicRisk\ToyExample"

# setup financial system
fs = FinancialSystem.from_folders(
    input_folder=os.path.join(fin_system_folder, 'Input'),
    output_folder=os.path.join(fin_system_folder, 'Output'),
    execution_folder=fin_system_folder
)

# kick off run
#fs.run('ore')

# inspect output
fs.parse_output()
fs.plots.plot_nettingset_exposures()
fs.plots.plot_trade_exposures()
print(fs.output.csv['npv'][['#TradeId', 'TradeType', 'NPV(Base)']])
print(fs.output.csv['xva'][['#TradeId', 'NettingSetId', 'BaselEEPE']])
print(fs.get_nettingset_eepe())

fs._set_graph()
fs.compute_systemic_balancedness()
d = fs.graph.nodes(data=True)
df = pd.DataFrame(index=[x[0] for x in d], data=[x[1] for x in d])
print(df)

fs.get_nettingset_eepe().to_csv(os.path.join(fs.output_folder, 'adjacency_matrix.csv'))
df['rho+'].to_csv(os.path.join(fs.output_folder, 'systemic_risk.csv'))
df['rho+'] = (df['rho+']*100).astype(int)
print("Total EEPE", df['EEPE+'].sum())
print("Total NPV", fs.output.csv['npv'][['NPV(Base)']].sum())
adj = fs.get_nettingset_eepe().astype(int)

print("Input consistent: " + str(fs.consistent_input()))