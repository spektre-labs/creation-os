import "@testing-library/jest-dom";

class ResizeObserverStub {
  observe(): void {}
  unobserve(): void {}
  disconnect(): void {}
}

globalThis.ResizeObserver = ResizeObserverStub as typeof ResizeObserver;

/* Recharts measures containers on mount; jsdom often reports 0×0 without a layout pass. */
if (typeof HTMLElement !== "undefined") {
  const size = { width: 480, height: 320 };
  const rect = () =>
    ({
      x: 0,
      y: 0,
      width: size.width,
      height: size.height,
      top: 0,
      left: 0,
      bottom: size.height,
      right: size.width,
      toJSON: () => ({}),
    }) as DOMRect;
  try {
    HTMLElement.prototype.getBoundingClientRect = rect;
    Object.defineProperty(HTMLElement.prototype, "clientWidth", {
      configurable: true,
      get: () => size.width,
    });
    Object.defineProperty(HTMLElement.prototype, "clientHeight", {
      configurable: true,
      get: () => size.height,
    });
  } catch {
    /* ignore */
  }
}
