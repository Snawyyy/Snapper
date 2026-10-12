# Analog and physical UI in games: notes for Snapper

Goal: find what makes game UI feel fun, physical and satisfying, then turn
it into cheap ideas for the Snapper animation editor (dark Win98 style, Teto
red accent, painted Qt widgets, no image files).

## How to read this

Most sources are reviews, fan wikis, developer interviews quoted second hand,
and talks we could only see as summaries. Where a number or quote comes from
one developer's test or a secondary write-up, the text says so. Nothing here
was checked by playing the games.

## Techniques

### 1. Liquid and mechanical gauges you read at a glance

- Diablo health globes: a red liquid drains and refills in a glass orb.
  Blizzard kept numbers off the orbs to avoid clutter, and placed them where
  the eye already is during a fight.
- Balatro: the chip counter rolls its digits like slot reels, so a score
  climbing feels like a machine winding up.
- Inscryption: golden scales on the table show damage, so the number has a
  weight you can see tip.

Why it works: a level you can read without reading text, and motion (liquid
sloshing, reels turning) that tells you it is alive.

### 2. Diegetic UI: information lives in the world

- Dead Space: health is a glowing segmented bar on Isaac's spine, and the
  RIG suit shows inventory and the weapon selector. The GDC 2013 talk "Crafting
  Destruction" by Dino Ignacio covers this; per a VRroom write-up, Ignacio
  said a normal overlay would be "a safe wall between you and the game".
- Metro 2033: a wristwatch tracks how long the gas mask filter lasts.
- Fallout 4: the Pip-Boy is a wrist computer you look at in first person.
- Death Stranding: the map appears on Sam's cuff link as a tilting 3D model.

