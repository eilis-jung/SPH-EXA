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
 * @brief Barnes-Hut breadth-first warp-aware tree traversal inspired by the original Bonsai implementation
 *
 * @author Rio Yokota <rioyokota@gsic.titech.ac.jp>
 * @author Sebastian Keller <sebastian.f.keller@gmail.com>
 */

#pragma once

#include <algorithm>
#include "cstone/cuda/gpu_config.cuh"
#include "cstone/primitives/warpscan.cuh"
#include "kernel.hpp"
#include "kernel_wrapper.hpp"

namespace ryoanji
{

using cstone::shflSync;
using cstone::imin;
using cstone::ballotSync;
using cstone::syncWarp;
using cstone::shflDownSync;
using cstone::shflUpSync;
using cstone::inclusiveScanInt;
using cstone::inclusiveSegscanInt;
using cstone::streamCompact;
using cstone::shflXorSync;
using cstone::warpMax;
using cstone::warpMin;
using cstone::GpuConfig;

struct TravConfig
{
    //! @brief size of global workspace memory per warp
    static constexpr int memPerWarp = 2048 * GpuConfig::warpSize;
    //! @brief number of threads per block for the traversal kernel
    static constexpr int numThreads = 256;

    static constexpr int numWarpsPerSm = 20;
    //! @brief maximum number of simultaneously active blocks
    inline static int maxNumActiveBlocks =
        GpuConfig::smCount * (TravConfig::numWarpsPerSm / (TravConfig::numThreads / GpuConfig::warpSize));

    //! @brief number of particles per target, i.e. per warp
    static constexpr int targetSize = 64;

