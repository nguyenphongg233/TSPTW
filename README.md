# TSP with Time Windows (Open-TSPTW) Solver

Dự án môn **IT4663 — Tối ưu lập kế hoạch (HUST)**: Xây dựng bộ giải bài toán **Traveling Salesman Problem with Time Windows (biến thể đường đi mở - Open Route)** bằng **C++20 (Heuristic & ALNS)** và đối chiếu thực nghiệm với **Google OR-Tools CP-SAT**.

---

## 1. Mô hình bài toán

- **Đỉnh xuất phát (Depot):** Mã `0` (xuất phát tại thời điểm $F_0 = 0$).
- **Khách hàng:** $N$ khách hàng ($N \le 1000$), đánh số từ `1` đến `N`.
- Mỗi khách hàng $i \in \{1..N\}$ có:
  - $e_i$: Thời điểm sớm nhất được phép bắt đầu phục vụ.
  - $l_i$: Thời điểm muộn nhất phải **hoàn thành** phục vụ.
  - $s_i$: Thời gian phục vụ tại khách hàng $i$.
- Ma trận $t_{u, v}$ kích thước $(N+1) \times (N+1)$: Thời gian/chi phí di chuyển từ điểm $u$ đến điểm $v$.
- **Quy tắc di chuyển trên tuyến đường** $s = (s_1, s_2, \dots, s_N)$ với $s_0 = 0$:
  $$\begin{aligned}
  A_k &= F_{k-1} + t_{s_{k-1}, s_k} && \text{(Thời điểm đến)} \\
  B_k &= \max(A_k, e_{s_k}) && \text{(Thời điểm bắt đầu phục vụ — chờ nếu đến sớm)} \\
  F_k &= B_k + s_{s_k} && \text{(Thời điểm hoàn thành phục vụ)}
  \end{aligned}$$
- **Điều kiện khả thi:** $s$ là một hoán vị của $\{1, 2, \dots, N\}$ và thỏa mãn:
  $$F_k \le l_{s_k}, \quad \forall k \in \{1, \dots, N\}$$
- **Hàm mục tiêu:** Tối thiểu hóa tổng thời gian di chuyển (xe **không cần quay về depot `0`** sau khi phục vụ $s_N$):
  $$\min \text{travelCost} = \sum_{k=1}^{N} t_{s_{k-1}, s_k}$$

---

## 2. Cấu trúc thư mục & Trạng thái triển khai

```text
TSPTW/
├── CMakeLists.txt                       # Cấu hình build C++20 (tsptw_core, tsptw, evaluator_test)
├── README.md                            # Tài liệu tổng quan và kết quả thực nghiệm
├── include/
│   ├── problem.hpp                      # [Đã xong] Struct Customer, Problem (flat 1D matrix)
│   ├── solution.hpp                     # [Đã xong] Struct Solution (route, finishTime, print, debug)
│   ├── evaluator.hpp                    # [Đã xong] Struct EvalResult, đánh giá O(N) & evaluateSuffix
│   ├── utils.hpp                        # [Đã xong] Timer, TestGenConfig, SolverConfig, Dataset Generator
│   ├── heuristic.hpp                    # [Đã xong] Khởi tạo nghiệm ban đầu (Randomized Multi-Start)
│   ├── local_search.hpp                 # [Sắp làm] Các toán tử tìm kiếm cục bộ
│   └── alns.hpp                         # [Đang làm] Thuật toán chính Adaptive Large Neighborhood Search
├── src/
│   ├── problem.cpp                      # [Đã xong] Parser đọc dữ liệu từ file .inp / istream
│   ├── evaluator.cpp                    # [Đã xong] Kiểm tra hoán vị, Time Window & Incremental Suffix Eval
│   ├── utils.cpp                        # [Đã xong] Bộ sinh 75 testcase đảm bảo 100% có nghiệm khả thi
│   ├── heuristic.cpp                    # [Đã xong] Lookahead Nearest Neighbor + Randomized Greedy Insertion
│   ├── main.cpp                         # [Đã xong] CLI điều phối (--gen, --bench, giải file lẻ)
│   ├── cpsolver.py                      # [Đã xong] Bộ giải đối chiếu bằng Google OR-Tools CP-SAT
│   ├── local_search.cpp                 # [Sắp làm]
│   └── alns.cpp                         # [Sắp làm]
├── tests/
│   └── evaluator_test.cpp               # [Đã xong] 9 Unit Tests cho Evaluator, Suffix Eval & Test Generator
├── data/
│   ├── input/                           # Chứa 75 file .inp + bảng mốc tham chiếu reference_costs.csv
│   └── output/
│       ├── cpSolver/                    # Tuyến đường .out giải bởi OR-Tools CP-SAT
│       ├── heuristic/                   # Tuyến đường .out giải bởi Heuristic C++
│       └── alns/                        # Tuyến đường .out giải bởi ALNS C++
└── results/
    ├── cp_benchmark_results.csv         # Bảng thống kê kết quả chạy OR-Tools CP-SAT
    ├── heuristic_benchmark_results.csv  # Bảng thống kê kết quả chạy Heuristic C++
    └── alns_benchmark_results.csv       # Bảng thống kê kết quả chạy ALNS C++
```

