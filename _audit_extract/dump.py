import xml.etree.ElementTree as ET
t = ET.parse(r'D:\Drone\drone_PCB\_audit_extract\netlist.xml')
r = t.getroot()
comps = {}
for c in r.iter('comp'):
    ref = c.get('ref')
    val = c.findtext('value') or ''
    fp = c.findtext('footprint') or ''
    comps[ref] = (val, fp)
print('=== COMPONENTS ===')
for ref,(val,fp) in sorted(comps.items()):
    print(f'{ref}\t{val}\t{fp}')
print()
print('=== NETS ===')
for n in r.iter('net'):
    name = n.get('name')
    nodes = []
    for nd in n.findall('node'):
        nodes.append(f"{nd.get('ref')}.{nd.get('pin')}({nd.get('pinfunction','')})")
    print(f'{name}: ' + ', '.join(nodes))
