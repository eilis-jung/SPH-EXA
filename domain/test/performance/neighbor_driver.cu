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
 * @brief  Find neighbors in Morton code sorted x,y,z arrays
 *
 * @author Sebastian Keller <sebastian.f.keller@gmail.com>
 */

#include <iomanip>
#include <iostream>
#include <iterator>

#include <thrust/device_vector.h>

#include "cstone/cuda/cuda_utils.cuh"
#include "cstone/findneighbors.hpp"

#include "cstone/traversal/find_neighbors.cuh"

#include "../coord_samples/random.hpp"
#include "timing.cuh"

using namespace cstone;

template<class T, class KeyType>
__global__ void findNeighborsKernel(const T* x,
                                    const T* y,
                                    const T* z,
                                    const T* h,
                                    LocalIndex firstId,
                                    LocalIndex lastId,
                                    LocalIndex n,
                                    cstone::Box<T> box,
                                    const KeyType* particleKeys,
                                    cstone::LocalIndex* neighbors,
                                    unsigned* neighborsCount,
                                    unsigned ngmax)
{
    cstone::LocalIndex tid = blockDim.x * blockIdx.x + threadIdx.x;
    cstone::LocalIndex id  = firstId + tid;
    if (id >= lastId) { return; }

    findNeighbors(id, x, y, z, h, box, particleKeys, neighbors + tid * ngmax, neighborsCount + tid, n, ngmax);
}

template<class T>
__global__ void findNeighborsTKernel(const T* x,
                                     const T* y,
                                     const T* z,
                                     const T* h,
                                     const TreeNodeIndex* childOffsets,
                                     const TreeNodeIndex* toLeafOrder,
                                     const LocalIndex* layout,
                                     const Vec3<T>* centers,
                                     const Vec3<T>* sizes,
                                     cstone::Box<T> box,
                                     LocalIndex firstId,
                                     LocalIndex lastId,
                                     unsigned ngmax,
                                     cstone::LocalIndex* neighbors,
                                     unsigned* neighborsCount)
{
    cstone::LocalIndex tid = blockDim.x * blockIdx.x + threadIdx.x;
    cstone::LocalIndex id  = firstId + tid;
    if (id >= lastId) { return; }

    findNeighborsT(id, x, y, z, h, childOffsets, toLeafOrder, layout, centers, sizes, box, ngmax,
                   neighbors + tid * ngmax, neighborsCount + id);
}