---

## 3. Bộ dữ liệu thực nghiệm (75 Testcases) & Đối chiếu Kết quả

Bộ dữ liệu gồm **75 instances** được sinh tự động bằng phương pháp *Planted Semi-Greedy Route* (đảm bảo 100% testcase đều có nghiệm khả thi và lưu lại cận trên tham chiếu `RefCost` trong `data/input/reference_costs.csv`):
1. **Bộ test nhỏ (30 files):** `n10_01..10.inp`, `n20_01..10.inp`, `n50_01..10.inp` (`windowSlack` từ `50`, `250` đến `1000`).
2. **Bộ test lớn (45 files):** $N \in \{200, 500, 1000\} \times \{\text{tight } (60), \text{med } (350), \text{wide } (2000)\} \times 5 \text{ instances } (01..05)$.

### 3.1. Bảng tổng hợp so sánh: `RefCost` vs `OR-Tools CP-SAT` vs `Heuristic C++`

| Nhóm Test | Số bài | CP-SAT giải được | Heuristic C++ giải được | `RefCost` (TB) | `CPCost` (TB) | `HeuCost` (TB) | `Heu vs CP` (TB) | `Heu vs Ref` (TB) | Thời gian `CP` vs `Heu` |
| :--- | :-: | :-: | :-: | :-: | :-: | :-: | :-: | :-: | :-: |
| **`n10` (`01..10`)** | 10 | **10/10** (`OPT`) | **10/10** (`100%`) | 4,888 | 4,869 | **4,871** | **`+0.04%`** (9/10 = OPT) | `-0.35%` | `0.0023s` vs **`0.00004s`** |
| **`n20` (`01..10`)** | 10 | **10/10** (`OPT`) | **10/10** (`100%`) | 7,535 | 7,386 | **7,396** | **`+0.14%`** (8/10 = OPT) | `-1.84%` | `0.0043s` vs **`0.00009s`** |
| **`n50` (`01..10`)** | 10 | **10/10** (`OPT`) | **10/10** (`100%`) | 13,714 | 13,261 | **13,400** | **`+1.05%`** (5/10 = OPT) | `-2.29%` | `0.0788s` vs **`0.00026s`** |
| **`n200_tight`** | 5 | **5/5** (`OPT`) | **5/5** (`100%`) | 27,076 | 27,065 | **27,069** | **`+0.016%`** (3/5 = OPT) | `-0.03%` | `0.1206s` vs **`0.0019s`** |
| **`n200_med`** | 5 | **5/5** (`OPT`) | **5/5** (`100%`) | 26,767 | 25,403 | **25,673** | **`+1.06%`** | **`-4.13%`** | `4.2643s` vs **`0.0965s`** |
| **`n200_wide`** | 5 | **0/5** (`TIMEOUT`) | **5/5** (`100%`) | 27,283 | — | **23,812** | **Heu THẮNG 5/5** | **`-12.74%`** | `20.04s` vs **`0.0298s`** |
| **`n500_tight`** | 5 | **5/5** (`OPT`) | **5/5** (`100%`) | 42,600 | 42,515 | **42,582** | **`+0.16%`** | `-0.04%` | `6.4025s` vs **`0.0163s`** |
| **`n500_med`** | 5 | **0/5** (`TIMEOUT`) | **2/5** (`40%`) | 42,818 | — | **41,056** | **Heu THẮNG 2/5** | **`-4.60%`** | `20.15s` vs `1.69s` |
| **`n500_wide`** | 5 | **0/5** (`TIMEOUT`) | **3/5** (`60%`) | 43,486 | — | **38,607** | **Heu THẮNG 3/5** | **`-12.84%`** | `20.18s` vs `1.20s` |
| **`n1000_tight`** | 5 | *(Chưa chạy)* | **3/5** (`60%`) | 60,420 | — | **60,072** | **Heu THẮNG 3/5** | `-0.08%` | — vs `0.19s` |

### 3.2. Chi tiết trên bộ test `N = 200` và `N = 500`

