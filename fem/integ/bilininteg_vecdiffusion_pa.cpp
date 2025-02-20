// Copyright (c) 2010-2024, Lawrence Livermore National Security, LLC. Produced
// at the Lawrence Livermore National Laboratory. All Rights reserved. See files
// LICENSE and NOTICE for details. LLNL-CODE-806117.
//
// This file is part of the MFEM library. For more information and source code
// availability visit https://mfem.org.
//
// MFEM is free software; you can redistribute it and/or modify it under the
// terms of the BSD-3 license. We welcome feedback and contributions, see file
// CONTRIBUTING.md for details.

#include "../../general/forall.hpp"
#include "../bilininteg.hpp"
#include "../gridfunc.hpp"
#include "../qfunction.hpp"
#include "../ceed/integrators/diffusion/diffusion.hpp"

namespace mfem
{

// PA Diffusion Assemble 2D kernel
static void PAVectorDiffusionSetup2D(const int Q1D,
                                     const int NE,
                                     const Array<real_t> &w,
                                     const Vector &j,
                                     const Vector &c,
                                     Vector &op)
{
   const int NQ = Q1D*Q1D;
   auto W = w.Read();

   auto J = Reshape(j.Read(), NQ, 2, 2, NE);
   auto y = Reshape(op.Write(), NQ, 3, NE);

   const bool const_c = c.Size() == 1;
   const auto C = const_c ? Reshape(c.Read(), 1,1) :
                  Reshape(c.Read(), NQ, NE);


   mfem::forall(NE, [=] MFEM_HOST_DEVICE (int e)
   {
      for (int q = 0; q < NQ; ++q)
      {
         const real_t J11 = J(q,0,0,e);
         const real_t J21 = J(q,1,0,e);
         const real_t J12 = J(q,0,1,e);
         const real_t J22 = J(q,1,1,e);

         const real_t C1 = const_c ? C(0,0) : C(q,e);
         const real_t c_detJ = W[q] * C1 / ((J11*J22)-(J21*J12));
         y(q,0,e) =  c_detJ * (J12*J12 + J22*J22); // 1,1
         y(q,1,e) = -c_detJ * (J12*J11 + J22*J21); // 1,2
         y(q,2,e) =  c_detJ * (J11*J11 + J21*J21); // 2,2
      }
   });
}

// PA Diffusion Assemble 3D kernel
static void PAVectorDiffusionSetup3D(const int Q1D,
                                     const int NE,
                                     const Array<real_t> &w,
                                     const Vector &j,
                                     const Vector &c,
                                     Vector &op)
{
   const int NQ = Q1D*Q1D*Q1D;
   auto W = w.Read();
   auto J = Reshape(j.Read(), NQ, 3, 3, NE);
   auto y = Reshape(op.Write(), NQ, 6, NE);

   const bool const_c = c.Size() == 1;
   const auto C = const_c ? Reshape(c.Read(), 1,1) :
                  Reshape(c.Read(), NQ,NE);


   mfem::forall(NE, [=] MFEM_HOST_DEVICE (int e)
   {
      for (int q = 0; q < NQ; ++q)
      {
         const real_t J11 = J(q,0,0,e);
         const real_t J21 = J(q,1,0,e);
         const real_t J31 = J(q,2,0,e);
         const real_t J12 = J(q,0,1,e);
         const real_t J22 = J(q,1,1,e);
         const real_t J32 = J(q,2,1,e);
         const real_t J13 = J(q,0,2,e);
         const real_t J23 = J(q,1,2,e);
         const real_t J33 = J(q,2,2,e);
         const real_t detJ = J11 * (J22 * J33 - J32 * J23) -
                             J21 * (J12 * J33 - J32 * J13) +
                             J31 * (J12 * J23 - J22 * J13);

         const real_t C1 = const_c ? C(0,0) : C(q,e);

         const real_t c_detJ = W[q] * C1 / detJ;
         // adj(J)
         const real_t A11 = (J22 * J33) - (J23 * J32);
         const real_t A12 = (J32 * J13) - (J12 * J33);
         const real_t A13 = (J12 * J23) - (J22 * J13);
         const real_t A21 = (J31 * J23) - (J21 * J33);
         const real_t A22 = (J11 * J33) - (J13 * J31);
         const real_t A23 = (J21 * J13) - (J11 * J23);
         const real_t A31 = (J21 * J32) - (J31 * J22);
         const real_t A32 = (J31 * J12) - (J11 * J32);
         const real_t A33 = (J11 * J22) - (J12 * J21);
         // detJ J^{-1} J^{-T} = (1/detJ) adj(J) adj(J)^T
         y(q,0,e) = c_detJ * (A11*A11 + A12*A12 + A13*A13); // 1,1
         y(q,1,e) = c_detJ * (A11*A21 + A12*A22 + A13*A23); // 2,1
         y(q,2,e) = c_detJ * (A11*A31 + A12*A32 + A13*A33); // 3,1
         y(q,3,e) = c_detJ * (A21*A21 + A22*A22 + A23*A23); // 2,2
         y(q,4,e) = c_detJ * (A21*A31 + A22*A32 + A23*A33); // 3,2
         y(q,5,e) = c_detJ * (A31*A31 + A32*A32 + A33*A33); // 3,3
      }
   });
}

static void PAVectorDiffusionSetup(const int dim,
                                   const int Q1D,
                                   const int NE,
                                   const Array<real_t> &W,
                                   const Vector &J,
                                   const Vector &C,
                                   Vector &op)
{
   if (!(dim == 2 || dim == 3))
   {
      MFEM_ABORT("Dimension not supported.");
   }
   if (dim == 2)
   {
      PAVectorDiffusionSetup2D(Q1D, NE, W, J, C, op);
   }
   if (dim == 3)
   {
      PAVectorDiffusionSetup3D(Q1D, NE, W, J, C, op);
   }
}

void VectorDiffusionIntegrator::AssemblePA(const FiniteElementSpace &fes)
{
   // Assumes tensor-product elements
   Mesh *mesh = fes.GetMesh();
   const FiniteElement &el = *fes.GetTypicalFE();
   const IntegrationRule *ir
      = IntRule ? IntRule : &DiffusionIntegrator::GetRule(el, el);
   if (DeviceCanUseCeed())
   {
      delete ceedOp;
      const bool mixed = mesh->GetNumGeometries(mesh->Dimension()) > 1 ||
                         fes.IsVariableOrder();
      if (mixed)
      {
         ceedOp = new ceed::MixedPADiffusionIntegrator(*this, fes, Q);
      }
      else
      {
         ceedOp = new ceed::PADiffusionIntegrator(fes, *ir, Q);
      }
      return;
   }
   const int dims = el.GetDim();
   const int symmDims = (dims * (dims + 1)) / 2; // 1x1: 1, 2x2: 3, 3x3: 6
   const int nq = ir->GetNPoints();
   dim = mesh->Dimension();
   sdim = mesh->SpaceDimension();
   ne = fes.GetNE();
   geom = mesh->GetGeometricFactors(*ir, GeometricFactors::JACOBIANS);
   maps = &el.GetDofToQuad(*ir, DofToQuad::TENSOR);
   dofs1D = maps->ndof;
   quad1D = maps->nqpt;
   pa_data.SetSize(symmDims * nq * ne, Device::GetDeviceMemoryType());

   MFEM_VERIFY(!VQ && !MQ,
               "Only scalar coefficient supported for partial assembly for VectorDiffusionIntegrator");

   QuadratureSpace qs(*mesh, *ir);
   CoefficientVector coeff(Q, qs, CoefficientStorage::COMPRESSED);

   const Array<real_t> &w = ir->GetWeights();
   const Vector &j = geom->J;
   Vector &d = pa_data;
   if (dim == 1) { MFEM_ABORT("dim==1 not supported in PAVectorDiffusionSetup"); }
   if (dim == 2 && sdim == 3)
   {
      constexpr int DIM = 2;
      constexpr int SDIM = 3;
      const int NQ = quad1D*quad1D;
      auto W = w.Read();
      auto J = Reshape(j.Read(), NQ, SDIM, DIM, ne);
      auto D = Reshape(d.Write(), NQ, SDIM, ne);

      const bool const_c = coeff.Size() == 1;
      const auto C = const_c ? Reshape(coeff.Read(), 1,1) :
                     Reshape(coeff.Read(), NQ,ne);

      mfem::forall(ne, [=] MFEM_HOST_DEVICE (int e)
      {
         for (int q = 0; q < NQ; ++q)
         {
            const real_t wq = W[q];
            const real_t J11 = J(q,0,0,e);
            const real_t J21 = J(q,1,0,e);
            const real_t J31 = J(q,2,0,e);
            const real_t J12 = J(q,0,1,e);
            const real_t J22 = J(q,1,1,e);
            const real_t J32 = J(q,2,1,e);
            const real_t E = J11*J11 + J21*J21 + J31*J31;
            const real_t G = J12*J12 + J22*J22 + J32*J32;
            const real_t F = J11*J12 + J21*J22 + J31*J32;
            const real_t iw = 1.0 / sqrt(E*G - F*F);
            const real_t C1 = const_c ? C(0,0) : C(q,e);
            const real_t alpha = wq * C1 * iw;
            D(q,0,e) =  alpha * G; // 1,1
            D(q,1,e) = -alpha * F; // 1,2
            D(q,2,e) =  alpha * E; // 2,2
         }
      });
   }
   else
   {
      PAVectorDiffusionSetup(dim, quad1D, ne, w, j, coeff, d);
   }
}

template<int T_D1D = 0, int T_Q1D = 0>
static void PAVectorDiffusionDiagonal2D(const int NE,
                                        const Array<real_t> &b,
                                        const Array<real_t> &g,
                                        const Vector &d,
                                        Vector &y,
                                        const int d1d = 0,
                                        const int q1d = 0)
{
   const int D1D = T_D1D ? T_D1D : d1d;
   const int Q1D = T_Q1D ? T_Q1D : q1d;
   MFEM_VERIFY(D1D <= DeviceDofQuadLimits::Get().MAX_D1D, "");
   MFEM_VERIFY(Q1D <= DeviceDofQuadLimits::Get().MAX_Q1D, "");
   auto B = Reshape(b.Read(), Q1D, D1D);
   auto G = Reshape(g.Read(), Q1D, D1D);
   // note the different shape for D, this is a (symmetric) matrix so we only
   // store necessary entries
   auto D = Reshape(d.Read(), Q1D*Q1D, 3, NE);
   auto Y = Reshape(y.ReadWrite(), D1D, D1D, 2, NE);
   mfem::forall(NE, [=] MFEM_HOST_DEVICE (int e)
   {
      const int D1D = T_D1D ? T_D1D : d1d;
      const int Q1D = T_Q1D ? T_Q1D : q1d;
      constexpr int MD1 = T_D1D ? T_D1D : DofQuadLimits::MAX_D1D;
      constexpr int MQ1 = T_Q1D ? T_Q1D : DofQuadLimits::MAX_Q1D;
      // gradphi \cdot Q \gradphi has four terms
      real_t QD0[MQ1][MD1];
      real_t QD1[MQ1][MD1];
      real_t QD2[MQ1][MD1];
      for (int qx = 0; qx < Q1D; ++qx)
      {
         for (int dy = 0; dy < D1D; ++dy)
         {
            QD0[qx][dy] = 0.0;
            QD1[qx][dy] = 0.0;
            QD2[qx][dy] = 0.0;
            for (int qy = 0; qy < Q1D; ++qy)
            {
               const int q = qx + qy * Q1D;
               const real_t D0 = D(q,0,e);
               const real_t D1 = D(q,1,e);
               const real_t D2 = D(q,2,e);
               QD0[qx][dy] += B(qy, dy) * B(qy, dy) * D0;
               QD1[qx][dy] += B(qy, dy) * G(qy, dy) * D1;
               QD2[qx][dy] += G(qy, dy) * G(qy, dy) * D2;
            }
         }
      }
      for (int dy = 0; dy < D1D; ++dy)
      {
         for (int dx = 0; dx < D1D; ++dx)
         {
            real_t temp = 0.0;
            for (int qx = 0; qx < Q1D; ++qx)
            {
               temp += G(qx, dx) * G(qx, dx) * QD0[qx][dy];
               temp += G(qx, dx) * B(qx, dx) * QD1[qx][dy];
               temp += B(qx, dx) * G(qx, dx) * QD1[qx][dy];
               temp += B(qx, dx) * B(qx, dx) * QD2[qx][dy];
            }
            Y(dx,dy,0,e) += temp;
            Y(dx,dy,1,e) += temp;
         }
      }
   });
}

template<int T_D1D = 0, int T_Q1D = 0>
static void PAVectorDiffusionDiagonal3D(const int NE,
                                        const Array<real_t> &b,
                                        const Array<real_t> &g,
                                        const Vector &d,
                                        Vector &y,
                                        const int d1d = 0,
                                        const int q1d = 0)
{
   constexpr int DIM = 3;
   const int D1D = T_D1D ? T_D1D : d1d;
   const int Q1D = T_Q1D ? T_Q1D : q1d;
   const int max_q1d = T_Q1D ? T_Q1D : DeviceDofQuadLimits::Get().MAX_Q1D;
   const int max_d1d = T_D1D ? T_D1D : DeviceDofQuadLimits::Get().MAX_D1D;
   MFEM_VERIFY(D1D <= max_d1d, "");
   MFEM_VERIFY(Q1D <= max_q1d, "");
   auto B = Reshape(b.Read(), Q1D, D1D);
   auto G = Reshape(g.Read(), Q1D, D1D);
   auto Q = Reshape(d.Read(), Q1D*Q1D*Q1D, 6, NE);
   auto Y = Reshape(y.ReadWrite(), D1D, D1D, D1D, 3, NE);
   mfem::forall(NE, [=] MFEM_HOST_DEVICE (int e)
   {
      const int D1D = T_D1D ? T_D1D : d1d;
      const int Q1D = T_Q1D ? T_Q1D : q1d;
      constexpr int MD1 = T_D1D ? T_D1D : DofQuadLimits::MAX_D1D;
      constexpr int MQ1 = T_Q1D ? T_Q1D : DofQuadLimits::MAX_Q1D;
      real_t QQD[MQ1][MQ1][MD1];
      real_t QDD[MQ1][MD1][MD1];
      for (int i = 0; i < DIM; ++i)
      {
         for (int j = 0; j < DIM; ++j)
         {
            // first tensor contraction, along z direction
            for (int qx = 0; qx < Q1D; ++qx)
            {
               for (int qy = 0; qy < Q1D; ++qy)
               {
                  for (int dz = 0; dz < D1D; ++dz)
                  {
                     QQD[qx][qy][dz] = 0.0;
                     for (int qz = 0; qz < Q1D; ++qz)
                     {
                        const int q = qx + (qy + qz * Q1D) * Q1D;
                        const int k = j >= i ?
                                      3 - (3-i)*(2-i)/2 + j:
                                      3 - (3-j)*(2-j)/2 + i;
                        const real_t O = Q(q,k,e);
                        const real_t Bz = B(qz,dz);
                        const real_t Gz = G(qz,dz);
                        const real_t L = i==2 ? Gz : Bz;
                        const real_t R = j==2 ? Gz : Bz;
                        QQD[qx][qy][dz] += L * O * R;
                     }
                  }
               }
            }
            // second tensor contraction, along y direction
            for (int qx = 0; qx < Q1D; ++qx)
            {
               for (int dz = 0; dz < D1D; ++dz)
               {
                  for (int dy = 0; dy < D1D; ++dy)
                  {
                     QDD[qx][dy][dz] = 0.0;
                     for (int qy = 0; qy < Q1D; ++qy)
                     {
                        const real_t By = B(qy,dy);
                        const real_t Gy = G(qy,dy);
                        const real_t L = i==1 ? Gy : By;
                        const real_t R = j==1 ? Gy : By;
                        QDD[qx][dy][dz] += L * QQD[qx][qy][dz] * R;
                     }
                  }
               }
            }
            // third tensor contraction, along x direction
            for (int dz = 0; dz < D1D; ++dz)
            {
               for (int dy = 0; dy < D1D; ++dy)
               {
                  for (int dx = 0; dx < D1D; ++dx)
                  {
                     real_t temp = 0.0;
                     for (int qx = 0; qx < Q1D; ++qx)
                     {
                        const real_t Bx = B(qx,dx);
                        const real_t Gx = G(qx,dx);
                        const real_t L = i==0 ? Gx : Bx;
                        const real_t R = j==0 ? Gx : Bx;
                        temp += L * QDD[qx][dy][dz] * R;
                     }
                     Y(dx, dy, dz, 0, e) += temp;
                     Y(dx, dy, dz, 1, e) += temp;
                     Y(dx, dy, dz, 2, e) += temp;
                  }
               }
            }
         }
      }
   });
}

static void PAVectorDiffusionAssembleDiagonal(const int dim,
                                              const int D1D,
                                              const int Q1D,
                                              const int NE,
                                              const Array<real_t> &B,
                                              const Array<real_t> &G,
                                              const Vector &op,
                                              Vector &y)
{
   if (dim == 2)
   {
      return PAVectorDiffusionDiagonal2D(NE, B, G, op, y, D1D, Q1D);
   }
   else if (dim == 3)
   {
      return PAVectorDiffusionDiagonal3D(NE, B, G, op, y, D1D, Q1D);
   }
   MFEM_ABORT("Dimension not implemented.");
}

void VectorDiffusionIntegrator::AssembleDiagonalPA(Vector &diag)
{
   if (DeviceCanUseCeed())
   {
      ceedOp->GetDiagonal(diag);
   }
   else
   {
      PAVectorDiffusionAssembleDiagonal(dim, dofs1D, quad1D, ne,
                                        maps->B, maps->G,
                                        pa_data, diag);
   }
}

// PA Diffusion Apply 2D kernel
template<int T_D1D = 0, int T_Q1D = 0, int T_VDIM = 0> static
void PAVectorDiffusionApply2D(const int NE,
                              const Array<real_t> &b,
                              const Array<real_t> &g,
                              const Array<real_t> &bt,
                              const Array<real_t> &gt,
                              const Vector &d_,
                              const Vector &x_,
                              Vector &y_,
                              const int d1d = 0,
                              const int q1d = 0,
                              const int vdim = 0)
{
   const int D1D = T_D1D ? T_D1D : d1d;
   const int Q1D = T_Q1D ? T_Q1D : q1d;
   const int VDIM = T_VDIM ? T_VDIM : vdim;
   MFEM_VERIFY(D1D <= DeviceDofQuadLimits::Get().MAX_D1D, "");
   MFEM_VERIFY(Q1D <= DeviceDofQuadLimits::Get().MAX_Q1D, "");
   auto B = Reshape(b.Read(), Q1D, D1D);
   auto G = Reshape(g.Read(), Q1D, D1D);
   auto Bt = Reshape(bt.Read(), D1D, Q1D);
   auto Gt = Reshape(gt.Read(), D1D, Q1D);
   auto D = Reshape(d_.Read(), Q1D*Q1D, 3, NE);
   auto x = Reshape(x_.Read(), D1D, D1D, VDIM, NE);
   auto y = Reshape(y_.ReadWrite(), D1D, D1D, VDIM, NE);
   mfem::forall(NE, [=] MFEM_HOST_DEVICE (int e)
   {
      const int D1D = T_D1D ? T_D1D : d1d;
      const int Q1D = T_Q1D ? T_Q1D : q1d;
      const int VDIM = T_VDIM ? T_VDIM : vdim;
      constexpr int max_D1D = T_D1D ? T_D1D : DofQuadLimits::MAX_D1D;
      constexpr int max_Q1D = T_Q1D ? T_Q1D : DofQuadLimits::MAX_Q1D;

      real_t grad[max_Q1D][max_Q1D][2];
      for (int c = 0; c < VDIM; c++)
      {
         for (int qy = 0; qy < Q1D; ++qy)
         {
            for (int qx = 0; qx < Q1D; ++qx)
            {
               grad[qy][qx][0] = 0.0;
               grad[qy][qx][1] = 0.0;
            }
         }
         for (int dy = 0; dy < D1D; ++dy)
         {
            real_t gradX[max_Q1D][2];
            for (int qx = 0; qx < Q1D; ++qx)
            {
               gradX[qx][0] = 0.0;
               gradX[qx][1] = 0.0;
            }
            for (int dx = 0; dx < D1D; ++dx)
            {
               const real_t s = x(dx,dy,c,e);
               for (int qx = 0; qx < Q1D; ++qx)
               {
                  gradX[qx][0] += s * B(qx,dx);
                  gradX[qx][1] += s * G(qx,dx);
               }
            }
            for (int qy = 0; qy < Q1D; ++qy)
            {
               const real_t wy  = B(qy,dy);
               const real_t wDy = G(qy,dy);
               for (int qx = 0; qx < Q1D; ++qx)
               {
                  grad[qy][qx][0] += gradX[qx][1] * wy;
                  grad[qy][qx][1] += gradX[qx][0] * wDy;
               }
            }
         }
         // Calculate Dxy, xDy in plane
         for (int qy = 0; qy < Q1D; ++qy)
         {
            for (int qx = 0; qx < Q1D; ++qx)
            {
               const int q = qx + qy * Q1D;
               const real_t O11 = D(q,0,e);
               const real_t O12 = D(q,1,e);
               const real_t O22 = D(q,2,e);
               const real_t gradX = grad[qy][qx][0];
               const real_t gradY = grad[qy][qx][1];
               grad[qy][qx][0] = (O11 * gradX) + (O12 * gradY);
               grad[qy][qx][1] = (O12 * gradX) + (O22 * gradY);
            }
         }
         for (int qy = 0; qy < Q1D; ++qy)
         {
            real_t gradX[max_D1D][2];
            for (int dx = 0; dx < D1D; ++dx)
            {
               gradX[dx][0] = 0.0;
               gradX[dx][1] = 0.0;
            }
            for (int qx = 0; qx < Q1D; ++qx)
            {
               const real_t gX = grad[qy][qx][0];
               const real_t gY = grad[qy][qx][1];
               for (int dx = 0; dx < D1D; ++dx)
               {
                  const real_t wx  = Bt(dx,qx);
                  const real_t wDx = Gt(dx,qx);
                  gradX[dx][0] += gX * wDx;
                  gradX[dx][1] += gY * wx;
               }
            }
            for (int dy = 0; dy < D1D; ++dy)
            {
               const real_t wy  = Bt(dy,qy);
               const real_t wDy = Gt(dy,qy);
               for (int dx = 0; dx < D1D; ++dx)
               {
                  y(dx,dy,c,e) += ((gradX[dx][0] * wy) + (gradX[dx][1] * wDy));
               }
            }
         }
      }
   });
}

// PA Diffusion Apply 3D kernel
template<const int T_D1D = 0,
         const int T_Q1D = 0> static
void PAVectorDiffusionApply3D(const int NE,
                              const Array<real_t> &b,
                              const Array<real_t> &g,
                              const Array<real_t> &bt,
                              const Array<real_t> &gt,
                              const Vector &op_,
                              const Vector &x_,
                              Vector &y_,
                              int d1d = 0, int q1d = 0)
{
   const int D1D = T_D1D ? T_D1D : d1d;
   const int Q1D = T_Q1D ? T_Q1D : q1d;
   constexpr int VDIM = 3;
   MFEM_VERIFY(D1D <= DeviceDofQuadLimits::Get().MAX_D1D, "");
   MFEM_VERIFY(Q1D <= DeviceDofQuadLimits::Get().MAX_Q1D, "");
   auto B = Reshape(b.Read(), Q1D, D1D);
   auto G = Reshape(g.Read(), Q1D, D1D);
   auto Bt = Reshape(bt.Read(), D1D, Q1D);
   auto Gt = Reshape(gt.Read(), D1D, Q1D);
   auto op = Reshape(op_.Read(), Q1D*Q1D*Q1D, 6, NE);
   auto x = Reshape(x_.Read(), D1D, D1D, D1D, VDIM, NE);
   auto y = Reshape(y_.ReadWrite(), D1D, D1D, D1D, VDIM, NE);
   mfem::forall(NE, [=] MFEM_HOST_DEVICE (int e)
   {
      const int D1D = T_D1D ? T_D1D : d1d;
      const int Q1D = T_Q1D ? T_Q1D : q1d;
      constexpr int max_D1D = T_D1D ? T_D1D : DofQuadLimits::MAX_D1D;
      constexpr int max_Q1D = T_Q1D ? T_Q1D : DofQuadLimits::MAX_Q1D;
      for (int c = 0; c < VDIM; ++ c)
      {
         real_t grad[max_Q1D][max_Q1D][max_Q1D][3];
         for (int qz = 0; qz < Q1D; ++qz)
         {
            for (int qy = 0; qy < Q1D; ++qy)
            {
               for (int qx = 0; qx < Q1D; ++qx)
               {
                  grad[qz][qy][qx][0] = 0.0;
                  grad[qz][qy][qx][1] = 0.0;
                  grad[qz][qy][qx][2] = 0.0;
               }
            }
         }
         for (int dz = 0; dz < D1D; ++dz)
         {
            real_t gradXY[max_Q1D][max_Q1D][3];
            for (int qy = 0; qy < Q1D; ++qy)
            {
               for (int qx = 0; qx < Q1D; ++qx)
               {
                  gradXY[qy][qx][0] = 0.0;
                  gradXY[qy][qx][1] = 0.0;
                  gradXY[qy][qx][2] = 0.0;
               }
            }
            for (int dy = 0; dy < D1D; ++dy)
            {
               real_t gradX[max_Q1D][2];
               for (int qx = 0; qx < Q1D; ++qx)
               {
                  gradX[qx][0] = 0.0;
                  gradX[qx][1] = 0.0;
               }
               for (int dx = 0; dx < D1D; ++dx)
               {
                  const real_t s = x(dx,dy,dz,c,e);
                  for (int qx = 0; qx < Q1D; ++qx)
                  {
                     gradX[qx][0] += s * B(qx,dx);
                     gradX[qx][1] += s * G(qx,dx);
                  }
               }
               for (int qy = 0; qy < Q1D; ++qy)
               {
                  const real_t wy  = B(qy,dy);
                  const real_t wDy = G(qy,dy);
                  for (int qx = 0; qx < Q1D; ++qx)
                  {
                     const real_t wx  = gradX[qx][0];
                     const real_t wDx = gradX[qx][1];
                     gradXY[qy][qx][0] += wDx * wy;
                     gradXY[qy][qx][1] += wx  * wDy;
                     gradXY[qy][qx][2] += wx  * wy;
                  }
               }
            }
            for (int qz = 0; qz < Q1D; ++qz)
            {
               const real_t wz  = B(qz,dz);
               const real_t wDz = G(qz,dz);
               for (int qy = 0; qy < Q1D; ++qy)
               {
                  for (int qx = 0; qx < Q1D; ++qx)
                  {
                     grad[qz][qy][qx][0] += gradXY[qy][qx][0] * wz;
                     grad[qz][qy][qx][1] += gradXY[qy][qx][1] * wz;
                     grad[qz][qy][qx][2] += gradXY[qy][qx][2] * wDz;
                  }
               }
            }
         }
         // Calculate Dxyz, xDyz, xyDz in plane
         for (int qz = 0; qz < Q1D; ++qz)
         {
            for (int qy = 0; qy < Q1D; ++qy)
            {
               for (int qx = 0; qx < Q1D; ++qx)
               {
                  const int q = qx + (qy + qz * Q1D) * Q1D;
                  const real_t O11 = op(q,0,e);
                  const real_t O12 = op(q,1,e);
                  const real_t O13 = op(q,2,e);
                  const real_t O22 = op(q,3,e);
                  const real_t O23 = op(q,4,e);
                  const real_t O33 = op(q,5,e);
                  const real_t gradX = grad[qz][qy][qx][0];
                  const real_t gradY = grad[qz][qy][qx][1];
                  const real_t gradZ = grad[qz][qy][qx][2];
                  grad[qz][qy][qx][0] = (O11*gradX)+(O12*gradY)+(O13*gradZ);
                  grad[qz][qy][qx][1] = (O12*gradX)+(O22*gradY)+(O23*gradZ);
                  grad[qz][qy][qx][2] = (O13*gradX)+(O23*gradY)+(O33*gradZ);
               }
            }
         }
         for (int qz = 0; qz < Q1D; ++qz)
         {
            real_t gradXY[max_D1D][max_D1D][3];
            for (int dy = 0; dy < D1D; ++dy)
            {
               for (int dx = 0; dx < D1D; ++dx)
               {
                  gradXY[dy][dx][0] = 0;
                  gradXY[dy][dx][1] = 0;
                  gradXY[dy][dx][2] = 0;
               }
            }
            for (int qy = 0; qy < Q1D; ++qy)
            {
               real_t gradX[max_D1D][3];
               for (int dx = 0; dx < D1D; ++dx)
               {
                  gradX[dx][0] = 0;
                  gradX[dx][1] = 0;
                  gradX[dx][2] = 0;
               }
               for (int qx = 0; qx < Q1D; ++qx)
               {
                  const real_t gX = grad[qz][qy][qx][0];
                  const real_t gY = grad[qz][qy][qx][1];
                  const real_t gZ = grad[qz][qy][qx][2];
                  for (int dx = 0; dx < D1D; ++dx)
                  {
                     const real_t wx  = Bt(dx,qx);
                     const real_t wDx = Gt(dx,qx);
                     gradX[dx][0] += gX * wDx;
                     gradX[dx][1] += gY * wx;
                     gradX[dx][2] += gZ * wx;
                  }
               }
               for (int dy = 0; dy < D1D; ++dy)
               {
                  const real_t wy  = Bt(dy,qy);
                  const real_t wDy = Gt(dy,qy);
                  for (int dx = 0; dx < D1D; ++dx)
                  {
                     gradXY[dy][dx][0] += gradX[dx][0] * wy;
                     gradXY[dy][dx][1] += gradX[dx][1] * wDy;
                     gradXY[dy][dx][2] += gradX[dx][2] * wy;
                  }
               }
            }
            for (int dz = 0; dz < D1D; ++dz)
            {
               const real_t wz  = Bt(dz,qz);
               const real_t wDz = Gt(dz,qz);
               for (int dy = 0; dy < D1D; ++dy)
               {
                  for (int dx = 0; dx < D1D; ++dx)
                  {
                     y(dx,dy,dz,c,e) +=
                        ((gradXY[dy][dx][0] * wz) +
                         (gradXY[dy][dx][1] * wz) +
                         (gradXY[dy][dx][2] * wDz));
                  }
               }
            }
         }
      }
   });
}

// PA Diffusion Apply kernel
void VectorDiffusionIntegrator::AddMultPA(const Vector &x, Vector &y) const
{
   if (DeviceCanUseCeed())
   {
      ceedOp->AddMult(x, y);
   }
   else
   {
      const int D1D = dofs1D;
      const int Q1D = quad1D;
      const Array<real_t> &B = maps->B;
      const Array<real_t> &G = maps->G;
      const Array<real_t> &Bt = maps->Bt;
      const Array<real_t> &Gt = maps->Gt;
      const Vector &D = pa_data;

      if (dim == 2 && sdim == 3)
      {
         switch ((dofs1D << 4 ) | quad1D)
         {
            case 0x22: return PAVectorDiffusionApply2D<2,2,3>(ne,B,G,Bt,Gt,D,x,y);
            case 0x33: return PAVectorDiffusionApply2D<3,3,3>(ne,B,G,Bt,Gt,D,x,y);
            case 0x44: return PAVectorDiffusionApply2D<4,4,3>(ne,B,G,Bt,Gt,D,x,y);
            case 0x55: return PAVectorDiffusionApply2D<5,5,3>(ne,B,G,Bt,Gt,D,x,y);
            default:
               return PAVectorDiffusionApply2D(ne,B,G,Bt,Gt,D,x,y,D1D,Q1D,sdim);
         }
      }
      if (dim == 2 && sdim == 2)
      { return PAVectorDiffusionApply2D(ne,B,G,Bt,Gt,D,x,y,D1D,Q1D,sdim); }

      if (dim == 3 && sdim == 3)
      { return PAVectorDiffusionApply3D(ne,B,G,Bt,Gt,D,x,y,D1D,Q1D); }

      MFEM_ABORT("Unknown kernel.");
   }
}

} // namespace mfem
