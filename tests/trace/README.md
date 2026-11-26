# Render regression check (PC, no emulator)

Checks that a change to cfl-mii draws the same Miis as upstream cfl-mii, for thousands of Miis in a few seconds.

```sh
tests/trace/run.sh /path/to/CFL_Res.dat [count] [seed] [-- cfl-trace options]
```

`CFL_Res.dat` is Nintendo's Mii resource: dump it from your own console and keep it out of the repository. `count`
defaults to 5000 and `seed` to 1. The check needs git and a C compiler (`$CC`, default `cc`).

## What it does

1. Takes upstream cfl-mii at a fixed commit (`UPSTREAM` in `run.sh`) with `git archive`, and builds the harness
   (`harness.c`) against it and against this working tree.
2. Runs every build on the same Miis: random ones from the seed, each with random build settings (expressions,
   sometimes all of them; valid and invalid resolutions; hair mode; icon size, expression and background).
3. Compares the builds Mii by Mii. The working tree runs twice: through the original API (`CFL_InitCharModel`,
   `CFL_CommandMakeModelIcon`), and through the per-frame API (`CFL_BeginCharModel`, `CFL_StepCharModel`,
   `CFL_DrawModelIcon`) when it has one (`CFL_HAS_FRAME_STEPS`).

On a mismatch it names the first Mii that differs and prints where the two event lists of that Mii part. The full lists
are in the work folder it names. Set `CFL_TRACE_WORK` to choose that folder.

## How it compares without a GPU

cfl-mii is compiled against a stand-in for libctru and citro3d (`fake/`). Nothing is rasterised. Instead:

- every texture has a content hash. An upload sets it from the data, and a render-target clear sets it from the clear
  values. Each draw into a target folds a description of that draw into the target's hash;
- a draw's description holds everything that decides its pixels on the 3DS:
  - the target's size, the viewport, culling, depth test and blending;
  - the attribute loaders, and the vertex and index data the draw reads;
  - the six texture combiners, and every texture they sample: its size, format, filters, wraps and content hash;
  - the fragment lighting (material, ambient, lookup tables, lights) and the shader's matrices;
- frame boundaries are not part of it. Neither are the times targets are created and deleted, nor the order in which
  state was set between draws: none of these change the pixels.

So an icon's hash covers every draw into it, and, through the textures it samples, every draw into the face and mask
textures before it. Each Mii's output line is:

```
index  built+drawn  icon  draws  files  prints  draw-count
```

- `icon`: the hash of the icon texture's content.
- `draws`: every draw in order.
- `files`: the writes to `CFL_DB.dat`, the recently-seen list.
- `prints`: what cfl-mii printed.

One case is outside the check: a buffer changed after its draw call but before the GPU runs the frame. The stand-in
reads buffers at the draw call. cfl-mii fills every buffer before drawing with it.

## Options (after `--`)

- `--real CFL_DB.dat` builds a console's own Miis and its recently-seen ones first.
- `--db CFL_DB.dat` gives a database for cfl-mii to read and write. It is kept in memory and never written to disk.
  Without it the harness makes an empty one, so every Mii goes through the recently-seen write.
- `--ubll ubll.lst` gives a blocklist. Upstream asks for 16,000 bytes of what is a 12,000-byte file, so it never applies
  the blocklist; this tree reads 12-byte entries. With a list that blocks a Mii you are building, the two differ, as
  intended.
- `--count`, `--seed` and `--frames` are set by `run.sh`. `--dump I` prints every event of Mii `I` as text.

`harness.c` documents the same options.
