comfortable_spacing = 20;
tolerance = 1;
finger_hole_diameter = 19.1;
liquid_cup_spacing = 10;
slot_spacing = 15;
tool_spacing = 15;
epsilon=0.1;
$fs=0.2;

module finger_hole(center, d=finger_hole_diameter) {
    translate(center)
        intersection() {
            sphere(d=d);
        
            translate([0, 0, -d/2]) {
                cube([d, d, d], center=true);
            }
        }
};

module x_cylinder(h, d) {
    // A cylinder in the first quadrant
    translate([0, d/2,  d/2])
    rotate(a=[0, 90, 0])
    cylinder(h, d=d);
}

module y_cylinder(h, d) {
    // A cylinder in the first quadrant
    translate([d/2, h, d/2])
    rotate(a=[90, 0, 0])
    cylinder(h, d=d);
}

// Vape - not used
vape_width = 24;
vape_cylindrical_length = 133.54;
vape_mouth_length = 22.8;

// Vape box
vape_foam_dimensions = [56.31, 167.5, 26.57];
vape_foam_corner = [0,0,0];

module foam_with_fingerholes() {
    union(){
    translate(vape_foam_corner)
        cube(vape_foam_dimensions);
        finger_hole([0, vape_foam_dimensions[1]/2,  vape_foam_dimensions[2]]);
        finger_hole([vape_foam_dimensions[0],   vape_foam_dimensions[1]/2, vape_foam_dimensions[2]]);
    manual_dimensions = [111, 55, 0.66]; // not used
    }
}

battery_length = 65.4 + tolerance*2;
battery_diameter = 18.5 + tolerance*2;
tier2_height = vape_foam_dimensions[2] - battery_diameter/2;
battery_position = [vape_foam_dimensions[0] + comfortable_spacing, vape_foam_dimensions[1] - battery_diameter, tier2_height - battery_diameter/2];
module battery_with_finger_holes() {
    // Spare battery      
    translate(battery_position) {
        x_cylinder(battery_length, battery_diameter);
        finger_hole([0, battery_diameter/2, battery_diameter/2], battery_diameter);
            finger_hole([battery_length, battery_diameter/2, battery_diameter/2], battery_diameter);
    };
}

liquid_cup_diameter = 10.8 + tolerance;
liquid_cup_height = 18;
liquid_cup_position = [vape_foam_dimensions[0] + comfortable_spacing, battery_position[1] - comfortable_spacing, tier2_height];
liquid_cup_position2 = [liquid_cup_position[0] + liquid_cup_diameter + liquid_cup_spacing, liquid_cup_position[1], tier2_height];
module liquid_cups_and_filters() {
    // Liquid cups (2)
    translate(liquid_cup_position)
        cylinder(liquid_cup_height, d=liquid_cup_diameter, center=true);
    translate(liquid_cup_position2) {
        cylinder(liquid_cup_height, d=liquid_cup_diameter, center=true);
    }

    // Extra filters
    filter_diameter_big = 11.6;
    filter_diameter_small = 8.4;
    filter_thickness = 0.22;
    filter_slot_thickness = filter_thickness + tolerance;
    num_large_filters = 2;
    num_small_filters = 2;

    large_filter_slot_position = [liquid_cup_position2[0] + comfortable_spacing, liquid_cup_position[1], tier2_height];
    translate(large_filter_slot_position)
        cube([filter_slot_thickness, filter_diameter_big, filter_diameter_big], center=true);
    small_filter_slot_position = [large_filter_slot_position[0] + slot_spacing, liquid_cup_position[1], tier2_height];
    translate(small_filter_slot_position)
        cube([filter_slot_thickness, filter_diameter_small, filter_diameter_small], center=true);
}

// Tools - brush
brush_rod_diameter = 1.63 + tolerance;
brush_hoop_diameter = 10.35 + tolerance;
// brush is curved -- this is max width perpendicalar to the rod. safety factor
brush_diameter = 12.43; // 10.7, but add safety since it's bent
brush_total_length = 70 + tolerance;
brush_length = 35;
brush_position = [vape_foam_dimensions[0] + comfortable_spacing, liquid_cup_position[1] - comfortable_spacing - brush_total_length, tier2_height - brush_diameter];

