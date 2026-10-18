#pragma once
// Android-only policy. Desktop keeps its existing transform consumer.
#ifndef TOOIE_ANDROID_SKELETAL_INTERPOLATION
#define TOOIE_ANDROID_SKELETAL_INTERPOLATION 1
#endif
#if defined(__ANDROID__) && TOOIE_ANDROID_SKELETAL_INTERPOLATION
#define TOOIE_SKELETAL_POSE_ENABLED 1
#else
#define TOOIE_SKELETAL_POSE_ENABLED 0
#endif
