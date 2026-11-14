"""Routing review checks for a KiCad .kicad_pcb (written 2026-10-29 for v2).

Usage:
  python routing_check.py BOARD.kicad_pcb [--png out.png] [--hl NET1,NET2] [--box x0,x1,y0,y1]

Prints:
  1. per net routed length by layer, widths, via count
  2. non GND copper within 1.4 mm of each NPTH mounting hole edge (about a 3 mm radius
     for M3 hardware)
  3. tracks within 3 mm of switch node copper on the same layer, and tracks under
     the bodies listed in BODIES (edit for a new layout)
  4. same layer parallel coupling, different nets, edge gap under 0.35 mm
Optional PNG: F.Cu red, B.Cu blue, highlighted nets green/teal, pads, vias, refs.
Distances are approximate (segments sampled), good to about 0.05 mm. Arcs are ignored.
"""
import re, sys, math, json, argparse, collections

SW_NETS = {'Net-(D2-K)', 'Net-(U6-BOOT)'}          # TPS5450 PH and BOOT
BODIES = {'L3': (141.6, 148.9, 101.6, 108.9)}       # x0, x1, y0, y1 in board mm

def parse(s):
    tok = re.findall(r'\(|\)|"(?:[^"\\]|\\.)*"|[^\s()]+', s)
    st = [[]]
    for t in tok:
        if t == '(':
            st.append([])
        elif t == ')':
            x = st.pop(); st[-1].append(x)
        else:
            st[-1].append(t[1:-1] if t.startswith('"') else t)
    return st[0][0]

def get(n, k):
    for c in n:
        if isinstance(c, list) and c and c[0] == k:
            return c

def all_(n, k):
    return [c for c in n if isinstance(c, list) and c and c[0] == k]

def load(path):
    B = parse(open(path, encoding='utf-8').read())
    nets = {n[1]: n[2] for n in all_(B, 'net') if len(n) >= 3}
    def netof(x):
        n = get(x, 'net')
        return None if not n else nets.get(n[1], n[1])
    g = {'seg': [], 'via': [], 'pads': [], 'edge': []}
    for s in all_(B, 'segment'):
        a, b = get(s, 'start'), get(s, 'end')
        g['seg'].append([float(a[1]), float(a[2]), float(b[1]), float(b[2]),
                         float(get(s, 'width')[1]), get(s, 'layer')[1], netof(s)])
    for v in all_(B, 'via'):
        a = get(v, 'at')
        g['via'].append([float(a[1]), float(a[2]), float(get(v, 'size')[1]), netof(v)])
    for e in B:
        if isinstance(e, list) and e and e[0] in ('gr_line', 'gr_arc') and get(e, 'layer') and get(e, 'layer')[1] == 'Edge.Cuts':
            g['edge'].append([(float(c[1]), float(c[2])) for c in e[1:] if isinstance(c, list) and c[0] in ('start', 'mid', 'end')])
    for f in all_(B, 'footprint'):
        ref = [p[2] for p in all_(f, 'property') if p[1] == 'Reference'][0]
        at = get(f, 'at'); fx, fy = float(at[1]), float(at[2]); rot = float(at[3]) if len(at) > 3 else 0
        for p in all_(f, 'pad'):
            pa = get(p, 'at'); px, py = float(pa[1]), float(pa[2]); prot = float(pa[3]) if len(pa) > 3 else 0
            r = math.radians(-rot)
            x = fx + px * math.cos(r) - py * math.sin(r); y = fy + px * math.sin(r) + py * math.cos(r)
            sz = get(p, 'size')
            g['pads'].append([ref, p[1], x, y, float(sz[1]), float(sz[2]), prot, p[2], netof(p)])
    return g

def pd(px, py, ax, ay, bx, by):
    dx, dy = bx - ax, by - ay; L = dx * dx + dy * dy
    t = 0 if L == 0 else max(0, min(1, ((px - ax) * dx + (py - ay) * dy) / L))
    return math.hypot(px - ax - t * dx, py - ay - t * dy)

def samples(s, step=0.25):
    L = math.hypot(s[2] - s[0], s[3] - s[1]); n = max(2, int(L / step))
    for i in range(n):
        t = (i + .5) / n
        yield s[0] + t * (s[2] - s[0]), s[1] + t * (s[3] - s[1]), L / n

