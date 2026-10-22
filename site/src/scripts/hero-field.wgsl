// The homepage hero's halftone field: soft, wispy clouds drifting left to right, drawn as small
// silver dots and lit by a gently breathing sun behind the dock, with a slow metallic sheen
// sweeping across where the light is strongest. Brightness sets how many dots light, not how big
// they are, so the clouds read as soft clusters of grain.
import { fbmSimplex2d, fbmSimplex3d } from "@vgpu/wgsl-std/noise/simplex";
import { pcg2d, unitFloat } from "@vgpu/wgsl-std/hash";
import { rotate2d, saturate } from "@vgpu/wgsl-std/math";

struct Params {
  size: vec2f,     // canvas size in physical pixels
  flare: vec2f,    // flare centre in physical pixels
  calm: vec4f,     // box kept quiet behind the hero copy (x0, y0, x1, y1), physical pixels
  time: f32,       // seconds
  dpr: f32,
  calmLevel: f32,  // how much of the field shows inside the calm box, 0..1
}
@group(0) @binding(0) var<uniform> params: Params;

const TAU = 6.2831853;
const CELL = 3.5;     // CSS px between dot centres
const DOT = 0.3;      // largest dot radius, in cells
const ANGLE = 0.349;  // grid rotation, about 20 degrees
const BREATH = 7.0;   // seconds per pulse
const DRIFT = 14.0;   // CSS px per second the clouds travel to the right
const EVOLVE = 0.025; // how fast the cloud shapes change

// Silver: cool grey in the shadows, brighter and nearly neutral in the highlights.
const SILVER_DARK = vec3f(0.78, 0.82, 0.88);
const SILVER_LIGHT = vec3f(0.96, 0.98, 1.0);

// 0 at rest, 1 at the top of each breath, eased by the cosine.
fn breath(t: f32) -> f32 {
  return 0.5 - 0.5 * cos(TAU * t / BREATH);
}

// Cloud density 0..1 at a point in CSS px. The noise is stretched wide so shapes read as
// long wisps, slides right with time, and slowly reshapes through the third noise axis.
fn clouds(p: vec2f, t: f32) -> f32 {
  let q = (p - vec2f(t * DRIFT, 0.0)) / vec2f(520.0, 240.0);
  let warp = vec2f(
    fbmSimplex2d(q * 0.8 + vec2f(1.7, 9.2), 2, 2.0, 0.5),
    fbmSimplex2d(q * 0.8 + vec2f(8.3, 2.8), 2, 2.0, 0.5),
  );
  let w = q + warp * 0.6;
  let body = fbmSimplex3d(vec3f(w, t * EVOLVE), 5, 2.0, 0.5);
  // Softer detail along the drift frays the edges into wisps.
  let wisp = fbmSimplex2d(w * vec2f(2.0, 3.0) + vec2f(0.0, t * EVOLVE), 3, 2.0, 0.5);
  return smoothstep(-0.05, 0.45, body * 0.85 + wisp * 0.15);
}

struct Field {
  lum: f32,   // overall brightness, 0..1
  core: f32,  // how close to the sun, 0..1
  r: f32,     // distance from the sun, in flare radii
  n: f32,     // cloud density
}

// Flare radius in CSS px, scaled to the hero.
fn flareRadius() -> f32 {
  let size = params.size / params.dpr;
  return clamp(min(size.x, size.y) * 0.9, 300.0, 680.0);
}

// 1 in the open field, calmLevel well inside the box behind the hero copy.
fn calm(p: vec2f) -> f32 {
  let box = params.calm / params.dpr;
  let outside = max(max(box.x - p.x, p.x - box.z), max(box.y - p.y, p.y - box.w));
  return mix(params.calmLevel, 1.0, smoothstep(-24.0, 140.0, outside));
}

// The clouds, lit by the sun, at a point given in CSS px.
fn field(p: vec2f, t: f32) -> Field {
  let r = length(p - params.flare / params.dpr) / flareRadius();
  let pulse = 0.9 + 0.2 * breath(t);
  let n = clouds(p, t);

  // Even ambient light across the hero, so the clouds read edge to edge; the sun only adds a
  // gentle lift around the dock.
  let light = 0.42 + (pow(saturate(1.0 - r), 2.0) * 0.5 + exp(-r * r * 12.0) * 0.25) * pulse;

  var out: Field;
  out.lum = n * light + 0.02;
  out.core = exp(-r * r * 3.5);
  out.r = r;
  out.n = n;
  return out;
}

// The silver dot pattern at a point in CSS px.
fn dots(p: vec2f, t: f32) -> vec3f {
  // Dot grid, rotated. Each cell samples the field at its centre.
  let g = rotate2d(p, ANGLE) / CELL;
  let cell = floor(g) + 0.5;
  let local = g - cell;
  let centre = rotate2d(cell * CELL, -ANGLE);

  let f = field(centre, t);
  let lum = saturate(f.lum) * calm(centre);

  // Each dot lights at its own random brightness, so denser cloud means more dots.
  let grain = unitFloat(pcg2d(bitcast<vec2u>(vec2i(floor(g)))).x);
  let threshold = grain * 0.9 + 0.05;
  let presence = smoothstep(threshold - 0.06, threshold + 0.06, lum);

  // Dots stay small; brighter areas only nudge them larger. Antialias over one device pixel.
  let radius = DOT * (0.6 + 0.4 * lum) * presence;
  let aa = 0.75 / (CELL * params.dpr);
  let dotMask = 1.0 - smoothstep(radius - aa, radius + aa, length(local));

  // Small sparse dots read dimmer than their tone, so it peaks bright.
  let tone = mix(0.3, 0.75, lum);
  // A soft diagonal glint that sweeps slowly across the clouds, like light on brushed metal.
  let band = dot(centre, vec2f(0.0035, 0.0018)) - t * 0.12 + f.n * 1.5 + grain * 0.2;
  let sheen = pow(0.5 + 0.5 * sin(band), 8.0) * (0.25 + 0.35 * f.core) * (0.85 + 0.3 * breath(t));
  return tone * mix(SILVER_DARK, SILVER_LIGHT, lum) * (1.0 + sheen) * dotMask;
}

@fragment fn fs_main(@location(0) uv: vec2f) -> @location(0) vec4f {
  let t = params.time;
  let p = uv * params.size / params.dpr; // CSS px
  let b = breath(t);
  let colour = dots(p, t);

  // A faint continuous glow under the dots, so the core radiates rather than just tiles.
  let rp = length(p - params.flare / params.dpr) / flareRadius();
  let glow = vec3f(0.9, 0.94, 1.0) * (pow(saturate(1.0 - rp), 3.0) * 0.045 + exp(-rp * rp * 20.0) * 0.03) * (0.85 + 0.3 * b) * calm(p);

  return vec4f(colour + glow, 1.0);
}
