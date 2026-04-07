import pandas as pd
import numpy as np
import glob
import os

RNG = np.random.default_rng(seed=42)
N_BOOTSTRAP = 10_000

COLUMNS = ["0deg_ns", "180deg_ns"]
LABELS  = ["0", "180"]


def load_group(directory: str, group_name: str) -> pd.DataFrame:
    """
    Concatenate all CSVs in a directory.
    Expects columns: Iteration, 0deg_ns, 180deg_ns.
    All iterations are included as-is.
    """
    files = sorted(glob.glob(os.path.join(directory, "*.csv")))
    if not files:
        raise FileNotFoundError(f"No CSV files found in: {directory}")

    dfs = []
    for f in files:
        df = pd.read_csv(f, usecols=["Iteration"] + COLUMNS, skipinitialspace=True)
        dfs.append(df)

    combined = pd.concat(dfs, ignore_index=True)
    combined["group"] = group_name
    return combined


def coefficient_of_variation(x: np.ndarray) -> float:
    """CV as a percentage: (std / mean) * 100."""
    return float((np.std(x, ddof=1) / np.mean(x)) * 100.0)


def bootstrap_cv_ci(
    x: np.ndarray,
    n_bootstrap: int = N_BOOTSTRAP,
    ci: float = 0.95,
) -> tuple[float, float]:
    """
    Non-parametric bootstrap 95% CI for CV.
    Returns (lower_distance, upper_distance) from the point estimate,
    ready for pgfplots y error minus / y error plus.
    """
    boot_cvs = np.array([
        coefficient_of_variation(RNG.choice(x, size=len(x), replace=True))
        for _ in range(n_bootstrap)
    ])
    alpha = (1.0 - ci) / 2.0
    cv    = coefficient_of_variation(x)
    lo    = float(np.percentile(boot_cvs, alpha * 100))
    hi    = float(np.percentile(boot_cvs, (1.0 - alpha) * 100))
    return cv - lo, hi - cv          # distances, not absolute values


def iqr_filter(x: np.ndarray, k: float = 3.0) -> np.ndarray:
    q1, q3 = np.percentile(x, 25), np.percentile(x, 75)
    iqr = q3 - q1
    return x[(x >= q1 - k * iqr) & (x <= q3 + k * iqr)]


def compute_stats(df: pd.DataFrame, group_name: str) -> list[dict]:
    rows = []
    for col, label in zip(COLUMNS, LABELS):
        x_raw = df[col].dropna().to_numpy(dtype=float)
        x     = iqr_filter(x_raw)
        cv    = coefficient_of_variation(x)
        ci_lo, ci_hi = bootstrap_cv_ci(x)
        rows.append({
            "group":       group_name,
            "measurement": label,
            "mean_ns":     float(np.mean(x)),
            "std_ns":      float(np.std(x, ddof=1)),
            "cv":          cv,
            "ci_lo":       ci_lo,
            "ci_hi":       ci_hi,
            "n_raw":       len(x_raw),
            "n_filtered":  len(x),
            "n_removed":   len(x_raw) - len(x),
        })
    return rows


# ── Configure paths ───────────────────────────────────────────────────────────
MMAP_DIR  = "../outputs/mmap"
SYSFS_DIR = "../outputs/sysfs"

mmap  = load_group(MMAP_DIR,  "mmap")
sysfs = load_group(SYSFS_DIR, "sysfs")

all_stats = (
    compute_stats(mmap,  "mmap") +
    compute_stats(sysfs, "sysfs")
)
stats = pd.DataFrame(all_stats)

# Two separate CSVs — one per group — keeps the LaTeX clean
stats[stats.group == "mmap"].to_csv("stats_mmap.csv",   index=False, float_format="%.4f")
stats[stats.group == "sysfs"].to_csv("stats_sysfs.csv", index=False, float_format="%.4f")

print(stats[["group", "measurement", "mean_ns", "std_ns", "cv", "ci_lo", "ci_hi", "n_raw", "n_filtered", "n_removed"]].to_string(index=False))
print("\nWrote stats_mmap.csv and stats_sysfs.csv")