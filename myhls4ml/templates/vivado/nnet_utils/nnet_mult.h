#ifndef NNET_MULT_H_
#define NNET_MULT_H_

#include "hls_stream.h"
#include "nnet_common.h"
#include "nnet_helpers.h"
#include "../ram_table_utils/ram_mul_tables.h"
#include <iostream>
#include <math.h>

namespace nnet {

namespace product {

/* ---
 * different methods to perform the product of input and weight, depending on the
 * types of each.
 * --- */

class Product {};

template <class x_T, class w_T> class both_binary : public Product {
  public:
    static x_T product(x_T a, w_T w) {
        // specialisation for 1-bit weights and incoming data
        #pragma HLS INLINE
        return a == w;
    }
};

template <class x_T, class w_T> class weight_binary : public Product {
  public:
    static auto product(x_T a, w_T w) -> decltype(-a) {
        // Specialisation for 1-bit weights, arbitrary data
        #pragma HLS INLINE
        if (w == 0)
            return -a;
        else
            return a;
    }
};

template <class x_T, class w_T> class data_binary : public Product {
  public:
    static auto product(x_T a, w_T w) -> decltype(-w) {
        // Specialisation for 1-bit data, arbitrary weight
        #pragma HLS INLINE
        if (a == 0)
            return -w;
        else
            return w;
    }
};

template <class x_T, class w_T> class weight_ternary : public Product {
  public:
    static auto product(x_T a, w_T w) -> decltype(-a) {
        // Specialisation for 2-bit weights, arbitrary data
        #pragma HLS INLINE
        if (w == 0)
            return 0;
        else if (w == -1)
            return -a;
        else
            return a; // if(w == 1)
    }
};

template<int I, int F, ap_q_mode Q = AP_TRN, ap_o_mode O = AP_SAT, int N = 0>
ap_ufixed<I, F, Q, O, N> to_unsigned_fixed(ap_fixed<I, F> input) {
    if (input < 0) {
        return ap_ufixed<I, F, Q, O, N>(-input);
    } 
    return ap_ufixed<I, F, Q, O, N>(input);  
}

template<int I, int F, ap_q_mode Q = AP_TRN, ap_o_mode O = AP_SAT, int N = 0>
ap_ufixed<I, F, Q, O, N> to_unsigned_fixed(ap_ufixed<I, F, Q, O, N> input) {
    return input;
}

template <class x_T, class w_T, bool heterogeneous_config = false, 
bool default_mul = true, bool fabric_mul = false, bool dsp_mul = false, bool ram_mul = false,
bool dual_port = true, bool lut_ram = false, int ram_partition_factor = 1> class mult : public Product {
  public:
    static auto product(x_T a, w_T w) -> decltype(a * w) {
        // 'Normal' product
        #pragma HLS INLINE
        if (!heterogeneous_config) {
            return a * w;
        }

        bool a_neg = false;
        bool w_neg = false;

        if (a < 0) {
            a_neg = true;
        }

        auto a_abs = to_unsigned_fixed(a);
    
        if (w < 0) {
            w_neg = true;
        }

        auto w_abs = to_unsigned_fixed(w);

        using result_t = decltype(a * w);

        result_t unsigned_result;
        if (default_mul) {
            unsigned_result = a_abs * w_abs;
        } else if (fabric_mul) {
            #pragma HLS RESOURCE variable=unsigned_result core=Mul_LUT
            unsigned_result = a_abs * w_abs;
        } else if (dsp_mul) {
            #pragma HLS RESOURCE variable=unsigned_result core=DSP48
            unsigned_result = a_abs * w_abs;
        } else if (ram_mul) {
            constexpr int a_width = decltype(a_abs)::width;
            //printf("A_WIDTH IS %u", a_width);
            constexpr int w_width = decltype(w_abs)::width;
            //printf("W_WIDTH IS %u", w_width);
            constexpr int result_width = result_t::width;
            ap_uint<a_width + w_width> a_index = a_abs.to_uint();
            ap_uint<w_width> w_index = w_abs.to_uint();
            ap_uint<a_width + w_width> lookup_result = ram_mul_tables<a_width, w_width, dual_port, lut_ram, ram_partition_factor>()[(a_index << w_width) | w_index];
            unsigned_result = lookup_result.range(result_width - 1, 0);
        }

        if (a_neg != w_neg) {
            return -unsigned_result;
        } 
        return unsigned_result;
    }
};

template <class x_T, class w_T> class weight_exponential : public Product {
  public:
    using r_T = ap_fixed<2 * (decltype(w_T::weight)::width + x_T::width), (decltype(w_T::weight)::width + x_T::width)>;
    static r_T product(x_T a, w_T w) {
        // Shift product for exponential weights
        #pragma HLS INLINE

        // Shift by the exponent. Negative weights shift right
        r_T y = static_cast<r_T>(a) << w.weight;

        // Negate or not depending on weight sign
        return w.sign == 1 ? y : static_cast<r_T>(-y);
    }
};

} // namespace product

template <class data_T, class res_T, typename CONFIG_T>
inline typename std::enable_if<std::is_same<data_T, ap_uint<1>>::value &&
                                   std::is_same<typename CONFIG_T::weight_t, ap_uint<1>>::value,
                               ap_int<nnet::ceillog2(CONFIG_T::n_in) + 2>>::type
cast(typename CONFIG_T::accum_t x) {
    return (ap_int<nnet::ceillog2(CONFIG_T::n_in) + 2>)(x - CONFIG_T::n_in / 2) * 2;
}

template <class data_T, class res_T, typename CONFIG_T>
inline typename std::enable_if<
    std::is_same<data_T, ap_uint<1>>::value && !std::is_same<typename CONFIG_T::weight_t, ap_uint<1>>::value, res_T>::type
cast(typename CONFIG_T::accum_t x) {
    return (res_T)x;
}

template <class data_T, class res_T, typename CONFIG_T>
inline typename std::enable_if<(!std::is_same<data_T, ap_uint<1>>::value), res_T>::type cast(typename CONFIG_T::accum_t x) {
    return (res_T)x;
}

} // namespace nnet

#endif
