# Roomove DSP Test Seams — Phase 0 Inventory

**Issue:** Phase 0 — Inventory and alignment: identify Roomove DSP test seams  
**Status:** Complete — ready for child test issues

---

## 1. Core DSP Entrypoint

### State struct — `RoomoveDspState` (`dsp/RoomoveDSP.h`)

The minimal building block for all headless testing. It is a plain C struct with no JUCE
dependency and no dynamic allocation, making it fully testable without a plugin host or
audio driver.

```c
typedef struct RoomoveDspState {
    float sampleRate;
    volatile unsigned int armorStrengthBits;  // atomic float encoding of armorStrength
    volatile unsigned int targetMaskBits;     // atomic float encoding of mask target
    float currentMask;                        // smoothed mask value (initialised to 1.0)
    float maskSmoothing;                      // computed from sampleRate
    float peakEnvelope;                       // peak follower (initialised to 0.0)
    float envelopeRelease;                    // computed from sampleRate
} RoomoveDspState;
```

### C-linkage API (`dsp/RoomoveDSP.h`)

| Function | Role | Test seam? |
|---|---|---|
| `roomoveDspStateInit(state, sampleRate)` | Initialise / reset one channel | ✅ Primary seam |
| `roomoveDspStateSetArmorStrength(state, v)` | Write armorStrength [0, 1] atomically | ✅ Parameter seam |
| `roomoveDspStateSetMask(state, v)` | Write target mask [0.02, 1] atomically | ✅ Parameter seam |
| `roomoveDspStateProcessAudio(state, in, out, n)` | Render n samples (alias-safe dispatcher) | ✅ Primary seam |
| `roomoveDspStateProcessAudioNoAlias(state, in, out, n)` | Render n samples (restrict kernel) | ✅ Inner kernel |

Global convenience wrappers (`roomoveDspInit`, `processRoomoveAudio`, etc.) delegate to a
single `gGlobalState`. Prefer the per-state API in tests to keep test cases fully isolated.

### JUCE host wrapper — `ArmorAudioProcessor` (`app/PluginProcessor.h`)

Wraps one `RoomoveDspState` per channel inside `std::vector<RoomoveDspState>`, drives the
APVTS parameter, and forwards into the C API inside `processBlock`. This class is **not**
exercised headlessly — use the C API directly for unit tests.

---

## 2. Supported Sample Rates and Block Sizes

| Property | Value | Source |
|---|---|---|
| Default sample rate | 48 000 Hz | `kDefaultSampleRate` (`dsp/RoomoveDSP.cpp:16`) |
| `prepareToPlay` accepts | any `sampleRate > 1000 Hz` | guard in `roomoveDspStateInit` |
| Hop length (internal mask period) | 512 samples | `kHopLengthSamples` (`dsp/RoomoveDSP.cpp:17`) |
| Validation test block size | 512 samples | `tests/RealtimeValidationTests.cpp:132` |
| DTT suite buffer size | 64 samples | `dtt/suites/DSH_SigCancellation.yml` |
| DSP seam test block size | 64 samples | `tests/DspSeamTests.cpp` |
| Overlap scratch buffer | 1 024 samples | `kOverlapScratchSamples` (`dsp/RoomoveDSP.cpp:27`) |

---

## 3. State / Parameter System

Roomove uses **JUCE AudioProcessorValueTreeState (APVTS)** as the host-facing parameter layer.

| Parameter ID | Display name | Type | Range | Default |
|---|---|---|---|---|
| `armor_strength` | "Armor Strength" | `AudioParameterFloat` | [0.0, 1.0] step 0.01 | 1.0 |

The APVTS is declared `public` on `ArmorAudioProcessor` (`app/PluginProcessor.h:42`). For
headless DSP unit tests, bypass the APVTS entirely and call
`roomoveDspStateSetArmorStrength(state, v)` directly.

### Internal DSP parameters (per `RoomoveDspState`)

