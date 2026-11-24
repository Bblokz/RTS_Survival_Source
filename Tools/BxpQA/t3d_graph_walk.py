"""Offline T3D parser: per BXP Blueprint, what the Bp_OnHealthChanged function graph and BP_OnUnitDies reach
(functions called, non-default pin values), plus HealthLevelsToNotifyOn array entries."""
import re, sys, glob, os

NODE_BEGIN = re.compile(r'^(\s*)Begin Object (?:Class=(\S+) )?Name="([^"]+)" ExportPath="[^\']*\'[^:]*:([^\.\']+)\.([^\'"]+)\'"')

def parse(path):
    lines = open(path, encoding='utf-8', errors='replace').read().splitlines()
    nodes = {}
    i = 0
    while i < len(lines):
        m = NODE_BEGIN.match(lines[i])
        if not m:
            i += 1
            continue
        indent, cls, name, graph, nodename = m.groups()
        body = []
        j = i + 1
        while j < len(lines) and not (lines[j].startswith(indent + 'End Object') and len(lines[j]) - len(lines[j].lstrip()) == len(indent)):
            body.append(lines[j]); j += 1
        key = graph + ':' + name
        d = nodes.setdefault(key, dict(graph=graph, cls=None, func=None, event=None, pins=[], props=''))
        if cls:
            d['cls'] = cls.split('.')[-1]
        text = '\n'.join(body)
        if 'CustomProperties Pin' in text or 'FunctionReference' in text:
            d['props'] = text
            f = re.search(r'FunctionReference=\(.*?MemberName="([^"]+)"', text)
            e = re.search(r'EventReference=\(.*?MemberName="([^"]+)"', text)
            vr = re.search(r'VariableReference=\(.*?MemberName="([^"]+)"', text)
            d['func'] = (f.group(1) if f else None) or (('var:' + vr.group(1)) if vr else None)
            d['event'] = e.group(1) if e else None
            for p in re.finditer(r'CustomProperties Pin \((.*)\)', text):
                s = p.group(1)
                g = lambda r: (re.search(r, s).group(1) if re.search(r, s) else '')
                lk = re.search(r'LinkedTo=\(([^)]*)\)', s)
                d['pins'].append(dict(name=g(r'PinName="([^"]*)"'), dir='out' if 'EGPD_Output' in s else 'in',
                                      default=g(r'DefaultValue="([^"]*)"') or g(r'DefaultObject="([^"]*)"'),
                                      cat=g(r'PinType.PinCategory="(\w+)"'),
                                      links=[graph + ':' + t for t, _ in re.findall(r'(\w+) (\w{32})', lk.group(1))] if lk else []))
        i = j + 1
    return '\n'.join(lines), nodes

def walk(nodes, start):
    seen, order, stack = set(), [], [start]
    while stack:
        n = stack.pop()
        if n in seen or n not in nodes:
            continue
        seen.add(n); order.append(n)
        for p in nodes[n]['pins']:
            if p['dir'] == 'out' and p['cat'] == 'exec':
                stack.extend(reversed(p['links']))
    return order

def describe(nodes, n):
    d = nodes[n]
    label = d['func'] or d['cls']
    defs = [p['name'] + '=' + p['default'].split('/')[-1].split('.')[-1][:45] for p in d['pins']
            if p['dir'] == 'in' and p['default'] and p['cat'] != 'exec' and p['name'] != 'self']
    return label + ('(' + ','.join(defs) + ')' if defs else '')

for path in sorted(glob.glob(sys.argv[1] + '/*.t3d')):
    text, nodes = parse(path)
    levels = re.findall(r'HealthLevelsToNotifyOn\(\d+\)=(\w+)', text)
    print('=== %s levels=%s' % (os.path.basename(path)[:-4], sorted(set(levels))))
    for n, d in nodes.items():
        if d['graph'] == 'Bp_OnHealthChanged' and d['cls'] == 'K2Node_FunctionEntry':
            print('  HEALTH: ' + ' -> '.join(describe(nodes, x) for x in walk(nodes, n)[1:])[:900])
        if d['event'] == 'Bp_OnHealthChanged':
            print('  HEALTH_EV: ' + ' -> '.join(describe(nodes, x) for x in walk(nodes, n)[1:])[:900])
        if d['event'] in ('BP_OnUnitDies',):
            print('  DIES: ' + ' -> '.join(describe(nodes, x) for x in walk(nodes, n)[1:])[:500])
    meshes = sorted(set(re.findall(r'(SM_[A-Za-z0-9_]*(?:At50|At33|Damaged)[A-Za-z0-9_]*)', text)))
    if meshes:
        print('  DMG_MESH_REFS: ' + ','.join(meshes))
