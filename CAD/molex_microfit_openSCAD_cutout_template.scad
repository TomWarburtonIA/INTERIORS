module molex_microfit_cutout() {
    base_w = 10.21;
    base_h = 4.21;

    ears_w = 13.89;
    ears_h = 3.05;

    notch_w = 3.18;
    notch_h = 1.3;

    union() {
        // Base rectangle
        translate([-(base_w/2), -(base_h/2)])
            square([base_w, base_h]);

        // Mounting ears rectangle
        translate([-(ears_w/2), -(ears_h/2)])
            square([ears_w, ears_h]);

        // Top notch
        translate([-(notch_w/2), base_h/2])
            square([notch_w, notch_h]);
    }
}

// Uncomment below to extrude into 3D
 linear_extrude(height = 3)
     molex_microfit_cutout();

// Or just preview in 2D
molex_microfit_cutout();
