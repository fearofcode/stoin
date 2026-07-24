# Third-Party Notices

## Project License and Plover-Derived Code

Stoin is free software licensed under the GNU General Public License, version 2,
or (at your option) any later version. See `LICENSE.txt`.

Portions of `src/format.c` and `src/orthography.c` are C ports or adaptations of
Plover's formatting and English orthography implementations:

<https://github.com/opensteno/plover>

Those upstream files carry the following notice:

```text
Copyright (c) 2010-2011 Joshua Harlan Lifton. See LICENSE.txt for details.
```

Plover is distributed under the GNU General Public License, version 2 or (at
your option) any later version. Stoin's ports have been rewritten and modified.

## Norvig Word-Frequency Data

`stoin-dict-augment` downloads Peter Norvig's `count_1w.txt` word-frequency
file on demand from:

<https://www.norvig.com/ngrams/count_1w.txt>

The file is cached locally and is not distributed in this repository. Norvig
states that the data files are derived from the Google Web Trillion Word Corpus,
which was distributed by the Linguistic Data Consortium. His corpus page is:

<https://www.norvig.com/ngrams/>

## Lapwing Dictionaries

The bundled `lapwing-base.json` and `lapwing-commands.json` dictionaries are
derived from `aerickt/plover-lapwing-aio`:

<https://github.com/aerickt/plover-lapwing-aio>

Lapwing AIO is MIT licensed:

```text
MIT License

Copyright (c) 2024 Aerick

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```

## Vendored C Libraries

- `third_party/stb_ds.h` is Sean Barrett's `stb_ds.h`, public domain / MIT licensed; see the header for the full notice.
- `third_party/cjson` is cJSON; see `third_party/cjson/LICENSE` and `third_party/cjson/UPSTREAM`.
