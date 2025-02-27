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
 * @brief SPH density kernel tests
 *
 * @author Ruben Cabezon <ruben.cabezon@unibas.ch>
 * @author Sebastian Keller <sebastian.f.keller@gmail.com>
 */

#include <vector>

#include "gtest/gtest.h"

#include "sph/hydro_ve/iad_kern.hpp"
#include "sph/tables.hpp"

using namespace sph;

TEST(IAD, JLoop)
{
    using T = double;

    T sincIndex = 6.0;
    T K         = compute_3d_k(sincIndex);

    std::array<double, lt::size> wh  = lt::createWharmonicLookupTable<double, lt::size>();
    std::array<double, lt::size> whd = lt::createWharmonicDerivativeLookupTable<double, lt::size>();

    cstone::Box<T> box(0, 6, cstone::BoundaryType::open);

    // particle 0 has 4 neighbors
    std::vector<cstone::LocalIndex> neighbors{1, 2, 3, 4};
    unsigned                        neighborsCount = 4, i;

    std::vector<T> x{1.0, 1.1, 3.2, 1.3, 2.4};
    std::vector<T> y{1.1, 1.2, 1.3, 4.4, 5.5};
    std::vector<T> z{1.2, 2.3, 1.4, 1.5, 1.6};
    std::vector<T> h{5.0, 5.1, 5.2, 5.3, 5.4};
    std::vector<T> m{1.0, 1.0, 1.0, 1.0, 1.0};
    std::vector<T> xm{m[0] / 1.1, m[1] / 1.2, m[2] / 1.3, m[3] / 1.4, m[4] / 1.5};
    std::vector<T> kx{-1.0, -1.0, -1.0, -1.0, -1.0};
    for (i = 0; i < neighborsCount + 1; i++)
    {
        kx[i] = K * xm[i] / math::pow(h[i], 3);
    }
    /* distances of particle zero to particle j
     *
     * j = 1   1.10905
     * j = 2   2.21811
     * j = 3   3.32716
     * j = 4   4.63465
     */

    // fill with invalid initial value to make sure that the kernel overwrites it instead of add to it
    std::vector<T> iad(6, -1);

    // compute the 6 tensor components for particle 0
    IADJLoop(0, sincIndex, K, box, neighbors.data(), neighborsCount, x.data(), y.data(), z.data(), h.data(), wh.data(),
             whd.data(), xm.data(), kx.data(), &iad[0], &iad[1], &iad[2], &iad[3], &iad[4], &iad[5]);

    EXPECT_NEAR(iad[0], 0.31413443265068125, 1e-10);
    EXPECT_NEAR(iad[1], -0.058841281079, 1e-10);
    EXPECT_NEAR(iad[2], -0.096300685874, 1e-10);
    EXPECT_NEAR(iad[3], 0.17170816943657527, 1e-10);
    EXPECT_NEAR(iad[4], -0.078629533251, 1e-10);
    EXPECT_NEAR(iad[5], 0.90805776843544594, 1e-10);
}

TEST(IAD, JLoopPBC)
{
    using T = double;

    T sincIndex = 6.0;
    T K         = compute_3d_k(sincIndex);

    std::array<double, lt::size> wh  = lt::createWharmonicLookupTable<double, lt::size>();
    std::array<double, lt::size> whd = lt::createWharmonicDerivativeLookupTable<double, lt::size>();

    // box length in any dimension must be bigger than 4*h for any particle
    // otherwise the PBC evaluation does not select the closest image
    cstone::Box<T> box(0, 10.5, cstone::BoundaryType::periodic);

    // particle 0 has 4 neighbors
    std::vector<cstone::LocalIndex> neighbors{1, 2, 3, 4};
    unsigned                        neighborsCount = 4;

    std::vector<T> x{1.0, 1.1, 3.2, 1.3, 9.4};
    std::vector<T> y{1.1, 1.2, 1.3, 8.4, 9.5};
    std::vector<T> z{1.2, 2.3, 1.4, 1.5, 9.6};
    std::vector<T> h{2.5, 2.51, 2.52, 2.53, 2.54};
    std::vector<T> m{1.1, 1.2, 1.3, 1.4, 1.5};
    std::vector<T> xm{m[0] / 0.014, m[1] / 0.015, m[2] / 0.016, m[3] / 0.017, m[4] / 0.018};
    std::vector<T> kx{1.0, 1.0, 1.0, 1.0, 1.0};

    /* distances of particle 0 to particle j
     *
     *          PBC
     * j = 1    1.10905
     * j = 2    2.21811
     * j = 3    3.22800
     * j = 4    3.63731
     */

    // fill with invalid initial value to make sure that the kernel overwrites it instead of add to it
    std::vector<T> iad(6, -1);

    IADJLoop(0, sincIndex, K, box, neighbors.data(), neighborsCount, x.data(), y.data(), z.data(), h.data(), wh.data(),
             whd.data(), xm.data(), kx.data(), &iad[0], &iad[1], &iad[2], &iad[3], &iad[4], &iad[5]);

    EXPECT_NEAR(iad[0], 0.42970014180599519, 1e-10);
    EXPECT_NEAR(iad[1], -0.2304555811353339, 1e-10);
    EXPECT_NEAR(iad[2], -0.052317231832885822, 1e-10);
    EXPECT_NEAR(iad[3], 2.8861688071845268, 1e-10);
    EXPECT_NEAR(iad[4], -0.23251632520430554, 1e-10);
    EXPECT_NEAR(iad[5], 0.36028770403046995, 1e-10);
}
