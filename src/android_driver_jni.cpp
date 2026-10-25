#include "android_driver_settings.h"
#include "android_vulkan_driver_policy.h"
#include <jni.h>
#include <SDL.h>
#include <SDL_system.h>

namespace tooie::android::gpu {
namespace {
struct Peer {
    JNIEnv* env=static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    jobject activity=nullptr;
    jclass type=nullptr;
    Peer() {
        if(!env) return;
        activity=static_cast<jobject>(SDL_AndroidGetActivity());
        if(activity&&!env->ExceptionCheck()) type=env->GetObjectClass(activity);
    }
    jmethodID method(const char* name,const char* signature) {
        return type&&!env->ExceptionCheck()?env->GetMethodID(type,name,signature):nullptr;
    }
    jstring string(const std::string& value) {
        return env&&!env->ExceptionCheck()?env->NewStringUTF(value.c_str()):nullptr;
    }
    void release(jobject value) {if(value)env->DeleteLocalRef(value);}
    ~Peer() {
        if(!env)return;
        if(env->ExceptionCheck())env->ExceptionClear();
        release(type);release(activity);
    }
};
}
void show_settings(int action) {
    Peer peer;auto method=peer.method("showGraphicsDriverSettings","(I)V");
    if(method)peer.env->CallVoidMethod(peer.activity,method,jint(action));
}
std::array<std::string,4> settings_state() {
    std::array<std::string,4> result{};Peer peer;
    auto method=peer.method("getGraphicsDriverState","(I)Ljava/lang/String;");
    if(!method)return result;
    for(int i=0;i<4&&!peer.env->ExceptionCheck();i++) {
        auto value=static_cast<jstring>(peer.env->CallObjectMethod(peer.activity,method,jint(i)));
        const char* utf=value&&!peer.env->ExceptionCheck()?peer.env->GetStringUTFChars(value,nullptr):nullptr;
        if(utf) {result[i]=utf;peer.env->ReleaseStringUTFChars(value,utf);}
        peer.release(value);
    }
    return result;
}
void recordSelection(const Selection& selection) {
    Peer peer;auto method=peer.method("recordGraphicsDriverSelection","(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
    if(!method)return;
    auto mode=peer.string(selection.mode==Mode::System?"system":selection.mode==Mode::Custom?"custom-turnip-test":"rejected");
    auto reason=peer.string(selection.error),package=peer.string(selection.packageSha256),library=peer.string(selection.librarySha256);
    if(!peer.env->ExceptionCheck())peer.env->CallVoidMethod(peer.activity,method,mode,reason,package,library);
    for(auto value:{mode,reason,package,library})peer.release(value);
}
void recordBackend(bool queried,unsigned id,const char* device,const char* name,const char* info,unsigned version,bool accepted) {
    Peer peer;auto method=peer.method("recordGraphicsDriverBackend","(ZILjava/lang/String;Ljava/lang/String;Ljava/lang/String;JZ)V");
    if(!method)return;
    auto deviceString=peer.string(device?device:""),nameString=peer.string(name?name:""),infoString=peer.string(info?info:"");
    if(!peer.env->ExceptionCheck())peer.env->CallVoidMethod(peer.activity,method,jboolean(queried),jint(id),deviceString,nameString,infoString,jlong(version),jboolean(accepted));
    for(auto value:{deviceString,nameString,infoString})peer.release(value);
}
void recordMenuReady() {
    Peer peer;auto method=peer.method("recordGraphicsMenuReady","()V");
    if(method)peer.env->CallVoidMethod(peer.activity,method);
}
}
