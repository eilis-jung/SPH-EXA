
#include "sfc/box_mpi.hpp"
#include "sfc/domaindecomp_mpi.hpp"
#include "sfc/halodiscovery.hpp"
#include "sfc/haloexchange.hpp"
#include "sfc/layout.hpp"
#include "sfc/octree_mpi.hpp"

namespace sphexa
{

template<class I, class T>
class Domain
{
public:
    explicit Domain(int rank, int nRanks, int bucketSize, bool pbcX=false, bool pbcY = false, bool pbcZ = false)
        : myRank_(rank), nRanks_(nRanks), bucketSize_(bucketSize),
          particleStart_(0), particleEnd_(-1), pbcX_(pbcX), pbcY_(pbcY), pbcZ_(pbcZ)
    {}

    void sync(std::vector<T>& x, std::vector<T>& y, std::vector<T>& z, std::vector<T>& h)
    {
        if (x.size() != y.size() || x.size() != z.size() || x.size() != h.size())
            throw std::runtime_error("x,y,z,h sizes do not match\n");

        // bounds initialization on first call, use all particles
        if (particleEnd_ == -1)
        {
            particleStart_ = 0;
            particleEnd_   = x.size();
        }

        Box<T> box = makeGlobalBox(cbegin(x) + particleStart_, cbegin(x) + particleEnd_,
                                   cbegin(y) + particleStart_,
                                   cbegin(z) + particleStart_, pbcX_, pbcY_, pbcZ_);

        // number of locally assigned particles to consider for global tree building
        int nParticles = particleEnd_ - particleStart_;

        // compute morton codes only for particles participating in tree build
        std::vector<I> mortonCodes(nParticles);
        computeMortonCodes(cbegin(x) + particleStart_, cbegin(x) + particleEnd_,
                           cbegin(y) + particleStart_,
                           cbegin(z) + particleStart_,
                           begin(mortonCodes), box);

        // compute the ordering that will sort the mortonCodes in ascending order
        std::vector<int> mortonOrder(nParticles);
        sort_invert(cbegin(mortonCodes), cend(mortonCodes), begin(mortonOrder));

        // reorder the codes according to the ordering
        // has the same net effect as std::sort(begin(mortonCodes), end(mortonCodes)),
        // but with the difference that we explicitly know the ordering, such
        // that we can later apply it to the x,y,z,h arrays or to access them in the Morton order
        reorder(mortonOrder, mortonCodes);

        // compute the global octree in cornerstone format (leaves only)
        // the resulting tree and node counts will be identical on all ranks
        std::vector<std::size_t> nodeCounts;
        std::tie(tree_, nodeCounts) = computeOctreeGlobal(mortonCodes.data(), mortonCodes.data() + nParticles, bucketSize_);

        // assign one single range of Morton codes each rank
        SpaceCurveAssignment<I> assignment = singleRangeSfcSplit(tree_, nodeCounts, nRanks_);
        int newNParticlesAssigned = assignment.totalCount(myRank_);

        // compute the maximum smoothing length (=halo radii) in each global node
        std::vector<T> haloRadii(nNodes(tree_));
        computeNodeMaxGlobal(tree_.data(), nNodes(tree_), mortonCodes.data(), mortonCodes.data() + nParticles,
                             mortonOrder.data(), h.data() + particleStart_, haloRadii.data());

        // find outgoing and incoming halo nodes of the tree
        // uses 3D collision detection
        std::vector<pair<int>> haloPairs;
        findHalos(tree_, haloRadii, box, assignment, myRank_, haloPairs);

        // group outgoing and incoming halo node indices by destination/source rank
        std::vector<std::vector<int>> incomingHaloNodes;
        std::vector<std::vector<int>> outgoingHaloNodes;
        computeSendRecvNodeList(tree_, assignment, haloPairs, incomingHaloNodes, outgoingHaloNodes);

        // compute list of local node index ranges
        std::vector<int> incomingHalosFlattened = flattenNodeList(incomingHaloNodes);
        std::vector<int> localNodeRanges        = computeLocalNodeRanges(tree_, assignment, myRank_);

        // Put all local node indices and incoming halo node indices in one sorted list.
        // and compute an offset for each node into these arrays.
        // This will be the new layout for x,y,z,h arrays.
        std::vector<int> presentNodes;
        std::vector<int> nodeOffsets;
        computeLayoutOffsets(localNodeRanges, incomingHalosFlattened, nodeCounts, presentNodes, nodeOffsets);

        int firstLocalNode = std::lower_bound(cbegin(presentNodes), cend(presentNodes), localNodeRanges[0])
                             - begin(presentNodes);

        int newParticleStart = nodeOffsets[firstLocalNode];
        int newParticleEnd   = newParticleStart + newNParticlesAssigned;

        // compute send array ranges for domain exchange
        // index ranges in domainExchangeSends are valid relative to the sorted code array mortonCodes
        // note that there is no offset applied to mortonCodes, because it was constructed
        // only with locally assigned particles
        SendList domainExchangeSends = createSendList(assignment, mortonCodes.data(), mortonCodes.data() + nParticles);

        // assigned particles + halos
        int totalNParticles = *nodeOffsets.rbegin();
        exchangeParticles<T>(domainExchangeSends, Rank(myRank_), totalNParticles, newNParticlesAssigned,
                             particleStart_, newParticleStart, mortonOrder.data(), x,y,z,h);

        // assigned particles have been moved to their new locations by the domain exchange exchangeParticles
        std::swap(particleStart_, newParticleStart);
        std::swap(particleEnd_, newParticleEnd);

        mortonCodes.resize(newNParticlesAssigned);
        computeMortonCodes(cbegin(x) + particleStart_, cbegin(x) + particleEnd_,
                           cbegin(y) + particleStart_,
                           cbegin(z) + particleStart_,
                           begin(mortonCodes), box);

        mortonOrder.resize(newNParticlesAssigned);
        sort_invert(cbegin(mortonCodes), cend(mortonCodes), begin(mortonOrder));

        // We have to reorder the locally assigned particles in the coordinate arrays
        // which are located in the index range [particleStart_, particleEnd_].
        // Due to the domain particle exchange, contributions from remote ranks
        // are received in arbitrary order
        reorder(mortonOrder, x, particleStart_);
        reorder(mortonOrder, y, particleStart_);
        reorder(mortonOrder, z, particleStart_);
        reorder(mortonOrder, h, particleStart_);

        SendList incomingHaloIndices = createHaloExchangeList(incomingHaloNodes, presentNodes, nodeOffsets);
        SendList outgoingHaloIndices = createHaloExchangeList(outgoingHaloNodes, presentNodes, nodeOffsets);

        haloexchange<T>(incomingHaloIndices, outgoingHaloIndices, x.data(), y.data(), z.data(), h.data());
    }

    [[nodiscard]] int startIndex() const { return particleStart_; }
    [[nodiscard]] int endIndex() const   { return particleEnd_; }

private:

    int myRank_;
    int nRanks_;

    int particleStart_;
    int particleEnd_;

    bool pbcX_;
    bool pbcY_;
    bool pbcZ_;

    int bucketSize_;
    std::vector<I> tree_;
};

} // namespace sphexa
