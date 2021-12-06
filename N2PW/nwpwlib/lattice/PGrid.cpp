/* PGrid.cpp
   Author - Eric Bylaska

	this class is used for defining 3d parallel maps
*/

/*
#include        <iostream>
#include        <cstdio>
#include        <stdio.h>
#include        <cmath>
#include        <cstdlib>
using namespace std;
*/



#include	"Parallel.hpp"
#include	"control.hpp"
#include	"lattice.hpp"
#include	"util.hpp"
#include	"blas.h"

#include	"PGrid.hpp"

/********************************
 *                              *
 *         Constructors         *
 *                              *
 ********************************/

PGrid::PGrid(Parallel *inparall) : d3db(inparall,control_mapping(),control_ngrid(0),control_ngrid(1),control_ngrid(2))
{
   int i,j,k,nxh,nyh,nzh,p,indx,k1,k2,k3,nb;
   int nwave_in[2],nwave_out[2];
   double *G1, *G2,*G3;
   double gx,gy,gz,gg,ggcut,eps;
   int *zero_arow3,*zero_arow2;

   eps = 1.0e-12;
   Garray = new double [3*nfft3d];
   G1 = Garray; 
   G2 = &Garray[nfft3d]; 
   G3 = &Garray[2*nfft3d];
   nxh = nx/2;
   nyh = ny/2;
   nzh = nz/2;
   for (k3 = (-nzh+1); k3<= nzh; ++k3)
   for (k2 = (-nyh+1); k2<= nyh; ++k2)
   for (k1 = 0;        k1<= nxh; ++k1)
   {
      gx = k1*lattice_unitg(0,0) + k2*lattice_unitg(0,1) + k3*lattice_unitg(0,2);
      gy = k1*lattice_unitg(1,0) + k2*lattice_unitg(1,1) + k3*lattice_unitg(1,2);
      gz = k1*lattice_unitg(2,0) + k2*lattice_unitg(2,1) + k3*lattice_unitg(2,2);
      i=k1; if (i < 0) i = i + nx;
      j=k2; if (j < 0) j = j + ny;
      k=k3; if (k < 0) k = k + nz;

      indx = ijktoindex(i,j,k);
      p    = ijktop(i,j,k);
      if (p==parall->taskid_i())
      {
         G1[indx] = gx;
         G2[indx] = gy;
         G3[indx] = gz;
      }

   }
   masker[0] = new int [2*nfft3d];
   masker[1] = &(masker[0][nfft3d]);
   //for (int k=0; k<(2*nfft3d); ++k) 
   //   masker[0][k] = 1;
   for (int k=0; k<(nfft3d); ++k) 
   {
      masker[0][k] = 1;
      masker[1][k] = 1;
   }

   for (nb=0; nb<=1; ++nb)
   {
      nwave[nb] = 0;
      if (nb==0) 
         ggcut = lattice_eggcut();
      else
         ggcut = lattice_wggcut();

      for (k3 = (-nzh+1); k3<  nzh; ++k3)
      for (k2 = (-nyh+1); k2<  nyh; ++k2)
      for (k1 = 0;        k1<  nxh; ++k1)
      {
         gx = k1*lattice_unitg(0,0) + k2*lattice_unitg(0,1) + k3*lattice_unitg(0,2);
         gy = k1*lattice_unitg(1,0) + k2*lattice_unitg(1,1) + k3*lattice_unitg(1,2);
         gz = k1*lattice_unitg(2,0) + k2*lattice_unitg(2,1) + k3*lattice_unitg(2,2);
         i=k1; if (i < 0) i = i + nx;
         j=k2; if (j < 0) j = j + ny;
         k=k3; if (k < 0) k = k + nz;

         indx = ijktoindex(i,j,k);
         p    = ijktop(i,j,k);
         if (p==parall->taskid_i())
         {
            gg = gx*gx + gy*gy + gz*gz;
            gg = gg-ggcut;
            if (gg < (-eps))
            {
               masker[nb][indx] = 0;
               ++nwave[nb] ;
            }
         }
      }
      nwave_entire[nb] = nwave[nb];
      nwave_entire[nb] = parall->ISumAll(1,nwave_entire[nb]);
   }


   packarray[0] = new int [2*nfft3d];
   packarray[1] = &(packarray[0][nfft3d]);

   for (nb=0; nb<=1; ++nb)
   {
      nida[nb]  = 0;
      nidb2[nb] = 0;

      /* k=(0,0,0)  */
      k1=0;
      k2=0;
      k3=0;
      indx = ijktoindex(k1,k2,k3);
      p    = ijktop(k1,k2,k3);
      if (p==parall->taskid_i())
         if (!masker[nb][indx])
         {
            packarray[nb][nida[nb]] = indx;
            ++nida[nb];
         }

      /* k=(0,0,k3) - neglect (0,0,-k3) points */
      for (k=1; k<=(nzh-1); ++k)
      {
         k1=0;
         k2=0;
         k3=k;
         indx = ijktoindex(k1,k2,k3);
         p    = ijktop(k1,k2,k3);
         if (p==parall->taskid_i())
            if (!masker[nb][indx])
            {
               packarray[nb][nida[nb]+nidb2[nb]] = indx;
               ++nidb2[nb];
            }
      }

      /* k=(0,k2,k3) - neglect (0,-k2, -k3) points */
      for (k=(-nzh+1); k<=(nzh-1); ++k)
      for (j=1;        j<=(nyh-1); ++j)
      {
         k1=0;
         k2=j;
         k3=k;
         if (k3 < 0) k3 = k3 + nz;
         indx = ijktoindex(k1,k2,k3);
         p    = ijktop(k1,k2,k3);
         if (p==parall->taskid_i())
            if (!masker[nb][indx])
            {
               packarray[nb][nida[nb]+nidb2[nb]] = indx;
               ++nidb2[nb];
            }
      }


      /* k=(k1,k2,k3) */
      for (k=(-nzh+1); k<=(nzh-1); ++k)
      for (j=(-nyh+1); j<=(nyh-1); ++j)
      for (i=1;        i<=(nxh-1); ++i)
      {
         k1=i;
         k2=j;
         k3=k;
         if (k2 < 0) k2 = k2 + ny;
         if (k3 < 0) k3 = k3 + nz;
         indx = ijktoindex(k1,k2,k3);
         p    = ijktop(k1,k2,k3);
         if (p==parall->taskid_i())
            if (!masker[nb][indx])
            {
               packarray[nb][nida[nb]+nidb2[nb]] = indx;
               ++nidb2[nb];
            }
      }

   }

   nwave_in[0] = nida[0] + nidb2[0];
   nwave_in[1] = nida[1] + nidb2[1];

   if (control_balance()) 
   {
      balanced = 1;
      mybalance = new Balance(parall,2,nwave_in,nwave_out);
   }
   else
   {
      balanced = 0;
      nwave_out[0] = nwave_in[0];
      nwave_out[1] = nwave_in[1];
   }
   nidb[0] = nidb2[0] + (nwave_out[0] - nwave_in[0]);
   nidb[1] = nidb2[1] + (nwave_out[1] - nwave_in[1]);

   nwave_all[0] = nida[0] + nidb2[0];
   nwave_all[1] = nida[1] + nidb2[1];
   parall->Vector_ISumAll(1,2,nwave_all);


   if (maptype==1)
   {

      zero_row3[0]   = new int[(nxh+1)*nq];
      zero_row3[1]   = new int[(nxh+1)*nq];
      zero_row2[0]   = new int[(nxh+1)*nq];
      zero_row2[1]   = new int[(nxh+1)*nq];
      zero_slab23[0] = new int[nxh+1];
      zero_slab23[1] = new int[nxh+1];

      zero_arow3 = new int [(nxh+1)*ny];
      for (nb=0; nb<=1; ++nb)
      {
         if (nb==0) 
            ggcut = lattice_eggcut();
         else
            ggcut = lattice_wggcut();

         /* find zero_row3 - (i,j,*) rows that are zero */
         for (i=0; i<((nxh+1)*nq); ++i) zero_row3[nb][i]  = 1;
         for (i=0; i<((nxh+1)*ny); ++i) zero_arow3[i] = 1;


      }

      delete [] zero_arow3;
   }
   else
   {
      zero_row3[0] = new int[nq3];
      zero_row3[1] = new int[nq3];
      zero_row2[0] = new int[nq2];
      zero_row2[1] = new int[nq2];
      zero_slab23[0] = new int[nxh+1];
      zero_slab23[1] = new int[nxh+1];

      zero_arow2 = new int [(nxh+1)*nz];
      zero_arow3 = new int [(nxh+1)*ny];

      delete [] zero_arow3;
      delete [] zero_arow2;
 
   }


}


