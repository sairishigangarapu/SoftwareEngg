#!/usr/bin/env python3
"""Generate the benchmark corpus + manifest. Seeded => reproducible (except source/, see manifest)."""
import gzip, glob, hashlib, json, math, os, random, array, sys
root = os.path.join(os.path.dirname(__file__), "..", "datasets")
out = os.path.join(root, "samples"); os.makedirs(out, exist_ok=True)
rng = random.Random(1337)
entries = []

def write(cat, name, data, desc, gen):
    d = os.path.join(out, cat); os.makedirs(d, exist_ok=True)
    p = os.path.join(d, name); open(p, "wb").write(data)
    entries.append(dict(category=cat, path=f"{cat}/{name}", bytes=len(data),
                        sha256=hashlib.sha256(data).hexdigest(), description=desc, generator=gen))

words = [''.join(rng.choice("etaoinshrdlucmfwypvbgkjqxz") for _ in range(rng.randint(2, 9))) for _ in range(3000)]
weights = [1 / (i + 1) for i in range(len(words))]
text = (" ".join(rng.choices(words, weights, k=700_000))).encode()[:4_000_000]
write("text", "synthetic_zipf.txt", text, "Zipf-distributed pseudo-English", "seeded rng 1337")

src = b""
for f in sorted(glob.glob("/usr/include/*.h")):
    src += open(f, "rb").read()
    if len(src) > 2_000_000: break
if not src:  # fallback: this repo's own sources
    for f in sorted(glob.glob(os.path.join(root, "..", "include", "pc", "*.hpp"))): src += open(f, "rb").read()
write("source", "headers.c", src, "Concatenated C headers (system dependent!)", "/usr/include/*.h sorted")

n = 500_000
vals = array.array("d", (math.sin(i / 50) * 100 + rng.gauss(0, 1) for i in range(n)))
write("numeric", "float64_signal.bin", vals.tobytes(), "float64 sine + gaussian noise", "seeded rng 1337")
ints = array.array("I", (i * 4 + rng.randint(0, 3) for i in range(1_000_000)))
write("numeric", "uint32_table.bin", ints.tobytes(), "Mostly-increasing uint32 records", "seeded rng 1337")

write("compressed", "text.gz", gzip.compress(text, 6, mtime=0), "gzip -6 of the text sample", "gzip mtime=0")
write("compressed", "random.bin", rng.randbytes(2_000_000), "Incompressible random bytes", "seeded rng 1337")

json.dump(dict(seed=1337, files=entries), open(os.path.join(root, "manifest.json"), "w"), indent=2)
print(f"wrote {len(entries)} samples + manifest.json")
