#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <set>
#include <utility>
namespace matching_test {
inline FILE *output=nullptr;
inline bool forceCollision=false;
inline uint64_t transformCandidates=0,tileCandidates=0;
inline thread_local uint64_t visits=0,inputCalls=0,tileLookups=0;
template<class T> void save(const T &value){if(output && std::fwrite(&value,sizeof(value),1,output)!=1)std::abort();}
template<class Pairs> inline void capture(const Pairs &transforms,
    const Pairs &tiles,const Pairs &lookAt) {
    transformCandidates=transforms.size();tileCandidates=tiles.size();
    for(const auto *set:{&transforms,&tiles,&lookAt}) {
        save(uint64_t(set->size()));for(const auto &pair:*set){save(pair.first);save(pair.second);}
    }
}
}
