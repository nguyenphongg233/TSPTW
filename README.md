# TSP with Time Windows (Open-TSPTW) Solver

---

## 1. Mô tả bài toán (Open-TSPTW)

- **Đỉnh xuất phát (Depot):** Mã `0`, xe xuất phát tại thời điểm $F_0 = 0$.
- **Khách hàng:** $N$ khách hàng ($N \le 1000$), đánh số từ `1` đến `N`. Mỗi khách hàng phải được phục vụ đúng 1 lần.
- **Thông số mỗi khách hàng $i = 1, \dots, N$:**
  - $e_i$: Thời điểm sớm nhất được phép bắt đầu phục vụ.
  - $l_i$: Thời điểm muộn nhất phải **hoàn thành** phục vụ.
  - $s_i$: Thời gian phục vụ tại điểm $i$.
- **Ma trận di chuyển:** $t_{u, v}$ kích thước $(N+1) \times (N+1)$ là thời gian/chi phí di chuyển từ điểm $u$ đến điểm $v$.

**Công thức lan truyền thời gian** trên tuyến đường $s = (s_1, s_2, \dots, s_N)$ với $s_0 = 0$:

$$
\begin{aligned}
A_k &= F_{k-1} + t_{s_{k-1}, s_k} \\
B_k &= \max(A_k, e_{s_k}) \\
F_k &= B_k + s_{s_k}
\end{aligned}
$$

Trong đó:
- $A_k$: Thời điểm đến khách hàng $s_k$.
- $B_k$: Thời điểm bắt đầu phục vụ (nếu $A_k < e_{s_k}$ xe phải chờ đến $e_{s_k}$).
- $F_k$: Thời điểm hoàn thành phục vụ tại $s_k$.

**Điều kiện khả thi:** $s$ là một hoán vị của $1, 2, \dots, N$ và thỏa mãn:

$$
F_k \le l_{s_k}, \quad \forall k = 1, \dots, N
$$

**Hàm mục tiêu (Open Route):** Xe **không cần quay về depot `0`** sau khi phục vụ khách hàng cuối cùng $s_N$:

$$
\min \text{travelCost} = \sum_{k=1}^{N} t_{s_{k-1}, s_k}
$$

---

## 2. Thiết kế thuật toán ALNS hiện tại

Thuật toán **Adaptive Large Neighborhood Search (ALNS)** được cài đặt trong [`include/alns.hpp`](include/alns.hpp) và [`src/alns.cpp`](src/alns.cpp) theo quy trình 4 thành phần:

### 2.1. Khởi tạo nghiệm ban đầu (`buildInitialSolution`)
- Kết hợp **Lookahead Nearest Feasible Neighbor** (ưu tiên các đỉnh không làm trễ hạn 25 đỉnh gấp nhất) và **Randomized Multi-Start Greedy Insertion** (xếp theo $l_i$, $(e_i+l_i)/2$, $e_i+s_i$ kèm nhiễu ngẫu nhiên và Ejection Repair).
- Nếu chưa chèn đủ $N$ đỉnh ở bước khởi tạo, ALNS sẽ giữ lại phần tuyến khả thi lớn nhất cùng danh sách `uninserted` để tiếp tục sửa trong vòng lặp chính.

### 2.2. Các toán tử Destroy (Tháo $q \in [2\%, 15\%]$ khách hàng)
1. **`Random Removal`:** Xóa ngẫu nhiên $q$ khách hàng để tạo sự đa dạng cho không gian tìm kiếm.
2. **`Worst Removal`:** Tính chi phí đi vòng $\text{detour}(i) = t_{\text{prev}, i} + t_{i, \text{next}} - t_{\text{prev}, \text{next}}$ (cộng nhiễu ngẫu nhiên), ưu tiên tháo các đỉnh đang gây tốn chi phí di chuyển lớn nhất.
3. **`Segment Removal`:** Xóa một đoạn $q$ khách hàng liên tiếp trên tuyến để tái cấu trúc cục bộ một vùng hành trình.

