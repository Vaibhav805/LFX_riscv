#!/usr/bin/env python3
"""HPC riscv64 ecosystem audit matrix.

This file keeps the compact package scoreboard and the expanded 40-package
macro-dependency database in one importable place. It prints JSON by default so
reports can consume the same data without scraping Markdown.
"""

from __future__ import annotations

import json
from collections import Counter


PACKAGES = {
    "OpenBLAS": {
        "version": "0.3.26",
        "domain": "BLAS",
        "deb": True,
        "rvv": True,
        "rvv_opcode_count": 724,
        "dgemm_error": "2.1664e-15",
        "blas_levels": "L1+L2+L3",
        "status": "VERIFIED_RVV",
    },
    "ARPACK-ng": {
        "version": "3.9.1",
        "domain": "LA",
        "deb": True,
        "rvv": True,
        "drivers": "dsbdr/dndrv/dsdrv validation target",
    },
    "LAPACK": {
        "version": "3.12.0",
        "domain": "LA",
        "deb": True,
        "routines": "dgels+dgesv validation target",
    },
    "SPOOLES": {
        "version": "2.2",
        "domain": "Sparse",
        "deb": True,
        "fix": "-fcommon",
    },
    "PETSc": {
        "version": "3.21",
        "domain": "PDE",
        "deb": False,
        "blocked_on": "LAPACK+OpenBLAS",
        "status": "UNBLOCKED_READY",
    },
    "ScaLAPACK": {
        "version": "2.2",
        "domain": "LA",
        "deb": False,
        "blocked_on": "LAPACK+MPI",
        "status": "PENDING",
    },
    "CalculiX": {
        "version": "2.21",
        "domain": "FEM",
        "deb": False,
        "blocked_on": "SPOOLES+ARPACK",
        "status": "UNBLOCKED_READY",
    },
    "MUMPS": {
        "version": "5.6",
        "domain": "Sparse",
        "deb": False,
        "blocked_on": "LAPACK+ScaLAPACK",
        "status": "PENDING",
    },
    "Eigen": {
        "version": "3.4",
        "domain": "LA",
        "deb": False,
        "note": "header-only, zero deps",
        "status": "TRIVIAL",
    },
    "SuiteSparse": {
        "version": "7.4",
        "domain": "Sparse",
        "deb": False,
        "blocked_on": "LAPACK",
        "status": "UNBLOCKED_READY",
    },
}