/* 
void c_indexcopy(const int n, const int *indx, double *A, double *B)
{
   int ii,jj;
   ii = 0;
   for (int i=0; i<n; ++i)
   {
      jj      = 2*indx[i];
      B[ii]   = A[jj];
      B[ii+1] = A[jj+1];
      ii +=2;
   }
}
void t_indexcopy(const int n, const int *indx, double *A, double *B)
{
   for (int i=0; i<n; ++i)
      B[i] = A[indx[i]];
}
*/

void PGrid::c_unpack(const int nb, double *a)
{
   int nn = 2*(nida[nb]+nidb2[nb]);
   int one      = 1;
   int zero     = 0;
   double rzero = 0.0;
   double *tmp1,*tmp2;
   double *tmp = new double [2*nfft3d];
   if (balanced)
      mybalance->c_unbalance(nb,a);

   dcopy_(&nn,a,&one,tmp,&one);
   dcopy_(&n2ft3d,&rzero,&zero,a,&one);
   c_bindexcopy(nida[nb]+nidb2[nb],packarray[nb],tmp,a);

   tmp1 = new double[2*zplane_size];
   tmp2 = new double[2*zplane_size];
   c_timereverse(a,tmp1,tmp2);
   delete [] tmp2;
   delete [] tmp1;
   delete [] tmp;
}

