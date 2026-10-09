# Snapper UI inventory

Union of main and the worktrees. Tags say where a thing lives when it is
not on main:

- [main] on main.
- [cut] cut-markers branch (Video tab, cut markers, gap fill, clip
  transitions). video-editing is an earlier cut of the same Video tab;
  its differences are marked [video-editing].
- [link] link-movement branch.
- [warp] warp-animations branch.
- [secs] shot-length-seconds branch.
- [main only] on main but removed by cut-markers.

Untagged = on main and on every branch.

Shared rules seen everywhere: a control that cannot act is greyed with a
tooltip saying why; every spin box edit is one live undo step (typing or
arrows preview, the step lands when editing finishes); field values are
never rewritten while the field has focus; errors go to the status bar.

## 1. Main window (frame)

1. Main window > title bar > shows "<project>[*] (<path or 'not saved yet'>) - Snapper"; * when unsaved.
2. Main window > close button (X) > asks "Save the changes to X first?" (Save / Discard / Cancel) when unsaved.
3. Main window > status bar > shows problems from every panel for 6 s; rig canvas hints stay until cleared; "Autosave failed: ..." on autosave error.
4. Main window > mode tabs > Pose tab: shot strip, stage, keyframe timeline.
5. Main window > mode tabs > Rig tab: rig canvas and rig panel side by side.
6. Main window > mode tabs > Video tab [cut]: shot bin, reel preview, toolbar row, reel timeline; switches playback to the reel timeline.
7. Main window > mode tabs > entering Video hides Cast and Inspector docks, leaving restores them as they were [cut].
8. Main window > mode tabs > tabs sit in a fixed, non-hideable "Modes" toolbar [video-editing]; on cut-markers they sit above the pages in the central widget.
9. Main window > Cast dock > left dock, closable/floatable (Qt default).
10. Main window > Inspector dock > right dock, closable/floatable (Qt default), nesting enabled.
11. Main window > splitter Pose tab > stage over timeline (3:1), draggable divider.
12. Main window > splitter Rig tab > rig canvas beside rig panel (3:1), draggable divider.
13. Main window > startup > if an autosave exists: "Recover work" dialog (Yes recovers, No discards autosave).
14. Main window > playhead follow > while on the shot timeline, playing or stepping into a shot picks that shot.
15. Main window > clean exit > deletes the autosave; unsaved exit keeps it for recovery.

## 2. Menu bar

### File menu

16. File > New... (Ctrl+N) > confirm discard, then New project dialog.
17. File > Open... (Ctrl+O) > confirm discard, then file picker for Snapper projects.
18. File > Open recent > submenu, one entry per recent file (name, full path as tooltip); opens after confirm discard; greyed when empty.
19. File > Save (Ctrl+S) > saves; falls to Save as when never saved.
20. File > Save as... (Ctrl+Shift+S) > file picker, saves there.
21. File > Choose song... > picks a sound file (wav, mp3, flac, ogg, opus, m4a, aac) as the project song.
22. File > Remove song > clears the song; greyed "There is no song yet." when none.
23. File > Export... (Ctrl+E) > opens the Export dialog (non-modal, raised).
24. File > Quit (Ctrl+Q) > closes the window (with the save prompt).
25. File > any failure > warning message box with the reason.

### Edit menu

26. Edit > Undo (Ctrl+Z) > undoes; text names the step ("Undo Rotate head"); greyed with why when nothing to undo.
27. Edit > Redo (Ctrl+Y, Ctrl+Shift+Z) > redoes; named the same way; greyed with why.

### Play menu

28. Play > Play / pause (Space) > toggles playback on the active timeline.
29. Play > Next frame (Right, .) > steps forward one frame (or to next change in Animation mode).
30. Play > Previous frame (Left, ,) > steps back the same way.

## 3. New project dialog

