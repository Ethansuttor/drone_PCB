import numpy as np, trimesh
from trimesh.transformations import rotation_matrix as R
from math import hypot, atan2, sqrt

# ---- PARAMETERS (edit these) ----
WHEELBASE   = 220.0   # motor-to-motor diagonal (mm) — 5" freestyle
PLATE_TH    = 5.0     # bottom-plate thickness (mm)
ARM_W       = 14.0    # arm width (mm)
BODY        = 46.0    # central body square side (mm)
PAD_R       = 14.0    # motor mount pad radius (mm)
STACK       = 30.5    # FC/ESC stack hole spacing (mm, square) -> M3
MOTOR_PAT   = 16.0    # motor bolt pattern (mm, square) -> M3
M3          = 3.2     # M3 clearance hole diameter
MOTOR_BORE  = 11.0    # center bore at each motor (bell/wires)
POCKET_R    = 9.0     # radius of the thinned recess in the motor pad center
POCKET_DEP  = 2.5     # how deep the recess is (leaves PLATE_TH-POCKET_DEP floor)
# ---------------------------------

off = WHEELBASE/2.0/sqrt(2.0)                 # motor x,y offset (true X)
motors = [(off,off),(-off,off),(-off,-off),(off,-off)]

def box(l,w,h): return trimesh.creation.box(extents=(l,w,h))
def cyl(r,h):   return trimesh.creation.cylinder(radius=r, height=h, sections=64)

solids=[box(BODY,BODY,PLATE_TH)]             # center
for mx,my in motors:
    L=hypot(mx,my)+8
    a=box(L,ARM_W,PLATE_TH)
    a.apply_transform(R(atan2(my,mx),[0,0,1]))
    a.apply_translation((mx/2,my/2,0))
    solids.append(a)
    p=cyl(PAD_R,PLATE_TH); p.apply_translation((mx,my,0)); solids.append(p)

frame=trimesh.boolean.union(solids)          # spans z = -PLATE_TH/2 .. +PLATE_TH/2

holes=[]
for sx in (STACK/2,-STACK/2):                # stack M3 (through)
    for sy in (STACK/2,-STACK/2):
        c=cyl(M3/2,PLATE_TH+4); c.apply_translation((sx,sy,0)); holes.append(c)
for mx,my in motors:                          # motor bore (through) + 16x16 M3 (through)
    b=cyl(MOTOR_BORE/2,PLATE_TH+4); b.apply_translation((mx,my,0)); holes.append(b)
    for dx in (MOTOR_PAT/2,-MOTOR_PAT/2):
        for dy in (MOTOR_PAT/2,-MOTOR_PAT/2):
            h=cyl(M3/2,PLATE_TH+4); h.apply_translation((mx+dx,my+dy,0)); holes.append(h)
    # thinned recess in the pad center (top face only), clears the screw bosses
    pk=cyl(POCKET_R, POCKET_DEP+2)
    pk.apply_translation((mx,my, PLATE_TH/2 - POCKET_DEP/2 + 1))
    holes.append(pk)

frame=trimesh.boolean.difference([frame]+holes)
frame.apply_translation((0,0,PLATE_TH/2))    # sit on z=0 build plate

for p in ["/sessions/focused-optimistic-einstein/mnt/outputs/5inch_frame_bottom.stl",
          "/sessions/focused-optimistic-einstein/mnt/drone_PCB/5inch_frame_bottom.stl"]:
    frame.export(p)

b=frame.bounds
print("watertight:",frame.is_watertight)
print("footprint mm: %.1f x %.1f, thickness %.1f"%(b[1][0]-b[0][0],b[1][1]-b[0][1],b[1][2]-b[0][2]))
print("pad floor at recess: %.1f mm ; full ring around screws: %.1f mm"%(PLATE_TH-POCKET_DEP, PLATE_TH))
print("volume cm3: %.1f  -> ~%.0f g PLA"%(frame.volume/1000, frame.volume/1000*1.24))
