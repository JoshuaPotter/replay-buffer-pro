import { clock, effect, frame, frameLoop, init, surface } from 'vgpu';
import type { Frame, FrameLoopHandle } from 'vgpu';
import heroFieldShader from './hero-field.wgsl';

// The box the copy's text lines and controls actually cover. Block elements span the whole
// column, so their own boxes would leave the space beside short lines dark.
function contentBox(copy: HTMLElement): DOMRect {
  const rects: DOMRect[] = [];
  const walker = document.createTreeWalker(copy, NodeFilter.SHOW_TEXT);
  const range = document.createRange();
  for (let node = walker.nextNode(); node; node = walker.nextNode()) {
    if (!node.textContent?.trim()) continue;
    range.selectNodeContents(node);
    rects.push(...range.getClientRects());
  }
  copy.querySelectorAll('a, button').forEach((el) => rects.push(el.getBoundingClientRect()));
  const visible = rects.filter((r) => r.width && r.height);
  if (!visible.length) return copy.getBoundingClientRect();
  const left = Math.min(...visible.map((r) => r.left));
  const top = Math.min(...visible.map((r) => r.top));
  const right = Math.max(...visible.map((r) => r.right));
  const bottom = Math.max(...visible.map((r) => r.bottom));
  return new DOMRect(left, top, right - left, bottom - top);
}

// Draws the hero's halftone field into `canvas`, lit from behind `anchor` (the dock) and kept
// quiet behind `copy` (the headline, lead and buttons). Without WebGPU, init() throws and the
// canvas stays hidden, leaving the flat black hero.
export async function initHeroField(
  canvas: HTMLCanvasElement,
  anchor: HTMLElement | null,
  copy: HTMLElement | null,
): Promise<void> {
  let gpu;
  try {
    gpu = await init({ powerPreference: 'low-power' });
  } catch {
    return;
  }

  const target = surface(gpu, canvas, { dpr: [1, 2] });
  const field = effect(gpu, heroFieldShader, {
    set: { params: { size: target.size, flare: [0, 0], calm: [0, 0, 0, 0], time: 0, dpr: target.dpr, calmLevel: 1 } },
  });
  await field.compile(target);

  // The flare sits behind the dock, a little above its middle; when the hero stacks (below
  // 48rem) it moves up to the dock's top edge, lighting the clouds between text and dock. The
  // copy stays calm: faint clouds behind it side by side, none at all when stacked, where the
  // field would otherwise sit right behind the buttons.
  const wide = matchMedia('(min-width: 48rem)');
  const place = () => {
    const box = canvas.getBoundingClientRect();
    const dpr = target.size[0] / Math.max(box.width, 1);
    const dock = (anchor ?? canvas).getBoundingClientRect();
    const x = dock.left + dock.width / 2 - box.left;
    const y = dock.top + dock.height * (wide.matches ? 0.4 : 0.05) - box.top;
    const text = copy ? contentBox(copy) : undefined;
    const calm = text
      ? [(text.left - box.left) * dpr, (text.top - box.top) * dpr, (text.right - box.left) * dpr, (text.bottom - box.top) * dpr]
      : [0, 0, 0, 0];
    field.set({
      params: { size: target.size, flare: [x * dpr, y * dpr], calm, dpr, calmLevel: wide.matches ? 0.5 : 0 },
    });
  };
  target.onResize(place);
  for (const el of [anchor, copy]) if (el) new ResizeObserver(place).observe(el);
  void document.fonts.ready.then(place); // web fonts rewrap the text lines

  const time = clock(gpu);
  const render = (f: Frame) => {
    field.set({ params: { time: time.time } });
    f.pass(target, field);
  };

  const reveal = () => requestAnimationFrame(() => canvas.classList.add('is-ready'));

  // Reduced motion: one still frame, redrawn only when the hero resizes.
  if (matchMedia('(prefers-reduced-motion: reduce)').matches) {
    const still = () => frame(gpu, (f) => {
      field.set({ params: { time: 2.5 } });
      f.pass(target, field);
    });
    still();
    reveal();
    new ResizeObserver(() => still()).observe(canvas);
    return;
  }

  // Animate only while the hero is on screen and the tab is visible.
  let loop: FrameLoopHandle | undefined;
  let onScreen = false;
  const update = () => {
    const run = onScreen && document.visibilityState === 'visible';
    if (run && !loop) loop = frameLoop(gpu, render, { fps: 30 });
    if (!run && loop) {
      loop.stop();
      loop = undefined;
    }
  };
  new IntersectionObserver(([entry]) => {
    onScreen = entry.isIntersecting;
    update();
  }).observe(canvas);
  document.addEventListener('visibilitychange', update);
  reveal();

  void gpu.lost.then(() => {
    loop = undefined;
    canvas.classList.remove('is-ready');
  });
}
