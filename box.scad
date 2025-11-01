// tbv schaakdingetje

// 24 naast elkaar, 32 breed
// 16 vooruit, 36 lang
// 14 hoog rs232, 20 binnen

thickness = 3;
pcb_width = 32 + 1;
pcb_length = 36 + 1;
wiring_height = 25;
connector_height = 17;
radius_pcb_holder = 0.8;

difference() {
  cube([pcb_width + thickness * 2, pcb_length + thickness * 2, wiring_height + thickness]);  // global box
  translate([thickness, thickness, thickness]) cube([pcb_width, pcb_length, wiring_height + 5]);  // inside
  translate([thickness, pcb_length + thickness-1, thickness]) cube([pcb_width, pcb_length + thickness * 2, connector_height]);  // rs232 connector hole
   // slot for usb connector
   translate([-thickness/2, thickness+8, thickness+wiring_height-8]) color([1,0,0]) cube([thickness*2, 12, 5]);
    // slot for antenna
   translate([-thickness/2 + pcb_length, thickness+8, thickness+wiring_height-8]) color([1,0,0]) cube([thickness*2, 12, thickness+8]);    
}

// left back
translate([thickness + 3 + 1, thickness + 3, thickness + connector_height /6]) cylinder(h=connector_height / 3, r1=radius_pcb_holder, r2=radius_pcb_holder, center=true);
// right back
translate([thickness + 24 + 4 + 1, thickness + 3, thickness + connector_height /6]) cylinder(h=connector_height / 3, r1=radius_pcb_holder, r2=radius_pcb_holder, center=true);
// left front
translate([thickness + 3 + 1, thickness + 3 + 17 + 1, thickness + connector_height /6]) cylinder(h=connector_height / 3, r1=radius_pcb_holder, r2=radius_pcb_holder, center=true);
// right front
translate([thickness + 24 + 4 + 1, thickness + 3 + 17 + 1, thickness + connector_height /6]) cylinder(h=connector_height / 3, r1=radius_pcb_holder, r2=radius_pcb_holder, center=true);


//// deksel
translate([-thickness*2,0,0])
rotate([0,180,0])
difference() {
    translate([32 * 1.5,0,0]) {
        translate([-thickness*1.5,-thickness*1.5,0])
        rotate([0,180,0])
        difference() {
          cube([pcb_width + thickness * 5, pcb_length + thickness * 5, wiring_height/2 ]);
          translate([thickness *1.25, thickness *1.25, thickness]) cube([pcb_width + thickness * 2.5, pcb_length + thickness * 2.5, thickness * 6]);
        }
    }
    // usb
    color([0,0,1]) translate([thickness*-1.9,9,-(thickness + 10)]) cube([thickness*2, 16, 11]);
    //antenne
    color([1,0,1]) translate([thickness*1.9+pcb_width,9,-(thickness + 10)]) cube([thickness*2, 16, 11+thickness]);
}