void PGrid::c_pack(const int nb, double *a)
{
   int one      = 1;
   int zero     = 0;
   double rzero = 0.0;
   double *tmp = new double [n2ft3d];

   dcopy_(&n2ft3d,a,&one,tmp,&one);
   dcopy_(&n2ft3d,&rzero,&zero,a,&one);
   c_aindexcopy(nida[nb]+nidb2[nb],packarray[nb],tmp,a);

   if (balanced)
      mybalance->c_balance(nb,a);

   delete [] tmp;
}

void PGrid::cc_pack_copy(const int nb, double *a, double *b)
{
   int one = 1;
   //int ng  = 2*(nida[nb]+nidb[nb]);
   int ng  = 2*(nida[nb]+nidb[nb]);
   dcopy_(&ng,a,&one,b,&one);
}

double PGrid::cc_pack_dot(const int nb, double *a, double *b)
{
   int one = 1;
   //int ng  = 2*(nida[nb]+nidb[nb]);
   int ng  = 2*(nida[nb]+nidb[nb]);
   int ng0 = 2*nida[nb];
   double tsum;

   tsum = 2.0*ddot_(&ng,a,&one,b,&one);
   tsum -= ddot_(&ng0,a,&one,b,&one);

   return d3db::parall->SumAll(1,tsum);
}

double PGrid::cc_pack_idot(const int nb, double *a, double *b)
{
   int one = 1;
   //int ng  = 2*(nida[nb]+nidb[nb]);
   int ng  = 2*(nida[nb]+nidb[nb]);
   int ng0 = 2*nida[nb];
   double tsum;

   tsum = 2.0*ddot_(&ng,a,&one,b,&one);
   tsum -= ddot_(&ng0,a,&one,b,&one);

   return tsum;
}

void PGrid::cc_pack_indot(const int nb, const int nn, double *a, double *b, double *sum)
{
   int i;
   int one = 1;
   //int ng  = 2*(nida[nb]+nidb[nb]);
   int ng  = 2*(nida[nb]+nidb[nb]);
   int ng0 = 2*nida[nb];

   for (i=0; i<nn; ++i)
   {
      sum[i] = 2.0*ddot_(&ng,&a[i*ng],&one,b,&one);
      sum[i] -= ddot_(&ng0,&a[i*ng],&one,b,&one);
   }

}



