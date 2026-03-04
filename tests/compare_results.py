"""
 * FAAC Benchmark Suite
 * Copyright (C) 2026 Nils Schimmelmann
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
"""

import json
import sys
import os
from collections import defaultdict


def analyze_pair(base_file, cand_file):
    try:
        with open(base_file, "r") as f:
            base = json.load(f)
    except Exception as e:
        sys.stderr.write(
            f"  Warning: Could not load baseline file {base_file}: {e}\n")
        base = {}

    try:
        with open(cand_file, "r") as f:
            cand = json.load(f)
    except Exception as e:
        sys.stderr.write(
            f"  Error: Could not load candidate file {cand_file}: {e}\n")
        return None

    suite_results = {
        "has_regression": False,
        "missing_data": False,
        "mos_delta_sum": 0,
        "mos_count": 0,
        "missing_mos_count": 0,
        "tp_reduction": 0,
        "lib_size_chg": 0,
        "bitrate_chg_sum": 0,
        "bitrate_count": 0,
        "bitrate_acc_sum": 0,
        "bitrate_acc_count": 0,
        "regressions": [],
        "new_wins": [],
        "significant_wins": [],
        "opportunities": [],
        "bit_exact_count": 0,
        "total_cases": 0,
        "all_cases": [],
        "scenario_stats": defaultdict(
            lambda: {
                "tp_sum_cand": 0,
                "tp_sum_base": 0,
                "count": 0}),
        "base_tp": base.get("throughput", {}),
        "cand_tp": cand.get("throughput", {})}

    base_m = base.get("matrix", {})
    cand_m = cand.get("matrix", {})

    if cand_m:
        suite_results["total_cases"] = len(cand_m)
        for k in sorted(cand_m.keys()):
            o = cand_m[k]
            b = base_m.get(k, {})

            filename = o.get("filename", k)
            scenario = o.get("scenario", "")
            display_name = f"{scenario}: {filename}"

            o_mos = o.get("mos")
            b_mos = b.get("mos")
            thresh = o.get("thresh", 1.0)

            o_size = o.get("size")
            b_size = b.get("size")

            o_bitrate = o.get("bitrate")
            o_target = o.get("bitrate_target")

            if o_bitrate is not None and o_target is not None and o_target > 0:
                acc = (1.0 - abs(o_bitrate - o_target) / o_target) * 100
                suite_results["bitrate_acc_sum"] += acc
                suite_results["bitrate_acc_count"] += 1

            o_time = o.get("time")
            b_time = b.get("time")

            if o_time is not None and b_time is not None and b_time > 0:
                suite_results["scenario_stats"][scenario]["tp_sum_cand"] += o_time
                suite_results["scenario_stats"][scenario]["tp_sum_base"] += b_time
                suite_results["scenario_stats"][scenario]["count"] += 1

            o_md5 = o.get("md5", "")
            b_md5 = b.get("md5", "")

            if o_md5 and b_md5 and o_md5 == b_md5:
                suite_results["bit_exact_count"] += 1

            size_chg = "N/A"
            if o_size is not None and b_size is not None:
                size_chg_val = (o_size - b_size) / b_size * 100
                size_chg = f"{size_chg_val:+.2f}%"
                suite_results["bitrate_chg_sum"] += size_chg_val
                suite_results["bitrate_count"] += 1
            elif o_size is None:
                suite_results["missing_data"] = True

            status = "✅"
            delta = 0
            if o_mos is not None:
                if b_mos is not None:
                    delta = o_mos - b_mos
                    suite_results["mos_delta_sum"] += delta
                    suite_results["mos_count"] += 1

                if o_mos < (thresh - 0.5):
                    status = "🤮"  # Awful
                elif o_mos < thresh:
                    status = "📉"  # Bad/Poor

                if b_mos is not None:
                    if (o_mos - b_mos) < -0.1:
                        status = "❌"  # Regression
                        suite_results["has_regression"] = True
                    elif (o_mos - b_mos) > 0.1:
                        status = "🌟"  # Significant Win

                # Check for New Win (Baseline failed, Candidate passed)
                if b_mos is not None and b_mos < thresh and o_mos >= thresh:
                    suite_results["new_wins"].append({
                        "display_name": display_name,
                        "mos": o_mos,
                        "b_mos": b_mos,
                        "delta": delta
                    })
            else:
                status = "❌"  # Missing MOS is a failure
                suite_results["missing_mos_count"] += 1
                suite_results["has_regression"] = True
                suite_results["missing_data"] = True
                delta = -10.0  # Force to top of regressions

            mos_str = f"{o_mos:.2f}" if o_mos is not None else "N/A"
            b_mos_str = f"{b_mos:.2f}" if b_mos is not None else "N/A"
            delta_mos = f"{(o_mos - b_mos):+.2f}" if (
                o_mos is not None and b_mos is not None) else "N/A"

            case_data = {
                "display_name": display_name,
                "status": status,
                "mos": o_mos,
                "b_mos": b_mos,
                "delta": delta,
                "size_chg": size_chg,
                "line": f"| {display_name} | {status} | {mos_str} ({b_mos_str}) | {delta_mos} | {size_chg} |"
            }

            suite_results["all_cases"].append(case_data)
            if status == "❌":
                suite_results["regressions"].append(case_data)
            elif status == "🌟":
                suite_results["significant_wins"].append(case_data)
            elif status in ["🤮", "📉"]:
                suite_results["opportunities"].append(case_data)
    else:
        suite_results["missing_data"] = True

    # Sorts
    suite_results["regressions"].sort(key=lambda x: x["delta"])
    suite_results["new_wins"].sort(key=lambda x: x["delta"], reverse=True)
    suite_results["significant_wins"].sort(
        key=lambda x: x["delta"], reverse=True)
    suite_results["opportunities"].sort(
        key=lambda x: x["mos"] if x["mos"] is not None else 6.0)

    # Throughput
    base_tp = base.get("throughput", {})
    cand_tp = cand.get("throughput", {})
    # Exclude "overall" to avoid double-counting in manual summation
    total_base_t = sum(v for k, v in base_tp.items() if k != "overall")
    total_cand_t = sum(v for k, v in cand_tp.items() if k != "overall")
    if total_cand_t > 0 and total_base_t > 0:
        suite_results["tp_reduction"] = (1 - total_cand_t / total_base_t) * 100
    else:
        # If overall throughput is missing, try to aggregate from scenarios
        cand_t_sum = sum(s["tp_sum_cand"]
                         for s in suite_results["scenario_stats"].values())
        base_t_sum = sum(s["tp_sum_base"]
                         for s in suite_results["scenario_stats"].values())
        if cand_t_sum > 0 and base_t_sum > 0:
            suite_results["tp_reduction"] = (1 - cand_t_sum / base_t_sum) * 100
        else:
            suite_results["missing_data"] = True

    # Binary Size
    base_lib = base.get("lib_size", 0)
    cand_lib = cand.get("lib_size", 0)
    if cand_lib > 0 and base_lib > 0:
        suite_results["lib_size_chg"] = ((cand_lib / base_lib) - 1) * 100
    else:
        suite_results["missing_data"] = True

    return suite_results


