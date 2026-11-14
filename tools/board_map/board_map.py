"""Plot footprint courtyards on each side of a KiCad board.
Usage: python board_map.py <board.kicad_pcb> [out.png]
Needs matplotlib. Courtyards are bounding boxes, so free space is understated."""
import re,math,sys
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Polygon,Circle,Rectangle
s=open(sys.argv[1]).read()
def blocks(s,key):
    out=[];i=0
    rx=re.compile(r"\("+key+r"[\s)]")
    while True:
        m=rx.search(s,i)
        if not m:break
        i=m.start()
        d=0
        for j in range(i,len(s)):
            c=s[j]
            if c=='(':d+=1
            elif c==')':
                d-=1
                if d==0:break
        out.append(s[i:j+1]);i=j+1
    return out
fps=blocks(s,"footprint")
ex=[];ey=[]
for g in blocks(s,"gr_line")+blocks(s,"gr_arc")+blocks(s,"gr_rect"):
    if 'Edge.Cuts' in g:
        for a,b in re.findall(r'\((?:start|end|mid) ([-\d.]+) ([-\d.]+)\)',g): ex.append(float(a)); ey.append(float(b))
X0,X1,Y0,Y1=min(ex),max(ex),min(ey),max(ey)
fig,axs=plt.subplots(1,2,figsize=(14,7))
for ax,side in zip(axs,["F","B"]):
    ax.set_title({"F":"Top (front)","B":"Bottom (back, seen from top)"}[side])
    pass
for fp in fps:
    name=re.match(r'\(footprint "([^"]*)"',fp)[1]
    lay=re.search(r'\(layer "([FB])\.Cu"\)',fp)[1]
    m=re.search(r'\n\s*\(at ([-\d.]+) ([-\d.]+)(?: ([-\d.]+))?\)',fp)
    x,y,r=float(m[1]),float(m[2]),float(m[3] or 0)
    ref=re.search(r'\(property "Reference" "([^"]*)"',fp)
    ref=ref[1] if ref else name
    pts=[]
    for g in blocks(fp,"fp_line")+blocks(fp,"fp_rect")+blocks(fp,"fp_poly")+blocks(fp,"fp_circle")+blocks(fp,"fp_arc"):
        if 'CrtYd' in g:
            pts+= [(float(a),float(b)) for a,b in re.findall(r'\((?:start|end|xy|mid) ([-\d.]+) ([-\d.]+)\)',g)]
    if not pts:
        for p in blocks(fp,"pad"):
            mm=re.search(r'\(at ([-\d.]+) ([-\d.]+)',p); sz=re.search(r'\(size ([-\d.]+) ([-\d.]+)',p)
            if mm and sz:
                px,py,w,h=float(mm[1]),float(mm[2]),float(sz[1]),float(sz[2])
                pts+=[(px-w/2,py-h/2),(px+w/2,py+h/2)]
    if not pts: continue
    xs=[p[0] for p in pts];ys=[p[1] for p in pts]
    corners=[(min(xs),min(ys)),(max(xs),min(ys)),(max(xs),max(ys)),(min(xs),max(ys))]
    a=math.radians(-r)  # kicad rotation CCW with y down
    tc=[(x+cx*math.cos(a)-cy*math.sin(a), y+cx*math.sin(a)+cy*math.cos(a)) for cx,cy in corners]
    th = "MountingHole" in name
    for ax,side in zip(axs,["F","B"]):
        if lay==side or th:
            ax.add_patch(Polygon(tc,closed=True,fc="#e57373" if not th else "#90a4ae",ec="k",lw=0.5,alpha=0.8))
            if not th: ax.text(sum(p[0] for p in tc)/4,sum(p[1] for p in tc)/4,ref,fontsize=6,ha="center",va="center")
for ax in axs:
    ax.add_patch(Rectangle((X0,Y0),X1-X0,Y1-Y0,fill=False,lw=2,ec="k"));ax.set_xlim(X0-2,X1+2);ax.set_ylim(Y1+2,Y0-2);ax.set_aspect("equal");ax.grid(True,lw=0.3)
    ax.tick_params(labelsize=7)
axs[1].invert_xaxis()
plt.tight_layout();plt.savefig(sys.argv[2] if len(sys.argv)>2 else "board_map.png",dpi=110)
