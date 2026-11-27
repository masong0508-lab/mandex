#!/usr/bin/env python3
"""Exercise real Chromium pointer/keyboard handlers and deterministic game ticks.

Requires Playwright and an installed Chromium:
    python3 test/browser_input_smoke.py --out-dir out/browser-input-smoke

Use --chromium PATH or CHROMIUM_EXECUTABLE for a system browser; otherwise
Playwright's normal Chromium launch is used. This is an optional browser CI
gate, not a hardware FPS benchmark. It writes JSON evidence and screenshots
on success or failure (a launch failure is recorded when no page is available).
"""

import argparse
import json
import math
import os
from pathlib import Path
import sys
import time
import traceback


ROOT = Path(__file__).resolve().parents[1]
MAX_FRAMES = 1600
MAX_SECONDS = 120
INIT = r"""
(() => {
  let seed = 0x51a7c0de;
  window.__inputSmoke = { rafRequests: 0, events: {} };
  window.requestAnimationFrame = () => (++__inputSmoke.rafRequests, 0);
  Math.random = () => {
    seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0;
    return seed / 4294967296;
  };
  for (const name of ['keydown', 'keyup', 'pointerdown', 'pointermove',
                      'pointerup', 'resize', 'orientationchange']) {
    window.addEventListener(name, event => {
      const key = name + ':' + (event.isTrusted ? 'trusted' : 'dispatched');
      __inputSmoke.events[key] = (__inputSmoke.events[key] || 0) + 1;
    }, true);
  }
  try { localStorage.clear(); } catch (_) {}
})();
"""