| Field | Default after `roomoveDspStateInit` | Clamped to |
|---|---|---|
| `armorStrengthBits` | `floatToBits(1.0f)` | [0.0, 1.0] |
| `targetMaskBits` | `floatToBits(1.0f)` | [0.02, 1.0] |
| `currentMask` | 1.0 | smoothed toward `targetMaskBits` |
| `peakEnvelope` | 0.0 | grows on attack, decays by `envelopeRelease` |

---

## 4. Neutral / Bypass Conditions and Defaults

| Condition | How to set | Expected output |
|---|---|---|
| Full bypass (pass-through) | `armorStrength = 0.0` | `output[i] == input[i]` (blendedMask forced to 1.0) |
| Default / full effect | `armorStrength = 1.0` | signal attenuated by adaptive mask |
| Silence in → silence out | any `armorStrength` | `output[i] == 0.0` (denormal guard clears near-zero) |
| State just initialised | fresh `roomoveDspStateInit` | `currentMask = 1.0`, `peakEnvelope = 0.0` |

**Key formula** (from `roomoveDspStateProcessAudioNoAlias`):

```
blendedMask = 1.0 + (currentMask - 1.0) * armorStrength
output[i]   = sanitizeDenormal(input[i] * blendedMask)
```

At `armorStrength = 0.0`: `blendedMask = 1.0` → pure pass-through.  
At `armorStrength = 1.0`: `blendedMask = currentMask` → full adaptive gate applied.

---

## 5. State / Preset Serialization

`ArmorAudioProcessor` serializes via standard JUCE helpers
(`app/PluginProcessor.cpp:85–98`):

```cpp
// Save
auto state = apvts.copyState();          // JUCE ValueTree, root tag "Parameters"
if (auto xml = state.createXml())
    copyXmlToBinary(*xml, destData);

// Load
if (auto xmlState = getXmlFromBinary(data, sizeInBytes))
    if (xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
```

**Format:** binary-wrapped XML.  
**Root tag:** `Parameters`.  
**Only parameter persisted:** `armor_strength` as a float attribute.

The DSP-layer `RoomoveDspState` carries no persistent state; it is fully reconstructed by
`prepareToPlay` after any preset load.

---

## 6. Headless Test Locations and Execution

| Test name | File | CTest target | Requires JUCE? |
|---|---|---|---|
| `testAbstractFifoHighContention` | `tests/RealtimeValidationTests.cpp` | `Roomove_ValidationTests` | `juce_core` only |
| `testScopedNoDenormalsPresent` | `tests/RealtimeValidationTests.cpp` | `Roomove_ValidationTests` | `juce_core` only |
| `testInferenceLatencyBudget` | `tests/RealtimeValidationTests.cpp` | `Roomove_ValidationTests` | `juce_core` only |
| `testDefaultStateAfterInit` | `tests/DspSeamTests.cpp` | `Roomove_DspSeamTests` | None (pure C++) |
| `testBypassAtArmorStrengthZero` | `tests/DspSeamTests.cpp` | `Roomove_DspSeamTests` | None (pure C++) |
| `testArmorStrengthClamping` | `tests/DspSeamTests.cpp` | `Roomove_DspSeamTests` | None (pure C++) |
| `testMaskClamping` | `tests/DspSeamTests.cpp` | `Roomove_DspSeamTests` | None (pure C++) |
| `testInPlaceProcessing` | `tests/DspSeamTests.cpp` | `Roomove_DspSeamTests` | None (pure C++) |
| `testSilencePassesThroughClean` | `tests/DspSeamTests.cpp` | `Roomove_DspSeamTests` | None (pure C++) |
| `testNullPointerSafety` | `tests/DspSeamTests.cpp` | `Roomove_DspSeamTests` | None (pure C++) |
| Real-time memory hygiene | `scripts/check_realtime_memory_hygiene.py` | `roomove_realtime_memory_hygiene` | Python 3 only |

