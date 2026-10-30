#pragma once
// Port-owned metadata in a reserved matrix-group flag bit. No guest matrix or
// standard component flag changes; Android's consumer handles this explicitly.
#define TOOIE_CAMERA_RELATIVE_GROUP 0x04000000U
// Android-only policy. Desktop keeps its existing transform consumer.
#ifndef TOOIE_ANDROID_SKELETAL_INTERPOLATION
#define TOOIE_ANDROID_SKELETAL_INTERPOLATION 1
#endif
#if defined(__ANDROID__) && TOOIE_ANDROID_SKELETAL_INTERPOLATION
#define TOOIE_SKELETAL_POSE_ENABLED 1
#else
#define TOOIE_SKELETAL_POSE_ENABLED 0
#endif
#ifndef TOOIE_ANDROID_ACTOR_INTERPOLATION
#define TOOIE_ANDROID_ACTOR_INTERPOLATION 1
#endif
#if TOOIE_SKELETAL_POSE_ENABLED && TOOIE_ANDROID_ACTOR_INTERPOLATION
#define TOOIE_ACTOR_POSE_ENABLED 1
#define TOOIE_ACTOR_POLICY "validated-marker-hierarchy"
#else
#define TOOIE_ACTOR_POSE_ENABLED 0
#define TOOIE_ACTOR_POLICY "unchanged"
#endif