HARNESS = r"""
(() => {
  function snapshot() {
    const invalid = [], seen = new Set();
    function finite(value, path) {
      if (typeof value === 'number' && !Number.isFinite(value)) invalid.push(path);
      else if (value && typeof value === 'object' && !seen.has(value)) {
        seen.add(value);
        for (const key of Object.keys(value)) finite(value[key], path + '.' + key);
      }
    }
    finite(man, 'man'); finite(CAM, 'CAM');
    const angle = man.mode === 'walk' && man.turn > 0
      ? walkTurnAngle(man.turn) : Math.PI / 2;
    const face = man.mode === 'walk' && man.turn > 0
      ? walkTurnFace(man.face, man.turn) : man.face;
    const r = cv.getBoundingClientRect();
    return { state, level, name: CL.name, mode: man.mode, time,
      x: man.x, y: man.y, vx: man.vx, vy: man.vy,
      face: man.face, turn: man.turn || 0,
      yaw: face < 0 ? 2 * Math.PI - angle : angle,
      grounded: !!man.gr, wt: man.wt, punch: man.pT || 0,
      stomp: !!man.stomp, pad_cooldown: man.padCd || 0,
      jump_hold: man.jumpHold || 0, ledge_grace: man.ledgeGrace || 0,
      hurt_cooldown: man.hurtCd || 0,
      opponent: targets[0] ? { alive: targets[0].alive, hp: targets[0].hp,
        hp_max: targets[0].hpMax, inv: targets[0].inv, stun: targets[0].stun,
        wind: targets[0].wind, x: targets[0].x, y: targets[0].y } : null,
      invalid, camera: { ...CAM }, world: [COLS, ROWS, WW, WH],
      actor_screen: toScreen(man.x, man.y),
      canvas: [W, H], portrait: isPortrait(),
      css: [r.x, r.y, r.width, r.height],
      men, shots: lv.shots, alive: aliveTargets(), targets: targets.length,
      chunks: chunks.length, blocks: grid.reduce((n, value) => n + !!value, 0),
      score, active_cases: cases.length,
      dragging, touches: Object.keys(TID).length,
      keys: { left: !!KEYS.ArrowLeft, right: !!KEYS.ArrowRight,
        jump: !!KEYS.Space, punch: !!KEYS.KeyX,
        virtualLeft: !!KEYS.TL, virtualRight: !!KEYS.TR },
      shake, freeze, slow };
  }
  function tick(count) {
    if (!Number.isInteger(count) || count < 1 || count > 30)
      throw Error('Invalid fixed-step batch: ' + count);
    const frames = [];
    for (let i = 0; i < count; i++) {
      // The display scheduler has its own regression. Here each input
      // observation gets exactly one production simulation step, avoiding
      // floating timestamp roundoff and accidental zero/two-tick samples.
      layoutTick(); frameDt = WEB_SIM_DT;
      update(WEB_SIM_DT); camTick(WEB_SIM_DT); draw();
      frames.push(snapshot());
    }
    return frames;
  }
  function prepare(kind) {
    muted = true; OPT().muted = true; OPT().fight = false;
    OPT().collapse = true; shake = 0; freeze = 0; slow = 0;
    if (kind === 'tower') {
      // A supported wood column lies directly across a horizontal shot.
      // Its cap has only a three-cell overhang (wood supports six).
      // Cutting the two-cell column detaches the cap through supportPass;
      // no blast injection, mocked collision, or mission victory is needed.
      load(-1, { name: 'Input smoke support tower', men: 3,
        custom: true, build() {
          rect(40, 35, 2, 25, WOOD);
          rect(37, 34, 8, 1, WOOD);
          tgt(95, 59); // Keep the objective safely beyond the input fixture.
        } });
    } else {
      const index = LEVELS.findIndex(l => l.name === 'Walk-Up Wreckage');
      if (index < CH1 || index >= CH2 || !LEVELS[index].walk)
        throw Error('Expected shipped Chapter 2 walking level');
      load(index);
    }
    layoutTick(); camTick(0); frameDt = 0; draw();
    return snapshot();
  }
  function screenPoint(x, y) {
    const r = cv.getBoundingClientRect();
    return { x: r.left + x * r.width / W, y: r.top + y * r.height / H,
      sx: r.width / W, sy: r.height / H };
  }
  function prepareGameplay(kind) {
    // Stable encounter setups isolate recovery and pad landing. Initial
    // actor placement is fixture data; all following attacks and movement
    // use real keyboard events, production update and the actual renderer.
    muted = true; OPT().muted = true;
    OPT().fight = kind === 'counterpunch' || kind === 'parry';
    OPT().collapse = true; shake = 0; freeze = 0; slow = 0;
    for (const key of Object.keys(KEYS)) KEYS[key] = false;
    load(-1, { name: kind === 'pad' ? 'Stomp boost input check' :
      kind === 'jump' ? 'Jump height input check' :
      kind === 'ledge' ? 'Ledge input check' :
      kind === 'parry' ? 'Briefcase parry input check' : 'Counterpunch input check',
      men: 3, walk: true, custom: true, build() {
        if (kind === 'pad') rect(38, 59, 5, 1, RUBBER);
        else if(kind === 'jump') rect(15, 59, 80, 1, STEEL);
        else if(kind === 'ledge') rect(15, 59, 20, 1, STEEL);
        else if (kind !== 'parry') tgt(26.5, 60, kind === 'counterpunch' ? 'walker' : '');
        tgt(110, 60); // A distant objective prevents a fixture victory.
      } });
    man.x = kind === 'pad' ? 324 : kind === 'ledge' ? 264 : 200;
    man.y = kind === 'pad' ? 410 : kind === 'jump' || kind === 'ledge' ? 450 : 461;
    enterWalk(); man.hurtCd = 0;
    if (kind === 'parry') {
      man.gr = true;
      // A real airborne case overlaps the punch's forward hitbox and would
      // strike the walker this frame if the player failed to counter.
      cases.push({x:man.x+17,y:man.y-3,vx:-60,vy:0,rot:0,vr:-9,l:4});
    }
    if (kind !== 'pad' && kind !== 'jump' && kind !== 'ledge' && kind !== 'parry') {
      const t = targets[0]; t.hp = t.hpMax = kind === 'counterpunch' ? 3 : 2;
      t.wind = kind === 'counterpunch' ? WEB_SIM_DT / 2 : 0;
      t.cd = 2; t.face = -1; man.gr = true;
    }
    if (kind === 'parry') texts.length = 0; // isolate actual PARRY/OUCH feedback
    banner = 0; layoutTick(); camTick(0); frameDt = 0; draw();
    return snapshot();
  }
  window.__inputSmokeGame = { snapshot, tick, prepare, prepareGameplay,
    slingPoint() {
      const p = toScreen(man.x, man.y), q = screenPoint(p[0], p[1]);
      q.sx *= CAM.on ? CAM.z : 1; q.sy *= CAM.on ? CAM.z : 1;
      return q;
    },
    leftButton() {
      const b = walkBtns().find(b => b.k === 'TL');
      return screenPoint(b.x + b.w / 2, b.y + b.h / 2);
    },
    rotate() {
      // Desktop viewport changes emit resize, but not mobile orientation.
      // Dispatch the latter to exercise the actual production release hook.
      window.dispatchEvent(new Event('orientationchange'));
      layoutTick(); camTick(0); frameDt = 0; draw();
      return snapshot();
    }
  };
})();
"""


