# Example TTD POV frames

PNG stills for the README from the default **640×360** adaptive dump.
The kill victim is a skinned glTF mesh; other players stay capsules.

Regenerate the interactive gallery:

```bash
./build/cyka2pp analyze testdata/demos/3835689269611987518.dem \
  --maps-dir ../cs2-maps-tri \
  --ttd-trace-dir testdata/ttd-traces \
  --format json --out /tmp/m.json
```

Then open `testdata/ttd-traces/index.html`.