Why it works: the player looks where the character would look, so there is no
glass between the player and the game. Critics note it can cost readability
(Sakurai criticised Death Stranding's small symbols and hold commands).

### 3. Desks, paper and documents

- Papers, Please: the whole screen is a booth desk. Passports, stamps and
  forms are objects you lift, inspect and put back. Lucas Pope built the
  deliberately clunky paperwork from his HyperCard background.
- Her Story: the search screen sits on a mock desktop with files you can open
  and close.
- Return of the Obra Dinn: a pocket watch ("Memento Mortem") replays a death,
  and a ledger book takes your notes.
- Tearaway: the papercraft look came from real cutting mats, and the pop-up
  UI matches the paper world.

Why it works: paper has a clear in tray and out tray, a stack you can shuffle,
and mistakes that feel like real mistakes. The player already knows how to use
a desk.

### 4. Cards, tokens and tabletop rules

- Inscryption: the whole game is a card table with a figurine on a parchment
  map, and the table is a skeuomorphic frame for the card rules (DiGRA paper
  "From Paper to Pixels").
- Disco Elysium: skill checks show a hover percentage, dice roll on screen,
  and the screen goes green or red on success or failure. Colour is never the
  only signal. Interactive objects get a green outline on hover, and Tab
  reveals them all.
- Resident Evil 4: the attache case is a grid you pack like Tetris. Items turn
  and fit into the empty squares.

Why it works: tabletop rules come with physical limits (space, turns, chance),
so the limit is part of the fun. Dice and falling cards add suspense.

### 5. Dials, knobs, switches and detents

- Teenage Engineering OP-1: four colour-coded rotary encoders whose job changes
  with the mode, next to a small screen with playful animations. Reviewers
  praise how the screen fits the hardware instead of pretending to be a panel.
- Force feedback on knobs: a 2024 study (Piao et al., Actuators) with 20
  musicians found a detent (click) mode improved pitch accuracy over no
  feedback, while a spring mode did not help. This was a musical task, not a
  general UI.
- Dead Space 3 bench: the analog stick worked like radio buttons, one choice
  at a time (secondhand).

Why it works: a detent gives a click per step, so the hand knows the value
without looking. Searches found little on rack-and-knob audio games; the OP-1
is the closest example found.

### 6. Juice: squash, stretch, overshoot, shake, hit-stop, particles, sound

- "Juice it or lose it" (Martin Jonasson and Petri Purho, 2012): a plain
  Breakout clone becomes fun after flash, shake, floating text, sound and
  particles are layered on.
- Vlambeer, "The art of screenshake" (Jan Willem Nijman, 2013): "fill your game
  with love and tiny details".
- Balatro: each scoring step stacks card motion, a rolling counter, screen
  effects, particles and sound. Cards flip with a spring bounce.
- Hit-stop: one developer found 0.05 to 0.1 seconds works best. Too short is
  not felt; too long reads as lag.
- Counterpoint: Folmer Kelly ("Don't Juice It or Lose It", GDC Europe) argues
  heavy polish can cost immersion. Big screen shake can make players queasy.

Why it works: a cause and effect chain you can see and hear. Each step
confirms the last, so the player learns the rules by feel.

### 7. Weight, inertia and spring

- Steve Swink, "Game Feel": defines game feel as real-time control of virtual
  objects in a simulated space, with interactions emphasised by polish. It
  studies words like floaty and heavy and how games produce them.
- Baba Is You: objects shake and react to death, which makes the rule blocks
  feel like they have weight.
- Mini Metro: hit boxes and element sizes were changed for touch, and pinch
  zoom was added so the map stays usable when it grows.
- Loop Hero: small menu animations are praised by reviewers.

Why it works: an object that eases in, overshoots and settles tells the hand
its mass without a number.

### 8. Sound on every action

- Papers, Please: the stamp thunk and the clacking shutter each day give the
  booth its feel (Game Developer and Ringer descriptions).
- Diablo: the pickup sound is a deep "ka-thoonk" that tells you a heal landed
  (fan and press description, not a developer quote).
- Rhythm games: an input with no sound feels dead; playtesters asked for it.
- Balatro: a rising audio ladder as cards score (illustrative, from a design
  write-up).

Why it works: sound confirms the action in the time the eye cannot track.

### 9. Diegetic and meta menus with personality

- Persona 5 and Metaphor: ReFantazio: each menu is built as its own unique
  design, which the director says is very costly to make. Some angular type
  was "impossible to read at first" (Hashino, as quoted by outlets).
- Disco Elysium: the interface acts like an unreliable narrator. The text
  column borrows from Twitter, and thoughts appear as coloured orbs.
- Zelda: Breath of the Wild shows UI only when needed and makes it stand out
  with animation. Hearts flash white when Link takes damage.

Why it works: a menu with its own voice makes browsing part of the mood.

### 10. Clarity first: minimal style and anticipation

- Mini Metro: a subway-map language with straight lines and bold colour.
  Dinosaur Polo Club's GDC 2017 talk "Mini Metro: When Less Is More" covers
  this.
- Hitman: the interface shows only what an agent needs (font analysis, low
  authority).
- Rhythm games: a metronome animation and pulsing buttons tell the player the
  beat before the note arrives.

Why it works: the player always knows where to look next, so the next action
feels earned.

## Why these feel satisfying

- Affordance: the shape says what it does (a stamp, a knob, a drawer).
- Immediate feedback: a click, flash or squash within a frame or two.
- Anticipation: a hover outline, a wind-up, a beat before the event.
- Rhythm: repeated actions with the same sound and motion become a groove.
- Weight: easing, overshoot and settle make a value feel like it has mass.
- Legible cause and effect: one step at a time, so the player sees why.

## Table

| Game | UI element | Analog metaphor | What makes it satisfying |
|---|---|---|---|
| Diablo | health globes | glass orb of liquid | liquid drains and refills at a glance |
| Balatro | chip counter | slot machine reels | digits roll and climb with each score |
| Inscryption | scales, card table | balance scale, tabletop | damage has visible weight |
| Dead Space | spine health bar, RIG | suit display | no screen wall, always in view |
| Metro 2033 | wristwatch | wrist watch | time and filter life in one glance |
| Fallout 4 | Pip-Boy | wrist computer | in-world, looked at in first person |
| Death Stranding | cuff link map | tilting 3D model | the map lives on the body |
| Papers, Please | booth desk | passports and stamps | lift, inspect, stamp, return |
| Her Story | search desktop | old police database | files open and close like a real PC |
| Obra Dinn | watch and ledger | pocket watch, book | replay on demand, notes written in |
| Tearaway | pop-up UI | cut paper | UI matches the paper world |
| Disco Elysium | dice, thought orbs | tabletop dice | suspense, colour plus text signals |
| Resident Evil 4 | attache case | packing a case | space limits make a puzzle |
| Teenage Engineering OP-1 | rotary encoders, screen | hardware synth | a click per step, a playful screen |
| Juice it or lose it | effects stack | game feel | many small layers add up to fun |
| Vlambeer | screenshake | impact | a small shake sells the hit |
| Baba Is You | object shakes | living blocks | rules have weight |
| Mini Metro | map lines | subway map | easy to read, touch sized |
| Persona 5 | menus | magazine layouts | each menu has a voice |
| Zelda BotW | heart flash | damage cue | UI shows only when needed |
| Rhythm games | beat pulse | metronome | the beat is seen before the note |

## Snapper ideas, ranked by value over effort

Cheap means painted Qt widgets, a spring or easing per value, and sounds made
in code. Each one follows the UX rules in CLAUDE.md (undo names the step,
greyed controls give a reason, Teto red for selected).

1. Sound pack for every action (value high, effort low). Six short sounds made
   in code: peg click, stamp thunk, detent tick, rewind whir, drawer slide,
   soft ding. Add a setting to mute. This gives "sound on every action" for
   little work.
2. Keys as pegs on a timeline board (value high, effort low). A key is a
   painted peg. Setting one drops it with a 1 or 2 frame squash and a click.
   Dragging slides it with a little spring. Matches "keys hold" by keeping the
   peg still until moved.
3. Playhead as a needle (value high, effort low). A painted needle or tape
   head with a small lag and overshoot when scrubbing, and a tick each time it
   crosses a frame.
4. Speed, zoom or volume as a detented dial (value medium-high, effort medium).
   A painted knob with detents, a value on its face, drag and wheel. Greyed out
   with a tooltip when it cannot act (for example, no audio track).
5. Undo as a rewind lever (value medium-high, effort low). A lever or stamp
   that depresses on press, with its label naming the step ("Undo Rotate
   head"). Greyed when there is nothing to undo.
6. Export progress as liquid (value medium, effort low). A glass tube with a
   wave line filling up, drawn with a clip path. Done can be a stamp.
7. Panels as pull-out drawers (value medium, effort medium). Panels slide in
   with a little overshoot and show a handle tab when closed. New drawers appear
   where they cause no jump.
8. Timeline as a film strip (value medium, effort high). Frame cells with
   doll thumbnails and sprocket edges; scrubbing turns the reel. Needs cached
   thumbnails, so do it after the others.
9. Hit-stop on key set (value medium, effort low). A 50 to 80 ms freeze of the
   stage preview and a small squash on the doll when a key is set. Test on the
   owner's machine before keeping it.
10. Ease as a rubber band (value medium, effort medium). Draw the ease curve
    as a band pulled between two pins beside the key; drag a pin to change it.

## Gaps

- Rack-and-knob audio games and Hades, Opus Magnum, Hitman and Cyberpunk
  returned only thin or secondary sources; no developer analysis was found.
- The Diablo "ka-thoonk" and the Balatro audio ladder are secondary
  descriptions, not developer statements.
- Sound design for Dead Space and Papers, Please was not found in developer
  sources.

## Sources

- Diablo health globes: https://diablo.fandom.com/wiki/Health_globes ;
  https://www.co-optimus.com/article/4709/love-your-little-globes-in-diablo-iii.html
- Dead Space: https://gdcvault.com/play/1017723/Crafting-Destruction-The-Evolution-of ;
  https://archive.vrroom.studio/?p=30438 ;
  https://www.hudsandguis.com/2012/08/22/dead-space-2-diegetic-interface-design
- Papers, Please: https://www.gamedeveloper.com/design/designing-the-bleak-genius-of-i-papers-please-i- ;
  https://mechanicsofmagic.com/2021/06/05/design-details-in-papers-please/
- Diegetic overview: https://www.gamedeveloper.com/design/user-interface-design-in-video-games
- Juice: https://rpgplayground.com/research-making-a-juicy-game/ ;
  https://bugnet.io/blog/game-feel-why-juice-matters-and-how-to-add-it
- Balatro: https://blakecrosley.com/guides/design/balatro
- Inscryption: https://dl.digra.org/index.php/dl/article/view/2474
- Disco Elysium: https://www.thesixthaxis.com/2021/04/14/disco-elysium-the-final-cut-review-ps5-ps4-stadia-pc/ ;
  https://80.lv/articles/disco-elysium-working-on-ui-design
- Her Story: https://mechanicsofmagic.com/2021/05/09/critical-play-her-story/
- Resident Evil 4: https://bloody-disgusting.com/video-games/3658221/resident-evil-4-perfected-inventory-system-resident-evil-25/
- Teenage Engineering OP-1: https://cdm.link/2010/01/17/teenage-engineerings-op-1-instrument-hands-on-videos-why-its-different/
- Knob detent study: https://doaj.org/article/ae2e7e0426c4436a95d2d8f88b16f08f
- Persona: https://www.gamespot.com/articles/persona-5s-menus-continue-to-look-stunning/1100-6439532/ ;
  https://theouterhaven.net/2024/10/persona-director-notes-that-beloved-in-game-feature-is-actually-annoying-to-make
- Mini Metro: https://gdconf.com/news/attend-gdc-2017-and-learn-the-value-of-less-is-more-from-a-mini-metro-dev ;
  https://www.indieorama.com/mini-metro-and-robert-curry-the-minimalism-was-one-of-the-things-that-made-the-game-hard-to-make
- Death Stranding: https://www.dualshockers.com/masahiro-sakurai-praises-death-stranding-hideo-kojima/
- Fallout Pip-Boy: https://www.usgamer.net/articles/history-evolution-fallout-pip-boy-obsidian-interplay-bethesda
- Tearaway: https://www.pocketgamer.biz/unfolding-story-the-making-of-tearaway/
- Zelda BotW: https://www.thefamicast.com/2017/12/part-2-immersive-impressionable-ui.html
- Swink, Game Feel: https://www.goodreads.com/book/show/42989445
- Obra Dinn: https://dukope.itch.io/return-of-the-obra-dinn
- Baba Is You: https://slugsites.ucsc.edu/hzhang92/2022/01/24/baba-is-you
- Dead Space overhaul critique: https://www.popmatters.com/107913-going-hudless-for-immersion-2496052745.html