def require(condition, message, details=None):
    if not condition:
        raise AssertionError(message + (': ' + json.dumps(details) if details is not None else ''))


def require_walk_camera(row):
    if not row['portrait'] or row['mode'] != 'walk':
        return
    x, y = row['actor_screen']
    w, h = row['canvas']
    camera = row['camera']
    z = camera['z']
    require(camera['on'] and .55 <= z <= 1.8, 'Invalid portrait walking zoom', row)
    require(0 <= camera['x'] <= max(0, row['world'][2] - w / z) + 1e-8,
            'Walking camera escaped the world bounds', row)
    require(0 <= x <= w, 'Walking camera lost the actor horizontally', row)
    # Conservative artwork envelope includes the hair, airborne tilt and
    # feet. Keep the whole actor between the mission HUD and timer strip.
    require(y - 40 * z >= 138 - 1e-8 and y + 22 * z <= h - 174 + 1e-8,
            'Walking actor overlaps the mission HUD or touch controls', row)


class Smoke:
    def __init__(self, page, out_dir, report):
        self.page, self.out_dir, self.report = page, out_dir, report
        self.deadline = time.monotonic() + MAX_SECONDS

    def snapshot(self):
        result = self.page.evaluate('__inputSmokeGame.snapshot()')
        self.report['last_state'] = result
        require(not result['invalid'], 'Non-finite character or camera state', result)
        require_walk_camera(result)
        return result

    def step(self, count=1):
        require(time.monotonic() < self.deadline, 'Browser smoke exceeded wall-time budget')
        require(self.report['frames_checked'] + count <= MAX_FRAMES,
                'Browser smoke exceeded fixed-step budget')
        before = self.snapshot()
        frames = self.page.evaluate('n => __inputSmokeGame.tick(n)', count)
        for row in frames:
            self.report['last_state'] = row
            require(not row['invalid'], 'Non-finite character or camera state', row)
            require_walk_camera(row)
            require(abs(row['time'] - before['time'] - 1 / 60) < 1e-9,
                    'Expected exactly one update per input observation', [before, row])
            before = row
        self.report['frames_checked'] += count
        require(not self.report['page_errors'] and not self.report['console_errors'],
                'Runtime browser errors', self.report['page_errors'] + self.report['console_errors'])
        return frames

    def screenshot(self, name):
        path = self.out_dir / (name + '.png')
        self.page.screenshot(path=str(path), timeout=5000)
        self.report['screenshots'].append(path.name)

    def prepare(self, kind):
        self.report['stage'] = 'prepare_' + kind
        self.page.evaluate('kind => __inputSmokeGame.prepare(kind)', kind)
        return self.step(30)[-1]

    def sling_drag(self, dx, dy):
        self.report['stage'] = 'pointer_launch'
        start = self.page.evaluate('__inputSmokeGame.slingPoint()')
        before = self.snapshot()
        self.page.mouse.move(start['x'], start['y'])
        self.page.mouse.down()
        require(self.snapshot()['dragging'], 'Real pointerdown did not acquire the sling')
        self.page.mouse.move(start['x'] + dx * start['sx'],
                             start['y'] + dy * start['sy'], steps=8)
        aim = self.snapshot()
        require(aim['mode'] == 'aim' and aim['dragging'], 'Pointermove lost the sling', aim)
        self.page.mouse.up()
        launched = self.snapshot()
        require(launched['mode'] == 'fly' and not launched['dragging'],
                'Real pointerup did not launch', launched)
        require(launched['men'] == before['men'] - 1
                and launched['shots'] == before['shots'] + 1,
                'A pointer release must spend exactly one launch', launched)
        return {'pull': [dx, dy], 'aim': aim, 'launched': launched}

    def continuous(self, count=1, expected_face=None):
        previous, deltas = self.snapshot(), []
        frames = self.step(count)
        for row in frames:
            require(row['mode'] == 'walk', 'Walking input unexpectedly exited walk mode', row)
            delta = abs(math.atan2(math.sin(row['yaw'] - previous['yaw']),
                                   math.cos(row['yaw'] - previous['yaw'])))
            require(delta < .407, 'Countersteering snapped beyond one normal turn step',
                    {'before': previous, 'after': row, 'delta': delta})
            require(0 <= row['turn'] < 12, 'Turn timer escaped its bounded range', row)
            if expected_face is not None:
                require(row['face'] == expected_face, 'Keyboard facing was not immediate', row)
            deltas.append(delta)
            previous = row
        return frames, deltas

    def gameplay_checks(self):
        p, report = self.page, self.report
        report['gameplay'] = {}
        for kind in ('recovery', 'counterpunch'):
            report['stage'] = 'gameplay_' + kind
            before = p.evaluate('kind => __inputSmokeGame.prepareGameplay(kind)', kind)
            p.keyboard.down('KeyX')
            hit = self.continuous()[0][-1]
            p.keyboard.up('KeyX')
            opponent = hit['opponent']
            require(opponent['alive'] and opponent['hp'] == before['opponent']['hp'] - 1,
                    'Punch plus body overlap did not consume exactly one enemy HP', hit)
            require(opponent['inv'] > 0 and opponent['stun'] > 0,
                    'A surviving punch lost its recovery window', hit)
            require(hit['hurt_cooldown'] == 0 and
                    abs(before['wt'] - hit['wt'] - 1 / 60) < 1e-8,
                    'Counterpunched enemy hit through its stun', hit)
            require(opponent['wind'] == 0 and hit['score'] == 0,
                    'Interrupted attack survived or a nonfatal punch scored an eviction', hit)
            later = self.continuous(8)[0][-1]
            require(later['opponent']['alive'] and later['opponent']['hp'] == opponent['hp'],
                    'Repeated real body contacts bypassed enemy recovery', later)
            self.screenshot('08-gameplay-' + kind)
            report['gameplay'][kind] = {'initial': before, 'after_punch': hit,
                                        'after_overlap': later}

        p.set_viewport_size({'width': 390, 'height': 844})
        launches = {}
        for boosted in (False, True):
            name = 'boosted_pad' if boosted else 'ordinary_pad'
            report['stage'] = 'gameplay_' + name
            initial = p.evaluate('__inputSmokeGame.prepareGameplay("pad")')
            if boosted:
                p.keyboard.down('KeyS')
            launch = None
            for frames_to_land in range(1, 91):
                row = self.continuous()[0][-1]
                if row['pad_cooldown'] > 0 and row['vy'] < 0:
                    launch = row
                    break
            require(launch is not None, 'Real falling walker never reached the rubber pad')
            expected = -540 if boosted else -470
            require(abs(launch['vy'] - expected) < 1e-8,
                    'Rubber pad produced the wrong ordinary/stomp launch speed', launch)
            require(not launch['grounded'] and not launch['stomp'],
                    'Pad launch kept grounded/stomping state', launch)
            rising = self.continuous(6)[0]
            require(all(row['vy'] < 0 and not row['stomp'] for row in rising),
                    'Held STOMP cancelled the fresh pad ascent', rising[-1])
            if boosted:
                p.keyboard.up('KeyS')
            self.screenshot('09-gameplay-' + name)
            launches[name] = {'initial': initial, 'frames_to_land': frames_to_land,
                              'launch': launch, 'after_six_ticks': rising[-1]}
        require(launches['boosted_pad']['launch']['vy'] <
                launches['ordinary_pad']['launch']['vy'], 'Stomp never increased the pad launch')
        report['gameplay']['pads'] = launches

        # Real keyboard events drive the shipped walkStep, not mocked physics.
        # Reload the same supported floor to compare release and hold fairly.
        jump_traces = {}
        for held_ticks, name in ((1, 'tap_hop'), (14, 'held_high')):
            report['stage'] = 'gameplay_' + name
            initial = p.evaluate('__inputSmokeGame.prepareGameplay("jump")')
            settled = self.continuous(12)[0][-1]
            require(settled['grounded'], 'Jump fixture failed to settle on its floor', settled)
            baseline_y = settled['y']
            p.keyboard.down('Space')
            launched = self.continuous(held_ticks)[0]
            p.keyboard.up('Space')
            require(launched[0]['vy'] < 0 and not launched[0]['grounded'],
                    'Trusted Space key did not begin a normal jump', launched[0])
            released = self.continuous(18)[0]
            if held_ticks == 1:
                require(-180 <= released[0]['vy'] <= -160 and
                        released[0]['jump_hold'] == 0,
                        'Early keyup failed to cap rising speed', released[0])
            self.screenshot('10-gameplay-' + name)
            completed = self.continuous(20)[0]
            sample = launched + released + completed
            apex = min(row['y'] for row in sample)
            require(apex < baseline_y - 8,
                    'Character never left the ground after Space', sample[:3])
            jump_traces[name] = {'initial': initial, 'grounded': settled,
                                 'first_launch': launched[0], 'first_release': released[0],
                                 'apex_y': apex, 'height_px': baseline_y - apex,
                                 'end': completed[-1]}
        require(jump_traces['held_high']['height_px'] >
                jump_traces['tap_hop']['height_px'] + 24,
                'Real held jump did not clear visibly more height', jump_traces)
        report['gameplay']['jump_height'] = jump_traces

        report['stage'] = 'gameplay_real_ledge_jump'
        p.evaluate('__inputSmokeGame.prepareGameplay("ledge")')
        settled = self.continuous(12)[0][-1]
        require(settled['grounded'], 'Ledge test did not settle on its short platform', settled)
        p.keyboard.down('ArrowRight')
        departed = None
        for _ in range(30):
            row = self.continuous()[0][-1]
            if not row['grounded']:
                departed = row
                break
        p.keyboard.up('ArrowRight')
        require(departed is not None and 0 < departed['ledge_grace'] <= .1,
                'Running off a real ledge failed to arm coyote time', departed)
        p.keyboard.down('Space')
        jumped = self.continuous()[0][-1]
        p.keyboard.up('Space')
        require(jumped['vy'] < 0 and jumped['ledge_grace'] == 0,
                'Trusted Space during coyote time did not jump exactly once', jumped)
        released = self.continuous()[0][-1]
        p.keyboard.down('Space')
        no_double = self.continuous()[0][-1]
        p.keyboard.up('Space')
        require(no_double['vy'] > released['vy'] and no_double['ledge_grace'] == 0,
                'Second airborne Space incorrectly restarted the jump', no_double)
        self.screenshot('12-gameplay-ledge-grace')
        report['gameplay']['ledge_grace'] = {
            'edge': departed, 'jump': jumped, 'release': released,
            'no_double_jump': no_double}

        # The same briefcase must be dangerous without a counterpunch and
        # safely destroyed by a trusted keyboard attack. No scripted hit
        # or score manipulation is performed after fixture setup.
        report['stage'] = 'gameplay_case_parry'
        before = p.evaluate('__inputSmokeGame.prepareGameplay("parry")')
        require(before['active_cases'] == 1 and before['score'] == 0 and
                before['hurt_cooldown'] == 0, 'Case fixture did not initialize', before)
        p.keyboard.down('KeyX')
        parried = self.continuous()[0][-1]
        p.keyboard.up('KeyX')
        require(parried['active_cases'] == 0 and parried['score'] == 150 and
                parried['hurt_cooldown'] == 0 and parried['punch'] > 0,
                'Trusted punch failed to parry a real briefcase', parried)
        require(0.95 < parried['wt'] - before['wt'] <= 1.001,
                'Parry failed to award its bounded one-second reward', [before, parried])
        self.screenshot('13-gameplay-briefcase-parry')
        followup = self.continuous(6)[0][-1]
        require(followup['active_cases'] == 0 and followup['score'] == 150 and
                followup['hurt_cooldown'] == 0,
                'Parried briefcase respawned or repeated its impact', followup)
        p.evaluate('__inputSmokeGame.prepareGameplay("parry")')
        unguarded = self.continuous()[0][-1]
        require(unguarded['active_cases'] == 0 and unguarded['score'] == 0 and
                unguarded['hurt_cooldown'] > 0,
                'Unparried briefcase must still hurt the walker', unguarded)
        self.screenshot('14-gameplay-unparried-briefcase')
        report['gameplay']['case_parry'] = {
            'before': before, 'parried': parried, 'after_six_ticks': followup,
            'unprotected': unguarded}

    def run(self):
        p, report = self.page, self.report
        require(p.evaluate('__inputSmoke.rafRequests') == 1,
                'RAF must be disabled before the game schedules its first frame')
        p.evaluate(HARNESS)
        initial = self.prepare('tower')
        require(initial['chunks'] == 0 and initial['blocks'] == 58,
                'The input tower must stand before it is struck', initial)
        self.screenshot('01-supported-tower')
        launch = self.sling_drag(-70, 0)
        report['stage'] = 'launch_and_collapse'
        maximum_chunks, first_impact = 0, None
        for _ in range(40):
            frames = self.step(6)
            maximum_chunks = max(maximum_chunks, *(row['chunks'] for row in frames))
            if first_impact is None and any(row['chunks'] for row in frames):
                first_impact = frames[-1]
                self.screenshot('02-pointer-shot-collapse')
        settled = self.snapshot()
        self.screenshot('03-after-collapse')
        require(maximum_chunks > 0, 'Pointer-driven shot did not detach the support tower')
        require(settled['blocks'] < initial['blocks'], 'Pointer-driven shot caused no block damage')
        report['slingshot'] = {'fixture': 'custom wood support tower, production load/build/physics',
            'initial': initial, 'input': launch, 'first_impact': first_impact,
            'after_240_frames': settled, 'maximum_chunks': maximum_chunks}

        initial = self.prepare('chapter2')
        launch = self.sling_drag(-20, 0)
        report['stage'] = 'chapter2_ground_landing'
        # This weak horizontal shot lands well left of the building (x=464).
        # manStep may skip once; its next ground contact calls stick(), whose
        # CL.walk branch enters production walking. Never assign man.mode.
        landing_frames = 0
        while self.snapshot()['mode'] != 'walk' and landing_frames < 180:
            self.step()
            landing_frames += 1
        require(self.snapshot()['mode'] == 'walk', 'Chapter 2 launch never entered walking')
        landed = self.step(15)[-1]
        require(landed['grounded'] and landed['face'] == 1 and landed['turn'] == 0,
                'Chapter 2 must begin with a settled right-facing walker', landed)
        require(landed['x'] < 400, 'Walking fixture must land clear of the building', landed)
        self.screenshot('04-chapter2-walking')
        report['chapter2'] = {'initial': initial, 'input': launch,
            'landing_frames': landing_frames, 'landed': landed}

        report['stage'] = 'keyboard_countersteer'
        report['turn_checks'] = []
        for initial_face in (1, -1):
            key = 'ArrowRight' if initial_face == 1 else 'ArrowLeft'
            opposite = 'ArrowLeft' if initial_face == 1 else 'ArrowRight'
            p.keyboard.down(key)
            self.continuous(12, initial_face)
            p.keyboard.up(key)
            require(self.snapshot()['turn'] == 0, 'Ordinary turn failed to finish')
            for elapsed in (1, 6, 11):
                p.keyboard.down(opposite)
                before, a = self.continuous(elapsed, -initial_face)
                if initial_face == 1 and elapsed == 6:
                    self.screenshot('05-chapter2-rear-turn')
                p.keyboard.up(opposite)
                p.keyboard.down(key)
                p.keyboard.down('x')
                after, b = self.continuous(1, initial_face)
                p.keyboard.up('x')
                p.keyboard.up(key)
                # Twenty neutral ticks finish the turn and punch cooldown,
                # so the next punch is ready without mutating timer fields.
                released, c = self.continuous(20, initial_face)
                require(released[-1]['turn'] == 0, 'Released controls did not finish the turn')
                require(not released[-1]['keys']['left'] and not released[-1]['keys']['right'],
                        'Real keyup left a direction held', released[-1])
                require(after[-1]['punch'] > .19, 'Countersteer tick did not accept a ready punch')
                report['turn_checks'].append({'initial_face': initial_face,
                    'interrupt_elapsed': elapsed, 'turn_before': before[-1]['turn'],
                    'turn_after': after[-1]['turn'], 'maximum_delta_radians': max(a + b + c),
                    'settled_yaw': released[-1]['yaw'], 'immediate_punch': True})
        for dwell in (1, 2, 6):
            maximum_delta = 0
            for change in range(24):
                key = 'ArrowLeft' if change % 2 == 0 else 'ArrowRight'
                p.keyboard.down(key)
                _, deltas = self.continuous(dwell, -1 if change % 2 == 0 else 1)
                maximum_delta = max(maximum_delta, *deltas)
                p.keyboard.up(key)
            released, deltas = self.continuous(12)
            require(released[-1]['turn'] == 0, 'Repeated direction changes failed to settle')
            report['turn_checks'].append({'rapid_dwell': dwell, 'alternations': 24,
                'maximum_delta_radians': max(maximum_delta, *deltas), 'settled_turn': 0})

        report['stage'] = 'keyboard_jump_and_land'
        p.keyboard.down('Space')
        jumped = self.continuous()[0][-1]
        p.keyboard.up('Space')
        require(jumped['vy'] < 0 and not jumped['grounded'], 'Real Space key did not jump', jumped)
        for frames_to_land in range(1, 121):
            landing = self.continuous()[0][-1]
            if landing['grounded']:
                break
        require(landing['grounded'], 'Jump failed to land within its bounded window', landing)
        report['chapter2']['jump'] = {'airborne': jumped, 'landing': landing,
                                    'frames_to_land': frames_to_land}

        report['orientations'] = []
        for width, height, label in ((390, 844, 'portrait'), (960, 540, 'landscape')):
            report['stage'] = 'rotate_to_' + label
            button = p.evaluate('__inputSmokeGame.leftButton()')
            p.mouse.move(button['x'], button['y'])
            p.mouse.down()
            require(self.snapshot()['keys']['virtualLeft'], 'Walk HUD pointer did not hold left')
            self.continuous(1, -1)
            before = self.snapshot()
            p.set_viewport_size({'width': width, 'height': height})
            after = p.evaluate('__inputSmokeGame.rotate()')
            self.report['last_state'] = after
            require(not after['invalid'], 'Rotation produced non-finite state', after)
            require([after['x'], after['y'], after['mode']] ==
                    [before['x'], before['y'], before['mode']],
                    'Layout-only rotation moved the physics actor', [before, after])
            require(after['world'] == before['world'] == [120, 60, 960, 540]
                    and after['blocks'] == before['blocks'], 'Rotation changed the physics world')
            require(after['portrait'] == (label == 'portrait') and
                    after['camera']['on'] == (label == 'portrait'), 'Wrong orientation/camera mode', after)
            if label == 'portrait':
                require_walk_camera(after)
                require(after['camera']['z'] == 1.8 and after['camera']['hold'] == 0,
                        'Walking rotation inherited the distant aiming view or hold', after)
            require(after['touches'] == 0 and not after['keys']['virtualLeft'],
                    'Orientation handler left a virtual movement key held', after)
            p.mouse.up()
            x, y, w, h = after['css']
            require(x >= -1 and y >= -1 and x + w <= width + 1 and y + h <= height + 1,
                    'Canvas escaped the resized viewport', after)
            p.keyboard.down('ArrowRight')
            resumed = self.continuous(18, 1)[0][-1]
            p.keyboard.up('ArrowRight')
            self.continuous(12)
            require(resumed['x'] > after['x'], 'Keyboard movement did not resume after rotation')
            self.screenshot('06-chapter2-' + label)
            observation = {'orientation': label, 'before': before,
                           'layout_only': after, 'resumed': resumed}
            if label == 'portrait':
                report['stage'] = 'portrait_camera_jump_and_land'
                p.keyboard.down('Space')
                jumped = self.continuous()[0][-1]
                p.keyboard.up('Space')
                require(jumped['vy'] < 0 and not jumped['grounded'],
                        'Portrait camera check did not begin a real jump', jumped)
                airborne = self.continuous(17)[0][-1]
                self.screenshot('07-chapter2-portrait-jump')
                for frames_to_land in range(1, 121):
                    landing = self.continuous()[0][-1]
                    if landing['grounded']:
                        break
                require(landing['grounded'], 'Portrait camera jump did not land', landing)
                require(landing['camera']['z'] == 1.8,
                        'Portrait camera lost readable walking scale during the jump', landing)
                observation['camera_jump'] = {'airborne': airborne, 'landing': landing,
                    'frames_to_land_after_capture': frames_to_land,
                    'physical_body_canvas_px': 38 * landing['camera']['z'],
                    'physical_body_css_px': 38 * landing['camera']['z'] * h / after['canvas'][1]}
            report['orientations'].append(observation)

        self.gameplay_checks()
        report['events'] = p.evaluate('__inputSmoke.events')
        report['raf_requests'] = p.evaluate('__inputSmoke.rafRequests')
        require(report['raf_requests'] == 1, 'The disabled display scheduler unexpectedly ran')
        for event in ('keydown:trusted', 'keyup:trusted', 'pointerdown:trusted', 'pointerup:trusted'):
            require(report['events'].get(event, 0) > 0, 'Missing real Chromium input: ' + event)
        require(not report['page_errors'] and not report['console_errors'], 'Browser runtime errors')
        report['final'] = self.snapshot()
        report['stage'] = 'complete'


