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
 * @brief octree utility tests
 *
 * @author Sebastian Keller <sebastian.f.keller@gmail.com>
 *
 * This file implements tests for OctreeMaker.
 * OctreeMaker can be used to generate octrees in cornerstone
 * format. It is only used to test the octree implementation.
 */

#include "gtest/gtest.h"

#include "cstone/tree/octree_internal_td.hpp"
#include "cstone/tree/octree_util.hpp"

using namespace cstone;

template<class KeyType>
void checkConnectivity(const TdOctree<KeyType>& fullTree)
{
    ASSERT_TRUE(fullTree.isRoot(0));

    // check all internal nodes
    for (TreeNodeIndex nodeIdx = 0; nodeIdx < fullTree.numTreeNodes(); ++nodeIdx)
    {
        if (!fullTree.isLeaf(nodeIdx))
        {
            KeyType prefix = fullTree.codeStart(nodeIdx);
            int level      = fullTree.level(nodeIdx);

            EXPECT_EQ(fullTree.codeEnd(nodeIdx), prefix + nodeRange<KeyType>(level));

            for (int octant = 0; octant < 8; ++octant)
            {
                TreeNodeIndex child = fullTree.child(nodeIdx, octant);
                EXPECT_EQ(prefix + octant * nodeRange<KeyType>(level + 1), fullTree.codeStart(child));
            }

            if (!fullTree.isRoot(nodeIdx))
            {
                TreeNodeIndex parent = fullTree.parent(nodeIdx);
                EXPECT_EQ(fullTree.level(parent), level - 1);

                KeyType parentPrefix = fullTree.codeStart(parent);
                EXPECT_EQ(parentPrefix, enclosingBoxCode(prefix, level - 1));
            }
            else
            {
                EXPECT_EQ(fullTree.codeStart(nodeIdx), 0);
                EXPECT_EQ(fullTree.level(nodeIdx), 0);
            }
        }
        else
        {
            KeyType prefix = fullTree.codeStart(nodeIdx);
            int level      = fullTree.level(nodeIdx);

            EXPECT_EQ(fullTree.codeEnd(nodeIdx), prefix + nodeRange<KeyType>(level));

            TreeNodeIndex parent = fullTree.parent(nodeIdx);
            EXPECT_EQ(fullTree.level(parent), level - 1);

            KeyType parentPrefix = fullTree.codeStart(parent);
            EXPECT_EQ(parentPrefix, enclosingBoxCode(prefix, level - 1));
        }
    }
}

TEST(InternalOctreeTd, rootNode)
{
    auto tree = makeRootNodeTree<unsigned>();

    TdOctree<unsigned> fullTree;
    fullTree.update(tree.data(), nNodes(tree));

    EXPECT_EQ(fullTree.numLeafNodes(), 1);
    EXPECT_EQ(fullTree.numTreeNodes(), 1);
    EXPECT_EQ(fullTree.numInternalNodes(), 0);
    EXPECT_EQ(fullTree.codeStart(0), 0);
    EXPECT_EQ(fullTree.codeEnd(0), nodeRange<unsigned>(0));
    EXPECT_EQ(fullTree.parent(0), 0);
}

/*! @brief test internal octree creation from a regular 4x4x4 grid of leaves
 *
 * This creates 64 level-2 leaf nodes. The resulting internal tree should
 * have 9 nodes, the root node and the 8 level-1 nodes.
 * The children of the root point to the level-1 nodes while the children
 * of the level-1 nodes point to the leaf nodes, i.e. the tree provided for constructing,
 * which is a separate array.
 */
template<class KeyType>
static void octree4x4x4()
{
    std::vector<KeyType> tree = makeUniformNLevelTree<KeyType>(64, 1);

    TdOctree<KeyType> fullTree;
    fullTree.update(tree.data(), nNodes(tree));

    ASSERT_EQ(fullTree.numInternalNodes(), (64 - 1) / 7);
    ASSERT_EQ(fullTree.numLeafNodes(), 64);

    EXPECT_EQ(fullTree.numTreeNodes(0), 1);
    EXPECT_EQ(fullTree.numTreeNodes(1), 8);
    EXPECT_EQ(fullTree.numTreeNodes(2), 64);

    EXPECT_EQ(fullTree.codeEnd(fullTree.toInternal(nNodes(tree) - 1)), nodeRange<KeyType>(0));

    checkConnectivity<KeyType>(fullTree);
}

