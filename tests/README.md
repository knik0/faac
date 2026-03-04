# FAAC Benchmark Suite

FAAC is the high-efficiency encoder for the resource-constrained world. From hobbyist projects to professional surveillance (VSS) and embedded VoIP, we prioritize performance where every cycle and byte matters.

This suite provides the objective data necessary to ensure that every change moves us closer to our Northstar: the optimal balance of quality, speed, and size.

---

## The "Golden Triangle" Philosophy

We evaluate every contribution against three competing pillars. While high-bitrate encoders like FDK-AAC or Opus target multi-channel, high-fidelity entertainment, FAAC focuses on remaining approachable and distributable for the global open-source community. We prioritize non-patent encumbered areas and the standard Low Complexity (LC-AAC) profile.

1.  **Audio Fidelity**: We target transparent audio quality for our bitrates. We use objective metrics like ViSQOL (MOS) to ensure psychoacoustic improvements truly benefit the listener without introducing "metallic" ringing or "underwater" artifacts.
2.  **Computational Efficiency**: FAAC must remain fast. We optimize for low-power cores where encoding speed is a critical requirement. Every CPU cycle saved is a win for our users.
3.  **Minimal Footprint**: Binary size is a feature. We ensure the library remains small enough to fit within restrictive embedded firmware.

---

## Benchmarking Scenarios

| Scenario | Mode | Source | Config | Project Goal |
| :--- | :--- | :--- | :--- | :--- |
| **VoIP** | Speech (16k) | TCD-VOIP | `-b 16` | Clear communication at low bitrates (16kbps). |
| **VSS** | Speech (16k) | TCD-VOIP | `-b 40` | High-fidelity Video Surveillance Systems recording (40kbps). |
| **Music** | Audio (48k) | PMLT / SoundExpert | `-b 64-256` | Full-range transparency for storage & streaming. |
| **Throughput** | Efficiency | Synthetic Signals | Default | Stability test using 10-minute Sine/Sweep/Noise/Silence. |

---

## Metric Definitions

| Metric | Definition | Reference |
| :--- | :--- | :--- |
| **MOS** | Mean Opinion Score (LQO). Predicted perceptual quality from 1.0 (Bad) to 5.0 (Excellent), computed via the **ViSQOL** model. | [ITU-T P.800](https://www.itu.int/rec/T-REC-P.800), [ViSQOL](https://github.com/google/visqol) |
| **Regressions** | Critical failure or a drop in MOS ≥ 0.1 compared to the baseline commit. Significant throughput drops (>10%) or increased binary size also warrant review. | |
| **Significant Win** | An improvement in MOS ≥ 0.1 compared to the baseline commit. | |
| **Consistency** | Percentage of test cases where bitstreams are MD5-identical to the baseline. | |
| **Throughput** | Normalized encoding speed improvement against baseline. Higher % indicates faster execution. | |
| **Library Size** | Binary footprint of `libfaac.so`. Delta measured against baseline. Critical for embedded VSS/IoT targets. | |
| **Bitrate Δ** | Percentage change in generated file size against baseline. Relative shift in bits used for the same target. | |
| **Bitrate Accuracy** | The closeness of the achieved bitrate to the specified target (ABR mode). Measures the encoder's ability to respect the user-defined bitrate budget. | |

---

## Dataset Sources

We are grateful to the following projects for providing high-quality research material:

*   **TCD-VoIP (Sigmedia-VoIP)**: [Listener Test Database](https://www.sigmedia.tv/datasets/tcd_voip_ltd/) - Specifically designed for assessing quality in VoIP applications.
*   **PMLT2014**: [Public Multiformat Listening Test](https://listening-test.coresv.net/) - A community-defined comprehensive multi-codec benchmark.
*   **SoundExpert**: [Sound Samples](https://soundexpert.org/sound-samples) - High-precision EBU SQAM CD excerpts for transparency testing.

---

## Quick Start

### 1. Install Dependencies
```bash
# System (Ubuntu/Debian)
sudo apt-get update && sudo apt-get install -y meson ninja-build bc ffmpeg

# Python
python3 -m venv venv
source venv/bin/activate
pip install -r tests/requirements.txt
```

### 2. Prepare Datasets
Downloads samples and generates 10-minute synthetic throughput signals (Sine, Sweep, Noise, Silence).
```bash
python3 tests/setup_datasets.py
```

### 3. Run a Benchmark
Perceptual analysis and full test suite coverage are enabled by default. Use `--skip-mos` or `--coverage 10` for faster iteration during local development.
```bash
python3 tests/run_benchmark.py build/frontend/faac build/libfaac/libfaac.so my_run tests/results/my_run.json
```

### 4. Compare Results
Generate a high-signal summary comparing your candidate against a baseline.
```bash
python3 tests/compare_results.py tests/results/
```

## Who This Suite Helps

*   **Maintainers**: Provides the confidence to merge PRs by proving that a change improves the encoder—or at least doesn't cause a regression.
*   **Developers**: Offers standardized, automated feedback during implementation.
*   **Users**: Ensures that every new version of FAAC remains a reliable choice for their critical firmware and communication projects.