void PGrid::t_pack(const int nb, double *a)
{
   int one      = 1;
   int zero     = 0;
   double rzero = 0.0;
   double *tmp  = new double [nfft3d];

   dcopy_(&nfft3d,a,&one,tmp,&one);
   dcopy_(&nfft3d,&rzero,&zero,a,&one);
   t_aindexcopy(nida[nb]+nidb2[nb],packarray[nb],tmp,a);

   if (balanced)
      mybalance->t_balance(nb,a);

   delete [] tmp;
}

void PGrid::tt_pack_copy(const int nb, double *a, double *b)
{
   int one = 1;
   int ng  = nida[nb]+nidb[nb];
   dcopy_(&ng,a,&one,b,&one);
}



void PGrid::i_pack(const int nb, int *a)
{
   int i;
   int *tmp  = new int [nfft3d];

   for (i=0; i<nfft3d; ++i) tmp[i] = a[i];
   for (i=0; i<nfft3d; ++i) a[i]    = 0;

   i_aindexcopy(nida[nb]+nidb2[nb],packarray[nb],tmp,a);

   if (balanced)
      mybalance->i_balance(nb,a);

   delete [] tmp;
}

void PGrid::ii_pack_copy(const int nb, int *a, int *b)
{
   int i;
   int ng  = nida[nb]+nidb[nb];
   for (i=0; i<ng; ++i) b[i] = a[i];
}




void PGrid::cr_pfft3b_queuein(const int nb, double *a)
{
   int np = parall->np_i();
}

void PGrid::cr_pfft3b_queueout(const int nb, double *a)
{
}


int  PGrid::cr_pfft3b_queuefilled()
{
   //return (qsize>=qmax);
}




void PGrid::tcc_Mul(const int nb, double *a, double *b, double *c)
{
   int i,ii;
   int ng  = nida[nb]+nidb[nb];

   ii = 0;
   for (i=0; i<ng; ++i) 
   {
      c[ii]   = b[ii]  *a[i];
      c[ii+1] = b[ii+1]*a[i];
      ii += 2;
   }
}

void PGrid::tcc_iMul(const int nb, double *a, double *b, double *c)
{
   int i,ii;
   int ng  = nida[nb]+nidb[nb];

   ii = 0;
   for (i=0; i<ng; ++i) 
   {
      c[ii]   = -b[ii+1]*a[i];
      c[ii+1] =  b[ii]  *a[i];
      ii += 2;
   }
}

void PGrid::tcc_MulSum2(const int nb, double *a, double *b, double *c)
{
   int i,ii;
   int ng  = nida[nb]+nidb[nb];

   ii = 0;
   for (i=0; i<ng; ++i) 
   {
      c[ii]   += b[ii]  *a[i];
      c[ii+1] += b[ii+1]*a[i];
      ii += 2;
   }
}


void PGrid::cc_Sum2(const int nb, double *a, double *b)
{
   int i;
   int ng  = 2*(nida[nb]+nidb[nb]);

   for (i=0; i<ng; ++i) b[i] += a[i];
}

void PGrid::c_zero(const int nb, double *b)
{
   int i;
   int ng  = 2*(nida[nb]+nidb[nb]);

   for (i=0; i<ng; ++i) b[i] = 0.0;
}

void PGrid::c_SMul(const int nb, double alpha, double *b)
{
   int i;
   int ng  = 2*(nida[nb]+nidb[nb]);

   for (i=0; i<ng; ++i) b[i] *= alpha;
}
void PGrid::cc_SMul(const int nb, double alpha, double *a, double *b)
{
   int i;
   int ng  = 2*(nida[nb]+nidb[nb]);

   for (i=0; i<ng; ++i) b[i] = alpha*a[i];
}

void PGrid::cc_daxpy(const int nb, double alpha, double *a, double *b)
{
   int i;
   int ng  = 2*(nida[nb]+nidb[nb]);

   for (i=0; i<ng; ++i) b[i] += alpha*a[i];
}