def main():
    SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

    summary_only = "--summary-only" in sys.argv
    if summary_only:
        sys.argv.remove("--summary-only")

    base_sha = None
    if "--base-sha" in sys.argv:
        idx = sys.argv.index("--base-sha")
        base_sha = sys.argv[idx + 1]
        sys.argv.pop(idx + 1)
        sys.argv.pop(idx)

    cand_sha = None
    if "--cand-sha" in sys.argv:
        idx = sys.argv.index("--cand-sha")
        cand_sha = sys.argv[idx + 1]
        sys.argv.pop(idx + 1)
        sys.argv.pop(idx)

    results_dir = sys.argv[1] if len(
        sys.argv) > 1 else os.path.join(
        SCRIPT_DIR, "results")

    if not os.path.exists(results_dir):
        sys.exit(1)

    files = os.listdir(results_dir)

    suites = {}
    for f in files:
        if f.endswith("_cand.json"):
            suite_name = f[:-10]
            base_f = suite_name + "_base.json"
            if base_f in files:
                suites[suite_name] = (
                    os.path.join(
                        results_dir, base_f), os.path.join(
                        results_dir, f))

    if not suites:
        sys.stderr.write("No result pairs found in directory.\n")
        sys.exit(1)

    all_suite_data = {}
    overall_regression = False
    overall_missing = False
    total_mos_delta = 0
    total_mos_count = 0
    total_missing_mos = 0
    total_tp_reduction = 0
    total_lib_chg = 0
    total_bitrate_chg = 0
    total_bitrate_count = 0
    total_bitrate_acc_sum = 0
    total_bitrate_acc_count = 0

    total_regressions = 0
    total_new_wins = 0
    total_significant_wins = 0
    total_bit_exact = 0
    total_cases_all = 0

    # For worst-case scenario throughput
    scenario_tp_deltas = []

    for name, (base, cand) in sorted(suites.items()):
        data = analyze_pair(base, cand)
        if data:
            all_suite_data[name] = data
            if data["has_regression"]:
                overall_regression = True
            if data["missing_data"]:
                overall_missing = True
            total_mos_delta += data["mos_delta_sum"]
            total_mos_count += data["mos_count"]
            total_missing_mos += data["missing_mos_count"]
            total_tp_reduction += data["tp_reduction"]
            total_lib_chg += data["lib_size_chg"]
            total_bitrate_chg += data["bitrate_chg_sum"]
            total_bitrate_count += data["bitrate_count"]
            total_bitrate_acc_sum += data["bitrate_acc_sum"]
            total_bitrate_acc_count += data["bitrate_acc_count"]

            total_regressions += len(data["regressions"])
            total_new_wins += len(data["new_wins"])
            total_significant_wins += len(data["significant_wins"])
            total_bit_exact += data["bit_exact_count"]
            total_cases_all += data["total_cases"]

            for sc_name, sc_data in data["scenario_stats"].items():
                if sc_data["tp_sum_base"] > 0:
                    delta = (1 - sc_data["tp_sum_cand"] /
                             sc_data["tp_sum_base"]) * 100
                    scenario_tp_deltas.append((f"{name} / {sc_name}", delta))

    avg_mos_delta_str = f"{(total_mos_delta /
                            total_mos_count):+.3f}" if total_mos_count > 0 else "N/A"
    avg_tp_reduction = total_tp_reduction / \
        len(all_suite_data) if all_suite_data else 0
    avg_lib_chg = total_lib_chg / len(all_suite_data) if all_suite_data else 0
    avg_bitrate_chg = total_bitrate_chg / \
        total_bitrate_count if total_bitrate_count > 0 else 0
    avg_bitrate_acc = total_bitrate_acc_sum / \
        total_bitrate_acc_count if total_bitrate_acc_count > 0 else 0

    bit_exact_percent = (
        total_bit_exact /
        total_cases_all *
        100) if total_cases_all > 0 else 0

    # Worst-case throughput
    worst_tp_scen, worst_tp_delta = (None, 0)
    if scenario_tp_deltas:
        worst_tp_scen, worst_tp_delta = min(
            scenario_tp_deltas, key=lambda x: x[1])

    report = []
    if overall_regression:
        report.append("## ❌ Quality Regression Detected")
    elif worst_tp_delta < -5.0:
        report.append("## ⚠️ Performance Regression Detected")
    elif overall_missing:
        report.append("## ❌ Incomplete/Missing Data Detected")
    elif bit_exact_percent == 100.0:
        report.append("## ✅ Refactor Verified (Bit-Identical)")
    elif total_new_wins > 0 or total_significant_wins > 0 or (total_mos_count > 0 and (total_mos_delta / total_mos_count) > 0.01) or avg_tp_reduction > 5:
        report.append("## 🚀 Perceptual & Efficiency Improvement")
    else:
        report.append("## 📊 Benchmark Summary")

    if not summary_only and (base_sha or cand_sha):
        report.append("\n### Environment")
        if base_sha:
            report.append(f"- **Baseline SHA**: `{base_sha}`")
        if cand_sha:
            report.append(f"- **Candidate SHA**: `{cand_sha}`")

    report.append("\n### Summary")
    report.append("| Metric | Value |")
    report.append("| :--- | :--- |")

    # Regressions (Always shown)
    reg_status = "0 ✅" if total_regressions == 0 else f"{total_regressions} ❌"
    report.append(f"| **Regressions** | {reg_status} |")

    # New Wins (Only if baseline < threshold and candidate >= threshold)
    if total_new_wins > 0:
        report.append(f"| **New Wins** | {total_new_wins} 🆕 |")

    # Significant Wins (MOS delta > 0.1)
    if total_significant_wins > 0:
        report.append(f"| **Significant Wins** | {total_significant_wins} 🌟 |")

    # Bitstream Consistency (Against baseline)
    consist_status = f"{bit_exact_percent:.1f}%"
    if bit_exact_percent == 100.0:
        consist_status += " (MD5 Match)"
    report.append(f"| **Consistency** | {consist_status} |")

    # Throughput
    if abs(avg_tp_reduction) > 0.1:
        tp_icon = "🚀" if avg_tp_reduction > 1.0 else "📉" if avg_tp_reduction < -1.0 else ""
        report.append(
            f"| **Throughput (Avg)** | {avg_tp_reduction:+.1f}% {tp_icon} |")

    # Per-signal throughput deltas if available
    tp_details = []
    if all_suite_data:
        first_data = list(all_suite_data.values())[0]
        base_tp = first_data.get("base_tp", {})
        cand_tp = first_data.get("cand_tp", {})
        for signal in sorted(cand_tp.keys()):
            if signal == "overall":
                continue
            if signal in base_tp and base_tp[signal] > 0:
                delta = (1 - cand_tp[signal] / base_tp[signal]) * 100
                icon = "🚀" if delta > 1.0 else "📉" if delta < -1.0 else ""
                tp_details.append(
                    f"{signal.split('.')[0]}: {delta:+.1f}% {icon}")

    if tp_details:
        report.append(f"| **TP Breakdown** | {', '.join(tp_details)} |")

    if worst_tp_delta < -1.0:
        report.append(
            f"| **Worst-case TP Δ** | {worst_tp_delta:.1f}% ({worst_tp_scen}) ⚠️ |")

    # Binary Size
    if abs(avg_lib_chg) > 0.01:
        size_icon = "📉" if avg_lib_chg < -0.1 else "📈" if avg_lib_chg > 0.1 else ""
        report.append(
            f"| **Library Size** | {avg_lib_chg:+.2f}% {size_icon} |")


    # Bitrate Δ
    if abs(avg_bitrate_chg) > 0.1:
        bitrate_icon = "📉" if avg_bitrate_chg < - \
            1.0 else "📈" if avg_bitrate_chg > 1.0 else ""
        report.append(
            f"| **Bitrate Δ** | {avg_bitrate_chg:+.2f}% {bitrate_icon} |")

    # Bitrate Accuracy
    if total_bitrate_acc_count > 0:
        acc_icon = "🎯" if avg_bitrate_acc > 95 else "⚠️" if avg_bitrate_acc < 80 else ""
        report.append(
            f"| **Bitrate Accuracy** | {avg_bitrate_acc:.1f}% {acc_icon} |")

    # Avg MOS Delta
    if total_mos_count > 0 and abs(total_mos_delta / total_mos_count) > 0.001:
        report.append(f"| **Avg MOS Delta** | {avg_mos_delta_str} |")

    if total_missing_mos > 0:
        report.append(
            f"\n⚠️ **Warning**: {total_missing_mos} MOS scores were missing/failed (treated as ❌).")

    if not summary_only:
        # 1. Collapsible Details: Regressions
        if total_regressions > 0:
            report.append(
                "\n<details><summary><b>❌ View Regression Details ({})</b></summary>\n".format(total_regressions))
            for name, data in sorted(all_suite_data.items()):
                if data["regressions"]:
                    report.append(f"\n#### {name}")
                    report.append(
                        "| Test Case | Status | MOS (Base) | Delta | Size Δ |")
                    report.append("| :--- | :---: | :---: | :---: | :---: |")
                    for r in data["regressions"]:
                        report.append(r["line"])
            report.append("\n</details>")

        # 2. Collapsible Additional Details
        report.append(
            "\n<details><summary><b>View Additional Suite Details & Wins</b></summary>\n")

        for name, data in sorted(all_suite_data.items()):
            status_icon = "✅"
            if data["has_regression"]:
                status_icon = "❌"
            elif data["missing_data"]:
                status_icon = "❌"

            avg_mos_suite = f"{(data['mos_delta_sum'] /
                                data['mos_count']):+.3f}" if data["mos_count"] > 0 else "N/A"
            suite_bit_exact_percent = (
                data["bit_exact_count"] /
                data["total_cases"] *
                100) if data["total_cases"] > 0 else 0

            report.append(f"\n#### {status_icon} {name}")
            report.append(
                f"- MOS Δ: {avg_mos_suite}, TP Δ: {data['tp_reduction']:+.1f}%, Size Δ: {data['lib_size_chg']:+.2f}%")
            report.append(
                f"- Bitstream Consistency: {suite_bit_exact_percent:.1f}%")

            if data["new_wins"]:
                report.append("\n**🆕 New Wins**")
                report.append("| Test Case | MOS (Base) | Delta |")
                report.append("| :--- | :---: | :---: |")
                for w in data["new_wins"]:
                    report.append("| {} | {:.2f} ({:.2f}) | {:+.2f} |".format(
                        w["display_name"], w["mos"], w["b_mos"], w["delta"]))

            if data["significant_wins"]:
                report.append("\n**🌟 Significant Wins**")
                report.append(
                    "| Test Case | Status | MOS (Base) | Delta | Size Δ |")
                report.append("| :--- | :---: | :---: | :---: | :---: |")
                for w in data["significant_wins"]:
                    report.append(w["line"])

            if data["opportunities"]:
                report.append("\n**💡 Opportunities**")
                report.append(
                    "| Test Case | Status | MOS (Base) | Delta | Size Δ |")
                report.append("| :--- | :---: | :---: | :---: | :---: |")
                for o in data["opportunities"]:
                    report.append(o["line"])

            if data["all_cases"]:
                report.append(
                    f"\n<details><summary>View all {len(data['all_cases'])} cases for {name}</summary>\n")
                report.append(
                    "| Test Case | Status | MOS (Base) | Delta | Size Δ |")
                report.append("| :--- | :---: | :---: | :---: | :---: |")
                for c in data["all_cases"]:
                    report.append(c["line"])
                report.append("\n</details>")

        report.append("\n</details>")

    output = "\n".join(report)
    sys.stdout.write(output + "\n")

    if overall_regression or overall_missing:
        sys.exit(1)


if __name__ == "__main__":
    main()