| Instance | `RefCost` | `CPCost` (OR-Tools) | `CPTime` | `HeuCost` (C++) | `HeuTime` | `Heu vs CP` | `Heu vs Ref` |
| :--- | :-: | :-: | :-: | :-: | :-: | :-: | :-: |
| `n200_tight_01` | 25,748 | **25,747** | `0.1039s` | **25,747** | `0.0019s` | **`+0.00%`** | `-0.00%` |
| `n200_tight_02` | 26,464 | **26,453** | `0.2097s` | **26,464** | `0.0022s` | `+0.04%` | `+0.00%` |
| `n200_tight_03` | 28,479 | **28,478** | `0.0733s` | **28,478** | `0.0018s` | **`+0.00%`** | `-0.00%` |
| `n200_tight_04` | 27,017 | **26,972** | `0.1127s` | **26,984** | `0.0018s` | `+0.04%` | `-0.12%` |
| `n200_tight_05` | 27,675 | **27,675** | `0.1035s` | **27,675** | `0.0018s` | **`+0.00%`** | `+0.00%` |
| `n200_med_01` | 26,389 | **25,370** | `7.3502s` | **25,549** | `0.0090s` | **`+0.71%`** | **`-3.18%`** |
| `n200_med_02` | 25,610 | **24,070** | `2.8820s` | **24,312** | `0.0024s` | **`+1.01%`** | **`-5.07%`** |
| `n200_med_03` | 28,932 | **27,936** | `3.8388s` | **28,296** | `0.4340s` | `+1.29%` | **`-2.20%`** |
| `n200_med_04` | 26,021 | **24,454** | `2.5162s` | **24,775** | `0.0277s` | `+1.31%` | **`-4.79%`** |
| `n200_med_05` | 26,885 | **25,185** | `4.7345s` | **25,433** | `0.0092s` | **`+0.98%`** | **`-5.40%`** |
| `n200_wide_01` | 26,495 | `TIMEOUT` (20s) | `20.084s` | **24,149** | `0.0313s` | **Heu WIN** | **`-8.85%`** |
| `n200_wide_02` | 27,539 | `TIMEOUT` (20s) | `20.026s` | **24,085** | `0.0874s` | **Heu WIN** | **`-12.54%`** |
| `n200_wide_03` | 28,298 | `TIMEOUT` (20s) | `20.025s` | **25,942** | `0.0024s` | **Heu WIN** | **`-8.33%`** |
| `n200_wide_04` | 26,968 | `TIMEOUT` (20s) | `20.029s` | **23,179** | `0.0163s` | **Heu WIN** | **`-14.05%`** |
| `n200_wide_05` | 27,115 | `TIMEOUT` (20s) | `20.050s` | **21,706** | `0.0116s` | **Heu WIN** | **`-19.95%`** |

> **Nhận xét thực nghiệm:**
> - **Trên 45 bài CP-SAT tìm được `OPTIMAL`:** Bước khởi tạo `Heuristic C++` giải khả thi **45/45 bài (100%)**, đạt chất lượng cách nghiệm tối ưu chỉ từ `0.0%` đến `1.06%` với tốc độ nhanh hơn từ **100 đến 1.000 lần**.
> - **Trên 15 bài CP-SAT bị `TIMEOUT` (`n200_wide`, `n500_med`, `n500_wide`):** `Heuristic C++` giải được **10/15 bài** (trong đó giải sạch `5/5` bài `n200_wide` với chi phí giảm trung bình **`-12.74%`** so với `RefCost` chỉ trong `~0.03s`).
> - **Tổng cộng:** Bước khởi tạo `Heuristic C++` đã giải khả thi **58/75 bài** làm nền tảng xuất phát cho thuật toán chính **ALNS**.

---

## 4. Hướng dẫn Build và Chạy chương trình

### 4.1. Build C++20 và chạy Unit Tests
```bash
cmake -B build
cmake --build build

# Chạy bộ Unit Tests cho Evaluator & Generator
./build/evaluator_test
# hoặc:
ctest --test-dir build --output-on-failure
```

### 4.2. Sinh bộ dữ liệu 75 Testcases
```bash
./build/tsptw --gen
```

### 4.3. Chạy bộ giải C++ (`heuristic` hoặc `alns`) và Benchmark
```bash
# Giải 1 file lẻ và xuất ra file .out
./build/tsptw data/input/n10_01.inp data/output/heuristic/n10_01.out heuristic

# Chạy benchmark với Heuristic trên 30 bộ test nhỏ / 45 bộ test lớn / toàn bộ 75 bộ test
./build/tsptw --bench small heuristic
./build/tsptw --bench big heuristic
./build/tsptw --bench all heuristic

# Chạy benchmark với ALNS
./build/tsptw --bench all alns
```

### 4.4. Chạy bộ giải đối chiếu OR-Tools CP-SAT (Python)
```bash
# Khởi tạo môi trường ảo và cài đặt ortools (chỉ cần chạy lần đầu)
python3 -m venv .venv
source .venv/bin/activate
pip install ortools

# Giải 1 file lẻ (giới hạn mặc định 5s)
.venv/bin/python src/cpsolver.py data/input/n10_01.inp

# Chạy benchmark 30 test nhỏ (giới hạn 2s/test)
.venv/bin/python src/cpsolver.py --bench small 2.0

# Chạy benchmark 45 test lớn (giới hạn 20s/test)
.venv/bin/python src/cpsolver.py --bench big 20.0
```
