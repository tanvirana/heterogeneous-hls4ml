template <int width, bool dual_port, bool lut_ram, int partition_factor>
ap_uint<width>* ram_add_tables() {
    // fallback: no table for these widths
    #ifndef __SYNTHESIS__
        // segfaults
        printf("Undefined bram add call");
    #endif
    return nullptr;
}