**Run all headless tests after a native host build:**

```sh
cd build_standalone      # or build_native on Windows
ctest --output-on-failure
```

`Roomove_DspSeamTests` links only the DSP translation unit and standard C++ libraries — no
JUCE, no audio driver, no plugin host. It can run on any native host (macOS arm64, Linux
x86-64, Windows x64) as part of CI.

---

## 7. CI and Hardware Build Targets

| Workflow file | Runner | Output artifact | DSP toolchain |
|---|---|---|---|
| `.github/workflows/mac-arm-standalone.yaml` | self-hosted macOS arm64 | `Roomove.app` (Standalone) | Clang (Apple Silicon) |
| `.github/workflows/manual.yaml` | self-hosted Windows x64 | `Roomove.aaxplugin` (AAX) + optional DSP lib | MSVC (native) + TI cl6x (DSP) |

**TI DSP configure command:**

```sh
cmake -S . -B build_dsp \
  -DCMAKE_TOOLCHAIN_FILE=ti_toolchain.cmake \
  -DTI_CGT_DIR=C:/TI/c6000_7.4.24 \
  -DAAX_SDK_PATH=C:/SDKs/AAX-SDK \
  -DCMAKE_BUILD_TYPE=Release
```

**TI compiler path:** `C:\TI\c6000_7.4.24\bin\cl6x.exe` (C6000 family, forced C++98 mode,
static library only — `CMAKE_SYSTEM_NAME=Generic`).

The `Roomove_DspSeamTests` and `Roomove_ValidationTests` CTest targets are built under the
**host world** branch (non-Generic `CMAKE_SYSTEM_NAME`) and are therefore exercisable in
the mac-arm-standalone workflow once a JUCE installation is available on the runner.

---

## 8. FleetwoodAir → Roomove Mapping Table

> FleetwoodAir is the reference test suite used as the porting baseline.  
> The table maps each FleetwoodAir test category to the nearest Roomove equivalent.

| FleetwoodAir test category | FleetwoodAir target | Roomove equivalent | Roomove file |
|---|---|---|---|
| DSP state init / reset | `FleetwoodDspState::reset()` | `roomoveDspStateInit(state, sr)` | `dsp/RoomoveDSP.cpp` |
| Gain parameter set | `FleetwoodDspState::setGain()` | `roomoveDspStateSetArmorStrength(state, v)` | `dsp/RoomoveDSP.cpp` |
| Adaptive mask parameter | `FleetwoodDspState::setMask()` | `roomoveDspStateSetMask(state, v)` | `dsp/RoomoveDSP.cpp` |
| Audio render (separate buffers) | `FleetwoodDspState::processNoAlias()` | `roomoveDspStateProcessAudioNoAlias(state, in, out, n)` | `dsp/RoomoveDSP.cpp` |
| Audio render (in-place) | `FleetwoodDspState::processInPlace()` | `roomoveDspStateProcessAudio(state, buf, buf, n)` | `dsp/RoomoveDSP.cpp` |
| Bypass / pass-through | `FleetwoodDspState::bypass()` | `roomoveDspStateSetArmorStrength(state, 0.0f)` | `dsp/RoomoveDSP.cpp` |
| Inference latency budget | `FleetwoodLatencyTest` | `testInferenceLatencyBudget()` | `tests/RealtimeValidationTests.cpp` |
| Real-time memory hygiene | `FleetwoodMemHygieneTest` | `roomove_realtime_memory_hygiene` | `scripts/check_realtime_memory_hygiene.py` |
| Lock-free FIFO concurrency | `FleetwoodFifoTest` | `testAbstractFifoHighContention()` | `tests/RealtimeValidationTests.cpp` |
| Denormal protection guard | `FleetwoodDenormalTest` | `testScopedNoDenormalsPresent()` | `tests/RealtimeValidationTests.cpp` |
| Native vs DSP parity (null test) | `FleetwoodNativeDspParity` | `DSH_SigCancellation` suite | `dtt/suites/DSH_SigCancellation.yml` |
| HDX TI cycle budget | `FleetwoodHDXCycleTest` | `DSH_TI_CycleCounts` suite | `dtt/suites/DSH_TI_CycleCounts.yml` |
| Preset save / load round-trip | `FleetwoodPresetTest` | `getStateInformation` / `setStateInformation` | `app/PluginProcessor.cpp` |

