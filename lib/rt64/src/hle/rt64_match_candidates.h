// RT64 matching candidate construction. Rendering draw calls are never removed.
#pragma once
#include "rt64_game_frame.h"
#include "rt64_workload.h"
#include <array>
#include <map>
#include <set>

namespace RT64 {
    struct CandidateExpansionStats {
        uint64_t inputCalls = 0, logicalCallPairs = 0, transformRangePairs = 0;
        uint64_t tileLookups = 0, tileVisits = 0, lookAtVisits = 0;
    };

    // Each feature has independent eligibility in the original matcher. Factor
    // its union of candidates independently: duplicate expansion inputs can be
    // coalesced, while different ranges/flags/context remain distinct. Final
    // sets retain the original lexicographic order used by stable tie-breaking.
    template<class Workloads>
    CandidateExpansionStats buildMatchCandidates(const Workloads &workloads,
        const GameScene &curScene, const GameScene &prevScene,
        const std::multimap<uint64_t, GameCallMap> &curCalls,
        const std::multimap<uint64_t, GameCallMap> &prevCalls,
        std::set<std::pair<uint32_t, uint32_t>> &transforms,
        std::set<std::pair<uint32_t, uint32_t>> &tiles,
        std::set<std::pair<uint32_t, uint32_t>> &lookAt)
    {
        using Ref = std::pair<uint32_t, uint32_t>; // workload, first element
        using Refs = std::set<Ref>;
        struct Features {
            std::map<uint32_t, Refs> transforms; // matrix count -> ranges
            std::map<uint32_t, std::array<Refs, 2>> tiles; // count, TMEM-match flag
            Refs lookAtFaces;
        };
        CandidateExpansionStats stats;
        auto collect = [&](const GameScene &scene, auto first, auto last) {
            Features result;
            for (auto it = first; it != last; ++it) {
                ++stats.inputCalls;
                const auto &indices = scene.projections[it->second.sceneProjIndex];
                const auto &workload = workloads[indices.workloadIndex];
                const auto &call = workload.fbPairs[indices.fbPairIndex].projections[indices.projectionIndex].gameCalls[it->second.callIndex];
                const auto &d = call.callDesc;
                if (it->second.doTransformMatching) {
                    result.transforms[(d.maxWorldMatrix - d.minWorldMatrix) + 1].emplace(indices.workloadIndex, d.minWorldMatrix);
                }
                if (it->second.doTileInterpolation && d.tileCount > 0) {
                    result.tiles[d.tileCount][it->second.doTileMatching].emplace(indices.workloadIndex, d.tileIndex);
                }
                const uint32_t mask = G_LIGHTING | G_TEXTURE_GEN;
                if ((d.geometryMode & mask) == mask) {
                    result.lookAtFaces.emplace(indices.workloadIndex, call.meshDesc.faceIndicesStart);
                }
            }
            return result;
        };

        for (auto cur = curCalls.begin(); cur != curCalls.end();) {
            const auto curEnd = curCalls.upper_bound(cur->first);
            const auto prevRange = prevCalls.equal_range(cur->first);
            if (prevRange.first != prevRange.second) {
                stats.logicalCallPairs += uint64_t(std::distance(cur, curEnd)) * uint64_t(std::distance(prevRange.first, prevRange.second));
                const auto c = collect(curScene, cur, curEnd);
                const auto p = collect(prevScene, prevRange.first, prevRange.second);
                for (const auto &countRanges : c.transforms) {
                    const auto previous = p.transforms.find(countRanges.first);
                    if (previous == p.transforms.end()) continue;
                    for (const Ref &cr : countRanges.second) for (const Ref &pr : previous->second) {
                        ++stats.transformRangePairs;
                        const auto &cd = workloads[cr.first].drawData;
                        const auto &pd = workloads[pr.first].drawData;
                        for (uint32_t offset = 0; offset < countRanges.first; ++offset) {
                            const uint32_t ci = cr.second + offset, pi = pr.second + offset;
                            const auto &cg = cd.transformGroups[cd.worldTransformGroups[ci]];
                            const auto &pg = pd.transformGroups[pd.worldTransformGroups[pi]];
                            if ((cg.matrixId == pg.matrixId) && ((cg.matrixId == G_EX_ID_AUTO) ||
                                ((cg.matrixId != G_EX_ID_IGNORE) && (cg.ordering == G_EX_ORDER_AUTO)))) {
                                transforms.emplace(ci, pi);
                            }
                        }
                    }
                }
                for (const auto &countRanges : c.tiles) {
                    const auto previous = p.tiles.find(countRanges.first);
                    if (previous == p.tiles.end()) continue;
                    for (unsigned cm = 0; cm < 2; ++cm) for (unsigned pm = 0; pm < 2; ++pm) {
                        const auto &currentRefs = countRanges.second[cm];
                        const auto &previousRefs = previous->second[pm];
                        if (currentRefs.empty() || previousRefs.empty()) continue;
                        for (uint32_t offset = 0; offset < countRanges.first; ++offset) {
                            if (cm && pm) {
                                // Only equal TMEM identities may pair in AUTO
                                // mode. Join by identity instead of visiting all
                                // incompatible pairs. Offset/count stay fixed.
                                std::map<uint64_t, std::vector<uint32_t>> byHash;
                                for (const Ref &pr : previousRefs) {
                                    ++stats.tileLookups;
                                    const uint32_t pi = pr.second + offset;
                                    byHash[workloads[pr.first].drawData.callTiles[pi].tmemHashOrID].push_back(pi);
                                }
                                for (const Ref &cr : currentRefs) {
                                    ++stats.tileLookups;
                                    const uint32_t ci = cr.second + offset;
                                    const auto match = byHash.find(workloads[cr.first].drawData.callTiles[ci].tmemHashOrID);
                                    if (match != byHash.end()) for (uint32_t pi : match->second) {
                                        ++stats.tileVisits;
                                        tiles.emplace(ci, pi);
                                    }
                                }
                            }
                            else {
                                // INTERPOLATE on either side bypasses the TMEM
                                // equality check in the original algorithm.
                                for (const Ref &cr : currentRefs) for (const Ref &pr : previousRefs) {
                                    ++stats.tileVisits;
                                    tiles.emplace(cr.second + offset, pr.second + offset);
                                }
                            }
                        }
                    }
                }
                if (!c.lookAtFaces.empty() && !p.lookAtFaces.empty()) {
                    auto indices = [&](const Refs &faces) {
                        Refs result;
                        for (const Ref &r : faces) {
                            const auto &d = workloads[r.first].drawData;
                            result.emplace(r.first, d.lookAtIndices[d.faceIndices[r.second]] >> RSP_LOOKAT_INDEX_SHIFT);
                        }
                        return result;
                    };
                    const auto ci = indices(c.lookAtFaces), pi = indices(p.lookAtFaces);
                    for (const Ref &cr : ci) for (const Ref &pr : pi) {
                        ++stats.lookAtVisits;
                        lookAt.emplace(cr.second, pr.second);
                    }
                }
            }
            cur = curEnd;
        }
        return stats;
    }
}
