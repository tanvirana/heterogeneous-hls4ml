#ifndef NNET_DENSE_LATENCY_H_
#define NNET_DENSE_LATENCY_H_

#include "hls_stream.h"
#include "nnet_common.h"
#include "nnet_helpers.h"
#include "nnet_mult.h"
#include "../ram_table_utils/ram_add_tables.h"
#include <math.h>

namespace nnet {

template <class data_T, class res_T, typename CONFIG_T>
void dense_latency(data_T data[CONFIG_T::n_in], res_T res[CONFIG_T::n_out],
                   typename CONFIG_T::weight_t weights[CONFIG_T::n_in * CONFIG_T::n_out],
                   typename CONFIG_T::bias_t biases[CONFIG_T::n_out]) {
    data_T cache;
    typename CONFIG_T::accum_t mult[CONFIG_T::n_in * CONFIG_T::n_out];
    typename CONFIG_T::accum_t acc[CONFIG_T::n_out];

    // Use a function_instantiate in case it helps to explicitly optimize unchanging weights/biases
    #pragma HLS function_instantiate variable=weights,biases

    // For parallel inputs:
    //   - completely partition arrays -- target fabric
    //   - if we have an unroll factor, limit number of multipliers
    #pragma HLS PIPELINE II=CONFIG_T::reuse_factor

    // #pragma HLS ARRAY_PARTITION variable=weights complete // remove this line for now, it breaks compression sometimes
    #pragma HLS ARRAY_PARTITION variable=biases complete
    #pragma HLS ARRAY_PARTITION variable=mult complete
    #pragma HLS ARRAY_PARTITION variable=acc complete

    if (!CONFIG_T::heterogeneous_config) {
        #pragma HLS ALLOCATION operation instances=mul limit=CONFIG_T::multiplier_limit
        // Do the matrix-multiply
        Product1:
            for (int ii = 0; ii < CONFIG_T::n_in; ii++) {
                cache = data[ii];
            Product2:
                for (int jj = 0; jj < CONFIG_T::n_out; jj++) {
                    int index = ii * CONFIG_T::n_out + jj;
                    mult[index] = CONFIG_T::template product<data_T, typename CONFIG_T::weight_t>::product(cache, weights[index]);
                }
            }

        // Initialize accumulator with input biases
        ResetAccum:
            for (int iacc = 0; iacc < CONFIG_T::n_out; iacc++) {
                acc[iacc] = (typename CONFIG_T::accum_t)biases[iacc];
            }

        // Accumulate multiplication result
        Accum1:
            for (int ii = 0; ii < CONFIG_T::n_in; ii++) {
            Accum2:
                for (int jj = 0; jj < CONFIG_T::n_out; jj++) {
                    int index = ii * CONFIG_T::n_out + jj;
                    acc[jj] += mult[index];
                }
            }

        // Cast to "res_t" type
        Result:
            for (int ires = 0; ires < CONFIG_T::n_out; ires++) {
                // res[ires] = (res_T) (acc[ires]);
                res[ires] = cast<data_T, res_T, CONFIG_T>(acc[ires]);
            }

    } else {

        #ifndef __SYNTHESIS__ 
            printf("Creating dense layer hetereneous resource implementation.\n");
        #endif

        data_T mul_in1[CONFIG_T::n_nonzeros]; // Will contain duplicates of input data
        #pragma HLS ARRAY_PARTITION variable=mul_in1 complete
        typename CONFIG_T::weight_t mul_in2[CONFIG_T::n_nonzeros];
        #pragma HLS ARRAY_PARTITION variable=mul_in2 complete
        typename CONFIG_T::accum_t mul_results[CONFIG_T::n_nonzeros];
        #pragma HLS ARRAY_PARTITION variable=mul_results complete
        int js[CONFIG_T::n_nonzeros];
        #pragma HLS ARRAY_PARTITION variable=js

        int packed_i = 0;
        for (int ii = 0; ii < CONFIG_T::n_in; ii++) {
            for (int jj = 0; jj < CONFIG_T::n_out; jj++) {
                int index = ii * CONFIG_T::n_out + jj;
                if (weights[index] != 0) {
                    mul_in1[packed_i] = data[ii];
                    mul_in2[packed_i] = weights[index];
                    js[packed_i] = jj;
                    packed_i += 1;
                }
            }
        }

        for (int i = 0; i < CONFIG_T::n_nonzeros; i++) {
            #pragma HLS unroll factor=CONFIG_T::multiplier_limit
            if (i < CONFIG_T::default_muls) {
                mul_results[i] = CONFIG_T::template product<data_T, typename CONFIG_T::weight_t, true, true, false, false, false, true, false, 1>::product(mul_in1[i], mul_in2[i]);
            } else if (i < CONFIG_T::default_muls + CONFIG_T::fabric_muls) {
                mul_results[i] = CONFIG_T::template product<data_T, typename CONFIG_T::weight_t, true, false, true, false, false, true, false, 1>::product(mul_in1[i], mul_in2[i]);
            } else if (i < CONFIG_T::default_muls + CONFIG_T::fabric_muls + CONFIG_T::dsp_muls) {
                mul_results[i] = CONFIG_T::template product<data_T, typename CONFIG_T::weight_t, true, false, false, true, false, true, false, 1>::product(mul_in1[i], mul_in2[i]);
            } else if (i < CONFIG_T::default_muls + CONFIG_T::fabric_muls + CONFIG_T::dsp_muls + CONFIG_T::ram_muls) {
                mul_results[i] = CONFIG_T::template product<data_T, typename CONFIG_T::weight_t, true, false, false, false, true, CONFIG_T::dual_port_muls, CONFIG_T::lut_ram_muls, CONFIG_T::ram_partition_factor_muls>::product(mul_in1[i], mul_in2[i]);
            }
        }

        // Adding n_in sets of n_out acc_t values, to obtain n_out res_T values
        // For each of the n_in sets, the number of layers is int(math.ceil(math.log(n_out + 1, 2))) without sparsity
        // With sparsity this changes for every single layer because the number of acc_t values added varies
        // We know n_nonzeros but not where they are... or do we? We could technically process the weights to find out
        // Although probably this level of control is way too fine grained... for now let's just do homogeneous configurations
        // Without sparsity you could do per-layer configurations because all sets would have the same number of inputs
        // Homogeneous configurations allow for reuse factor to extend to the accumulation

        for (int iacc = 0; iacc < CONFIG_T::n_out; iacc++) {
            acc[iacc] = (typename CONFIG_T::accum_t) biases[iacc];
        }

        for (int i = 0; i < CONFIG_T::n_nonzeros; i++) {
            #pragma HLS unroll factor=CONFIG_T::multiplier_limit
            typename CONFIG_T::accum_t add_result;
            if (CONFIG_T::default_add) {
                add_result = acc[js[i]] + mul_results[i];
            } else if (CONFIG_T::fabric_add) {
                add_result = acc[js[i]] + mul_results[i];
            } else if (CONFIG_T::dsp_add) {
                add_result = acc[js[i]] + mul_results[i];
            } else if (CONFIG_T::ram_add) {
                constexpr int width = res_T::width;
                ap_uint<2 * width> in1 = acc[js[i]].to_uint();
                ap_uint<width> in2 = mul_results[i].to_uint();
                printf("In width is %u", width);
                // ap_uint<2 * width> lookup_result = ram_add_tables<width, CONFIG_T::dual_port_add, CONFIG_T::lut_ram_add, CONFIG_T::ram_partition_factor_add>()[(in1 << width) | in2];
                //add_result = lookup_result.range(width - 1, 0);
                add_result = acc[js[i]] + mul_results[i];
            }
            acc[js[i]] = add_result;
        }

        for (int ires = 0; ires < CONFIG_T::n_out; ires++) {
            res[ires] = cast<data_T, res_T, CONFIG_T>(acc[ires]);
        }
    }
}

} // namespace nnet

#endif