31. New project > Name (line edit) > project name, default "Untitled".
32. New project > Size (combo) > 1920 x 1080 (YouTube), 1080 x 1920 (Shorts, TikTok), 1080 x 1080 (square), Custom.
33. New project > Width (spin, px, step 2) > canvas width; typing off-preset flips Size to Custom.
34. New project > Height (spin, px, step 2) > canvas height; same.
35. New project > OK > creates project; greyed "Give the project a name." or "Video sizes must be even numbers.".
36. New project > Cancel > closes.

## 4. Export dialog

37. Export > Format (combo) > MP4 video with the song / GIF, no sound; switching sets Size (GIF half, MP4 full) and swaps the file suffix.
38. Export > Range (combo) > Whole master track (or "Whole final video" when exporting the reel [cut]) / Loop range / Frames...
39. Export > From frame (spin) > first frame; row shown only for Frames...
40. Export > Up to frame (spin) > last frame (inclusive); row shown only for Frames...
41. Export > Size (combo) > Full / Half / Quarter size.
42. Export > File (line edit) > output path.
43. Export > Browse... > save picker with the right suffix, adds suffix if missing.
44. Export > progress bar > frames done of all.
45. Export > status label > "Exporting...", "Saved <path>", failure reason, "Cancelled; nothing was saved.".
46. Export > Export button > starts; greyed with why (busy, no loop range, last frame before first, no file, "Already exporting.").
47. Export > Close / Cancel export > closes when idle; cancels while running.
48. Export > while running > Format, Range, Size, File, Browse greyed.

## 5. Pose tab: shot strip (custom-drawn bar above the stage)

49. Shot strip > shot block > shows colour swatch, name, length "N f" (and " > <transition>" on [main only]); picked = light with orange border, focused = thicker border.
50. Shot strip > "+" block > click adds a shot after the focused one, picks it and seeks to it.
51. Shot strip > click shot > picks just that shot and seeks to its start.
52. Shot strip > Shift+click > adds shot to the pick.
53. Shot strip > Ctrl+click > toggles shot in the pick.
54. Shot strip > click on a shot already in a bigger pick (no drag) > picks just it.
55. Shot strip > drag shot body > reorders the shot (drop by position).
56. Shot strip > hover right edge > resize cursor.
57. Shot strip > drag right edge > lengthens/shortens every picked shot by the same frames; one undo step "Change shot length".
58. Shot strip > right-click shot > picks it if outside the pick, opens the shot menu.
59. Shot strip menu > Rename... > text dialog "Rename shot".
60. Shot strip menu > Background colour... > colour dialog, applies to all picked shots.
61. Shot strip menu > Transition into next > <kind> > 2/4/6/8 frames [main only] > sets the transition of picked shots.
62. Shot strip menu > Duplicate > duplicates picked shots.
63. Shot strip menu > Delete > removes picked shots.
64. Shot strip > Delete / Backspace > removes picked shots.
65. Shot strip > Ctrl+A > picks every shot.

## 6. Pose tab: stage (custom-drawn canvas)

Drawing:

66. Stage > empty state > "No shot here. Add one in the shot strip above.".
67. Stage > picked whole layer/doll > orange outline of its shape.
68. Stage > picked whole doll > yellow lean bar (tall, drag up/down) and swivel bar (wide, drag sideways).
69. Stage > picked piece > orange outline plus yellow pivot dot.
70. Stage > picked piece with warp grid > faint grid lines and small yellow dots.
71. Stage > drag-node dots > yellow ring around the dot.
72. Stage > dots that animate by themselves > yellow square around the dot [warp].
73. Stage > picked warp dot > orange ring, dashed reach ellipse, label "reach N.N".
74. Stage > IK tips of the focused doll > small yellow squares.
75. Stage > box select > dashed orange rectangle with light fill.

Mouse:

76. Stage > click thing > picks it (piece, or the whole doll if that doll is already picked whole); starts a move drag.
77. Stage > Shift+click thing > adds to pick, then drag moves.
78. Stage > Ctrl+click thing > adds and drag scales (right/up grows, left/down shrinks); Ctrl+click without drag on a picked thing toggles it out.
79. Stage > drag picked thing > moves every pick by the cursor amount; one undo "Move <what>".
80. Stage > Shift+drag > move locked to horizontal or vertical.
81. Stage > Ctrl+drag > scales every pick; one undo "Scale <what>".
82. Stage > drag lean bar > leans picked dolls; undo "Lean <doll>".
83. Stage > drag swivel bar > swivels picked dolls; undo "Swivel <doll>".
84. Stage > drag warp dot > pulls that grid point; undo "Warp <piece>".
85. Stage > click warp dot (no drag) > picks the dot (for wheel and D); click again drops it.
86. Stage > drag IK tip square > bends the chain; undo "Bend <chain>".
87. Stage > click empty space + drag > box select (replace); Shift adds; Ctrl removes.
88. Stage > click empty space (no drag) > clears the pick.
89. Stage > double-click thing > picks the whole doll/layer.
90. Stage > wheel > turns picked things (wheel up = counterclockwise); "Click a piece first, then turn it." if none.
91. Stage > Shift+wheel > fine turn.
92. Stage > wheel with a warp dot picked > grows/shrinks the dot's rubber reach; Shift = fine.
93. Stage > right-click > menu "Link movement to <whole thing>" and "Link movement to <piece>" for the thing under the cursor; greyed with why; tooltip says picked things copy its movement from this frame on [link].
94. Stage > right-click > "Unlink movement" when the pick is linked [link].

Keys:

95. Stage > Escape > cancels the drag; else drops the picked warp dot; else clears the pick.
96. Stage > Ctrl+A > picks all pieces of the focused doll, or every non-effect layer.
97. Stage > D > toggles "drags behind" on the picked warp dot; "Click a warp dot of a picked piece first." if none.

## 7. Pose tab: keyframe timeline (custom-drawn)

Drawing:

98. Timeline > step mode button (top-left) > "Scrub mode" / "Animation mode" toggle; tooltip explains arrow-key behaviour.
99. Timeline > ruler > frame ticks (seconds tall, 2s mid), numbers at a zoom-fitting step, area past shot end dimmed.
100. Timeline > song band > waveform of the song under the shot ("Song" / "No song").
101. Timeline > rows > Camera row, then one per layer; focused layer row highlighted.
102. Timeline > keys > diamonds (orange when picked); bar = hold, thin line = eased.
103. Timeline > playhead > orange line with a frame-number tag on the ruler.
104. Timeline > empty state > "No shot picked.".

Mouse:

105. Timeline > click / drag ruler or song band > scrubs the playhead.
106. Timeline > click row name/plain click on a row > focuses that layer.
107. Timeline > click key > picks the keys at that frame in the row and seeks there.
108. Timeline > Shift+click key > adds those keys.
109. Timeline > Ctrl+click key > toggles those keys.
110. Timeline > drag key > slides all picked keys by whole frames; one undo "Move keys".
111. Timeline > click empty row space > clears key pick and seeks there (Shift/Ctrl click keeps pick).
112. Timeline > drag empty space > box selects keys (replace; Shift adds; Ctrl removes).
113. Timeline > click below the rows > clears the key pick.
114. Timeline > double-click row at frame > keys the layer in place there; on Camera row keys the camera.
115. Timeline > wheel > scrolls frames.
116. Timeline > Ctrl+wheel > zooms frames.
117. Timeline > right-click key > picks it if outside pick, opens key menu.
118. Timeline key menu > Ease to next key > Hold (step) / Linear / Ease in / Ease out / Ease in and out.
119. Timeline key menu > Space evenly > On 1s / On 2s / On 3s / On 4s; greyed with why.
120. Timeline key menu > Copy keys.
121. Timeline key menu > Delete keys.
122. Timeline > right-click empty row space > empty menu.
123. Timeline empty menu > Paste keys here > greyed with why when nothing copied.
124. Timeline empty menu > Hold one frame longer > retimes that row from that frame by +1.
125. Timeline empty menu > Hold one frame shorter > retimes by -1.

Keys:

126. Timeline > Delete / Backspace > removes picked keys.
127. Timeline > Ctrl+C > copies picked keys.
128. Timeline > Ctrl+V > pastes at the playhead on the focused layer.
129. Timeline > Ctrl+A > picks every key of the shot.

