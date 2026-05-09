# LFX_riscv

> **LFX Mentorship: Broadening the RISC-V High Precision Code Base and Reach**

A comprehensive technical submission for the LFX Mentorship coding challenge, featuring a core algorithm implementation and a sophisticated automation suite for validating high-precision numerical libraries on the RISC-V architecture.

**Applicant:** Vaibhav Binwal (IIT Jodhpur)

---

## 🚀 Key Features

### Sysroot Healing Engine
Automated resolution of broken symbolic links in the Ubuntu 24.04 / GCC-13 cross-toolchain environment.

### Numerical Convergence Audit
Extraction and verification of relative errors for ARPACK-ng binaries under QEMU emulation.

### eBPF Observability
Performance profiling using `syscount-bpfcc` to monitor syscall frequency and emulation stability.

### Cross-Platform Portability
Verified execution of recursive algorithms in an ISA-agnostic environment.

---

## 📂 Project Structure

```
.
├── hanoi_reccursion.py                    # Core coding challenge (Tower of Hanoi)
├── arpack-ng/                              # Targeted high-precision code base
│   ├── riscv_validation_engine_v3.py      # Automation & Healing Engine
│   ├── run_arpack_final.sh                # Orchestration & Audit script
│   └── [Source Files]                     # ARPACK-ng driver components
└── final_report.pdf                       # Consolidated Technical Portfolio
```

---

## 🛠️ Getting Started

### Prerequisites

- **Ubuntu 24.04** (Noble Tahr) or compatible Linux distribution
- **riscv64-linux-gnu-gcc** (Version 13)
- **qemu-user-static** for RISC-V emulation
- **bcc tools** for eBPF observability

### Execution

Run the core algorithm audit:

```bash
python3 hanoi_reccursion.py 4
```

Run the full RISC-V validation suite for ARPACK-ng:

```bash
cd arpack-ng
bash run_arpack_final.sh
```

---

## 📊 Validation Results

The engine successfully validated the numerical stability of ARPACK-ng drivers on RISC-V.

| Component | Status    | Relative Error               |
|-----------|-----------|------------------------------|
| dndrv4    | ✅ SUCCESS | $8.48731 \times 10^{-16}$   |

Full results, including eBPF syscall traces and toolchain repair logs, are available in **[final_report.pdf](./final_report.pdf)**.

---

## 📖 Documentation

For detailed implementation specifics and architectural decisions, refer to:
- **Technical Report:** `final_report.pdf`
- **Validation Engine:** `arpack-ng/riscv_validation_engine_v3.py`
- **Orchestration Script:** `arpack-ng/run_arpack_final.sh`

---

## 📝 License

This project is part of the LFX Mentorship Program. For licensing details, refer to the respective projects and documentation.

---

## 👤 Author

**Vaibhav Binwal**  
*Indian Institute of Technology, Jodhpur*
