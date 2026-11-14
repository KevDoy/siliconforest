/* SPDX-License-Identifier: MIT */
/* Own declarations from Finch implementations; no Apple SDK metadata. */
#pragma once
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <Accelerate/DSP.h>
#include <Accelerate/Misc.h>
#if defined(SF_COMPONENT_LAPACK) || defined(SF_COMPONENT_BLAS)
typedef int __LAPACK_int;
#endif
#ifdef SF_COMPONENT_vImage
#include <Accelerate/Image.h>
#endif
#ifdef SF_COMPONENT_Quadrature
#include <Accelerate/Quadrature.h>
#endif
#ifdef SF_COMPONENT_Sparse
#include <Accelerate/Sparse.h>
#endif
#ifndef SF_ACCELERATE_PREPROCESS
#ifdef SF_COMPONENT_BLAS
#include <Accelerate/BLAS-public.h>
#endif
#ifdef SF_COMPONENT_vDSP
#include <Accelerate/vDSP-generated.h>
#endif
#ifdef SF_COMPONENT_vMisc
#include <Accelerate/vMisc-generated.h>
#endif
#ifdef SF_COMPONENT_LAPACK
#include <Accelerate/LAPACK-generated.h>
#endif
#ifdef SF_COMPONENT_vImage
#include <Accelerate/vImage-generated.h>
#endif
#ifdef SF_COMPONENT_Quadrature
#include <Accelerate/Quadrature-generated.h>
#endif
#ifdef SF_COMPONENT_Sparse
#include <Accelerate/Sparse-generated.h>
#endif
#endif
