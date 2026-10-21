#pragma once
namespace tooie::android {
inline const char *&descriptorContext() { static thread_local const char *value = "unspecified"; return value; }
class DescriptorContextScope {
    const char *previous;
public:
    explicit DescriptorContextScope(const char *value) : previous(descriptorContext()) { descriptorContext() = value; }
    ~DescriptorContextScope() { descriptorContext() = previous; }
};
}