---

## 9. Architecture Note — How Roomove Processing Is Exercised in Tests

```
┌─────────────────────────────────────────────────────────────────┐
│  Test Layer 1 — Pure C DSP (no JUCE, no I/O)                   │
│  File:   tests/DspSeamTests.cpp                                 │
│  Target: Roomove_DspSeamTests  (CTest)                          │
│                                                                 │
│  Instantiates RoomoveDspState on the stack; drives the C API    │
│  directly without a plugin host or audio driver:                │
│                                                                 │
│    RoomoveDspState state;                                       │
│    roomoveDspStateInit(&state, 48000.0f);                       │
│    roomoveDspStateSetArmorStrength(&state, 0.0f);               │
│    roomoveDspStateProcessAudio(&state, in, out, 64);            │
│                                                                 │
│  ✅ Runs headlessly on macOS, Linux, and Windows                │
│  ✅ No DAW, plugin host, or audio driver required               │
│  ✅ Buildable without JUCE (pure C++ + math.h)                  │
└────────────────────────┬────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────────┐
│  Test Layer 2 — Real-time validation (juce_core + C DSP)       │
│  File:   tests/RealtimeValidationTests.cpp                      │
│  Target: Roomove_ValidationTests  (CTest)                       │
│                                                                 │
│  • testAbstractFifoHighContention  — lock-free FIFO safety      │
│  • testScopedNoDenormalsPresent    — denormal guard presence     │
│  • testInferenceLatencyBudget      — DSP latency ≤ 4 ms / block │
│                                                                 │
│  ✅ Runs headlessly on any native host                          │
│  ✅ Requires only juce::juce_core (no audio I/O)               │
└────────────────────────┬────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────────┐
│  Test Layer 3 — Source hygiene (Python, no compilation needed) │
│  File:   scripts/check_realtime_memory_hygiene.py               │
│  Target: roomove_realtime_memory_hygiene  (CTest)               │
│                                                                 │
│  Parses PluginProcessor.cpp statically; confirms:               │
│  • processBlock contains ScopedNoDenormals                      │
│  • processBlock contains no dynamic allocation call             │
│                                                                 │
│  ✅ Runs on any host with Python 3 — no compilation needed      │
└────────────────────────┬────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────────┐
│  Test Layer 4 — DSH / AAX integration (hardware shell only)    │
│  Files:  dtt/suites/DSH_SigCancellation.yml                     │
│          dtt/suites/DSH_TI_CycleCounts.yml                      │
│                                                                 │
│  Requires: Avid DSH (DigiShell), Pro Tools HDX hardware,        │
│  and a signed AAX bundle produced by manual.yaml.               │
│                                                                 │
│  ✅ Native-vs-DSP null test (buffer-level bit-exactness)        │
│  ✅ HDX TI cycle-count capture via DAE.cyclesshared            │
│  ❌ Cannot run in CI — hardware-only                            │
└─────────────────────────────────────────────────────────────────┘
```

### APVTS integration seam (plugin host process only)

`ArmorAudioProcessor::prepareToPlay` allocates and initialises per-channel
`RoomoveDspState` objects. `processBlock` reads `armor_strength` from the APVTS atomic
pointer and forwards to `roomoveDspStateSetArmorStrength` + `roomoveDspStateProcessAudio`
per channel.

The APVTS serialization round-trip (`getStateInformation` / `setStateInformation`) is
exercisable in any JUCE plugin test harness and is the target for the preset serialization
child issue.