### 2.3. Các toán tử Repair (Lắp lại các khách hàng vừa tháo)
1. **`Greedy Insertion`:** Lần lượt chèn từng đỉnh vào vị trí khả thi tốt nhất theo điểm đánh giá kết hợp chi phí tăng thêm (`deltaCost`) và độ trễ đẩy về sau (`pushedDelay`).
2. **`Two-Regret Insertion`:** Tính độ hối tiếc $\text{regret}(u) = \text{score}_2(u) - \text{score}_1(u)$ giữa vị trí tốt nhì và tốt nhất; ưu tiên chèn trước các đỉnh chỉ còn đúng 1 vị trí khả thi hoặc có `regret` lớn nhất để tránh bế tắc Time Window.
- **Tối ưu tốc độ đánh giá vị trí chèn:** Dùng tìm kiếm nhị phân (`std::lower_bound` / `std::upper_bound`) trên mảng `finishTime` để thu hẹp khoảng vị trí hợp lệ $[minPos, maxPos]$ và cập nhật hậu tố `finishTime` cục bộ (`updatePartialFinish`).

### 2.4. Điều phối thích nghi & Simulated Annealing
- **Tìm kiếm 2 giai đoạn:**
  - *Giai đoạn 1 (khi chưa tìm được nghiệm đủ $N$ khách):* Thưởng điểm cao cho phép biến làm giảm số đỉnh `uninserted`.
  - *Giai đoạn 2 (khi đã có nghiệm khả thi):* Tối ưu `travelCost` trên `currentSol`, tách biệt hoàn toàn với `bestSol`.
- **Chấp nhận nghiệm (Simulated Annealing):** Chấp nhận nghiệm tốt hơn ($\Delta < 0$) hoặc chấp nhận nghiệm xấu hơn với xác suất $e^{-\Delta / T}$ ($T$ giảm dần theo `coolingRate`).
- **Adaptive Weights (Roulette Wheel):** Sau mỗi 50 vòng lặp, cập nhật trọng số `weight` của từng toán tử dựa trên tỷ lệ điểm thưởng `periodScore / periodUses`. Ghi log hội tụ (`#iter`, `obj`, `time`, `op`) và bảng thống kê từng toán tử vào `results/logs/`.

---

## 3. Cấu trúc thư mục

```text
TSPTW/
├── CMakeLists.txt                       # Build C++20 (-O3 -Wall -Wextra)
├── README.md                            # Tài liệu tổng quan và kết quả thực nghiệm
├── include/
│   ├── problem.hpp                      # Struct Customer, Problem (flat 1D travel matrix)
│   ├── solution.hpp                     # Struct Solution (route, finishTime, print, debug)
│   ├── evaluator.hpp                    # EvalResult, isValidPermutation, evaluateRoute, evaluateSuffix
│   ├── utils.hpp                        # Timer, TestGenConfig, SolverConfig, Dataset Generator
│   ├── heuristic.hpp                    # Khởi tạo nghiệm ban đầu (Randomized Multi-Start)
│   └── alns.hpp                         # Cây kế thừa Operator, 5 toán tử và struct ALNS
├── src/
│   ├── problem.cpp                      # Đọc dữ liệu từ file .inp / istream
│   ├── evaluator.cpp                    # Kiểm tra hoán vị, Time Window & Incremental Suffix Eval
│   ├── utils.cpp                        # Bộ sinh 75 testcase đảm bảo 100% có nghiệm khả thi
│   ├── heuristic.cpp                    # Lookahead Nearest Neighbor + Randomized Greedy Insertion
│   ├── alns.cpp                         # Cài đặt ALNS, 5 toán tử Destroy/Repair & ghi log
│   ├── main.cpp                         # CLI (--gen, --bench đa luồng -j, giải file lẻ -t)
│   └── cpsolver.py                      # Bộ giải đối chiếu đa nhân bằng Google OR-Tools CP-SAT
├── tests/
│   └── evaluator_test.cpp               # 9 Unit Tests cho Evaluator, Suffix Eval & Test Generator
├── data/
│   ├── input/                           # 75 file .inp + reference_costs.csv
│   └── output/
│       ├── cpSolver/                    # Tuyến đường .out giải bởi OR-Tools CP-SAT
│       ├── heuristic/                   # Tuyến đường .out giải bởi Heuristic C++
│       └── alns/                        # Tuyến đường .out giải bởi ALNS C++
└── results/
    ├── cp_benchmark_results.csv         # Kết quả chạy OR-Tools CP-SAT (30s/bài)
    ├── heuristic_benchmark_results.csv  # Kết quả chạy Heuristic C++
    ├── alns_benchmark_results.csv       # Kết quả chạy ALNS C++ (3s/bài)
    └── logs/                            # Log hội tụ theo từng iteration & thống kê toán tử
```

