// ============================================================
//  Basic 5" FPV frame — bottom plate
//  Parametric. Open in OpenSCAD (free), change the numbers in the
//  PARAMETERS block, press F5 to preview / F6 to render, then
//  File > Export > STL.
//  Built to fit: 30.5x30.5 M3 stack (your FC+ESC), 16x16 M3 motors.
//  Motor pads have a thinned recess in the center (keeps full
//  thickness only around the screw holes) to save weight.
// ============================================================

/* [PARAMETERS] */
wheelbase   = 220;   // motor-to-motor diagonal (mm). 5" freestyle ~ 210-225
plate_th    = 5;     // plate thickness (mm)
arm_w       = 14;    // arm width (mm)
body        = 46;    // central body square side (mm)
pad_r       = 14;    // motor mount pad radius (mm)
stack       = 30.5;  // FC/ESC stack hole spacing (square, mm)
motor_pat   = 16;    // motor bolt pattern (square, mm)
m3          = 3.2;   // M3 clearance hole diameter
motor_bore  = 11;    // center bore at each motor (bell + wires)
pocket_r    = 9;     // radius of thinned recess in motor pad center
pocket_dep  = 2.5;   // recess depth (leaves plate_th - pocket_dep floor)
$fn         = 64;

off = wheelbase/2/sqrt(2);                 // true-X motor offset
motors = [[off,off],[-off,off],[-off,-off],[off,-off]];

module profile2d() {                        // flat outline + through holes
    difference() {
        union() {
            square([body,body], center=true);           // central body
            for (m=motors) translate(m) circle(r=pad_r); // motor pads
            for (m=motors)                                // arms (center -> pad)
                translate([m[0]/2, m[1]/2])
                    rotate(atan2(m[1],m[0]))
                        square([norm(m)+8, arm_w], center=true);
        }
        for (sx=[stack/2,-stack/2], sy=[stack/2,-stack/2]) // stack M3
            translate([sx,sy]) circle(d=m3);
        for (m=motors) {                                   // motor bore + 16x16 M3
            translate(m) circle(d=motor_bore);
            for (dx=[motor_pat/2,-motor_pat/2], dy=[motor_pat/2,-motor_pat/2])
                translate([m[0]+dx, m[1]+dy]) circle(d=m3);
        }
    }
}

difference() {
    linear_extrude(plate_th) profile2d();
    // thinned recess in each motor pad (top face)
    for (m=motors)
        translate([m[0], m[1], plate_th - pocket_dep])
            cylinder(r=pocket_r, h=pocket_dep + 1);
}