def capture_failure(report, page, out_dir, error):
    report['status'] = 'fail'
    report['error'] = str(error)
    report['traceback'] = traceback.format_exc()
    if page is None:
        report['failure_artifacts'] = 'Browser did not create a page; screenshot unavailable.'
        return
    try:
        report['failure_state'] = page.evaluate(
            '() => window.__inputSmokeGame ? __inputSmokeGame.snapshot() : {url:location.href}')
    except Exception as diagnostic_error:
        report['failure_state_error'] = str(diagnostic_error)
    try:
        page.screenshot(path=str(out_dir / 'failure.png'), timeout=5000)
        report['screenshots'].append('failure.png')
    except Exception as screenshot_error:
        report['failure_screenshot_error'] = str(screenshot_error)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out-dir', type=Path, default=ROOT / 'out' / 'browser-input-smoke')
    parser.add_argument('--chromium', default=os.environ.get('CHROMIUM_EXECUTABLE') or None,
                        help='Executable path; otherwise use Playwright Chromium')
    args = parser.parse_args()
    args.out_dir.mkdir(parents=True, exist_ok=True)
    report = {'status': 'running', 'stage': 'browser_launch', 'seed': '0x51a7c0de',
              'frames_checked': 0, 'frame_limit': MAX_FRAMES, 'screenshots': [],
              'page_errors': [], 'console_errors': [], 'console_warnings': []}
    page = None
    try:
        from playwright.sync_api import sync_playwright
        with sync_playwright() as playwright:
            browser = None
            try:
                options = {'headless': True, 'timeout': 30000}
                if args.chromium:
                    options['executable_path'] = args.chromium
                browser = playwright.chromium.launch(**options)
                report['chromium'] = browser.version
                page = browser.new_page(viewport={'width': 960, 'height': 540}, device_scale_factor=1)
                page.set_default_timeout(10000)
                page.set_default_navigation_timeout(15000)
                page.on('pageerror', lambda error: report['page_errors'].append(str(error)))

                def console(message):
                    if message.type == 'error':
                        report['console_errors'].append(message.text)
                    elif message.type == 'warning':
                        report['console_warnings'].append(message.text)

                page.on('console', console)
                page.add_init_script(INIT)
                report['stage'] = 'page_load'
                page.goto((ROOT / 'web' / 'index.html').as_uri(), wait_until='load')
                Smoke(page, args.out_dir, report).run()
                report['status'] = 'pass'
            except Exception as error:
                capture_failure(report, page, args.out_dir, error)
            finally:
                if browser is not None:
                    browser.close()
    except Exception as error:
        if report['status'] != 'fail':
            capture_failure(report, None, args.out_dir, error)
    finally:
        path = args.out_dir / 'report.json'
        path.write_text(json.dumps(report, indent=2, allow_nan=False) + '\n')
        summary = {'status': report['status'], 'stage': report['stage'],
                   'frames_checked': report['frames_checked'], 'report': str(path),
                   'page_errors': report['page_errors'], 'console_errors': report['console_errors']}
        if report.get('error'):
            summary['error'] = report['error']
        print('BROWSER_INPUT_SMOKE_' + report['status'].upper(), json.dumps(summary), flush=True)
    return 0 if report['status'] == 'pass' else 1


if __name__ == '__main__':
    sys.exit(main())