EXTENDED_HPC_ECOSYSTEM = {
    "OpenBLAS": {"layer": 1, "status": "VERIFIED_RVV", "deps": [], "domain": "Low-Level Dense Linear Algebra"},
    "LAPACK": {"layer": 1, "status": "VERIFIED_BUILD", "deps": ["OpenBLAS"], "domain": "Dense Linear Algebra Solvers"},
    "ARPACK-ng": {"layer": 1, "status": "VERIFIED_NATIVE", "deps": ["LAPACK"], "domain": "Sparse Eigenvalue Extremum Computations"},
    "SPOOLES": {"layer": 1, "status": "VERIFIED_GCC13_FIX", "deps": [], "domain": "Sparse Symmetric Linear Solver"},
    "ScaLAPACK": {"layer": 2, "status": "UNBLOCKED_READY", "deps": ["LAPACK"], "domain": "Distributed-Memory Dense Solvers"},
    "SuiteSparse": {"layer": 2, "status": "UNBLOCKED_READY", "deps": ["LAPACK"], "domain": "Sparse Matrix Factorization Suite"},
    "PETSc": {"layer": 2, "status": "UNBLOCKED_READY", "deps": ["LAPACK"], "domain": "Partial Differential Equation Solvers"},
    "MUMPS": {"layer": 2, "status": "UNBLOCKED_READY", "deps": ["ScaLAPACK"], "domain": "Distributed Multifrontal Sparse Solver"},
    "SuperLU": {"layer": 2, "status": "UNBLOCKED_READY", "deps": ["OpenBLAS"], "domain": "Sparse LU Factorization Engine"},
    "Eigen": {"layer": 2, "status": "TRIVIAL_PASSTHROUGH", "deps": [], "domain": "C++ Template Linear Algebra Library"},
    "FFTW3": {"layer": 2, "status": "PORTING_EVAL", "deps": [], "domain": "Fast Fourier Transform Engine"},
    "NetCDF": {"layer": 2, "status": "UNBLOCKED_READY", "deps": ["HDF5"], "domain": "Scientific Array Data Protocol Layer"},
    "HDF5": {"layer": 2, "status": "UNBLOCKED_READY", "deps": [], "domain": "Hierarchical Scientific Data Format Layer"},
    "METIS": {"layer": 2, "status": "UNBLOCKED_READY", "deps": [], "domain": "Graph/Mesh Partitioning Tool"},
    "Trilinos": {"layer": 2, "status": "PENDING_UPSTREAM", "deps": ["PETSc"], "domain": "Object-Oriented Scientific Solver Engine"},
    "GSL": {"layer": 2, "status": "UNBLOCKED_READY", "deps": ["OpenBLAS"], "domain": "GNU Scientific Library Basics"},
    "CalculiX": {"layer": 3, "status": "UNBLOCKED_READY", "deps": ["SPOOLES"], "domain": "Structural Mechanics Finite Element Code"},
    "OpenFOAM": {"layer": 3, "status": "PORTING_EVAL", "deps": ["PETSc"], "domain": "Computational Fluid Dynamics Engine"},
    "Code_Aster": {"layer": 3, "status": "PENDING_UPSTREAM", "deps": ["MUMPS"], "domain": "Nuclear Structural Design & Deformation"},
    "ElmerFEM": {"layer": 3, "status": "UNBLOCKED_READY", "deps": ["LAPACK"], "domain": "Multi-Physics Finite Element Framework"},
    "SU2": {"layer": 3, "status": "PORTING_EVAL", "deps": ["OpenBLAS"], "domain": "Aerodynamic Shape Design Solver"},
    "deal.II": {"layer": 3, "status": "PENDING_UPSTREAM", "deps": ["Trilinos"], "domain": "Differential Equation Discretization Suite"},
    "FEniCS": {"layer": 3, "status": "UNBLOCKED_READY", "deps": ["PETSc"], "domain": "Automated Partial Differential Solvers"},
    "Chrono": {"layer": 3, "status": "PORTING_EVAL", "deps": ["Eigen"], "domain": "Multi-Body Physics Simulation System"},
    "GROMACS": {"layer": 4, "status": "PORTING_EVAL", "deps": ["FFTW3"], "domain": "Biomolecular Trajectory Engine"},
    "NWChem": {"layer": 4, "status": "PENDING_UPSTREAM", "deps": ["ScaLAPACK"], "domain": "Ab Initio Molecular Orbitals Core"},
    "CP2K": {"layer": 4, "status": "PENDING_UPSTREAM", "deps": ["ScaLAPACK"], "domain": "Solid State Physics Atomistic Simulations"},
    "QuantumESPRESSO": {"layer": 4, "status": "UNBLOCKED_READY", "deps": ["LAPACK"], "domain": "Electronic-Structure Materials Software"},
    "LAMMPS": {"layer": 4, "status": "UNBLOCKED_READY", "deps": ["FFTW3"], "domain": "Large-scale Atomic Simulation Engine"},
    "NAMD": {"layer": 4, "status": "PORTING_EVAL", "deps": [], "domain": "High-Performance Molecular Dynamics"},
    "ORCA": {"layer": 4, "status": "PENDING_UPSTREAM", "deps": ["OpenBLAS"], "domain": "Quantum Chemistry Spectroscopy Matrix Solver"},
    "GAMESS": {"layer": 4, "status": "PENDING_UPSTREAM", "deps": ["LAPACK"], "domain": "Advanced Molecular Quantum Computations"},
    "WRF": {"layer": 5, "status": "PORTING_EVAL", "deps": ["NetCDF"], "domain": "Weather Research and Forecasting Model"},
    "CFITSIO": {"layer": 5, "status": "TRIVIAL_PASSTHROUGH", "deps": [], "domain": "FITS Astronomical Image Data Streamer"},
    "HEALPix": {"layer": 5, "status": "UNBLOCKED_READY", "deps": ["CFITSIO"], "domain": "Cosmic Microwave Background Grid Analytics"},
    "Gadget": {"layer": 5, "status": "PORTING_EVAL", "deps": ["FFTW3"], "domain": "Cosmological N-Body Hydrodynamics solver"},
    "OpenCV": {"layer": 5, "status": "PORTING_EVAL", "deps": ["Eigen"], "domain": "Computer Vision Transformation Matrices"},
    "PROJ": {"layer": 5, "status": "TRIVIAL_PASSTHROUGH", "deps": [], "domain": "Cartographic Coordinate Transformations"},
    "GDAL": {"layer": 5, "status": "UNBLOCKED_READY", "deps": ["PROJ"], "domain": "Geospatial Data Abstraction Engine"},
    "QGIS_Core": {"layer": 5, "status": "PENDING_UPSTREAM", "deps": ["GDAL"], "domain": "Geographical Mapping Computation Engine"},
}


def summarize() -> dict:
    layer_counts = Counter(pkg["layer"] for pkg in EXTENDED_HPC_ECOSYSTEM.values())
    status_counts = Counter(pkg["status"] for pkg in EXTENDED_HPC_ECOSYSTEM.values())
    return {
        "compact_package_count": len(PACKAGES),
        "extended_package_count": len(EXTENDED_HPC_ECOSYSTEM),
        "layers": dict(sorted(layer_counts.items())),
        "statuses": dict(sorted(status_counts.items())),
    }


def main() -> None:
    print(json.dumps({
        "summary": summarize(),
        "packages": PACKAGES,
        "extended_hpc_ecosystem": EXTENDED_HPC_ECOSYSTEM,
    }, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
