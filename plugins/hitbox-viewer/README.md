Hitbox Viewer draws what the game actually tests for collisions, on top of the match.

- **Red** capsules are hitboxes. Each is drawn from where it was last frame to where it is now, because the game checks that whole swept shape.
- **Purple** capsules are grab boxes.
- **Yellow** capsules are hurtboxes: the parts of a fighter that can be hit.
- **Green** hurtboxes are invincible: hits connect but do nothing, as in the first frames of a ledge grab.
- **Blue** hurtboxes are intangible: hits pass straight through, as in rolls, spot dodges and air dodges.

Press **Numpad1** to show or hide the boxes (F3 belongs to Melee Unlocked's Lab view). You can change the key, turn either kind off, or make them fainter in the plugin's settings.

It pairs well with Training Lab: pause with F5 and step with F6 to see exactly which frame a move comes out, or where a dodge stops being intangible.

The boxes are projected with the game's own camera, so they follow zooms and camera shakes. The plugin only reads game memory; it changes nothing.
