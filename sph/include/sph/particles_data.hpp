/*
 * MIT License
 *
 * Copyright (c) 2021 CSCS, ETH Zurich
 *               2021 University of Basel
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*! @file
 * @brief Contains the object holding all particle data
 *
 */

#pragma once

#include <array>
#include <vector>
#include <variant>

#include "cstone/cuda/cuda_utils.hpp"
#include "cstone/tree/accel_switch.hpp"
#include "cstone/tree/definitions.h"
#include "cstone/util/reallocate.hpp"

#include "sph/kernels.hpp"
#include "sph/tables.hpp"

#include "cstone/fields/data_util.hpp"
#include "cstone/fields/field_states.hpp"
#include "particles_data_stubs.hpp"

#if defined(USE_CUDA)
#include "sph/util/pinned_allocator.cuh"
#include "particles_data_gpu.cuh"
#endif

namespace sphexa
{

template<typename T, typename KeyType_, class AccType>
class ParticlesData : public cstone::FieldStates<ParticlesData<T, KeyType_, AccType>>
{
public:
    using KeyType         = KeyType_;
    using RealType        = T;
    using Tmass           = float;
    using XM1Type         = float;
    using AcceleratorType = AccType;

    template<class ValueType>
    using PinnedVec = std::vector<ValueType, PinnedAlloc_t<AcceleratorType, ValueType>>;

    template<class ValueType>
    using FieldVector = std::vector<ValueType, std::allocator<ValueType>>;

    ParticlesData()                     = default;
    ParticlesData(const ParticlesData&) = delete;

    size_t iteration{1};
    size_t numParticlesGlobal;
    size_t totalNeighbors;

    T ttot{0.0}, etot{0.0}, ecin{0.0}, eint{0.0}, egrav{0.0};
    T linmom{0.0}, angmom{0.0};

    //! current and previous (global) time-steps
    T minDt, minDt_m1;
    //! temporary MPI rank local timestep;
    T minDt_loc;

    //! @brief gravitational constant
    T g{0.0};

    //! @brief adiabatic index
    T gamma{5.0 / 3.0};
    //! @brief mean molecular weight of ions for models that use one value for all particles
    T muiConst{10.0};

    /*! @brief Particle fields
     *
     * The length of these arrays equals the local number of particles including halos
     * if the field is active and is zero if the field is inactive.
     */
    FieldVector<T>        x, y, z;                      // Positions
    FieldVector<XM1Type>  x_m1, y_m1, z_m1;             // Difference between current and previous positions
    FieldVector<T>        vx, vy, vz;                   // Velocities
    FieldVector<T>        rho;                          // Density
    FieldVector<T>        temp;                         // Temperature
    FieldVector<T>        u;                            // Internal Energy
    FieldVector<T>        p;                            // Pressure
    FieldVector<T>        prho;                         // p / (kx * m^2 * gradh)
    FieldVector<T>        h;                            // Smoothing Length
    FieldVector<Tmass>    m;                            // Mass
    FieldVector<T>        c;                            // Speed of sound
    FieldVector<T>        cv;                           // Specific heat
    FieldVector<T>        mue, mui;                     // mean molecular weight (electrons, ions)
    FieldVector<T>        divv, curlv;                  // Div(velocity), Curl(velocity)
    FieldVector<T>        ax, ay, az;                   // acceleration
    FieldVector<XM1Type>  du, du_m1;                    // energy rate of change (du/dt)
    FieldVector<T>        c11, c12, c13, c22, c23, c33; // IAD components
    FieldVector<T>        alpha;                        // AV coeficient
    FieldVector<T>        xm;                           // Volume element definition
    FieldVector<T>        kx;                           // Volume element normalization
    FieldVector<T>        gradh;                        // grad(h) term
    FieldVector<KeyType>  keys;                         // Particle space-filling-curve keys
    FieldVector<unsigned> nc;                           // number of neighbors of each particle

    //! @brief Indices of neighbors for each particle, length is number of assigned particles * ngmax. CPU version only.
    std::vector<cstone::LocalIndex> neighbors;

    DeviceData_t<AccType, T, KeyType> devData;

    const std::array<T, ::sph::lt::size> wh  = ::sph::lt::createWharmonicLookupTable<T, ::sph::lt::size>();
    const std::array<T, ::sph::lt::size> whd = ::sph::lt::createWharmonicDerivativeLookupTable<T, ::sph::lt::size>();

