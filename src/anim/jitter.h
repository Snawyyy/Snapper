#ifndef SNAPPER_ANIM_JITTER_H_
#define SNAPPER_ANIM_JITTER_H_

namespace snapper {

// A repeatable random number from -1 to 1 for step and axis (0 to
// 63): the same shake, glitch or jitter every time a frame renders, in
// preview and export alike.
double Jitter(int step, int axis);

}  // namespace snapper

#endif  // SNAPPER_ANIM_JITTER_H_