    //! @brief number of warps per target, used all over the place, hence the short name
    static constexpr int nwt = targetSize / GpuConfig::warpSize;
};

__device__ __forceinline__ int ringAddr(const int i) { return i & (TravConfig::memPerWarp - 1); }

/*! @brief evaluate multipole acceptance criterion
 *
 * @tparam T
 * @param sourceCenter  center-mass coordinates of the source cell
 * @param MAC           the square of the MAC acceptance radius around @p sourceCenter
 * @param targetCenter  geometric center of the target particle group bounding box
 * @param targetSize    the half length in each dimension of the target bounding box
 * @return              true if the target is too close for using the multipole
 */
template<class T>
__host__ __device__ __forceinline__ bool applyMAC(Vec3<T> sourceCenter, T MAC, Vec3<T> targetCenter, Vec3<T> targetSize)
{
    Vec3<T> dX = abs(targetCenter - sourceCenter) - targetSize;
    dX += abs(dX);
    dX *= T(0.5);
    T R2 = norm2(dX);
    return R2 < MAC;
}

/*! @brief apply M2P kernel with GpuConfig::warpSize different multipoles to the target bodies
 *
 * @tparam       T            float or double
 * @tparam       MType        Multipole type, e.g. Cartesian or Spherical of varying expansion order
 * @param[inout] acc_i        target particle acceleration to add to
 * @param[in]    pos_i        target particle (x,y,z,h) of each lane
 * @param[in]    cellIdx      the index of each lane of the multipole to apply, in [0:numSourceCells]
 * @param[in]    srcCenter    pointer to source cell centers in global memory, length numSourceCells
 * @param[in]    Multipoles   pointer to the multipole array in global memory, length numSourceCells
 * @param[-]     warpSpace    shared memory for temporary multipole storage, uninitialized
 *
 * Number of computed M2P interactions per call is GpuConfig::warpSize^2 * TravConfig::nwt
 */
template<class Ta, class Tc, class Tf, class MType>
__device__ void approxAcc(Vec4<Ta> acc_i[TravConfig::nwt], const Vec4<Tc> pos_i[TravConfig::nwt], const int cellIdx,
                          const Vec4<Tf>* __restrict__ srcCenter, const MType* __restrict__ Multipoles,
                          volatile int* warpSpace)
{
    constexpr int termSize = MType{}.size();
    static_assert(termSize <= GpuConfig::warpSize, "multipole size too large for shared-mem warpSpace");

    using MValueType = typename MType::value_type;

    auto* sm_Multipole      = reinterpret_cast<volatile MValueType*>(warpSpace);
    auto* __restrict__ gm_M = reinterpret_cast<const MValueType*>(Multipoles);

    const int laneIdx = threadIdx.x & (GpuConfig::warpSize - 1);

    for (int j = 0; j < GpuConfig::warpSize; j++)
    {
        int currentCell = shflSync(cellIdx, j);
        if (currentCell < 0) { continue; }

        Vec3<Tf> pos_j = makeVec3(srcCenter[currentCell]);

        if (laneIdx < termSize) { sm_Multipole[laneIdx] = gm_M[currentCell * termSize + laneIdx]; }
        syncWarp();

#pragma unroll
        for (int k = 0; k < TravConfig::nwt; k++)
        {
            acc_i[k] = M2P(acc_i[k], makeVec3(pos_i[k]), pos_j, *(MType*)sm_Multipole);
        }
    }
}

/*! @brief compute body-body interactions
 *
 * @tparam       T            float or double
 * @param[in]    sourceBody   source body x,y,z,m
 * @param[in]    hSource      source body smoothing length
 * @param[inout] acc_i        target acceleration to add to
 * @param[in]    pos_i        target body x,y,z,h
 *
 * Number of computed P2P interactions per call is GpuConfig::warpSize^2 * TravConfig::nwt
 */
template<class Tc, class Th, class Ta>
__device__ void directAcc(Vec4<Tc> sourceBody, Th hSource, Vec4<Ta> acc_i[TravConfig::nwt],
                          const Vec4<Tc> pos_i[TravConfig::nwt])
{
    for (int j = 0; j < GpuConfig::warpSize; j++)
    {
        Vec3<Tc> pos_j{shflSync(sourceBody[0], j), shflSync(sourceBody[1], j), shflSync(sourceBody[2], j)};
        Tc       m_j = shflSync(sourceBody[3], j);
        Th       h_j = shflSync(hSource, j);

#pragma unroll
        for (int k = 0; k < TravConfig::nwt; k++)
        {
            acc_i[k] = P2P(acc_i[k], makeVec3(pos_i[k]), pos_j, m_j, h_j, Th(pos_i[k][3]));
        }
    }
}

/*! @brief traverse one warp with up to 64 target bodies down the tree
 *
 * @param[inout] acc_i          acceleration of target to add to, TravConfig::nwt Vec4 per lane
 * @param[in]    pos_i          target positions, and smoothing lengths, TravConfig::nwt per lane
 * @param[in]    targetCenter   geometrical target center
 * @param[in]    targetSize     geometrical target bounding box size
 * @param[in]    x,y,z,m,h      source bodies as referenced by tree cells
 * @param[in]    childOffsets   location (index in [0:numTreeNodes]) of first child of each cell, 0 indicates a leaf
 * @param[in]    internalToLeaf for each cell in [0:numTreeNodes], stores the leaf cell (cstone) index in [0:numLeaves]
 *                              if the cell is not a leaf, the value is negative
 * @param[in]    layout         for each leaf cell in [0:numLeaves], stores the index of the first body in the cell
 * @param[in]    sourceCenter   x,y,z center and square MAC radius of each cell in [0:numTreeNodes]
 * @param[in]    Multipoles     the multipole expansions in the same order as srcCells
 * @param[in]    rootRange      source cell indices indices of the top 8 octants
 * @param[-]     tempQueue      shared mem int pointer to GpuConfig::warpSize ints, uninitialized
 * @param[-]     cellQueue      pointer to global memory, 4096 ints per thread, uninitialized
 * @return                      Number of M2P and P2P interactions applied to the group of target particles.
 *                              The totals for the warp are the numbers returned here times the number of valid
 *                              targets in the warp.
 *
 * Constant input pointers are additionally marked __restrict__ to indicate to the compiler that loads
 * can be routed through the read-only/texture cache.
 */
template<class Tc, class Th, class Tm, class Ta, class Tf, class MType>
__device__ util::tuple<unsigned, unsigned>
           traverseWarp(Vec4<Ta>* acc_i, const Vec4<Tc> pos_i[TravConfig::nwt], const Vec3<Tf> targetCenter,
                        const Vec3<Tf> targetSize, const Tc* __restrict__ x, const Tc* __restrict__ y, const Tc* __restrict__ z,
                        const Tm* __restrict__ m, const Th* __restrict__ h, const TreeNodeIndex* __restrict__ childOffsets,
                        const TreeNodeIndex* __restrict__ internalToLeaf, const LocalIndex* __restrict__ layout,
                        const Vec4<Tf>* __restrict__ sourceCenter, const MType* __restrict__ Multipoles, int2 rootRange,
                        volatile int* tempQueue, int* cellQueue)
{
    const int laneIdx = threadIdx.x & (GpuConfig::warpSize - 1);

    unsigned p2pCounter = 0;
    unsigned m2pCounter = 0;

    int approxQueue; // warp queue for multipole approximation cell indices
    int bodyQueue;   // warp queue for source body indices

    // populate initial cell queue
    for (int root = rootRange.x; root < rootRange.y; root += GpuConfig::warpSize)
    {
        if (root + laneIdx < rootRange.y) { cellQueue[ringAddr(root - rootRange.x + laneIdx)] = root + laneIdx; }
    }

    // these variables are always identical on all warp lanes
    int numSources   = rootRange.y - rootRange.x; // current stack size
    int newSources   = 0;                         // stack size for next level
    int oldSources   = 0;                         // cell indices done
    int sourceOffset = 0; // current level stack pointer, once this reaches numSources, the level is done
    int apxFillLevel = 0; // fill level of the multipole approximation warp queue
    int bdyFillLevel = 0; // fill level of the source body warp queue

    while (numSources > 0) // While there are source cells to traverse
    {
        const int      sourceIdx   = sourceOffset + laneIdx;                      // Source cell index of current lane
        int            sourceQueue = cellQueue[ringAddr(oldSources + sourceIdx)]; // Global source cell index in queue
        const Vec4<Tf> MAC         = sourceCenter[sourceQueue];                   // load source cell center + MAC
        const Vec3<Tf> curSrcCenter{MAC[0], MAC[1], MAC[2]};                      // Current source cell center
        const int      childBegin = childOffsets[sourceQueue];                    // First child cell
        const bool     isNode     = childBegin;
        const bool     isClose    = applyMAC(curSrcCenter, MAC[3], targetCenter, targetSize); // Is too close for MAC
        const bool     isSource   = sourceIdx < numSources; // Source index is within bounds
        const bool     isDirect   = isClose && !isNode && isSource;
        const int      leafIdx    = (isDirect) ? internalToLeaf[sourceQueue] : 0; // the cstone leaf index

        // Split
        const bool isSplit      = isNode && isClose && isSource; // Source cell must be split
        const int  numChild     = 8 & -int(isSplit);             // Number of child cells (masked by split flag)
        const int  numChildScan = inclusiveScanInt(numChild);    // Inclusive scan of numChild
        const int  numChildLane = numChildScan - numChild;       // Exclusive scan of numChild
        const int  numChildWarp = shflSync(numChildScan, GpuConfig::warpSize - 1); // Total numChild of current warp
        sourceOffset += imin(GpuConfig::warpSize, numSources - sourceOffset);  // advance current level stack pointer
        if (numChildWarp + numSources - sourceOffset > TravConfig::memPerWarp) // If cell queue overflows
            return {0xFFFFFFFF, 0xFFFFFFFF};                                   // Exit kernel
        int childIdx = oldSources + numSources + newSources + numChildLane;    // Child index of current lane
        for (int i = 0; i < numChild; i++)                                     // Loop over child cells for each lane
            cellQueue[ringAddr(childIdx + i)] = childBegin + i;                // Queue child cells for next level
        newSources += numChildWarp; //  Increment source cell count for next loop

        // Multipole approximation
        const bool isApprox    = !isClose && isSource; // Source cell can be used for M2P
        int        numKeepWarp = streamCompact(&sourceQueue, isApprox, tempQueue);
        // push valid approx source cell indices into approxQueue
        const int apxTopUp = shflUpSync(sourceQueue, apxFillLevel);
        approxQueue        = (laneIdx < apxFillLevel) ? approxQueue : apxTopUp;
        apxFillLevel += numKeepWarp;

        if (apxFillLevel >= GpuConfig::warpSize) // If queue is larger than warp size,
        {
            // Call M2P kernel
            approxAcc(acc_i, pos_i, approxQueue, sourceCenter, Multipoles, tempQueue);
            apxFillLevel -= GpuConfig::warpSize;
            // pull down remaining source cell indices into now empty approxQueue
            approxQueue = shflDownSync(sourceQueue, numKeepWarp - apxFillLevel);
            m2pCounter += warpSize;
        }

        // Direct
        bool      directTodo    = isDirect;
        const int firstBody     = layout[leafIdx];
        const int numBodies     = (layout[leafIdx + 1] - firstBody) & -int(isDirect); // Number of bodies in cell
        const int numBodiesScan = inclusiveScanInt(numBodies);                        // Inclusive scan of numBodies
        int       numBodiesLane = numBodiesScan - numBodies;                          // Exclusive scan of numBodies
        int       numBodiesWarp = shflSync(numBodiesScan, GpuConfig::warpSize - 1);   // Total numBodies of current warp
        int       prevBodyIdx   = 0;
        while (numBodiesWarp > 0) // While there are bodies to process from current source cell set
        {
            tempQueue[laneIdx] = 1; // Default scan input is 1, such that consecutive lanes load consecutive bodies
            if (directTodo && (numBodiesLane < GpuConfig::warpSize))
            {
                directTodo               = false;          // Set cell as processed
                tempQueue[numBodiesLane] = -1 - firstBody; // Put first source cell body index into the queue
            }
            const int bodyIdx = inclusiveSegscanInt(tempQueue[laneIdx], prevBodyIdx);
            // broadcast last processed bodyIdx from the last lane to restart the scan in the next iteration
            prevBodyIdx = shflSync(bodyIdx, GpuConfig::warpSize - 1);

            if (numBodiesWarp >= GpuConfig::warpSize) // Process bodies from current set of source cells
            {
                // Load source body coordinates
                const Vec4<Tc> sourceBody = {x[bodyIdx], y[bodyIdx], z[bodyIdx], Tc(m[bodyIdx])};
                const Tm       hSource    = h[bodyIdx];
                directAcc(sourceBody, hSource, acc_i, pos_i);
                numBodiesWarp -= GpuConfig::warpSize;
                numBodiesLane -= GpuConfig::warpSize;
                p2pCounter += GpuConfig::warpSize;
            }
            else // Fewer than warpSize bodies remaining from current source cell set
            {
                // push the remaining bodies into bodyQueue
                int topUp = shflUpSync(bodyIdx, bdyFillLevel);
                bodyQueue = (laneIdx < bdyFillLevel) ? bodyQueue : topUp;

                bdyFillLevel += numBodiesWarp;
                if (bdyFillLevel >= GpuConfig::warpSize) // If this causes bodyQueue to spill
                {
                    // Load source body coordinates
                    const Vec4<Tc> sourceBody = {x[bodyQueue], y[bodyQueue], z[bodyQueue], Tc(m[bodyQueue])};
                    const Tm       hSource    = h[bodyQueue];
                    directAcc(sourceBody, hSource, acc_i, pos_i);
                    bdyFillLevel -= GpuConfig::warpSize;
                    // bodyQueue is now empty; put body indices that spilled into the queue
                    bodyQueue = shflDownSync(bodyIdx, numBodiesWarp - bdyFillLevel);
                    p2pCounter += GpuConfig::warpSize;
                }
                numBodiesWarp = 0; // No more bodies to process from current source cells
            }
        }

        //  If the current level is done
        if (sourceOffset >= numSources)
        {
            oldSources += numSources;      // Update finished source size
            numSources   = newSources;     // Update current source size
            sourceOffset = newSources = 0; // Initialize next source size and offset
        }
    }

    if (apxFillLevel > 0) // If there are leftover approx cells
    {
        // Call M2P kernel
        approxAcc(acc_i, pos_i, laneIdx < apxFillLevel ? approxQueue : -1, sourceCenter, Multipoles, tempQueue);

        m2pCounter += apxFillLevel;
    }

    if (bdyFillLevel > 0) // If there are leftover direct bodies
    {
        const int bodyIdx = laneIdx < bdyFillLevel ? bodyQueue : -1;
        // Load position of source bodies, with padding for invalid lanes
        const Vec4<Tc> sourceBody = bodyIdx >= 0 ? Vec4<Tc>{x[bodyIdx], y[bodyIdx], z[bodyIdx], Tc(m[bodyIdx])}
                                                 : Vec4<Tc>{Tc(0), Tc(0), Tc(0), Tc(0)};
        const Tm       hSource    = bodyIdx >= 0 ? h[bodyIdx] : Tm(0);
        directAcc(sourceBody, hSource, acc_i, pos_i);
        p2pCounter += bdyFillLevel;
    }

    return {m2pCounter, p2pCounter};
}

__device__ unsigned long long sumP2PGlob = 0;
__device__ unsigned           maxP2PGlob = 0;
__device__ unsigned long long sumM2PGlob = 0;
__device__ unsigned           maxM2PGlob = 0;

__device__ float        totalPotentialGlob = 0;
__device__ unsigned int targetCounterGlob  = 0;

__global__ void resetTraversalCounters()
{
    sumP2PGlob = 0;
    maxP2PGlob = 0;
    sumM2PGlob = 0;
    maxM2PGlob = 0;

    totalPotentialGlob = 0;
    targetCounterGlob  = 0;
}

/*! @brief Compute approximate body accelerations with Barnes-Hut
 *
 * @tparam       P              If P == Tc*, then the potential of each body will be stored.
 *                              Otherwise only the global potential sum will be calculated.
 * @param[in]    firstBody      index of first body in @p bodyPos to compute acceleration for
 * @param[in]    lastBody       index (exclusive) of last body in @p bodyPos to compute acceleration for
 * @param[in]    rootRange      (start,end) index pair of cell indices to start traversal from
 * @param[in]    x,y,z,m,h      bodies, in SFC order and as referenced by sourceCells
 * @param[in]    childOffsets   location (index in [0:numTreeNodes]) of first child of each cell, 0 indicates a leaf
 * @param[in]    internalToLeaf for each cell in [0:numTreeNodes], stores the leaf cell (cstone) index in [0:numLeaves]
 *                              if the cell is not a leaf, the value is negative
 * @param[in]    layout         for each leaf cell in [0:numLeaves], stores the index of the first body in the cell
 * @param[in]    sourceCenter   x,y,z center and square MAC radius of each cell in [0:numTreeNodes]
 * @param[in]    Multipole      cell multipoles, on device
 * @param[in]    G              gravitational constant
 * @param[inout] p              output body potential to add to if not nullptr
 * @param[inout] ax, ay, az     output body acceleration to add to
 * @param[-]     globalPool     temporary storage for the cell traversal stack, uninitialized
 *                              each active warp needs space for TravConfig::memPerWarp int32,
 *                              so the total size is TravConfig::memPerWarp * numWarpsPerBlock * numBlocks
 */
template<class Tc, class Th, class Tm, class Ta, class Tf, class MType, class P>
__global__ __launch_bounds__(TravConfig::numThreads) void traverse(
    int firstBody, int lastBody, const int2 rootRange, const Tc* __restrict__ x, const Tc* __restrict__ y,
    const Tc* __restrict__ z, const Tm* __restrict__ m, const Th* __restrict__ h,
    const TreeNodeIndex* __restrict__ childOffsets, const TreeNodeIndex* __restrict__ internalToLeaf,
    const LocalIndex* __restrict__ layout, const Vec4<Tf>* __restrict__ sourceCenter,
    const MType* __restrict__ Multipoles, Tc G, P p, Ta* ax, Ta* ay, Ta* az, int* globalPool)
{
    const int laneIdx = threadIdx.x & (GpuConfig::warpSize - 1);
    const int warpIdx = threadIdx.x >> GpuConfig::warpSizeLog2;

    constexpr int termSize         = MType{}.size();
    constexpr int numWarpsPerBlock = TravConfig::numThreads / GpuConfig::warpSize;

    using MValueType         = typename MType::value_type;
    constexpr int mSizeRatio = sizeof(MValueType) / sizeof(int);

    const int numTargets = (lastBody - firstBody - 1) / TravConfig::targetSize + 1;

    static_assert(termSize <= GpuConfig::warpSize, "review approxAcc function before disabling this check");
    constexpr int smSize =
        (TravConfig::numThreads > termSize * numWarpsPerBlock) ? TravConfig::numThreads : termSize * numWarpsPerBlock;
    __shared__ int sharedPool[smSize * mSizeRatio];

    // warp-common shared mem, 1 int per thread
    int* tempQueue = sharedPool + GpuConfig::warpSize * warpIdx * mSizeRatio;
    // warp-common global mem storage
    int* cellQueue = globalPool + TravConfig::memPerWarp * ((blockIdx.x * numWarpsPerBlock) + warpIdx);

    // int targetIdx = (blockIdx.x * numWarpsPerBlock) + warpIdx;
    int targetIdx = 0;

    while (true)
    // for(; targetIdx < numTargets; targetIdx += (gridDim.x * numWarpsPerBlock))
    {
        // first thread in warp grabs next target
        if (laneIdx == 0)
        {
            // this effectively randomizes which warp gets which targets, which better balances out
            // the load imbalance between different targets compared to static assignment
            targetIdx = atomicAdd(&targetCounterGlob, 1);
        }
        targetIdx = shflSync(targetIdx, 0);

        if (targetIdx >= numTargets) return;

        const int bodyBegin = firstBody + targetIdx * TravConfig::targetSize;
        const int bodyEnd   = imin(bodyBegin + TravConfig::targetSize, lastBody);

        // load target coordinates
        Vec4<Tc> pos_i[TravConfig::nwt];
        for (int i = 0; i < TravConfig::nwt; i++)
        {
            int bodyIdx = imin(bodyBegin + i * GpuConfig::warpSize + laneIdx, bodyEnd - 1);
            pos_i[i]    = {x[bodyIdx], y[bodyIdx], z[bodyIdx], h[bodyIdx]};
        }

        Vec3<Tc> Xmin = makeVec3(pos_i[0]);
        Vec3<Tc> Xmax = makeVec3(pos_i[0]);
        for (int i = 1; i < TravConfig::nwt; i++)
        {
            Xmin = min(Xmin, makeVec3(pos_i[i]));
            Xmax = max(Xmax, makeVec3(pos_i[i]));
        }

        Xmin = {warpMin(Xmin[0]), warpMin(Xmin[1]), warpMin(Xmin[2])};
        Xmax = {warpMax(Xmax[0]), warpMax(Xmax[1]), warpMax(Xmax[2])};

        Vec3<Tf>       targetCenter = (Xmax + Xmin) * Tf(0.5);
        const Vec3<Tf> targetSize   = (Xmax - Xmin) * Tf(0.5);

        Vec4<Tf> acc_i[TravConfig::nwt];
        for (int i = 0; i < TravConfig::nwt; i++)
        {
            acc_i[i] = Vec4<Tc>{Tc(0), Tc(0), Tc(0), Tc(0)};
        }

        auto [numM2P, numP2P] =
            traverseWarp(acc_i, pos_i, targetCenter, targetSize, x, y, z, m, h, childOffsets, internalToLeaf, layout,
                         sourceCenter, Multipoles, rootRange, tempQueue, cellQueue);
        assert(numM2P != 0xFFFFFFFF && numP2P != 0xFFFFFFFF);

        const int bodyIdxLane = bodyBegin + laneIdx;

        float warpPotential = 0;
        for (int i = 0; i < TravConfig::nwt; i++)
        {
            const int bodyIdx = bodyIdxLane + i * GpuConfig::warpSize;
            if (bodyIdx < bodyEnd) { warpPotential += m[bodyIdx] * acc_i[i][0]; }
        }

#pragma unroll
        for (int i = 0; i < GpuConfig::warpSizeLog2; i++)
        {
            warpPotential += shflXorSync(warpPotential, 1 << i);
        }

        if (laneIdx == 0)
        {
            int targetGroupSize = bodyEnd - bodyBegin;
            atomicMax(&maxP2PGlob, numP2P);
            atomicAdd(&sumP2PGlob, numP2P * targetGroupSize);
            atomicMax(&maxM2PGlob, numM2P);
            atomicAdd(&sumM2PGlob, numM2P * targetGroupSize);
            atomicAdd(&totalPotentialGlob, warpPotential);
        }

        for (int i = 0; i < TravConfig::nwt; i++)
        {
            const int bodyIdx = bodyIdxLane + i * GpuConfig::warpSize;
            if (bodyIdx < bodyEnd)
            {
                if constexpr (std::is_same_v<P, Tc*>) { p[bodyIdx] += m[bodyIdx] * acc_i[i][0]; }
                ax[bodyIdx] += G * acc_i[i][1];
                ay[bodyIdx] += G * acc_i[i][2];
                az[bodyIdx] += G * acc_i[i][3];
            }
        }
    }
}

/*! @brief Compute approximate body accelerations with Barnes-Hut
 *
 * @param[in]    firstBody      index of first body in @p bodyPos to compute acceleration for
 * @param[in]    lastBody       index (exclusive) of last body in @p bodyPos to compute acceleration for
 * @param[in]    x,y,z,m,h      bodies, in SFC order and as referenced by sourceCells
 * @param[in]    G              gravitational constant
 * @param[inout] p              body potential to add to, on device
 * @param[inout] ax,ay,az       body acceleration to add to
 * @param[in]    childOffsets   location (index in [0:numTreeNodes]) of first child of each cell, 0 indicates a leaf
 * @param[in]    internalToLeaf for each cell in [0:numTreeNodes], stores the leaf cell (cstone) index in [0:numLeaves]
 *                              if the cell is not a leaf, the value is negative
 * @param[in]    layout         for each leaf cell in [0:numLeaves], stores the index of the first body in the cell
 * @param[in]    sourceCenter   x,y,z center and square MAC radius of each cell in [0:numTreeNodes]
 * @param[in]    Multipole      cell multipoles, on device
 * @param[in]    levelRange     first and last cell of each level in the source tree, on host
 * @return                      P2P and M2P interaction statistics
 */
template<class Tc, class Th, class Tm, class Ta, class Tf, class MType, class P>
auto computeAcceleration(size_t firstBody, size_t lastBody, const Tc* x, const Tc* y, const Tc* z, const Tm* m,
                         const Th* h, Tc G, P p, Ta* ax, Tc* ay, Tc* az, const TreeNodeIndex* childOffsets,
                         const TreeNodeIndex* internalToLeaf, const LocalIndex* layout, const Vec4<Tf>* sourceCenter,
                         const MType* Multipole, const int2* levelRange)
{
    constexpr int numWarpsPerBlock = TravConfig::numThreads / GpuConfig::warpSize;

    int numBodies = lastBody - firstBody;

    // each target gets a warp (numWarps == numTargets)
    int numWarps  = (numBodies - 1) / TravConfig::targetSize + 1;
    int numBlocks = (numWarps - 1) / numWarpsPerBlock + 1;
    numBlocks     = std::min(numBlocks, TravConfig::maxNumActiveBlocks);

    printf("launching %d blocks\n", numBlocks);

    const int                  poolSize = TravConfig::memPerWarp * numWarpsPerBlock * numBlocks;
    thrust::device_vector<int> globalPool(poolSize);

    resetTraversalCounters<<<1, 1>>>();
    auto t0 = std::chrono::high_resolution_clock::now();
    traverse<<<numBlocks, TravConfig::numThreads>>>(firstBody, lastBody, {levelRange[1].x, levelRange[1].y}, x, y, z, m,
                                                    h, childOffsets, internalToLeaf, layout, sourceCenter, Multipole, G,
                                                    p, ax, ay, az, rawPtr(globalPool));
    kernelSuccess("traverse");

    auto   t1 = std::chrono::high_resolution_clock::now();
    double dt = std::chrono::duration<double>(t1 - t0).count();

    uint64_t     sumP2P, sumM2P;
    unsigned int maxP2P, maxM2P;

    checkGpuErrors(cudaMemcpyFromSymbol(&sumP2P, sumP2PGlob, sizeof(uint64_t)));
    checkGpuErrors(cudaMemcpyFromSymbol(&maxP2P, maxP2PGlob, sizeof(unsigned int)));
    checkGpuErrors(cudaMemcpyFromSymbol(&sumM2P, sumM2PGlob, sizeof(uint64_t)));
    checkGpuErrors(cudaMemcpyFromSymbol(&maxM2P, maxM2PGlob, sizeof(unsigned int)));

    float totalPotential;
    checkGpuErrors(cudaMemcpyFromSymbol(&totalPotential, totalPotentialGlob, sizeof(float)));

    util::array<Tc, 5> interactions;
    interactions[0] = Tc(sumP2P) / Tc(numBodies);
    interactions[1] = Tc(maxP2P);
    interactions[2] = Tc(sumM2P) / Tc(numBodies);
    interactions[3] = Tc(maxM2P);
    interactions[4] = totalPotential;

    Tc flops = (interactions[0] * 20.0 + interactions[2] * 2.0 * powf(ExpansionOrder<MType{}.size()>{}, 3)) *
               Tc(numBodies) / dt / 1e12;

    fprintf(stdout, "Traverse             : %.7f s (%.7f TFlops)\n", dt, flops);

    return interactions;
}

} // namespace ryoanji
