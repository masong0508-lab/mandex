# Demolition Contracts — expansion milestone

- 16 new playable missions appended at indices 60–75. The original 60 maps keep their IDs; the first three Contracts are available separately, and further contracts follow star-based progression on the locked builds.
- 120 authored rooms, 118 landlord placements, 6,994 material cells before physics settling, 253 builder operations, two walking missions, four pads and two tubes. Every contract has a tactical hint, briefing, and two charges of a recommended power.
- Browser: dedicated Contracts tab, cached schematic preview, briefing and score/stars display. Only the selected map is constructed; the original 120×60 grid and existing native pool limits remain.
- Original soundtrack: Permit Panic (26.67 s), Concrete Shuffle (30 s), Final Notice Funk (24 s). Three different 16-bar arrangements, generated reproducibly from original note and percussion patterns. 3,557,532 bytes of mono 16-bit PCM audio; 80.67 seconds total.
- Audio is optional in the browser. Only the current track is loaded; leaving play, muting, and backgrounding release the audio. The standalone HTML and console ports retain their synthesized music. The full web ZIP includes the audio folder; the single HTML artifact remains playable without it.
- Saves: original version 2 format stays intact, new slots append by level count. The new 456-byte payload plus 12-byte journal fits the existing 512-byte sector budget.
- Browser source regressions passed for the new geometry and the previous story, power, orientation, flight, input, and performance behavior. Native full builds/actual playthrough, real-device FPS and listening QA are not yet certified. Audio checks validate the PCM format, level, distinctness and fallback, not subjective mix quality.
