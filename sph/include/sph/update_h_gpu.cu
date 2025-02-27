/*
 * MIT License
 *
 * Copyright (c) 2022 CSCS, ETH Zurich
 *               2022 University of Basel
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
 * @brief Smoothing length update on the GPU
 * @author Sebastian Keller <sebastian.f.keller@gmail.com>
 */

#include <cmath>

#include "cstone/util/util.hpp"
#include "cstone/tree/definitions.h"

namespace sph
{

template<class Th>
__global__ void updateSmoothingLengthGpuKernel(size_t first, size_t last, unsigned ng0, const unsigned* nc, Th* h)
{
    cstone::LocalIndex i = first + blockDim.x * blockIdx.x + threadIdx.x;
    if (i >= last) { return; }

    // Note: these constants are duplicated in the CPU version, so don't forget to change them there as well
    constexpr double c0  = 7.0;
    constexpr double exp = 1.0 / 3.0;

    unsigned nn = nc[i];
    h[i]        = h[i] * 0.5 * std::pow((1.0 + c0 * ng0 / nn), exp);
}

template<class Th>
void updateSmoothingLengthGpu(size_t first, size_t last, unsigned ng0, const unsigned* nc, Th* h)
{
    unsigned numThreads = 256;
    unsigned numBlocks  = iceil(last - first, 256);

    updateSmoothingLengthGpuKernel<<<numBlocks, numThreads>>>(first, last, ng0, nc, h);
}

template void updateSmoothingLengthGpu(size_t first, size_t last, unsigned ng0, const unsigned* nc, float* h);
template void updateSmoothingLengthGpu(size_t first, size_t last, unsigned ng0, const unsigned* nc, double* h);

} // namespace sph