template <int width1, int width2, bool dual_port, bool lut_ram, int partition_factor>
ap_uint<width1 + width2>* ram_mul_tables() {
    // fallback: no table for these widths
    #ifndef __SYNTHESIS__
        // segfaults
        printf("Undefined bram mul called for  for %u %u", width1, width2);
    #endif
    return nullptr;
}