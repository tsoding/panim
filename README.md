# Panim

Programming Animation Engine. Heavily inspired by (but not based on) [Manim](https://github.com/3b1b/manim)

## Quick Start

```console
$ cc -o nob nob.c
$ ./nob
$ ./build/panim ./build/libplug.so
```

## Architecture

The whole engine consists of two parts:
1. panim executable - the engine itself
2. the animation dynamic library (a.k.a. `libplug.so`) that you as the user of Panim develops

Panim the Executable enables you to control your animation: pause it, replay it, and, most importantly, render it into the final video with FFmpeg. It also allows you to dynamically reload the animation library without restarting the whole Engine which improves the feedback loop during the development of the animation.

### Assets vs State

While developing your animation dynamic library it's good to separate your things into 2 lifetimes:

1. Assets - things that never change throughout the animation, but reloaded when the `libplug.so` is reloaded
2. State - things that survive the `libplug.so` reload, but are reset on `plug_reset()`.

You can safely assume that string literals reside in the Assets lifetime. So if a string literal cross a "lifetime boundary" from Asset to State it has to be copied to an appropriet region of memory. Something like an arena works well here.
