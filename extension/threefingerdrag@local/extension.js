import Clutter from 'gi://Clutter';
import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';

const MIN_SWIPE_FINGERS = 4;

/*
 * libinput (with 3fg drag on) emits SWIPE_BEGIN(3) before converting the
 * gesture into a BTN_LEFT drag. GNOME's TouchpadSwipeGesture sees that and
 * starts a workspace switch, swallowing the drag.
 *
 * Patching TouchpadSwipeGesture.prototype._handleEvent does NOT work: the
 * handler is wired as `this._handleEvent.bind(this)` in _init(), so the
 * function reference is captured at construction and a later prototype patch
 * is never consulted.
 *
 * Instead we filter in the CAPTURE phase on global.stage, which runs before
 * the actor's 'event::touchpad' handler. Swallowing the swipe event does not
 * affect the drag, which arrives as separate pointer button/motion events.
 */
export default class ThreeFingerDragExtension extends Extension {
    enable() {
        this._stopped = 0;
        this._passed = 0;
        this._handlerId = global.stage.connect('captured-event', (_actor, event) => {
            try {
                if (event.type() === Clutter.EventType.TOUCHPAD_SWIPE) {
                    const n = event.get_touchpad_gesture_finger_count();
                    if (n < MIN_SWIPE_FINGERS) {
                        if (this._stopped++ % 50 === 0)
                            console.log(`[3fgdrag] swallowed ${n}-finger swipe (count=${this._stopped})`);
                        return Clutter.EVENT_STOP;
                    }
                    if (this._passed++ % 50 === 0)
                        console.log(`[3fgdrag] passing ${n}-finger swipe through (count=${this._passed})`);
                }
            } catch (e) {
                console.warn(`[3fgdrag] filter error: ${e}`);
            }
            return Clutter.EVENT_PROPAGATE;
        });
        console.log(`[3fgdrag] capture-phase filter installed (min swipe = ${MIN_SWIPE_FINGERS})`);
    }

    disable() {
        if (this._handlerId) {
            global.stage.disconnect(this._handlerId);
            this._handlerId = 0;
        }
        console.log('[3fgdrag] capture-phase filter removed');
    }
}