def report(g):
    L = collections.defaultdict(lambda: collections.defaultdict(float)); W = collections.defaultdict(set); V = collections.Counter()
    for s in g['seg']:
        L[s[6]][s[5]] += math.hypot(s[2] - s[0], s[3] - s[1]); W[s[6]].add(s[4])
    for v in g['via']:
        V[v[3]] += 1
    print('== 1. routed length by net (mm) ==')
    for n in sorted(L, key=lambda k: -sum(L[k].values())):
        print('%-26s %6.1f %s widths %s vias %d' % (n, sum(L[n].values()), {k: round(v, 1) for k, v in L[n].items()}, sorted(W[n]), V[n]))
    print('\n== 2. non GND copper within 1.4 mm of a mounting hole edge ==')
    for h in [p for p in g['pads'] if p[7] == 'np_thru_hole' and p[4] >= 2.5]:  # M3 holes only, skips connector pegs
        hr = h[4] / 2
        near = {}
        for s in g['seg']:
            if s[6] == 'GND': continue
            d = pd(h[2], h[3], *s[:4]) - s[4] / 2 - hr
            if d < 1.4: near[(s[6], s[5])] = min(near.get((s[6], s[5]), 9), round(d, 2))
        for v in g['via']:
            if v[3] == 'GND': continue
            d = math.hypot(v[0] - h[2], v[1] - h[3]) - v[2] / 2 - hr
            if d < 1.4: near[('via', v[3])] = min(near.get(('via', v[3]), 9), round(d, 2))
        print((round(h[2], 2), round(h[3], 2)), near or 'clear')
    print('\n== 3. switch node proximity (same layer, edge gap < 3 mm) and tracks under bodies ==')
    sw = [s for s in g['seg'] if s[6] in SW_NETS]
    res = {}
    for s in g['seg']:
        if s[6] in SW_NETS or s[6] == 'GND': continue
        for w in sw:
            if w[5] != s[5]: continue
            d = min(pd(x, y, *w[:4]) for x, y, _ in samples(s)) - s[4] / 2 - w[4] / 2
            if d < 3: res[(s[6], s[5])] = min(res.get((s[6], s[5]), 9), round(d, 2))
    print(res or 'none')
    for name, (x0, x1, y0, y1) in BODIES.items():
        under = collections.defaultdict(float)
        for s in g['seg']:
            for x, y, dl in samples(s):
                if x0 < x < x1 and y0 < y < y1: under[(s[6], s[5])] += dl
        print(name, {k: round(v, 1) for k, v in under.items()})
    print('\n== 4. parallel coupling, edge gap < 0.35 mm (mm) ==')
    cp = collections.defaultdict(float)
    for a in g['seg']:
        for x, y, dl in samples(a):
            nb = {b[6] for b in g['seg'] if b[5] == a[5] and b[6] != a[6] and pd(x, y, *b[:4]) - a[4] / 2 - b[4] / 2 < 0.35}
            for k in nb: cp[tuple(sorted((a[6], k)))] += dl / 2
    for k, v in sorted(cp.items(), key=lambda x: -x[1])[:20]:
        if v > 1: print(k, round(v, 1))

def render(g, out, hl, box):
    import matplotlib; matplotlib.use('Agg')
    import matplotlib.pyplot as plt, matplotlib.transforms as T
    from matplotlib.patches import Rectangle
    xs = [p[0] for e in g['edge'] for p in e]; ys = [p[1] for e in g['edge'] for p in e]
    x0, x1, y0, y1 = box or (min(xs) - 1, max(xs) + 1, min(ys) - 1, max(ys) + 1)
    fig, ax = plt.subplots(figsize=(16, 16 * (y1 - y0) / (x1 - x0)))
    for s in g['seg']:
        c = '#d03030' if s[5] == 'F.Cu' else '#3060d0'; a = .55
        if s[6] in hl: c = '#00b000' if s[5] == 'F.Cu' else '#008080'; a = .95
        ax.plot([s[0], s[2]], [s[1], s[3]], color=c, lw=s[4] * 72 * 16 / (x1 - x0) * .8, solid_capstyle='round', alpha=a)
    for p in g['pads']:
        r = Rectangle((p[2] - p[4] / 2, p[3] - p[5] / 2), p[4], p[5], color='#b08000', alpha=.5, zorder=3)
        r.set_transform(T.Affine2D().rotate_deg_around(p[2], p[3], -p[6]) + ax.transData); ax.add_patch(r)
    for v in g['via']: ax.add_patch(plt.Circle((v[0], v[1]), v[2] / 2, color='k', alpha=.6, zorder=4))
    refs = collections.defaultdict(list)
    for p in g['pads']: refs[p[0]].append((p[2], p[3]))
    for r, pts in refs.items():
        cx = sum(a for a, b in pts) / len(pts); cy = sum(b for a, b in pts) / len(pts)
        if x0 < cx < x1 and y0 < cy < y1:
            ax.text(cx, cy, r, fontsize=8, ha='center', va='center', weight='bold', zorder=5, bbox=dict(fc='white', alpha=.6, lw=0, pad=.5))
    for e in g['edge']: ax.plot([p[0] for p in e], [p[1] for p in e], 'y-', lw=2)
    ax.set_xlim(x0, x1); ax.set_ylim(y1, y0); ax.set_aspect('equal'); ax.grid(alpha=.2)
    plt.savefig(out, dpi=90, bbox_inches='tight'); print('wrote', out)

if __name__ == '__main__':
    ap = argparse.ArgumentParser(); ap.add_argument('pcb'); ap.add_argument('--png'); ap.add_argument('--hl', default='')
    ap.add_argument('--box'); ap.add_argument('--json', help='also dump parsed geometry')
    a = ap.parse_args(); g = load(a.pcb)
    if a.json: json.dump(g, open(a.json, 'w'))
    report(g)
    if a.png: render(g, a.png, set(filter(None, a.hl.split(','))), tuple(map(float, a.box.split(','))) if a.box else None)
