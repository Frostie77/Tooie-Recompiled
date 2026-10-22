# Captured geometry inputs

`odin-step81-geometry.txt` is a 51-record numeric subset of the supplied Step 81
Odin trace: PID 24984/TID 25058, integration `d5a1d1f1213a57d2adc878d25d815a76f1cc5f13`,
rendering parent `6da5adb0f0a73f93f723152a19306a00fa580e0a`.
Its hash, original archive provenance, sample identities and limitations are in
[the Step 81 investigation](../../docs/development/history/android-geometry-replay-step81.md).

The native test reproduces the recorded **distortion**, not an authored correct
pose. Passing it does not fix or verify gameplay. It supplies no ROM, complete
mesh, skeleton hierarchy, actor identity or prior cache history. Do not replace
it with nicer synthetic geometry or treat its group IDs as bone identity.
