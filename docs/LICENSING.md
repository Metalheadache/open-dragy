<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- Copyright 2026 Yizhan Wu -->
# Licensing

Copyright 2026 Yizhan Wu. Open Dragy's original contributions are licensed under **MPL-2.0**. The complete, unmodified text is in [LICENSE](../LICENSE) and [LICENSES/MPL-2.0.txt](../LICENSES/MPL-2.0.txt).

## File-level terms

| Material | Terms |
| --- | --- |
| Decoder, receiver clock, geometry, speed and drag calculations, rate configuration, consistency wrapper, tests, tools, build files and documentation | MPL-2.0 |
| `include/dragy_ref/ble_session.hpp` and `examples/esp32/main/main.cpp` | MPL-2.0 AND Apache-2.0: local contributions under MPL; retained upstream portions under Apache |
| Standard license texts | Reproduced unmodified |
| External SDKs and dependencies | Their original licenses; source is fetched separately |

The [license map](../licensing-map.json) records every packaged file. The mixed-file expression is conjunctive, not an option to take all local contributions under Apache alone. Independent Apache rights in original upstream material remain intact. Preserve [NOTICE](../NOTICE), applicable copyright credit and the Apache license text when redistributing those portions.

MPL requires covered source and modifications to remain available under its terms when distributed. Separate files containing no MPL-covered code may remain proprietary. Commercial use is permitted. See the [MPL text](https://www.mozilla.org/en-US/MPL/2.0/) and [Mozilla FAQ](https://www.mozilla.org/en-US/MPL/2.0/FAQ/).

## Attribution

The observed BLE protocol references **DragyDash ESP32 by Jarel Remick**, pinned at `8d2aed470157f03cc4e14b4061e540f9b9b164c8` (Apache-2.0). UBX packet and configuration fields reference the u-blox interface document linked in NOTICE. ESP-IDF and esp-nimble-cpp retain their original licenses. Vendor names are used descriptively; no vendor code or documents are relicensed by this repository.