---

## 4. Tổng kết Benchmark trên 75 Testcases

Bộ dữ liệu gồm **75 instances** (`30` bài nhỏ $N \in \{10, 20, 50\}$ và `45` bài lớn $N \in \{200, 500, 1000\}$ chia làm 3 mức độ rộng khung thời gian `tight=60`, `med=350`, `wide=2000`), kèm mốc tham chiếu khả thi `RefCost` trong `data/input/reference_costs.csv`.

### 4.1. Tổng quan hiệu năng 3 bộ giải

| Tiêu chí | `OR-Tools CP-SAT` (`30s` / bài, đa nhân) | `Heuristic Khởi tạo` (C++20) | **`ALNS` (C++20, `3s` / bài)** |
| :--- | :---: | :---: | :---: |
| **Tỷ lệ giải khả thi** | `45 / 75` (`60.0%`) | `58 / 75` (`77.3%`) | **`75 / 75` (`100.0%`)** |
| **Chất lượng trên 45 bài CP có nghiệm** | `39` `OPTIMAL`, `6` `FEASIBLE` | `25 / 45` bài bằng `CPCost` | **`45 / 45` bài bằng `100% CPCost` (`0.00% Gap`)** |
| **30 bài CP `NOT_FEASIBLE` (`30s`)** | `0 / 30` bài giải được | `13 / 30` bài khả thi | **`30 / 30` bài khả thi** (giảm tới **`-26.36%`** vs `RefCost`) |
| **Tổng thời gian chạy thực tế (75 bài)** | `243.81 giây` (5 procs $\times$ 3 workers) | `40.70 giây` (1 thread) | **`15.09 giây`** (15 threads) |

### 4.2. So sánh chi tiết theo từng nhóm Testcase

| Nhóm Test | Số bài | `RefCost` (TB) | `CPCost` (`30s`) | `HeuCost` (TB) | **`ALNSCost` (`3s`)** | **`ALNS vs CP`** | **`ALNS vs Ref`** |
| :--- | :-: | :-: | :-: | :-: | :-: | :-: | :-: |
| **`n10` (`01..10`)** | 10 | 4,888 | 4,869 (`10/10 OPT`) | 4,871 (`10/10`) | **4,869 (`10/10`)** | **`+0.00%`** | `-0.39%` |
| **`n20` (`01..10`)** | 10 | 7,535 | 7,386 (`10/10 OPT`) | 7,396 (`10/10`) | **7,386 (`10/10`)** | **`+0.00%`** | `-1.98%` |
| **`n50` (`01..10`)** | 10 | 13,714 | 13,261 (`10/10 OPT`) | 13,400 (`10/10`) | **13,261 (`10/10`)** | **`+0.00%`** | `-3.30%` |
| **`n200_tight`** | 5 | 27,076 | 27,065 (`5/5 OPT`) | 27,069 (`5/5`) | **27,065 (`5/5`)** | **`+0.00%`** | `-0.04%` |
| **`n200_med`** | 5 | 26,767 | 25,403 (`5/5 FEAS`) | 25,673 (`5/5`) | **25,403 (`5/5`)** | **`+0.00%`** | **`-5.10%`** |
| **`n200_wide`** | 5 | 27,283 | `NOT_FEASIBLE` (`0/5`) | 23,812 (`5/5`) | **21,045 (`5/5`)** | **ALNS THẮNG 5/5** | **`-22.86%`** |
| **`n500_tight`** | 5 | 42,600 | 42,515 (`4 OPT, 1 FEAS`) | 42,582 (`5/5`) | **42,515 (`5/5`)** | **`+0.00%`** | `-0.20%` |
| **`n500_med`** | 5 | 42,818 | `NOT_FEASIBLE` (`0/5`) | 41,056 (`2/5`) | **39,970 (`5/5`)** | **ALNS THẮNG 5/5** | **`-6.65%`** |
| **`n500_wide`** | 5 | 43,486 | `NOT_FEASIBLE` (`0/5`) | 38,607 (`3/5`) | **33,857 (`5/5`)** | **ALNS THẮNG 5/5** | **`-22.14%`** |
| **`n1000_tight`** | 5 | 60,420 | `NOT_FEASIBLE` (`0/5`) | 60,072 (`3/5`) | **60,203 (`5/5`)** | **ALNS THẮNG 5/5** | `-0.36%` |
| **`n1000_med`** | 5 | 60,969 | `NOT_FEASIBLE` (`0/5`) | `NO` (`0/5`) | **55,889 (`5/5`)** | **ALNS THẮNG 5/5** | **`-8.33%`** |
| **`n1000_wide`** | 5 | 59,548 | `NOT_FEASIBLE` (`0/5`) | `NO` (`0/5`) | **46,865 (`5/5`)** | **ALNS THẮNG 5/5** | **`-21.30%`** |

