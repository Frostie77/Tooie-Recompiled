// Observation of the CPU inputs consumed by RSPWorldCS; no matching decisions.
#pragma once
#include "hle/rt64_game_frame.h"
#include "hle/rt64_workload.h"
#include "../../../../src/runtime_diagnostics.h"
#include <cmath>
#include <unordered_set>

namespace RT64::geometryDiagnostics {
struct Window {
    uint64_t previous = 0;
    bool sampled = false;
    bool due(uint64_t now, float weight) {
        if (!std::isfinite(weight) || weight <= 0 || weight >= 1) return false;
        if (sampled && now >= previous && now - previous < 5'000'000) return false;
        previous = now; sampled = true; return true;
    }
};
struct Vertex {
    uint32_t index=0, transform=0, previousTransform=UINT32_MAX;
    uint32_t id=G_EX_ID_AUTO, previousId=G_EX_ID_AUTO, address=0, previousAddress=0;
    uint32_t count=0, previousCount=0, previousVertex=UINT32_MAX;
    uint8_t vertexMode=0, positionMode=0, rotationMode=0;
    uint32_t lerpFlags=0;
    bool mapped=false, previousVertexValid=false, singular=false, addressValid=false, previousAddressValid=false;
    hlslpp::float3 position={}, velocity={}, previousSource={};
    hlslpp::float4 worldPrevious={}, worldCurrent={}, worldSample={};
    hlslpp::float4x4 previous={}, current={}, consumed={}, consumedPrevious={};
};
struct Triangle {
    bool found=false;
    uint32_t framebuffer=0, projection=0, call=0, uid=0, face=0, geometryMode=0;
    uint32_t otherModeH=0, otherModeL=0;
    float departure=0;
    std::array<Vertex,3> vertices;
};
struct Snapshot {
    uint64_t triangles=0, invalid=0, unmappedVertices=0, inspected=0, calls=0;
    bool truncated=false;
    // AUTO is not synonymous with NPC. Singular is not synonymous with shadow.
    std::array<Triangle,3> worst;
};
inline bool finite(const hlslpp::float4 &v) {
    for (unsigned i=0;i<4;++i) if (!std::isfinite(float(v[i]))) return false;
    return true;
}
inline bool vertex(const DrawData &d, const DrawData *p, const GameFrameMap::WorkloadMap *map,
    uint32_t index, float weight, Vertex &v) {
    if (index>=d.worldIndices.size() || index>=d.posFloats.size()/3 || index>=d.velFloats.size()/3) return false;
    v.index=index;v.transform=d.worldIndices[index];
    const auto t=v.transform;
    if (t>=d.worldTransforms.size() || t>=d.lerpWorldTransforms.size() || t>=d.worldTransformGroups.size()) return false;
    const auto group=d.worldTransformGroups[t];
    if (group>=d.transformGroups.size()) return false;
    v.id=d.transformGroups[group].matrixId;v.vertexMode=d.transformGroups[group].vertexInterpolation;
    v.positionMode=d.transformGroups[group].positionInterpolation;v.rotationMode=d.transformGroups[group].rotationInterpolation;
    v.position={d.posFloats[index*3],d.posFloats[index*3+1],d.posFloats[index*3+2]};
    v.velocity={d.velFloats[index*3],d.velFloats[index*3+1],d.velFloats[index*3+2]};
    v.current=d.worldTransforms[t];v.consumed=d.lerpWorldTransforms[t];v.previous=v.current;
    if (t>=d.prevWorldTransforms.size()) return false;
    v.consumedPrevious=d.prevWorldTransforms[t];
    if (t<d.worldTransformPhysicalAddresses.size()) {v.address=d.worldTransformPhysicalAddresses[t];v.addressValid=true;}
    if (t<d.worldTransformVertexIndices.size()) {
        const uint32_t start=d.worldTransformVertexIndices[t];
        const uint32_t end=(t+1<d.worldTransformVertexIndices.size())?d.worldTransformVertexIndices[t+1]:d.vertexCount();
        if (start<=index && index<end && end<=d.vertexCount()) v.count=end-start;
    }
    if (p && map && t<map->transforms.size() && map->transforms[t].mapped) {
        v.mapped=true;v.previousTransform=map->transforms[t].prevTransformIndex;
        const auto &body=map->transforms[t].rigidBody;
        v.lerpFlags=unsigned(body.lerpTranslation) | (unsigned(body.lerpRotation)<<1) | (unsigned(body.lerpScale)<<2)
            | (unsigned(body.lerpSkew)<<3) | (unsigned(body.lerpPerspective)<<4) | (unsigned(body.lerpDecompose)<<5);
        const auto pt=v.previousTransform;
        if (pt>=p->worldTransforms.size() || pt>=p->worldTransformGroups.size()) return false;
        v.previous=p->worldTransforms[pt];
        const auto pg=p->worldTransformGroups[pt];
        if (pg>=p->transformGroups.size()) return false;
        v.previousId=p->transformGroups[pg].matrixId;
        if (pt<p->worldTransformPhysicalAddresses.size()) {v.previousAddress=p->worldTransformPhysicalAddresses[pt];v.previousAddressValid=true;}
        if (v.count && pt<p->worldTransformVertexIndices.size()) {
            const uint32_t start=p->worldTransformVertexIndices[pt];
            const uint32_t end=(pt+1<p->worldTransformVertexIndices.size())?p->worldTransformVertexIndices[pt+1]:p->vertexCount();
            if (start<=end && end<=p->vertexCount()) {
                v.previousCount=end-start;
                const uint64_t pv=uint64_t(start)+index-d.worldTransformVertexIndices[t];
                if (v.count==v.previousCount && pv<end && pv<p->posFloats.size()/3) {
                    v.previousVertex=uint32_t(pv);v.previousVertexValid=true;
                    v.previousSource={p->posFloats[pv*3],p->posFloats[pv*3+1],p->posFloats[pv*3+2]};
                }
            }
        }
    }
    // Row-vector CPU convention corresponds to the matrix storage consumed by
    // RSPWorldCS. Endpoints here use the raw source matrices and the shader's
    // position/velocity equation, not the possibly SKIP-filtered previous
    // consumed matrix. previousSource is the independent matched source value.
    v.worldPrevious=hlslpp::mul(hlslpp::float4(v.position-v.velocity,1),v.previous);
    v.worldCurrent=hlslpp::mul(hlslpp::float4(v.position,1),v.current);
    v.worldSample=hlslpp::mul(hlslpp::float4(v.position-v.velocity*(1-weight),1),v.consumed);
    v.singular=std::fabs(float(hlslpp::determinant(v.previous)))<1e-6f || std::fabs(float(hlslpp::determinant(v.current)))<1e-6f;
    return finite(v.worldPrevious) && finite(v.worldCurrent) && finite(v.worldSample);
}
inline Snapshot collect(const Workload &w, const Workload *previous, const GameFrameMap::WorkloadMap *map, float weight,
                        const std::unordered_set<uint32_t> *onlyTransforms=nullptr,uint64_t scanLimit=UINT64_MAX) {
    Snapshot out;
    if (!std::isfinite(weight) || weight<=0 || weight>=1) return out;
    const auto &d=w.drawData;
    const auto *p=previous?&previous->drawData:nullptr;
    for (uint32_t f=0;f<w.fbPairCount && f<w.fbPairs.size();++f) {
        const auto &fb=w.fbPairs[f];
        for (uint32_t pr=0;pr<fb.projectionCount && pr<fb.projections.size();++pr) {
            const auto &proj=fb.projections[pr];
            if (proj.type!=Projection::Type::Perspective && proj.type!=Projection::Type::Orthographic) continue;
            for (uint32_t c=0;c<proj.gameCallCount && c<proj.gameCalls.size();++c) {
                if(out.calls>=scanLimit) {out.truncated=true;return out;}
                ++out.calls;
                const auto &call=proj.gameCalls[c];
                const uint64_t start=call.meshDesc.faceIndicesStart, count=uint64_t(call.callDesc.triangleCount)*3;
                if (start>d.faceIndices.size() || count>d.faceIndices.size()-start) {++out.invalid;continue;}
                for (uint64_t i=start;i<start+count;i+=3) {
                    if(out.inspected>=scanLimit) {out.truncated=true;return out;}
                    ++out.inspected;
                    if (onlyTransforms) {
                        bool selected=false;
                        for (unsigned k=0;k<3;++k) {
                            const uint32_t v=d.faceIndices[i+k];
                            selected |= v<d.worldIndices.size() && onlyTransforms->count(d.worldIndices[v]);
                        }
                        if (!selected) continue;
                    }
                    Triangle sample; sample.framebuffer=f;sample.projection=pr;sample.call=c;sample.uid=call.callDesc.uid;sample.face=uint32_t(i);
                    sample.geometryMode=call.callDesc.geometryMode;sample.otherModeH=call.callDesc.otherMode.H;sample.otherModeL=call.callDesc.otherMode.L;
                    bool valid=true,automatic=false,player=false,singular=false;
                    for (unsigned k=0;k<3;++k) {
                        auto &v=sample.vertices[k];
                        if (!vertex(d,p,map,d.faceIndices[i+k],weight,v)) {valid=false;break;}
                        automatic|=v.id==G_EX_ID_AUTO;player|=v.id>=0x10000000U && v.id<0x10010000U;singular|=v.singular;
                    }
                    if (!valid) {++out.invalid;continue;}
                    ++out.triangles;
                    for (const auto &v:sample.vertices) if (!v.mapped) ++out.unmappedVertices;
                    for (unsigned k=0;k<3;++k) {
                        const auto &a=sample.vertices[k], &b=sample.vertices[(k+1)%3];
                        const float lo=hlslpp::length(a.worldPrevious.xyz-b.worldPrevious.xyz);
                        const float hi=hlslpp::length(a.worldCurrent.xyz-b.worldCurrent.xyz);
                        const float mid=hlslpp::length(a.worldSample.xyz-b.worldSample.xyz);
                        const float outside=std::max({0.f,std::min(lo,hi)-mid,mid-std::max(lo,hi)});
                        sample.departure=std::max(sample.departure,outside/std::max({lo,hi,1e-6f}));
                    }
                    sample.found=true;
                    const bool classes[]={automatic,player,singular};
                    for (unsigned k=0;k<3;++k) if (classes[k] && (!out.worst[k].found || sample.departure>out.worst[k].departure)) out.worst[k]=sample;
                }
            }
        }
    }
    return out;
}
inline void report(const Snapshot &s, uint64_t sample, uint64_t workload, float weight, uint64_t scanUs, float previousWeight=0) {
    char line[1500];
    std::snprintf(line,sizeof(line),"sample=%llu workload=%llu weight=%.9g previous_weight=%.9g scan_us=%llu triangles=%llu invalid=%llu unmapped_vertex_references=%llu identity=unknown edge_metric=endpoint-envelope-departure-not-error",
        (unsigned long long)sample,(unsigned long long)workload,double(weight),double(previousWeight),(unsigned long long)scanUs,
        (unsigned long long)s.triangles,(unsigned long long)s.invalid,(unsigned long long)s.unmappedVertices);
    tooie::diagnostics::log("geometry",line);
    const char *classes[]={"auto","player","singular"};
    for (unsigned k=0;k<3;++k) {
        const auto &t=s.worst[k];
        std::snprintf(line,sizeof(line),"sample=%llu workload=%llu class=%s found=%u fb=%u projection=%u call=%u uid=%u face=%u geometry=0x%X other=0x%X/0x%X departure=%.9g",
            (unsigned long long)sample,(unsigned long long)workload,classes[k],unsigned(t.found),t.framebuffer,t.projection,t.call,t.uid,t.face,t.geometryMode,t.otherModeH,t.otherModeL,double(t.departure));
        tooie::diagnostics::log("geometry",line);
        if (!t.found) continue;
        for (unsigned i=0;i<3;++i) {
            const auto &v=t.vertices[i];
            std::snprintf(line,sizeof(line),"sample=%llu workload=%llu class=%s corner=%u vertex=%u transform=%u prev_transform=%u mapped=%u id=0x%X prev_id=0x%X matrix_address=0x%X prev_address=0x%X addresses_valid=%u/%u vertex_count=%u prev_count=%u vertex_mode=%u position_mode=%u rotation_mode=%u lerp_flags=0x%X prev_vertex=%u prev_vertex_in_range=%u pos=%.9g/%.9g/%.9g vel=%.9g/%.9g/%.9g prev_source=%.9g/%.9g/%.9g",
                (unsigned long long)sample,(unsigned long long)workload,classes[k],i,v.index,v.transform,v.previousTransform,unsigned(v.mapped),v.id,v.previousId,v.address,v.previousAddress,unsigned(v.addressValid),unsigned(v.previousAddressValid),v.count,v.previousCount,unsigned(v.vertexMode),unsigned(v.positionMode),unsigned(v.rotationMode),v.lerpFlags,v.previousVertex,unsigned(v.previousVertexValid),
                double(v.position.x),double(v.position.y),double(v.position.z),double(v.velocity.x),double(v.velocity.y),double(v.velocity.z),double(v.previousSource.x),double(v.previousSource.y),double(v.previousSource.z));
            tooie::diagnostics::log("geometry",line);
            const hlslpp::float4x4 *matrices[]={&v.previous,&v.current,&v.consumed,&v.consumedPrevious};
            const char *names[]={"previous","current","consumed","consumed_previous"};
            for (unsigned m=0;m<4;++m) {
                const auto &a=*matrices[m];
                std::snprintf(line,sizeof(line),"sample=%llu workload=%llu class=%s corner=%u matrix=%s rows=%.9g/%.9g/%.9g/%.9g;%.9g/%.9g/%.9g/%.9g;%.9g/%.9g/%.9g/%.9g;%.9g/%.9g/%.9g/%.9g",
                    (unsigned long long)sample,(unsigned long long)workload,classes[k],i,names[m],
                    double(a[0][0]),double(a[0][1]),double(a[0][2]),double(a[0][3]),double(a[1][0]),double(a[1][1]),double(a[1][2]),double(a[1][3]),
                    double(a[2][0]),double(a[2][1]),double(a[2][2]),double(a[2][3]),double(a[3][0]),double(a[3][1]),double(a[3][2]),double(a[3][3]));
                tooie::diagnostics::log("geometry",line);
            }
        }
    }
}
} // namespace RT64::geometryDiagnostics
