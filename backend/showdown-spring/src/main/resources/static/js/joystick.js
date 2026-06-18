/**
 * VirtualJoystick Class
 * Uses PointerEvents to handle both Touch and Mouse inputs natively.
 */
class VirtualJoystick {
  constructor(trackElement, knobElement, options = {}) {
    this.track = trackElement;
    this.knob = knobElement;
    this.onChange = options.onChange || null;
    this.onRelease = options.onRelease || null;

    this.x = 0; // -1.0 to 1.0
    this.y = 0; // -1.0 to 1.0
    this.active = false;
    this.pointerId = null;

    // Track bounds (radius)
    this.radius = this.track.clientWidth / 2 || 110; // Fallback to 110 (half of 220px track) if clientWidth is 0
    
    // Center point of the track relative to the viewport
    this.centerX = 0;
    this.centerY = 0;

    this.init();
  }

  init() {
    // Recalculate dimensions
    this.recalculateBounds();
    window.addEventListener('resize', () => this.recalculateBounds());

    // Bind pointer events
    this.track.addEventListener('pointerdown', (e) => this.onPointerDown(e));
    window.addEventListener('pointermove', (e) => this.onPointerMove(e));
    window.addEventListener('pointerup', (e) => this.onPointerUp(e));
    window.addEventListener('pointercancel', (e) => this.onPointerUp(e));
  }

  recalculateBounds() {
    const rect = this.track.getBoundingClientRect();
    this.radius = rect.width / 2 || 110; // Fallback to 110
    this.centerX = rect.left + (rect.width / 2 || 110);
    this.centerY = rect.top + (rect.height / 2 || 110);
  }

  onPointerDown(e) {
    if (this.active) return; // Only handle one pointer

    e.preventDefault();
    this.active = true;
    this.pointerId = e.pointerId;
    try {
      this.track.setPointerCapture(e.pointerId);
    } catch (err) {
      console.warn('Pointer capture failed:', err);
    }
    
    this.knob.classList.add('active');
    this.recalculateBounds(); // Ensure bounds are fresh
    this.processInput(e.clientX, e.clientY);
  }

  onPointerMove(e) {
    if (!this.active || e.pointerId !== this.pointerId) return;
    
    e.preventDefault();
    this.processInput(e.clientX, e.clientY);
  }

  onPointerUp(e) {
    if (!this.active || e.pointerId !== this.pointerId) return;

    this.active = false;
    this.pointerId = null;
    this.knob.classList.remove('active');
    
    // Reset knob position using CSS transitions
    this.knob.style.transition = 'all 0.15s cubic-bezier(0.25, 0.8, 0.25, 1)';
    this.knob.style.left = '50%';
    this.knob.style.top = '50%';
    
    this.x = 0;
    this.y = 0;

    if (this.onRelease) {
      this.onRelease();
    }
    if (this.onChange) {
      this.onChange(0, 0);
    }
  }

  processInput(clientX, clientY) {
    // Remove transition during active dragging
    this.knob.style.transition = 'none';

    // Calculate distance from center
    let deltaX = clientX - this.centerX;
    let deltaY = clientY - this.centerY;
    
    const distance = Math.sqrt(deltaX * deltaX + deltaY * deltaY);

    // Limit distance to track radius
    if (distance > this.radius) {
      const angle = Math.atan2(deltaY, deltaX);
      deltaX = Math.cos(angle) * this.radius;
      deltaY = Math.sin(angle) * this.radius;
    }

    // Set knob position relative to track center (50% is center)
    // Map -radius -> radius to -50% -> 50% offset
    const percentX = (deltaX / this.radius) * 50;
    const percentY = (deltaY / this.radius) * 50;

    this.knob.style.left = `calc(50% + ${percentX}%)`;
    this.knob.style.top = `calc(50% + ${percentY}%)`;

    // Map output vector to -1.0 to 1.0 range (y-up is negative in screen space, let's keep screen space or match unreal coordinates)
    // Unreal Y-axis goes right (X) and forward (Y).
    // Usually, clientX mapping to X, and clientY mapping to Y.
    // Screen coordinate: clientX increases right, clientY increases down.
    // Let's negate Y so that pushing forward (up on screen) returns a positive value.
    const rawX = deltaX / this.radius;
    const rawY = -(deltaY / this.radius);

    // Smooth value rounding (2 decimal points)
    this.x = Math.round(rawX * 100) / 100;
    this.y = Math.round(rawY * 100) / 100;

    if (this.onChange) {
      this.onChange(this.x, this.y);
    }
  }
}