template<class T, class KeyType>
void benchmarkGpu()
{
    using Integer = typename KeyType::ValueType;

    Box<T> box{0, 1, BoundaryType::periodic};
    int n = 2000000;

    RandomCoordinates<T, KeyType> coords(n, box);
    std::vector<T> h(n, 0.012);

    int ngmax = 200;

    std::vector<LocalIndex> neighborsCPU(ngmax * n);
    std::vector<unsigned> neighborsCountCPU(n);

    const T* x        = coords.x().data();
    const T* y        = coords.y().data();
    const T* z        = coords.z().data();
    const auto* codes = (Integer*)(coords.particleKeys().data());

    unsigned bucketSize   = 64;
    auto [csTree, counts] = computeOctree(codes, codes + n, bucketSize);
    Octree<Integer> octree;
    octree.update(csTree.data(), nNodes(csTree));
    const TreeNodeIndex* childOffsets = octree.childOffsets().data();
    const TreeNodeIndex* toLeafOrder  = octree.toLeafOrder().data();

    std::vector<LocalIndex> layout(nNodes(csTree) + 1);
    std::exclusive_scan(counts.begin(), counts.end() + 1, layout.begin(), 0);

    std::vector<Vec3<T>> centers(octree.numTreeNodes()), sizes(octree.numTreeNodes());
    gsl::span<const Integer> nodeKeys(octree.nodeKeys().data(), octree.numTreeNodes());
    nodeFpCenters<KeyType>(nodeKeys, centers.data(), sizes.data(), box);

    auto findNeighborsCpu = [&]()
    {
#pragma omp parallel for
        for (LocalIndex i = 0; i < n; ++i)
        {
            // findNeighbors(i, x, y, z, h.data(), box, sfcKindPointer(codes), neighborsCPU.data() + i * ngmax,
            //               neighborsCountCPU.data() + i, n, ngmax);
            findNeighborsT(i, x, y, z, h.data(), childOffsets, toLeafOrder, layout.data(), centers.data(), sizes.data(),
                           box, ngmax, neighborsCPU.data() + i * ngmax, neighborsCountCPU.data() + i);
        }
    };

    float cpuTime = timeCpu(findNeighborsCpu);

    std::cout << "CPU time " << cpuTime << " s" << std::endl;
    std::copy(neighborsCountCPU.data(), neighborsCountCPU.data() + 64, std::ostream_iterator<int>(std::cout, " "));
    std::cout << std::endl;

    std::vector<cstone::LocalIndex> neighborsGPU(ngmax * n);
    std::vector<unsigned> neighborsCountGPU(n);

    thrust::device_vector<T> d_x(coords.x().begin(), coords.x().end());
    thrust::device_vector<T> d_y(coords.y().begin(), coords.y().end());
    thrust::device_vector<T> d_z(coords.z().begin(), coords.z().end());
    thrust::device_vector<T> d_h = h;
    thrust::device_vector<Integer> d_codes(coords.particleKeys().begin(), coords.particleKeys().end());
    thrust::device_vector<TreeNodeIndex> d_childOffsets(childOffsets, childOffsets + octree.numTreeNodes());
    thrust::device_vector<TreeNodeIndex> d_toLeafOrder(toLeafOrder, toLeafOrder + octree.numTreeNodes());
    thrust::device_vector<LocalIndex> d_layout = layout;
    thrust::device_vector<Vec3<T>> d_centers   = centers;
    thrust::device_vector<Vec3<T>> d_sizes     = sizes;

    thrust::device_vector<LocalIndex> d_neighbors(neighborsGPU.size());
    thrust::device_vector<unsigned> d_neighborsCount(neighborsCountGPU.size());

    const auto* deviceKeys = (const KeyType*)(rawPtr(d_codes));

    auto findNeighborsLambda = [&]()
    {
        // findNeighborsKernel<<<iceil(n, 256), 256>>>(rawPtr(d_x), rawPtr(d_y), rawPtr(d_z), rawPtr(d_h), 0, n, n, box,
        //                                             deviceKeys, rawPtr(d_neighbors), rawPtr(d_neighborsCount),
        //                                             ngmax);
        // findNeighborsTKernel<<<iceil(n, 128), 128>>>(rawPtr(d_x), rawPtr(d_y), rawPtr(d_z), rawPtr(d_h),
        //                                              rawPtr(d_childOffsets), rawPtr(d_toLeafOrder), rawPtr(d_layout),
        //                                              rawPtr(d_centers), rawPtr(d_sizes), box, 0, n, ngmax,
        //                                              rawPtr(d_neighbors), rawPtr(d_neighborsCount));

        findNeighborsBT(0, n, rawPtr(d_x), rawPtr(d_y), rawPtr(d_z), rawPtr(d_h), rawPtr(d_childOffsets),
                        rawPtr(d_toLeafOrder), rawPtr(d_layout), rawPtr(d_centers), rawPtr(d_sizes), box,
                        rawPtr(d_neighborsCount), rawPtr(d_neighbors), ngmax);
    };

    float gpuTime = timeGpu(findNeighborsLambda);

    thrust::copy(d_neighborsCount.begin(), d_neighborsCount.end(), neighborsCountGPU.begin());
    thrust::copy(d_neighbors.begin(), d_neighbors.end(), neighborsGPU.begin());

    std::cout << "GPU time " << gpuTime / 1000 << " s" << std::endl;
    std::copy(neighborsCountGPU.data(), neighborsCountGPU.data() + 64, std::ostream_iterator<int>(std::cout, " "));
    std::cout << std::endl;

    int numFails     = 0;
    int numFailsList = 0;
    for (int i = 0; i < n; ++i)
    {
        std::sort(neighborsCPU.data() + i * ngmax, neighborsCPU.data() + i * ngmax + neighborsCountCPU[i]);

        std::vector<cstone::LocalIndex> nilist(neighborsCountGPU[i]);
        for (unsigned j = 0; j < neighborsCountGPU[i]; ++j)
        {
            size_t warpOffset = (i / TravConfig::targetSize) * TravConfig::targetSize * ngmax;
            size_t laneOffset = i % TravConfig::targetSize;
            nilist[j]         = neighborsGPU[warpOffset + TravConfig::targetSize * j + laneOffset];
        }
        std::sort(nilist.begin(), nilist.end());

        if (neighborsCountGPU[i] != neighborsCountCPU[i])
        {
            std::cout << i << " " << neighborsCountGPU[i] << " " << neighborsCountCPU[i] << std::endl;
            numFails++;
        }

        if (!std::equal(begin(nilist), end(nilist), neighborsCPU.begin() + i * ngmax)) { numFailsList++; }
    }

    bool allEqual = std::equal(begin(neighborsCountGPU), end(neighborsCountGPU), begin(neighborsCountCPU));
    if (allEqual)
        std::cout << "Neighbor counts: PASS\n";
    else
        std::cout << "Neighbor counts: FAIL " << numFails << std::endl;

    std::cout << "numFailsList " << numFailsList << std::endl;
}

int main()
{
    // benchmarkGpu<double, MortonKey<uint64_t>>();
    benchmarkGpu<double, HilbertKey<uint64_t>>();
}
