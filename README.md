# Camera
A USB camera app for Linux.<br>
Built using V4L2 and FLTK.
## TODOs
- **Imporant:** Error hanldling.
- Decide a name.
- Camera controls.
  - Responsive scorllable UI.
  - More control types.
  - Load actual values, not default.
  - Maybe - verify values after setting to make sure they are set correctly?
- ~~Camera, format and resolution selection.~~
- Control FPS.
- Handle multiple camera formats.
- Optimize
  - Format -> RGB
  - RGB -> Scale
  - Scale -> Render
  - We can use OpenGL for RGB -> Scale -> Render?
- Camera shutter sound effect.
- Maybe: Video?