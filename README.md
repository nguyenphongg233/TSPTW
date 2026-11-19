# TSP with Time Windows (Open-TSPTW) Solver

Dự án môn **IT4663 — Tối ưu lập kế hoạch (HUST)**: Xây dựng bộ giải bài toán **Traveling Salesman Problem with Time Windows (biến thể đường đi mở - Open Route)** bằng **C++20 (ALNS)** và đối chiếu thực nghiệm với **Google OR-Tools CP-SAT**.

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
├── CMakeLists.txt              # Cấu hình build C++20 (tsptw_core, tsptw, evaluator_test)
├── README.md                   # Tài liệu tổng quan và hướng dẫn sử dụng
├── include/
│   ├── problem.hpp             # [Đã xong] Struct Customer, Problem (flat 1D matrix)
│   ├── solution.hpp            # [Đã xong] Struct Solution (route, finishTime, print, debug)
│   ├── evaluator.hpp           # [Đã xong] Struct EvalResult, đánh giá O(N) & evaluateSuffix
│   ├── utils.hpp               # [Đã xong] Timer, TestGenConfig, SolverConfig, Dataset Generator
│   ├── heuristic.hpp           # [Sắp làm] Khởi tạo nghiệm ban đầu
│   ├── local_search.hpp        # [Sắp làm] Các toán tử tìm kiếm cục bộ
│   └── alns.hpp                # [Sắp làm] Thuật toán chính Adaptive Large Neighborhood Search
├── src/
│   ├── problem.cpp             # [Đã xong] Parser đọc dữ liệu từ file .inp / istream
│   ├── evaluator.cpp           # [Đã xong] Kiểm tra hoán vị, Time Window & Incremental Suffix Eval
│   ├── utils.cpp               # [Đã xong] Bộ sinh 75 testcase đảm bảo 100% có nghiệm khả thi
│   ├── main.cpp                # [Đã xong] CLI điều phối (--gen, --bench, giải file lẻ)
│   ├── cpsolver.py             # [Đã xong] Bộ giải đối chiếu bằng Google OR-Tools CP-SAT
│   ├── heuristic.cpp           # [Sắp làm]
│   ├── local_search.cpp        # [Sắp làm]
│   └── alns.cpp                # [Sắp làm]
├── tests/
│   └── evaluator_test.cpp      # [Đã xong] 9 Unit Tests cho Evaluator, Suffix Eval & Test Generator
├── data/
│   ├── input/                  # Chứa 75 file .inp + bảng mốc tham chiếu reference_costs.csv
│   └── output/
│       ├── cpSolver/           # Kết quả tuyến đường .out giải bởi OR-Tools CP-SAT
│       └── alns/               # Kết quả tuyến đường .out giải bởi chương trình C++
└── results/
    ├── cp_benchmark_results.csv # Bảng thống kê kết quả chạy OR-Tools CP-SAT
    └── benchmark_results.csv    # Bảng thống kê kết quả chạy bộ giải C++
```

---

## 3. Bộ dữ liệu thực nghiệm (75 Testcases) & Baseline CP-SAT

Bộ dữ liệu gồm **75 instances** được sinh tự động bằng phương pháp *Planted Semi-Greedy Route* (đảm bảo 100% testcase đều có nghiệm khả thi và lưu lại cận trên tham chiếu `RefCost` trong `data/input/reference_costs.csv`):

1. **Bộ test nhỏ (30 files):** `n10_01..10.inp`, `n20_01..10.inp`, `n50_01..10.inp` (`windowSlack` trải từ `50`, `250` đến `1000`).
2. **Bộ test lớn (45 files):** $N \in \{200, 500, 1000\} \times \{\text{tight } (60), \text{med } (350), \text{wide } (2000)\} \times 5 \text{ instances } (01..05)$.

### Kết quả đối chiếu Baseline bằng OR-Tools CP-SAT (Time limit: 20s/test)

| Nhóm Test | Số test | `windowSlack` | Kết quả CP-SAT | `RefCost` (TB) | `CPCost` (TB) | Độ lệch so với `RefCost` | Thời gian CP (TB) |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **`n10_01..10`** | 10 | 50 – 1000 | **10/10 OPTIMAL** | 4,888 | 4,869 | `-0.39%` (Tốt nhất `-3.65%`) | `0.0023s` |
| **`n20_01..10`** | 10 | 50 – 1000 | **10/10 OPTIMAL** | 7,535 | 7,386 | `-1.98%` (Tốt nhất `-13.62%`) | `0.0043s` |
| **`n50_01..10`** | 10 | 50 – 1000 | **10/10 OPTIMAL** | 13,714 | 13,261 | `-3.30%` (Tốt nhất `-13.77%`) | `0.0788s` |
| **`n200_tight`** | 5 | 60 | **5/5 OPTIMAL** | 27,076 | 27,065 | `-0.04%` (Tốt nhất `-0.17%`) | `0.1206s` |
| **`n200_med`** | 5 | 350 | **5/5 OPTIMAL** | 26,767 | 25,403 | **`-5.10%`** (Tốt nhất `-6.32%`) | `4.2643s` |
| **`n200_wide`** | 5 | 2000 | **0/5 (Timeout)** | 27,283 | — | Không tìm được nghiệm khả thi | `20.0427s` |
| **`n500_tight`** | 5 | 60 | **5/5 OPTIMAL** | 42,600 | 42,515 | `-0.20%` (Tốt nhất `-0.46%`) | `6.4025s` |
| **`n500_med`** | 5 | 350 | **0/5 (Timeout)** | 42,818 | — | Không tìm được nghiệm khả thi | `20.1471s` |
| **`n500_wide`** | 5 | 2000 | **0/5 (Timeout)** | 43,486 | — | Không tìm được nghiệm khả thi | `20.1810s` |

> **Nhận xét:**
> - Trên **45 bộ test** nhỏ và khung thời gian hẹp (`tight`), CP-SAT tìm được **nghiệm tối ưu tuyệt đối (`OPTIMAL`)** nhờ cắt tỉa được các cạnh vi phạm $e_i + s_i + t_{i,j} + s_j > l_j$.
> - Khi quy mô tăng lên ($N \ge 200$) và khung thời gian mở rộng (`med`, `wide`), số biến cạnh khả thi tăng bậc hai $O(N^2)$ khiến CP-SAT bùng nổ tổ hợp và không tìm được nghiệm khả thi trong 20 giây — đây là động lực chính để phát triển thuật toán **ALNS** bằng C++20.

---

## 4. Hướng dẫn Build và Chạy chương trình

### 4.1. Build C++20 và chạy Unit Tests
```bash
# Cấu hình và biên dịch vào thư mục build/
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
*(Tự động tạo 75 file `.inp` cùng bảng `data/input/reference_costs.csv` trong ~1.5 giây).*

### 4.3. Chạy bộ giải C++ và Benchmark
```bash
# Giải 1 file lẻ và xuất ra file .out
./build/tsptw data/input/n10_01.inp data/output/alns/n10_01.out

# Chạy benchmark trên 30 bộ test nhỏ (n10, n20, n50)
./build/tsptw --bench small

# Chạy benchmark trên 45 bộ test lớn (n200, n500, n1000)
./build/tsptw --bench big

# Chạy benchmark toàn bộ 75 bộ test
./build/tsptw --bench all
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