    /*! @brief
     * Name of each field as string for use e.g in HDF5 output. Order has to correspond to what's returned by data().
     */
    inline static constexpr std::array fieldNames{
        "x",   "y",   "z",   "x_m1", "y_m1", "z_m1", "vx", "vy",    "vz",    "rho",   "u",     "p",    "prho",
        "h",   "m",   "c",   "ax",   "ay",   "az",   "du", "du_m1", "c11",   "c12",   "c13",   "c22",  "c23",
        "c33", "mue", "mui", "temp", "cv",   "xm",   "kx", "divv",  "curlv", "alpha", "gradh", "keys", "nc"};

    static_assert(!cstone::HaveGpu<AcceleratorType>{} ||
                      fieldNames.size() == DeviceData_t<AccType, T, KeyType>::fieldNames.size(),
                  "ParticlesData on CPU and GPU must have the same fields");

    /*! @brief return a tuple of field references
     *
     * Note: this needs to be in the same order as listed in fieldNames
     */
    auto dataTuple()
    {
        auto ret = std::tie(x, y, z, x_m1, y_m1, z_m1, vx, vy, vz, rho, u, p, prho, h, m, c, ax, ay, az, du, du_m1, c11,
                            c12, c13, c22, c23, c33, mue, mui, temp, cv, xm, kx, divv, curlv, alpha, gradh, keys, nc);

        static_assert(std::tuple_size_v<decltype(ret)> == fieldNames.size());
        return ret;
    }

    /*! @brief return a vector of pointers to field vectors
     *
     * We implement this by returning an rvalue to prevent having to store pointers and avoid
     * non-trivial copy/move constructors.
     */
    auto data()
    {
        using FieldType =
            std::variant<FieldVector<float>*, FieldVector<double>*, FieldVector<unsigned>*, FieldVector<uint64_t>*>;

        return std::apply([](auto&... fields) { return std::array<FieldType, sizeof...(fields)>{&fields...}; },
                          dataTuple());
    }

    void setOutputFields(const std::vector<std::string>& outFields)
    {
        outputFieldNames   = outFields;
        outputFieldIndices = cstone::fieldStringsToInt(outFields, fieldNames);
    }

    void resize(size_t size)
    {
        double growthRate = 1.05;
        auto   data_      = data();

        for (size_t i = 0; i < data_.size(); ++i)
        {
            if (this->isAllocated(i))
            {
                std::visit([size, growthRate](auto& arg) { reallocate(*arg, size, growthRate); }, data_[i]);
            }
        }

        devData.resize(size);
    }

    //! @brief particle fields selected for file output
    std::vector<int>         outputFieldIndices;
    std::vector<std::string> outputFieldNames;

    constexpr static T sincIndex     = 6.0;
    constexpr static T Kcour         = 0.2;
    constexpr static T maxDtIncrease = 1.1;

    // Min. Atwood number in ramp function in momentum equation (crossed/uncrossed selection)
    // Complete uncrossed option (Atmin>=1.d50, Atmax it doesn't matter).
    // Complete crossed (Atmin and Atmax negative)
    constexpr static T Atmin = 0.1;
    constexpr static T Atmax = 0.2;
    constexpr static T ramp  = 1.0 / (Atmax - Atmin);

    // AV switches floor and ceiling
    constexpr static T alphamin       = 0.05;
    constexpr static T alphamax       = 1.0;
    constexpr static T decay_constant = 0.2;

    // Interpolation kernel normalization constant
    const static T K;
};

template<typename T, typename I, class Acc>
const T ParticlesData<T, I, Acc>::K = ::sph::compute_3d_k(sincIndex);

//! @brief resizes the neighbors list, only used in the CPU version
template<class Dataset>
void resizeNeighbors(Dataset& d, size_t size)
{
    double growthRate = 1.05;
    //! If we have a GPU, neighbors are calculated on-the-fly, so we don't need space to store them
    reallocate(d.neighbors, cstone::HaveGpu<typename Dataset::AcceleratorType>{} ? 0 : size, growthRate);
}

template<class Dataset, std::enable_if_t<not cstone::HaveGpu<typename Dataset::AcceleratorType>{}, int> = 0>
void transferToDevice(Dataset&, size_t, size_t, const std::vector<std::string>&)
{
}

template<class Dataset, std::enable_if_t<not cstone::HaveGpu<typename Dataset::AcceleratorType>{}, int> = 0>
void transferToHost(Dataset&, size_t, size_t, const std::vector<std::string>&)
{
}

template<class Vector, class T, std::enable_if_t<not IsDeviceVector<Vector>{}, int> = 0>
void fill(Vector& v, size_t first, size_t last, T value)
{
    std::fill(v.data() + first, v.data() + last, value);
}

} // namespace sphexa
