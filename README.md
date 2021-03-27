# Rabbit game platform

## TODO
- [x] Input.
- [x] POC half-screen scrolling. Not really a Rabbit concern but I'm dying to see it. ...works great!
- [x] Eliminate rb_framebuffer, use rb_image for everything. Think it through.
- [x] Review blitting into alpha image. I've turned framebuffer into image broadly, but didn't look at this hard.
- [ ] Keyboard as input, connect vmgr and inmgr without a lot of drama.
- [ ] Cross-platform build scripts.
- [ ] Compile-time config file with optional unit flags, etc.
- [ ] Install scripts.
- [ ] More audio nodes.
- [ ] Default instrument set.
- [ ] Private song format? Could make it much smaller than MIDI.
- [ ] Drivers for MacOS
- [ ] Drivers for MS Windows
- [ ] Drivers for Raspberry Pi
- [ ] HTTP/WebSocket input driver -- "join in from your phone!"
- [ ] Global timing regulator
- [ ] Song store in synth. Or should we add a generic asset store for that?
- [ ] Consistently getting video tearing (glx), usually near the top of the window
- [ ] Pre-render grid in vmgr
- [ ] Render primitives
- [ ] Lighting