module brush() {
    translate([brush_diameter/2-brush_rod_diameter/2, 0, brush_diameter/2-brush_rod_diameter/2])
    y_cylinder(brush_total_length, brush_rod_diameter);
    translate([0, brush_total_length-brush_length, 0])
    y_cylinder(brush_length, brush_diameter);
    translate([brush_diameter/2, brush_hoop_diameter/2, brush_diameter/2])
    sphere(d=brush_hoop_diameter);
}

// Used to center some tools (packer)
tool_finger_hole_y = brush_position[1] + (brush_total_length-brush_length-brush_hoop_diameter)/2+brush_hoop_diameter;

// Tools - tweezers
tweezer_dimensions = [6 + tolerance, 92.4 + tolerance, 11.8 + tolerance]; //compressed

tweezer_position = [brush_position[0]+brush_diameter + tool_spacing, liquid_cup_position[1] - comfortable_spacing - tweezer_dimensions[1], tier2_height - tweezer_dimensions[2]];

// Tools - packer
//packer_rod_diameter = 2.6;
packer_max_diameter = 4.66 + tolerance;
packer_length = 57.6 + tolerance;
module packer() {
    y_cylinder(packer_length, packer_max_diameter);
}

packer_position = [tweezer_position[0] + tweezer_dimensions[0] + tool_spacing, tool_finger_hole_y - packer_length/2, tier2_height - packer_max_diameter];

module tweezers() {
    cube(tweezer_dimensions);
}

module tools() {
    translate(brush_position) {
        brush();
        translate([0,0,brush_diameter/2])
            linear_extrude(height=brush_diameter/2+epsilon)
                projection()
                    brush();
    }
    translate(tweezer_position)
        tweezers();
    translate(packer_position) {
        packer();
        translate([0,0,packer_max_diameter/2])
            linear_extrude(height=packer_max_diameter/2+epsilon)
                projection()
                    packer();
    }
    hull() {
        finger_hole([brush_position[0] + brush_diameter/2, tool_finger_hole_y, tier2_height]);
        finger_hole([tweezer_position[0] + tweezer_dimensions[0]/2, tool_finger_hole_y, tweezer_position[2] + tweezer_dimensions[2]]);
        finger_hole([packer_position[0] + packer_max_diameter/2, tool_finger_hole_y, tier2_height]);
    }
}

tier1_dimensions = [vape_foam_dimensions[0]+finger_hole_diameter/2 + comfortable_spacing, vape_foam_dimensions[1] + comfortable_spacing + comfortable_spacing, vape_foam_dimensions[2] + comfortable_spacing];
tier1_position = [-comfortable_spacing, -comfortable_spacing, -comfortable_spacing];

module tier1(){
    translate(tier1_position)
        cube(tier1_dimensions);
}

tier2_dimensions = [battery_length+battery_diameter+comfortable_spacing/2 + finger_hole_diameter/2, tweezer_dimensions[1]+comfortable_spacing*2.5+liquid_cup_diameter + battery_diameter, tier2_height+comfortable_spacing];
tier2_position = [tier1_position[0] + tier1_dimensions[0], tier1_position[1] -comfortable_spacing/2+ tier1_dimensions[1]-tier2_dimensions[1], -comfortable_spacing];
module tier2() {
    translate(tier2_position)
        cube(tier2_dimensions);
}

//difference() {
//    tier1();
//    minkowski(){
//        foam_with_fingerholes();
//        cube(epsilon,center=true);
//    };
//};

difference() {
    tier2();
    minkowski() {
        translate([finger_hole_diameter/2,0,0])
        union() {
            battery_with_finger_holes();
            liquid_cups_and_filters();
            tools();
        };
        cube(epsilon, center=true);
    }
}




// Weed compartment
// Film canisters
film_canister_diameter = 34.06;
film_canister_height = 54;

// Weed grinder
grinder_diameter = 55.6;
grinder_height = 38.8;