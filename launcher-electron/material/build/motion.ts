// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

// Spring of the expressive motion scheme (with unit mass).
//
interface spring
{
  name: string;      // Token name, for example fast-spatial.
  stiffness: number;
  damping: number;   // Damping ratio, 1 being critically damped.
}

const springs: readonly spring[] =
[
  {name: 'fast-spatial',    stiffness:  800, damping: 0.6},
  {name: 'default-spatial', stiffness:  380, damping: 0.8},
  {name: 'slow-spatial',    stiffness:  200, damping: 0.8},
  {name: 'fast-effects',    stiffness: 3800, damping: 1.0},
  {name: 'default-effects', stiffness: 1600, damping: 1.0},
  {name: 'slow-effects',    stiffness:  800, damping: 1.0}
];

// Distance from rest below which we consider the spring settled, as a
// fraction of the distance it travels.
//
const rest_threshold = 0.001;

// Number of points that sample the spring for the linear() easing. With 64
// points the polyline deviates from the curve by at most 0.3% of the
// distance traveled, which is about a pixel for the longest motion that we
// have (the 400dp side sheet).
//
const easing_points = 64;

// Return the position of the spring at time t (in seconds) as it moves from
// 0 at rest towards 1.
//
function
position (s: spring, t: number): number
{
  const w0 = Math.sqrt (s.stiffness);
  const z = s.damping;

  if (z < 1)
  {
    const wd = w0 * Math.sqrt (1 - z * z);

    return 1 - Math.exp (-z * w0 * t) *
               (Math.cos (wd * t) + (z * w0 / wd) * Math.sin (wd * t));
  }

  // Critically damped (we have no overdamped springs).
  //
  return 1 - Math.exp (-w0 * t) * (1 + w0 * t);
}

// Return the settling time of the spring in seconds, that is, the time after
// which it stays within the rest threshold of its destination.
//
// We step through the motion in milliseconds and remember the last step that
// is still outside of the threshold. Note that every spring settles well
// within the ten seconds that we examine (the slowest takes 600ms).
//
function
settling_time (s: spring): number
{
  let r = 0;

  for (let ms = 0; ms <= 10000; ++ms)
  {
    const t = ms / 1000;

    if (Math.abs (1 - position (s, t)) >= rest_threshold)
      r = t;
  }

  return r + 0.001;
}

// Return the linear() easing function that follows the spring over the
// duration (its settling time).
//
function
easing (s: spring, duration: number): string
{
  const ps: string[] = [];

  for (let i = 0; i <= easing_points; ++i)
  {
    const v = i === easing_points
      ? 1
      : position (s, duration * i / easing_points);

    ps.push (String (Math.round (v * 10000) / 10000));
  }

  return `linear(${ps.join (', ')})`;
}

// Return the motion tokens stylesheet.
//
// For each spring we write its parameters as in the specification plus a
// duration and an easing for CSS transitions. If the user prefers reduced
// motion, then each spatial spring takes the timing of the effects spring of
// the same speed, which doesn't bounce and settles sooner.
//
export function
motion_css (): string
{
  const ds: string[] = [];

  for (const s of springs)
  {
    const d = settling_time (s);
    const p = `--md-sys-motion-spring-${s.name}`;

    ds.push (`  ${p}-stiffness: ${s.stiffness};`,
             `  ${p}-damping: ${s.damping};`,
             `  ${p}-duration: ${Math.round (d * 1000)}ms;`,
             `  ${p}-easing: ${easing (s, d)};`,
             '');
  }

  const rs: string[] = [];

  for (const v of ['fast', 'default', 'slow'])
  {
    const p = '--md-sys-motion-spring';

    rs.push (`    ${p}-${v}-spatial-duration: var(${p}-${v}-effects-duration);`,
             `    ${p}-${v}-spatial-easing: var(${p}-${v}-effects-easing);`);
  }

  return `:root
{
${ds.join ('\n').trimEnd ()}
}

@media (prefers-reduced-motion: reduce)
{
  :root
  {
${rs.join ('\n')}
  }
}
`;
}