## 8. Cast dock

130. Cast > "Doll library" title > shows "(N old doll(s) not converted)" with the problems as tooltip.
131. Cast > library list > Krita dolls found; notes "(in project)" or "(art updated)"; Shift/Ctrl multi-pick.
132. Cast > Import > imports picked dolls not yet in the project; reports rig parts dropped; greyed with why.
133. Cast > Reload > reloads art for picked dolls in the project; greyed "Import the doll first.".
134. Cast > Put on stage > adds picked dolls to the focused shot and picks them; greyed "Import the doll first." / "Add a shot first.".
135. Cast > "Layers (top first)" list > layers of the focused shot "name (kind)"; picking rows picks layers on stage; Shift/Ctrl multi-pick.
136. Cast > layer checkbox > shows/hides the layer (all picked layers when the row is picked).
137. Cast > Add (drop-down) > greyed "Add a shot first.".
138. Cast > Add > Picture... > image picker (png, jpg, jpeg, webp, bmp), adds picture layer.
139. Cast > Add > Text... > multi-line text dialog, adds text layer.
140. Cast > Add > Effect > Flash / Fill / Invert / Zoom punch / Halftone / Glitch / Posterize > adds an effect layer.
141. Cast > Copy > duplicates picked layers; greyed "Pick a layer first.".
142. Cast > Delete > removes picked layers; same greying.
143. Cast > Up > moves picked layers one up; same greying.
144. Cast > Down > moves picked layers one down; same greying.

## 9. Inspector dock

Which boxes show: no shot = hint "Add a shot in the strip above to start."; shot but no pick = Shot + Camera; pick = Pose + Motion (+ Layer if a layer is focused, + Saved poses if a doll, + Drag node if a warp dot is picked, + Dot animation [warp] with a dot, + Link [link] if linked).

145. Inspector > hint label > shown when there is no shot.

### Shot box

146. Shot > title > "Shot" or "Shot (N picked, changes go to each)"; whole box greyed "Add a shot first.".
147. Shot > Name (line edit) > renames on leaving the field; greyed "Pick one shot to rename it." with several picked.
148. Shot > Length (spin, frames) > shifts length of all picked shots; undo "Shot length".
149. Shot > Length seconds (spin, s, step 0.5, 2 decimals) > same length in seconds, rounded to nearest frame [secs].
150. Shot > Background (colour button) > colour dialog for all picked shots.
151. Shot > Into next (combo) > transition kind to next shot; greyed on last shot [main only].
152. Shot > Overlap (spin, frames) > transition length; greyed on last shot or for a cut [main only].

### Camera at playhead box

153. Camera > Look at x / y (spins, px) > camera centre key at playhead; undo "Move camera".
154. Camera > Zoom (spin) > camera zoom key.
155. Camera > Turn (spin, deg) > camera rotation key.
156. Camera > Shake (spin, px) > camera shake key.
157. Camera > whole box > greyed "Move the playhead onto a shot.".

### Pose at playhead box

158. Pose > title > "Pose at playhead" or "(N picked, changes add to each)"; greyed "Pick a piece or layer on the stage first.".
159. Pose > Move x / y (spins, px) > offset; change is added to every pick; undo "Pose".
160. Pose > Scale x / y (spins) > scale.
161. Pose > Turn (spin, deg) > rotation.
162. Pose > Skew (spin, deg, +-85) > skew.
163. Pose > Opacity (spin, 0-1) > opacity.
164. Pose > Lean (spin, deg) > doll lean; greyed "Double-click a doll to pick it whole first.".
165. Pose > Swivel (spin, deg) > doll swivel; same greying.
166. Pose > Drawing (combo) > "Rig default" or a named drawing swap at playhead; greyed "This piece has only one drawing.".

### Drag node box (picked warp dot)

167. Drag node > Drags behind (D) (checkbox) > makes the dot trail and bounce; greyed until a warp dot is picked.
168. Drag node > Lag (spin, 0-1) > how far it trails; greyed until Drags behind is ticked.
169. Drag node > Bounce (spin, 0-1) > how much it wobbles; same.

