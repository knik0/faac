# <img src="frontend/faac.ico" alt="FAAC" width="48" height="48" align="top" /> Freeware Advanced Audio Coder

FAAC is an open-source, dependency-free AAC encoder aimed at embedded and pipeline use cases where footprint and throughput matter as much as quality.

### Key features:

- MPEG-4 AAC-LC and HE-AAC v1 (SBR) profiles
- Sample rates from 8 kHz to 96 kHz, supporting mono up to 5.1 multichannel
- VBR and ABR rate control with a bit reservoir
- Advanced encoding tools: Dynamic block-switching, PNS, and TNS
- Flexible output options: Raw ADTS, ADIF bitstream

## Copyrights

FAAC is free software, licensed under the GNU Lesser General Public License (LGPL), version 2.1 or later:

```
FAAC - Freeware Advanced Audio Coder
Copyright (C) 1999-2001, Menno Bakker
Copyright (C) 2002-2017, Krzysztof Nikiel
Copyright (C) 2004, Dan Villiom P. Christiansen
Copyright (C) 2005-2026, Fabian Greffrath
Copyright (C) 2026, Nils Schimmelmann

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.
```

> **Important:** The use of this software may require the payment of patent royalties. You need to consider this issue before you start building derivative works. We are not warranting or indemnifying you in any way for patent royalities! **YOU ARE SOLELY RESPONSIBLE FOR YOUR OWN ACTIONS!**

## Compiling Instructions

1. Make sure you have recent versions of meson and ninja installed.
2. cd to FAAC source dir
3. Run:
   ```bash
   mkdir -p build
   cd build
   meson setup ..
   meson install
   ```

