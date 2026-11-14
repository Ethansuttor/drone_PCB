import math, uuid, sys
from shapely.geometry import LineString, Point, Polygon, MultiPolygon, box
from shapely.ops import unary_union, split
from matplotlib.textpath import TextPath
from matplotlib.font_manager import FontProperties

def quad(p0,c,p1,n=60):
    return [((1-t)**2*p0[0]+2*(1-t)*t*c[0]+t*t*p1[0], (1-t)**2*p0[1]+2*(1-t)*t*c[1]+t*t*p1[1]) for t in [i/n for i in range(n+1)]]

cx,cy=60,36
sw=3.6; bw=2.4
upper=quad((6,cy),(60,cy-56),(114,cy)); lower=quad((114,cy),(60,cy+56),(6,cy))
outline=LineString(upper+lower[1:]).buffer(sw/2,join_style=1,cap_style=1)
iris=Point(cx,cy).buffer(17,256).exterior.buffer(sw/2)
R,r=15.6,6.4
blades=[]
pts=[(cx+r*math.cos(math.radians(-90+60*k)),cy+r*math.sin(math.radians(-90+60*k))) for k in range(6)]
for k in range(6):
    x0,y0=pts[k];x1,y1=pts[(k+1)%6];dx,dy=x1-x0,y1-y0;L=math.hypot(dx,dy);dx/=L;dy/=L
    fx,fy=x0-cx,y0-cy;b=fx*dx+fy*dy;c=fx*fx+fy*fy-R*R;t=-b+math.sqrt(b*b-c)
    blades.append(LineString([(x0,y0),(x0+dx*t,y0+dy*t)]).buffer(bw/2,cap_style=1))
eye=unary_union([outline,iris]+blades)

# text IRIS centered under eye
fp=FontProperties(fname="/usr/share/fonts/opentype/inter/Inter-Bold.otf")
size=float(sys.argv[2]) if len(sys.argv)>2 else 26
gap=size*0.24
glyphs=[];cur=0
for ch in "IRIS":
    tp=TextPath((0,0),ch,prop=fp,size=size)
    g=Polygon()
    for ring in tp.to_polygons():
        if len(ring)>=3: g=g.symmetric_difference(Polygon([(x,-y) for x,y in ring]))
    minx=g.bounds[0]
    from shapely import affinity
    g=affinity.translate(g,cur-minx,0)
    glyphs.append(g); cur=g.bounds[2]+gap
text=unary_union(glyphs)
from shapely import affinity
tb=text.bounds
eb=eye.bounds
text=affinity.translate(text,(eb[0]+eb[2])/2-(tb[0]+tb[2])/2, eb[3]+size*0.42-tb[1])
logo=unary_union([eye,text])

def fracture(g):
    polys=list(g.geoms) if hasattr(g,"geoms") else [g]
    out=[]
    while polys:
        p=polys.pop()
        if p.is_empty: continue
        if not p.interiors: out.append(p); continue
        hx=p.interiors[0].centroid.x
        bx=p.bounds
        parts=split(p,LineString([(hx,bx[1]-1),(hx,bx[3]+1)]))
        polys+= [q for q in parts.geoms if q.area>1e-9]
    return out

W=float(sys.argv[1]) if len(sys.argv)>1 else 14.0
b=logo.bounds; k=W/(b[2]-b[0]); mx=(b[0]+b[2])/2; my=(b[1]+b[3])/2
logo_mm=affinity.scale(affinity.translate(logo,-mx,-my),k,k,origin=(0,0))
pieces=fracture(logo_mm)
name=f"IRIS_eye_stack_{W:g}mm"
H=logo_mm.bounds[3]-logo_mm.bounds[1]
lines=[f'(footprint "{name}" (version 20221018) (generator "pcbnew") (generator_version "10.0")',
 '  (layer "F.Cu")',
 f'  (descr "IRIS eye logo with IRIS text below, {W:g} x {H:.2f} mm, front silkscreen")',
 '  (attr board_only exclude_from_pos_files exclude_from_bom)',
 f'  (fp_text reference "G***" (at 0 0) (layer "F.SilkS") hide\n      (effects (font (size 1 1) (thickness 0.15)))\n    (uuid {uuid.uuid4()})\n  )',
 f'  (fp_text value "{name}" (at 0 {H/2+1:.2f}) (layer "F.Fab") hide\n      (effects (font (size 1 1) (thickness 0.15)))\n    (uuid {uuid.uuid4()})\n  )']
for p in pieces:
    c=list(p.exterior.coords)[:-1]
    lines.append('  (fp_poly\n    (pts\n'+"\n".join(f'      (xy {x:.4f} {y:.4f})' for x,y in c)+f'\n    )\n    (stroke (width 0) (type solid)) (fill solid) (layer "F.SilkS") (uuid {uuid.uuid4()}))')
lines.append(')')
open(name+".kicad_mod","w").write("\n".join(lines)+"\n")
# svg preview
def path(g):
    gs=list(g.geoms) if hasattr(g,"geoms") else [g]
    d=""
    for p in gs:
        for ring in [p.exterior]+list(p.interiors):
            cs=list(ring.coords); d+="M"+" L".join(f"{x:.3f} {y:.3f}" for x,y in cs)+"Z "
    return d
bb=logo_mm.bounds;pad=0.6
svg=f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{bb[0]-pad} {bb[1]-pad} {bb[2]-bb[0]+2*pad} {bb[3]-bb[1]+2*pad}" style="color:#000"><path fill="currentColor" fill-rule="evenodd" d="{path(logo_mm)}"/></svg>'
open("iris_eye_stack.svg","w").write(svg)
print(name, f"{W:g} x {H:.2f} mm", len(pieces),"polygons")
# min feature check: thinnest stroke
print("stroke widths mm: outline %.3f, blades %.3f"%(sw*k,bw*k))
