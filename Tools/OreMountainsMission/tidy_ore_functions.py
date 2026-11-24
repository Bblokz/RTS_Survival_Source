"""Tidy the Ore stage FUNCTION graphs (and the macro): splice reroute knots, arrange tightly, splice again.
Event graphs are hand laid out by build_ore_stages.py and are never auto-arranged.
Usage: python tidy_ore_functions.py [BlueprintName ...]"""
import sys

import ue
import tidy_graphs as tg
from build_ore_stages import STAGES

tg.BASE = ue.BASE


def tidy(name):
    bp = ue.BASE + name
    for graph in tg.graph_names(ue.PORT, bp):
        if graph == 'EventGraph':
            continue
        count, note = tg.splice_knots(ue.PORT, bp, graph)
        ue.a('arrange_blueprint_nodes', blueprint_path=bp, graph_name=graph, spacing_x=30, spacing_y=20)
        again, _ = tg.splice_knots(ue.PORT, bp, graph)
        print(f'{name}.{graph}: knots {count}+{again} ({note})', flush=True)
    result = ue.a('compile_blueprint', blueprint_path=bp)
    print(f'{name}: compile errors={result.get("error_count") if isinstance(result, dict) else result}')
    ue.py(f"import unreal\nprint(unreal.EditorAssetLibrary.save_asset('{bp}', only_if_is_dirty=False))")


if __name__ == '__main__':
    names = sys.argv[1:] or (['Mission_Ore_Base'] + ['Mission_Ore_' + s for s in STAGES])
    for n in names:
        tidy(n)