### Dot animation box [warp]

170. Dot animation > Moves (combo) > None / Wave / Pulse / Breathe / Sway / Shiver; greyed until a warp dot is picked.
171. Dot animation > Size (spin, px) > how far the dot moves; greyed "Pick how the dot moves first.".
172. Dot animation > Cycle (spin, "every N fr") > frames per cycle.
173. Dot animation > Direction (spin, deg, wraps) > move direction; greyed for Shiver.
174. Dot animation > Delay (spin, 0-1) > phase offset in its cycle.

### Layer box (focused layer)

175. Layer > whole box > greyed "Pick a layer first.".
176. Layer > Name (line edit) > renames layer on leaving the field.
177. Layer > Frames "from" (spin) > layer start frame; undo "Change layer timing".
178. Layer > Frames "for" (spin, 0 = "to the end") > layer length.
179. Layer > Mirrored (checkbox) > flips picked dolls (doll layers only).
180. Layer > Text: Words (multi-line) > text, lands on leaving the field (text layers).
181. Layer > Text: Size (spin, px) > font size; undo "Edit text".
182. Layer > Text: Bold (checkbox) > bold.
183. Layer > Text: Colours fill (colour button, alpha) > fill colour.
184. Layer > Text: Colours outline (colour button, alpha) > outline colour.
185. Layer > Text: Outline (spin, px) > outline width.
186. Layer > Effect: Kind (combo) > effect type (effect layers).
187. Layer > Effect: colour (colour button) > effect colour.
188. Layer > Effect: Strength (spin, 0-1) > keyed at the playhead; undo "Effect strength".

### Motion box

189. Motion > Loop (combo) > Bob / Bounce / Shake / Nod / Sway preset.
190. Motion > Amount (spin, default 10) > pixels for moves, degrees for turns.
191. Motion > Timing (combo) > On 1s / On 2s (default) / On 3s.
192. Motion > Length (spin, frames, default 24) > loop length.
193. Motion > Add at playhead (button) > lays the loop keys on picked tracks; greyed "Pick a piece or layer on the stage first.".

### Link box [link]

194. Link > "Follows <leader> from frame N." label > what the pick copies movement from.
195. Link > Strength (spin, 0 to max) > how much movement is copied (1 same, 0.5 half, 2 double); undo "Link strength".
196. Link > Unlink (button) > removes the link from picked things.

### Saved poses box (doll picked)

197. Saved poses > list > saved pose names.
198. Saved poses > name field > placeholder "Name for a pose".
199. Saved poses > Save > saves the focused doll's pose under the name; greyed (load error / "Type a name first." / "Pick a doll on the stage first.").
200. Saved poses > Use > applies picked pose to picked dolls at playhead; greyed "Pick a saved pose first." / no doll.
201. Saved poses > Forget > deletes picked pose; greyed "Pick a saved pose first.".

## 10. Rig tab: rig canvas (custom-drawn)

Drawing:

202. Rig canvas > empty state > "Import a doll from the Cast panel to rig it.".
203. Rig canvas > doll at rest on white canvas, yellow joint dots (bigger when picked), dashed yellow bone lines to parents, orange outline on picked pieces, yellow squares for IK tips.
204. Rig canvas > hang mode > dashed orange line from the joint to the cursor, crosshair cursor.
205. Rig canvas > box select > dashed orange rectangle.

Mouse:

206. Rig canvas > click piece > picks it.
207. Rig canvas > Shift+click piece > adds to pick.
208. Rig canvas > Ctrl+click piece > toggles in pick.
209. Rig canvas > drag joint dot > moves its pivot (all picked joints if it is picked); undo "Move pivot of X" / "Move N joints".
210. Rig canvas > drag IK tip square > moves the chain tip; undo "Move IK tip of X".
211. Rig canvas > drag empty space > box select (replace; Shift adds; Ctrl removes); click empty = clear.
212. Rig canvas > double-click joint > enters hang mode; status hint "Click the part X hangs from; click empty space to make it a root; Escape cancels.".
213. Rig canvas > click in hang mode > picked pieces hang from the clicked piece, or become roots on empty space.