> **Kết luận thực nghiệm:**
> - **Độ chính xác:** Trên toàn bộ 45 bài toán mà `OR-Tools CP-SAT` giải được (gồm 39 nghiệm `OPTIMAL` tuyệt đối), **ALNS đạt `0.00%` Gap trên cả 45/45 bài** chỉ với `3.0s`/bài.
> - **Khả năng mở rộng (Scalability):** Khi $N \ge 200$ với khung thời gian rộng (`wide`) hoặc $N \ge 500$ (`med`, `wide`, `n1000`), `CP-SAT` gặp bùng nổ tổ hợp ($O(N^2)$ biến cạnh) dẫn đến `NOT_FEASIBLE` sau 30 giây, trong khi **ALNS** vẫn đạt **tỷ lệ khả thi 100% (`75/75`)** và giảm chi phí tới **21%–26%** so với nghiệm tham chiếu.

---

## 5. Hướng dẫn Build và Chạy chương trình

### 5.1. Build C++20 và chạy Unit Tests
```bash
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

### 5.2. Sinh bộ dữ liệu 75 Testcases
```bash
./build/tsptw --gen
```

### 5.3. Chạy bộ giải C++ (Hỗ trợ `-t` giới hạn thời gian & `-j` chạy đa luồng)
```bash
# Giải 1 file lẻ bằng ALNS với giới hạn 5 giây (log lưu ở results/logs/last_run.log)
./build/tsptw data/input/n1000_wide_01.inp data/output/alns/n1000_wide_01.out alns -t 5.0

# Chạy benchmark song song toàn bộ 75 bài với ALNS (mỗi bài 3s, tự động dùng hết nhân CPU)
./build/tsptw --bench all alns -t 3.0

# Chạy benchmark với số luồng tùy chỉnh (ví dụ 8 luồng) trên 45 bài lớn
./build/tsptw --bench big alns -t 5.0 -j 8

# Chạy benchmark bằng Heuristic khởi tạo
./build/tsptw --bench all heuristic
```

### 5.4. Chạy bộ giải đối chiếu OR-Tools CP-SAT (Python)
```bash
python3 -m venv .venv
source .venv/bin/activate
pip install ortools

# Giải 1 file lẻ (giới hạn 30s, 8 search workers)
.venv/bin/python src/cpsolver.py data/input/n10_01.inp 30.0 8

# Chạy benchmark song song toàn bộ 75 bài (30s/bài, 5 tiến trình x 3 workers)
.venv/bin/python src/cpsolver.py --bench all 30.0 5 3
```
