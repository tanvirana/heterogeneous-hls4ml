#ifndef NNET_DENSE_H_
#define NNET_DENSE_H_

#include "hls_stream.h"
#include "nnet_common.h"
#include "nnet_dense_latency.h"
#include "nnet_dense_resource.h"
#include "nnet_function_stubs.h"
#include "nnet_helpers.h"
#include "nnet_mult.h"
#include <math.h>

namespace nnet {

struct dense_config {
    // Internal data type definitions
    typedef float bias_t;
    typedef float weight_t;
    typedef float accum_t;

    // Layer Sizes
    static const unsigned n_in = 10;
    static const unsigned n_out = 10;

    // Resource reuse info
    static const unsigned io_type = io_parallel;
    static const unsigned strategy = latency;
    static const unsigned reuse_factor = 1;
    static const bool store_weights_in_bram = false;
    static const unsigned n_zeros = 0;

    template <class data_T, class res_T, class CONFIG_T> using kernel = nnet::DenseKernel<data_T, res_T, CONFIG_T>;

    // Partitioning arrays cyclically to go with roll factors?

    // Product function to use
    template <class x_T,
    class y_T,
    bool heterogeneous_config = false, 
    bool default_mul = true, 
    bool fabric_mul = false, 
    bool dsp_mul = false, 
    bool ram_mul = false,
    bool dual_port = true, 
    bool lut_ram = false, 
    int ram_partition_factor = 1> using product = nnet::product::mult<x_T, y_T, heterogeneous_config, default_mul, fabric_mul, dsp_mul, ram_mul, dual_port, lut_ram>;

    static const bool heterogeneous_config = false;
    static const int default_muls = 0;
    static const int fabric_muls = 0;
    static const int dsp_muls = 0;
    static const int ram_muls = 0;
    static const bool dual_port_muls = false;
    static const bool lut_ram_muls = false;
    static const int ram_partition_factor_muls = 1;
    static const bool default_add = true;
    static const bool fabric_add = false;
    static const bool dsp_add = false;
    static const bool ram_add = false;
    static const bool dual_port_add = false;
    static const bool lut_ram_add = false;
    static const int ram_partition_factor_add = 1;
};

template <class data_T, class res_T, typename CONFIG_T>
void dense(data_T data[CONFIG_T::n_in], res_T res[CONFIG_T::n_out],
           typename CONFIG_T::weight_t weights[CONFIG_T::n_in * CONFIG_T::n_out],
           typename CONFIG_T::bias_t biases[CONFIG_T::n_out]) {
    #pragma HLS INLINE
    CONFIG_T::template kernel<data_T, res_T, CONFIG_T>::dense(data, res, weights, biases);
}

template <class data_T, class res_T, typename CONFIG_T> class DenseLatency : public DenseKernel<data_T, res_T, CONFIG_T> {
  public:
    static void dense(data_T data[CONFIG_T::n_in], res_T res[CONFIG_T::n_out],
                      typename CONFIG_T::weight_t weights[CONFIG_T::n_in * CONFIG_T::n_out],
                      typename CONFIG_T::bias_t biases[CONFIG_T::n_out]) {
        #pragma HLS INLINE
        dense_latency<data_T, res_T, CONFIG_T>(data, res, weights, biases);
    }
};

template <class data_T, class res_T, typename CONFIG_T>
class DenseResource_rf_leq_nin : public DenseKernel<data_T, res_T, CONFIG_T> {
  public:
    static void dense(data_T data[CONFIG_T::n_in], res_T res[CONFIG_T::n_out],
                      typename CONFIG_T::weight_t weights[CONFIG_T::n_in * CONFIG_T::n_out],
                      typename CONFIG_T::bias_t biases[CONFIG_T::n_out]) {
        #pragma HLS INLINE
        dense_resource_rf_leq_nin<data_T, res_T, CONFIG_T>(data, res, weights, biases);
    }
};

template <class data_T, class res_T, typename CONFIG_T>
class DenseResource_rf_gt_nin_rem0 : public DenseKernel<data_T, res_T, CONFIG_T> {
  public:
    static void dense(data_T data[CONFIG_T::n_in], res_T res[CONFIG_T::n_out],
                      typename CONFIG_T::weight_t weights[CONFIG_T::n_in * CONFIG_T::n_out],
                      typename CONFIG_T::bias_t biases[CONFIG_T::n_out]) {
        #pragma HLS INLINE
        dense_resource_rf_gt_nin_rem0<data_T, res_T, CONFIG_T>(data, res, weights, biases);
    }
};

} // namespace nnet

#endif