TEST(InternalOctreeTd, octree4x4x4)
{
    octree4x4x4<unsigned>();
    octree4x4x4<uint64_t>();
}

/*! @brief test internal octree creation with an irregular leaf tree
 *
 * The leaf tree is the result of subdividing the root node, then further
 * subdividing octant 0. This results in 15 leaves, so the internal tree
 * should have two nodes: the root and the one internal level-1 node for the
 * first octant. The root points to the one internal node and to leaves [8:15].
 * The internal level-1 nodes points to leaves [0:8].
 */
template<class KeyType>
static void octreeIrregularL2()
{
    std::vector<KeyType> tree = OctreeMaker<KeyType>{}.divide().divide(0).makeTree();

    TdOctree<KeyType> fullTree;
    fullTree.update(tree.data(), nNodes(tree));

    ASSERT_EQ(fullTree.numInternalNodes(), (15 - 1) / 7);
    ASSERT_EQ(fullTree.numLeafNodes(), 15);

    EXPECT_EQ(fullTree.numTreeNodes(0), 1);
    EXPECT_EQ(fullTree.numTreeNodes(1), 8);
    EXPECT_EQ(fullTree.numTreeNodes(2), 8);

    checkConnectivity<KeyType>(fullTree);
}

TEST(InternalOctreeTd, irregularL2)
{
    octreeIrregularL2<unsigned>();
    octreeIrregularL2<uint64_t>();
}

//! @brief This creates an irregular tree. Checks geometry relations between children and parents.
template<class KeyType>
static void octreeIrregularL3()
{
    std::vector<KeyType> tree = OctreeMaker<KeyType>{}.divide().divide(0).divide(0, 2).divide(3).makeTree();

    TdOctree<KeyType> fullTree;
    fullTree.update(tree.data(), nNodes(tree));
    EXPECT_EQ(fullTree.numTreeNodes(), 33);
    EXPECT_EQ(fullTree.numLeafNodes(), 29);
    EXPECT_EQ(fullTree.numInternalNodes(), 4);

    EXPECT_EQ(fullTree.numTreeNodes(0), 1);
    EXPECT_EQ(fullTree.numTreeNodes(1), 8);
    EXPECT_EQ(fullTree.numTreeNodes(2), 16);
    EXPECT_EQ(fullTree.numTreeNodes(3), 8);

    checkConnectivity<KeyType>(fullTree);
}

TEST(InternalOctreeTd, irregularL3)
{
    octreeIrregularL3<unsigned>();
    octreeIrregularL3<uint64_t>();
}


//! @brief this generates a max-depth cornerstone tree
template<class KeyType>
static void spanningTree()
{
    std::vector<KeyType> cornerstones{0, 1, nodeRange<KeyType>(0) - 1, nodeRange<KeyType>(0)};
    std::vector<KeyType> spanningTree = computeSpanningTree<KeyType>(cornerstones);

    TdOctree<KeyType> fullTree;
    fullTree.update(spanningTree.data(), nNodes(spanningTree));

    checkConnectivity(fullTree);
}

TEST(InternalOctreeTd, spanningTree)
{
    spanningTree<unsigned>();
    spanningTree<uint64_t>();
}