Keys:

214. Rig canvas > Escape > leaves hang mode; else cancels a drag; else clears the pick.
215. Rig canvas > Ctrl+A > picks every piece of the doll.

## 11. Rig tab: rig panel

216. Rig panel > doll combo > picks which doll to rig.
217. Rig panel > pieces list > indented tree of pieces; Shift/Ctrl multi-pick; synced with canvas pick.
218. Rig panel > Joint: Hangs from (combo) > "(nothing: a root piece)" or a parent, for all picked; greyed "Pick a piece first.".
219. Rig panel > Joint: Rest turn (spin, deg) > rest rotation shift for all picked; undo "Rest turn".
220. Rig panel > Joint: Draw order (spin, -1000..1000) > higher draws in front; undo "Restack".
221. Rig panel > Joint: Drawing (combo) > "As in Krita" or default drawing.
222. Rig panel > Keep shape when leaning (checkbox) > piece never squashes on lean/swivel.
223. Rig panel > Warp: Bend with a grid (checkbox) > turns on a warp grid for picked pieces.
224. Rig panel > Warp: Cells across / down (spins) > grid size; undo "Warp grid"; greyed "Tick "Bend with a grid" first.".
225. Rig panel > Copy to other side (button) > mirrors picked pieces' rig to their twins; greyed with why.
226. Rig panel > Copy whole side (button) > mirrors the whole side; greyed with why.
227. Rig panel > IK chains list > chains of the doll.
228. Rig panel > Add for piece (button) > adds IK chain "IK <piece>"; greyed "A chain needs the piece to hang from one.".
229. Rig panel > Flip bend (button) > flips chain bend direction; greyed "Pick an IK chain first.".
230. Rig panel > Remove (button) > removes the chain; same greying.
231. Rig panel > Save rig to library (button) > writes the rig back to the doll library; greyed "Import a doll first.".
232. Rig panel > hint label > "Drag a yellow joint to move it. Double-click a joint, then click the part it hangs from. Drag a square to move an IK tip.".

## 12. Video tab [cut]

### Toolbar row

233. Video > Import video... > picks video files, lays them end to end on the bottom track, picks them.
234. Video > Split at playhead > splits picked clips; greyed with why.
235. Video > Mark cut (M, works while playing, never takes focus) > drops or removes a cut marker at the playhead; clips snap to markers [cut only, not video-editing].
236. Video > Add track > adds a track on top; greyed with why.

### Shot bin (list)

237. Shot bin > list of shots with thumbnail, name and length in seconds.
238. Shot bin > drag a shot > onto a reel track to add it as a clip (tooltip "Drag a shot onto a track below.").

### Reel preview (canvas)

239. Reel preview > shows the final video at the reel playhead; empty text "Drag shots from the list, or a video file, onto the tracks below.".

### Reel timeline (custom-drawn)

Drawing:

240. Reel timeline > ruler > m:ss labels; corner shows playhead time.
241. Reel timeline > track headers > V1, V2, ...
242. Reel timeline > clips > video vs shot colours, shot colour swatch, name (file name, shot name, or "Shot removed"), hatched part where a shot is now shorter.
243. Reel timeline > picked clip > orange border with yellow trim ends.
244. Reel timeline > cut markers > yellow notch on ruler with dashed line down [cut only].
245. Reel timeline > joints between touching clips > diamond on the cut; shaded mix span when a transition is set [cut only].
246. Reel timeline > drop preview > dashed yellow outline of where a drop lands.
247. Reel timeline > playhead > orange line.

Mouse:

248. Reel timeline > click / drag ruler > seeks the reel playhead.
249. Reel timeline > click clip > picks it (plain click on a picked clip keeps the group until release, then picks just it).
250. Reel timeline > Shift+click clip > adds to pick.
251. Reel timeline > Ctrl+click clip > toggles in pick.
252. Reel timeline > drag clip body > moves all picked clips in time and across tracks (up = higher), snapping to clip edges, markers, playhead; undo "Move clip(s)".
253. Reel timeline > hover clip end > resize cursor.
254. Reel timeline > drag clip end > trims start or end, snapping; undo "Trim clip".
255. Reel timeline > drag empty space > box select (replace; Shift adds; Ctrl removes); click empty = clear pick.
256. Reel timeline > hover joint > tooltip "Double-click to pick the transition here." [cut only].
257. Reel timeline > double-click joint > Transition dialog [cut only].
258. Reel timeline > double-click track space > fill the gap between two markers: uses the video there or asks for a file, then Fill the gap dialog; "Mark a cut on both sides of the gap first (M)." if no gap [cut only].
259. Reel timeline > drop shot from bin > adds a shot clip at the snapped frame on that track, picks it.
260. Reel timeline > drop video file(s) > adds each one after the other on that track, picks them.
261. Reel timeline > wheel > scrolls time.
262. Reel timeline > Ctrl+wheel > zooms around the mouse.
263. Reel timeline > right-click clip > picks it if outside pick, opens clip menu.
264. Reel timeline > right-click empty track > track menu.

Clip menu (also top of track menu), each greyed with why:

265. Clip menu > Split at playhead (S).
266. Clip menu > Copy (Ctrl+C) [cut only].
267. Clip menu > Cut (Ctrl+X) [cut only].
268. Clip menu > Paste at playhead (Ctrl+V) [cut only].
269. Clip menu > Duplicate (Ctrl+D) [cut only].
270. Clip menu > Remove (Delete).
271. Clip menu > Ripple remove (Shift+Delete) > removes and closes the gap [cut only].
272. Track menu > Add track on top.
273. Track menu > Remove track Vn > greyed with why.

Keys (a key that cannot act reports why in the status bar):

274. Reel timeline > S > split picked clips at playhead.
275. Reel timeline > Delete / Backspace > remove picked clips.
276. Reel timeline > Shift+Delete / Shift+Backspace > ripple remove [cut only].
277. Reel timeline > Ctrl+C / Ctrl+X / Ctrl+V > copy / cut / paste at playhead [cut only].
278. Reel timeline > Ctrl+D > duplicate picked clips [cut only].
279. Reel timeline > Ctrl+A > picks every clip.
280. Reel timeline > Escape > cancels a drag; else clears the clip pick.

## 13. Transition dialog [cut]

281. Transition > Kind (combo) > Cut / Swipe left / Swipe right / Swipe up / Swipe down / Flash / Crossfade.
282. Transition > Length (spin, frames) > mix length before the cut, max fits the clips; greyed "A cut has no length."; default 4 when coming from a cut.
283. Transition > OK / Cancel > applies or drops.

## 14. Fill the gap dialog [cut]

284. Fill the gap > picture > first frame of the gap from the chosen video ("No picture" / "A shot").
285. Fill the gap > start slider > which part of the video plays in the gap; greyed "That video is shorter than the gap.".
286. Fill the gap > label > "Plays m:ss to m:ss of m:ss".
287. Fill the gap > Other video... > picks a different video file.
288. Fill the gap > OK > fills the gap; greyed when the video is too short.
289. Fill the gap > Cancel > closes.

## 15. Small system dialogs

290. Unsaved changes > Save / Discard / Cancel.
291. Recover work > Yes / No.
292. Rename shot > text input.
293. Add text > multi-line input "Words (Enter for a line break)".
294. Colour pickers > shot background (no alpha), text fill/outline and effect colour (with alpha).
295. File pickers > Open project, Save project, Choose song, Add picture, Import video, Fill the gap from, Export to.

## Counts per screen

- Main window frame: 15
- Menu bar (File, Edit, Play): 15
- New project dialog: 6
- Export dialog: 12
- Shot strip: 17
- Stage: 32
- Keyframe timeline: 32
- Cast dock: 15
- Inspector: 57
- Rig canvas: 14
- Rig panel: 17
- Video tab (toolbar, bin, preview, reel timeline): 48
- Transition dialog: 3
- Fill the gap dialog: 6
- System dialogs: 6
- Total: 295