template<class KeyType>
static void locate()
{
    {
        std::vector<KeyType> cornerstones{0, 1, nodeRange<KeyType>(0) - 1, nodeRange<KeyType>(0)};
        std::vector<KeyType> spanningTree = computeSpanningTree<KeyType>(cornerstones);

        TdOctree<KeyType> fullTree;
        fullTree.update(spanningTree.data(), nNodes(spanningTree));

        for (TreeNodeIndex i = 0; i < fullTree.numTreeNodes(); ++i)
        {
            KeyType key1 = fullTree.codeStart(i);
            KeyType key2 = fullTree.codeEnd(i);

            EXPECT_EQ(i, fullTree.locate(key1, key2));
        }
    }
    {
        std::vector<KeyType> tree = makeUniformNLevelTree<KeyType>(4096, 1);
        TdOctree<KeyType> fullTree;
        fullTree.update(tree.data(), nNodes(tree));

        for (TreeNodeIndex i = 0; i < fullTree.numTreeNodes(); ++i)
        {
            KeyType key1 = fullTree.codeStart(i);
            KeyType key2 = fullTree.codeEnd(i);

            EXPECT_EQ(i, fullTree.locate(key1, key2));
        }
    }
}

TEST(InternalOctreeTd, locate)
{
    locate<unsigned>();
    locate<uint64_t>();
}

template<class KeyType>
static void cstoneIndex()
{
    std::vector<KeyType> cornerstones{0, 1, nodeRange<KeyType>(0) - 1, nodeRange<KeyType>(0)};
    std::vector<KeyType> spanningTree = computeSpanningTree<KeyType>(cornerstones);

    TdOctree<KeyType> fullTree;
    fullTree.update(spanningTree.data(), nNodes(spanningTree));

    for (TreeNodeIndex i = 0; i < fullTree.numTreeNodes(); ++i)
    {
        if (fullTree.isLeaf(i))
        {
            KeyType internalKey = fullTree.codeStart(i);

            TreeNodeIndex cstoneIndex = fullTree.cstoneIndex(i);
            EXPECT_EQ(internalKey, spanningTree[cstoneIndex]);
        }
    }
}

TEST(InternalOctreeTd, cstoneIndex)
{
    cstoneIndex<unsigned>();
    cstoneIndex<uint64_t>();
}

template<class KeyType>
static void extractLeaves()
{
    std::vector<KeyType> cornerstones{0, 1, nodeRange<KeyType>(0) - 1, nodeRange<KeyType>(0)};
    std::vector<KeyType> spanningTree = computeSpanningTree<KeyType>(cornerstones);

    TdOctree<KeyType> fullTree;
    fullTree.update(spanningTree.data(), nNodes(spanningTree));

    std::vector<KeyType> prefixes(fullTree.numTreeNodes());
    for (size_t i = 0; i < prefixes.size(); ++i)
    {
        prefixes[i] = fullTree.codeStart(i);
    }

    std::vector<KeyType> leafExtract(fullTree.numLeafNodes());
    fullTree.template extractLeaves<KeyType>(prefixes, leafExtract);

    EXPECT_TRUE(std::equal(leafExtract.begin(), leafExtract.end(), spanningTree.begin()));
}

TEST(InternalOctreeTd, extractLeaves)
{
    extractLeaves<unsigned>();
    extractLeaves<uint64_t>();
}

template<class KeyType>
static void upsweepSumIrregularL3()
{
    std::vector<KeyType> cstoneTree = OctreeMaker<KeyType>{}.divide().divide(0).divide(0, 2).divide(3).makeTree();
    TdOctree<KeyType> octree;
    octree.update(cstoneTree.data(), nNodes(cstoneTree));

    std::vector<unsigned> leafCounts(nNodes(cstoneTree), 1);
    std::vector<unsigned> nodeCounts(octree.numTreeNodes());

    upsweepSum<unsigned>(octree, leafCounts, nodeCounts);

    //                                      L1                       L2
    //                                                               00                       30
    std::vector<unsigned> refNodeCounts{29, 15, 1, 1, 8, 1, 1, 1, 1,  1, 1, 8, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,
                                   //  L3
                                   // 020
                                        1, 1, 1, 1, 1, 1, 1, 1};

    EXPECT_EQ(nodeCounts, refNodeCounts);
    EXPECT_EQ(nodeCounts[0], 29);
}

TEST(UpsweepTd, sumIrregularL3)
{
    upsweepSumIrregularL3<unsigned>();
    upsweepSumIrregularL3<uint64_t>();
}
