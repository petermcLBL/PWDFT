/* PGrid.cpp
   Author - Eric Bylaska

	this class is used for defining 3d parallel maps
*/



#include        <cmath>
#include 	<cstring> //memset
#include        <iostream>
#include	"Control2.hpp"
#include	"Lattice.hpp"
#include	"util.hpp"
#include	"nwpw_timing.hpp"
#include	"gdevice.hpp"

#include	"PGrid.hpp"

#include "blas.h"

/********************************
 *                              *
 *         Constructors         *
 *                              *
 ********************************/

//PGrid::PGrid(Parallel *inparall, Lattice *inlattice, Control2& control) : d3db(inparall,control.mapping(),control.ngrid(0),control.ngrid(1),control.ngrid(2))

PGrid::PGrid(Parallel *inparall, Lattice *inlattice, int mapping0, int balance0, int nx0, int ny0, int nz0) : d3db(inparall,mapping0,nx0,ny0,nz0)
{
   int i,j,k,nxh,nyh,nzh,p,q,indx,k1,k2,k3,nb;
   int nwave_in[2],nwave_out[2];
   double *G1, *G2,*G3;
   double gx,gy,gz,gg,ggcut,eps,ggmax,ggmin;
   int *zero_arow3,*zero_arow2;
   int yzslab,zrow;

   lattice = inlattice;

   eps = 1.0e-12;
   Garray = new double [3*nfft3d];
   G1 = Garray;
   G2 = (double *) &Garray[nfft3d];
   G3 = (double *) &Garray[2*nfft3d];
   nxh = nx/2;
   nyh = ny/2;
   nzh = nz/2;
   ggmin = 9.9e9;
   ggmax = 0.0;
   for (k3 = (-nzh+1); k3<= nzh; ++k3)
   for (k2 = (-nyh+1); k2<= nyh; ++k2)
   for (k1 = 0;        k1<= nxh; ++k1)
   {
      gx = k1*lattice->unitg(0,0) + k2*lattice->unitg(0,1) + k3*lattice->unitg(0,2);
      gy = k1*lattice->unitg(1,0) + k2*lattice->unitg(1,1) + k3*lattice->unitg(1,2);
      gz = k1*lattice->unitg(2,0) + k2*lattice->unitg(2,1) + k3*lattice->unitg(2,2);
      gg = gx*gx + gy*gy + gz*gz;
      if (gg>ggmax) ggmax = gg;
      if ((gg<ggmin)&&(gg>1.0e-6)) ggmin = gg;
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
   Gmax = sqrt(ggmax);
   Gmin = sqrt(ggmin);
   masker[0] = new int [2*nfft3d];
   masker[1] = (int *) &(masker[0][nfft3d]);
   //masker[0] = new int [nfft3d];
   //masker[1] = new int [nfft3d];
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
         ggcut = lattice->eggcut();
      else
         ggcut = lattice->wggcut();

      for (k3 = (-nzh+1); k3<  nzh; ++k3)
      for (k2 = (-nyh+1); k2<  nyh; ++k2)
      for (k1 = 0;        k1<  nxh; ++k1)
      {
         gx = k1*lattice->unitg(0,0) + k2*lattice->unitg(0,1) + k3*lattice->unitg(0,2);
         gy = k1*lattice->unitg(1,0) + k2*lattice->unitg(1,1) + k3*lattice->unitg(1,2);
         gz = k1*lattice->unitg(2,0) + k2*lattice->unitg(2,1) + k3*lattice->unitg(2,2);
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
   packarray[1] = (int *) &(packarray[0][nfft3d]);
   //packarray[0] = new int [nfft3d];
   //packarray[1] = new int [nfft3d];

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


   //if (control.balance())
   if (balance0)
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
            ggcut = lattice->eggcut();
         else
            ggcut = lattice->wggcut();

         /* find zero_row3 - (i,j,*) rows that are zero */
         for (i=0; i<((nxh+1)*nq); ++i) zero_row3[nb][i]  = 1;
         for (i=0; i<((nxh+1)*ny); ++i) zero_arow3[i] = 1;

         for (auto k2=(-nyh+1); k2<nyh; ++k2)
         for (auto k1=0;        k1<nxh; ++k1)
         {
            i = k1; j = k2;
            if (i<0) i = i + nx;
            if (j<0) j = j + ny;
            zrow = 1;
            for (auto k3=(-nzh+1); k3<nzh; ++k3)
            {
               gx = k1*lattice->unitg(0,0) + k2*lattice->unitg(0,1) + k3*lattice->unitg(0,2);
               gy = k1*lattice->unitg(1,0) + k2*lattice->unitg(1,1) + k3*lattice->unitg(1,2);
               gz = k1*lattice->unitg(2,0) + k2*lattice->unitg(2,1) + k3*lattice->unitg(2,2);
               gg = gx*gx + gy*gy + gz*gz;
               gg= gg-ggcut;
               if (gg<(-eps)) zrow = 0;
            }
            if (!zrow)
            {
               zero_arow3[i-1+(nxh+1)*(j-1)] = 0;
               q = ijktoq1(0,j,0);
               p = ijktop1(0,j,0);
               if (p==parall->taskid_i())
               {
                 zero_row3[nb][i+(nxh+1)*q] = 0;
               }
             }
         }
         // call D3dB_c_ptranspose_jk_init(nb,log_mb(zero_arow3(1)))


         /* find zero_slab23 - (i,*,*) slabs that are zero */
         for (auto i=0;  i<nxh; ++i)
            zero_slab23[nb][i] = 1;

         for (auto k1=0;  k1<nxh; ++k1)
         {
            i = k1;
            if (i<0) i = i + nx;
            yzslab = 1;
            for (auto k3=(-nzh+1); k3<nzh; ++k3)
            for (auto k2=(-nyh+1); k2<nyh; ++k2)
            {
               gx = k1*lattice->unitg(0,0) + k2*lattice->unitg(0,1) + k3*lattice->unitg(0,2);
               gy = k1*lattice->unitg(1,0) + k2*lattice->unitg(1,1) + k3*lattice->unitg(1,2);
               gz = k1*lattice->unitg(2,0) + k2*lattice->unitg(2,1) + k3*lattice->unitg(2,2);
               gg = gx*gx + gy*gy + gz*gz;
               gg= gg-ggcut;
               if (gg<(-eps)) yzslab = 0;
            }
            if (!yzslab)
               zero_slab23[nb][i] = 0;
         }

         /* find zero_row2 - (i,*,k) rows that are zero after fft of (i,j,*) */
         for (k=0; k<nz;      ++k)
         for (i=0; i<(nxh+1); ++i)
         {
            q = ijktoq(i,0,k);
            p = ijktop(i,0,k);
            if (p==parall->taskid_i())
               zero_row2[nb][q] = zero_slab23[nb][i];
         }



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

      for (nb=0; nb<=1; ++nb)
      {
         if (nb==0)
            ggcut = lattice->eggcut();
         else
            ggcut = lattice->wggcut();

         /*  find zero_row3 - (i,j,*) rows that are zero */
         for (auto q=0; q<nq3; ++q)
            zero_row3[nb][q] =  1;

         for (auto q=0; q<(nxh+1)*ny; ++q)
            zero_arow3[q] = 1;


         for (auto k2=(-nyh+1); k2<nyh; ++k2)
         for (auto k1=0;        k1<nxh; ++k1)
         {
            i = k1; j = k2;
            if (i<0) i = i + nx;
            if (j<0) j = j + ny;
            zrow = 1;
            for (auto k3=(-nzh+1); k3<nzh; ++k3)
            {
               gx = k1*lattice->unitg(0,0) + k2*lattice->unitg(0,1) + k3*lattice->unitg(0,2);
               gy = k1*lattice->unitg(1,0) + k2*lattice->unitg(1,1) + k3*lattice->unitg(1,2);
               gz = k1*lattice->unitg(2,0) + k2*lattice->unitg(2,1) + k3*lattice->unitg(2,2);
               gg = gx*gx + gy*gy + gz*gz;
               gg= gg-ggcut;
               if (gg<(-eps)) zrow = 0;
            }
            if (!zrow)
            {
               zero_arow3[i-1+(nxh+1)*(j-1)] = 0;
               q = ijktoq(i,j,0);
               p = ijktop(i,j,0);
               if (p==parall->taskid_i())
               {
                 zero_row3[nb][q] = 0;
               }
             }
         }

         /* find zero_slab23 - (i,*,*) slabs that are zero */
         for (auto i=0;  i<nxh; ++i)
            zero_slab23[nb][i] = 1;

         for (auto k1=0;  k1<nxh; ++k1)
         {
            i = k1;
            if (i<0) i = i + nx;
            yzslab = 1;
            for (auto k3=(-nzh+1); k3<nzh; ++k3)
            for (auto k2=(-nyh+1); k2<nyh; ++k2)
            {
               gx = k1*lattice->unitg(0,0) + k2*lattice->unitg(0,1) + k3*lattice->unitg(0,2);
               gy = k1*lattice->unitg(1,0) + k2*lattice->unitg(1,1) + k3*lattice->unitg(1,2);
               gz = k1*lattice->unitg(2,0) + k2*lattice->unitg(2,1) + k3*lattice->unitg(2,2);
               gg = gx*gx + gy*gy + gz*gz;
               gg= gg-ggcut;
               if (gg<(-eps)) yzslab = 0;
            }
            if (!yzslab)
               zero_slab23[nb][i] = 0;
         }

         /* find zero_row2 - (i,*,k) rows that are zero after fft of (i,j,*) */
         for (k=0; k<nz;      ++k)
         for (i=0; i<(nxh+1); ++i)
         {
            q = ijktoq1(i,0,k);
            p = ijktop1(i,0,k);
            if (p==parall->taskid_i())
               zero_row2[nb][q] = zero_slab23[nb][i];
         }


         //call D3dB_c_ptranspose_ijk_init(nb,log_mb(zero_arow2(1)),log_mb(zero_arow3(1)))

      }


      delete [] zero_arow3;
      delete [] zero_arow2;

   }

   Gpack[0] = new double [3*(nida[0]+nidb[0]) + 3*(nida[1]+nidb[1])];
   Gpack[1] = (double *) &(Gpack[0][3*(nida[0]+nidb[0])]);
   //Gpack[0] = new double [3*(nida[0]+nidb[0])];
   //Gpack[1] = new double [3*(nida[1]+nidb[1])];
   double *Gtmp = new double [nfft3d];
   int one      = 1;
   for (nb=0; nb<=1; ++nb)
   {
      for (i=0; i<3; ++i)
      {
         DCOPY_PWDFT(nfft3d,&(Garray[i*nfft3d]),one,Gtmp,one);

         this->t_pack(nb,Gtmp);
         this->tt_pack_copy(nb,Gtmp,&(Gpack[nb][i*(nida[nb]+nidb[nb])]));
      }
   }

   delete [] Gtmp;
}

PGrid::PGrid(Parallel *inparall, Lattice *inlattice, Control2& control) : PGrid(inparall,inlattice,control.mapping(),control.balance(),control.ngrid(0),control.ngrid(1),control.ngrid(2)) {}


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

/********************************
 *                              *
 *       PGrid:c_unpack         *
 *                              *
 ********************************/
void PGrid::c_unpack(const int nb, double *a)
{
   int one=1;
   int nn = 2*(nida[nb]+nidb2[nb]);
   double *tmp1,*tmp2;
   double *tmp = new double [2*nfft3d];
   if (balanced)
      mybalance->c_unbalance(nb,a);

   DCOPY_PWDFT(nn,a,one,tmp,one);

   //dcopy_(&n2ft3d,&rzero,&zero,a,&one);
   memset(a, 0, n2ft3d * sizeof(double));

   c_bindexcopy(nida[nb]+nidb2[nb],packarray[nb],tmp,a);

   tmp1 = new double[2*zplane_size+1];
   tmp2 = new double[2*zplane_size+1];
   c_timereverse(a,tmp1,tmp2);
   delete [] tmp2;
   delete [] tmp1;
   delete [] tmp;
}

/********************************
 *                              *
 *       PGrid:c_pack           *
 *                              *
 ********************************/
void PGrid::c_pack(const int nb, double *a)
{
   int one=1;
   double *tmp = new double [n2ft3d];

   DCOPY_PWDFT(n2ft3d,a,one,tmp,one);

   memset(a, 0, n2ft3d * sizeof(double));

   c_aindexcopy(nida[nb]+nidb2[nb],packarray[nb],tmp,a);

   if (balanced)
      mybalance->c_balance(nb,a);

   delete [] tmp;
   return;
}

/********************************
 *                              *
 *       PGrid:cc_pack_copy     *
 *                              *
 ********************************/
void PGrid::cc_pack_copy(const int nb, double *a, double *b)
{
   int one = 1;
   //int ng  = 2*(nida[nb]+nidb[nb]);
   int ng  = 2*(nida[nb]+nidb[nb]);

   DCOPY_PWDFT(ng,a,one,b,one);
}

/********************************
 *                              *
 *       PGrid:cc_pack_dot      *
 *                              *
 ********************************/
double PGrid::cc_pack_dot(const int nb, double *a, double *b)
{
   int one = 1;
   //int ng  = 2*(nida[nb]+nidb[nb]);
   int ng  = 2*(nida[nb]+nidb[nb]);
   int ng0 = 2*nida[nb];
   double tsum;

   tsum = 2.0*DDOT_PWDFT(ng,a,one,b,one);
   tsum -= DDOT_PWDFT(ng0,a,one,b,one);

   return d3db::parall->SumAll(1,tsum);
}

/********************************
 *                              *
 *       PGrid:tt_pack_dot      *
 *                              *
 ********************************/
double PGrid::tt_pack_dot(const int nb, double *a, double *b)
{
   int one = 1;
   int ng  = (nida[nb]+nidb[nb]);
   int ng0 = nida[nb];
   double tsum;

   tsum = 2.0*DDOT_PWDFT(ng,a,one,b,one);
   tsum -= DDOT_PWDFT(ng0,a,one,b,one);

   return d3db::parall->SumAll(1,tsum);
}


/********************************
 *                              *
 *       PGrid:cc_pack_idot     *
 *                              *
 ********************************/
double PGrid::cc_pack_idot(const int nb, double *a, double *b)
{
   int one = 1;
   //int ng  = 2*(nida[nb]+nidb[nb]);
   int ng  = 2*(nida[nb]+nidb[nb]);
   int ng0 = 2*nida[nb];
   double tsum=0.0;

   tsum = 2.0*DDOT_PWDFT(ng,a,one,b,one);
   tsum -= DDOT_PWDFT(ng0,a,one,b,one);

   return tsum;
}

/********************************
 *                              *
 *       PGrid:tt_pack_idot     *
 *                              *
 ********************************/
double PGrid::tt_pack_idot(const int nb, double *a, double *b)
{
   int one = 1;
   //int ng  = 2*(nida[nb]+nidb[nb]);
   int ng  = (nida[nb]+nidb[nb]);
   int ng0 = nida[nb];
   double tsum=0.0;

   tsum = 2.0*DDOT_PWDFT(ng,a,one,b,one);
   tsum -= DDOT_PWDFT(ng0,a,one,b,one);

   return tsum;
}


/********************************
 *                              *
 *       PGrid:cc_pack_indot    *
 *                              *
 ********************************/
void PGrid::cc_pack_indot(const int nb, const int nn, double *a, double *b, double *sum)
{
   int one = 1;
   //int ng  = 2*(nida[nb]+nidb[nb]);
   int ng  = 2*(nida[nb]+nidb[nb]);
   int ng0 = 2*nida[nb];

   for (int i=0; i<nn; ++i)
   {
      sum[i] = 2.0 * DDOT_PWDFT(ng,&(a[i*ng]),one,b,one);
      sum[i] -= DDOT_PWDFT(ng0,&(a[i*ng]),one,b,one);

   }

}


/********************************
 *                              *
 *    PGrid:cc_pack_inprjdot    *
 *                              *
 ********************************/
void PGrid::cc_pack_inprjdot(const int nb, int nn, int nprj, double *a, double *b, double *sum)
{
   int ng  = 2*(nida[nb]+nidb[nb]);
   int ng0 = 2*nida[nb];
   int one = 1;
   double rtwo  = 2.0;
   double rone  = 1.0;
   double rmone = -1.0;
   double rzero = 0.0;

   // DGEMM_PWDFT((char *) "T",(char *) "N",nn,nprj,ng,
   // 	       rtwo,
   // 	       a,ng,
   // 	       b,ng,
   // 	       rzero,
   // 	       sum,nn);
   gdevice_TN_dgemm(nn,nprj,ng,rtwo,a,b,rzero,sum);

   if (ng0>0)
   {
      DGEMM_PWDFT((char *) "T",(char *) "N",nn,nprj,ng0,
                 rmone,
                 a,ng,
                 b,ng,
                 rone,
                 sum,nn);
   }
}

/********************************
 *                              *
 *       PGrid:t_unpack         *
 *                              *
 ********************************/
void PGrid::t_unpack(const int nb, double *a)
{
   int one=1;
   int nn = (nida[nb]+nidb2[nb]);
   double *tmp1,*tmp2;
   double *tmp = new double [nfft3d];
   if (balanced)
      mybalance->t_unbalance(nb,a);

   DCOPY_PWDFT(nn,a,one,tmp,one);

   //dcopy_(&n2ft3d,&rzero,&zero,a,&one);
   memset(a, 0, nfft3d * sizeof(double));

   t_bindexcopy(nida[nb]+nidb2[nb],packarray[nb],tmp,a);

   tmp1 = new double[2*zplane_size+1];
   tmp2 = new double[2*zplane_size+1];
   t_timereverse(a,tmp1,tmp2);
   delete [] tmp2;
   delete [] tmp1;
   delete [] tmp;
}



/********************************
 *                              *
 *       PGrid:t_pack           *
 *                              *
 ********************************/
void PGrid::t_pack(const int nb, double *a)
{
   int one      = 1;
   int zero     = 0;
   double rzero = 0.0;
   double *tmp  = new double [nfft3d];

   DCOPY_PWDFT(nfft3d,a,one,tmp,one);

   //dcopy_(&nfft3d,&rzero,&zero,a,&one);
   memset(a, 0, nfft3d * sizeof(double));

   t_aindexcopy(nida[nb]+nidb2[nb],packarray[nb],tmp,a);

   if (balanced)
      mybalance->t_balance(nb,a);

   delete [] tmp;
}

/********************************
 *                              *
 *       PGrid:tt_pack_copy     *
 *                              *
 ********************************/
void PGrid::tt_pack_copy(const int nb, double *a, double *b)
{
   int one = 1;
   int ng  = nida[nb]+nidb[nb];

   DCOPY_PWDFT(ng,a,one,b,one);
}

/********************************
 *                              *
 *       PGrid:t_pack_nzero     *
 *                              *
 ********************************/
void PGrid::t_pack_nzero(const int nb, const int n, double *a)
{
   //int one   = 1;
  // int zero = 0;
  // double azero = 0.0;
   int ng  = n*(nida[nb]+nidb[nb]);
   memset(a, 0, ng * sizeof(double));
}



/********************************
 *                              *
 *       PGrid:i_pack           *
 *                              *
 ********************************/
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

/********************************
 *                              *
 *       PGrid:ii_pack_copy     *
 *                              *
 ********************************/
void PGrid::ii_pack_copy(const int nb, int *a, int *b)
{
   int i;
   int ng  = nida[nb]+nidb[nb];
   for (i=0; i<ng; ++i) b[i] = a[i];
}




/********************************
 *                              *
 *    PGrid:cr_pfft3b_queuein   *
 *                              *
 ********************************/
void PGrid::cr_pfft3b_queuein(const int nb, double *a)
{
   int np = parall->np_i();
}

/********************************
 *                              *
 *    PGrid:cr_pfft3b_queueout  *
 *                              *
 ********************************/
void PGrid::cr_pfft3b_queueout(const int nb, double *a)
{
}


/********************************
 *                              *
 * PGrid:cr_pfft3b_queuefilled  *
 *                              *
 ********************************/
int  PGrid::cr_pfft3b_queuefilled()
{
   //return (qsize>=qmax);
   return true;
}



/********************************
 *                              *
 *     PGrid:tc_pack_copy       *
 *                              *
 ********************************/
void PGrid::tc_pack_copy(const int nb, double *a, double *b)
{
   int i,ii;
   int ng  = nida[nb]+nidb[nb];

   ii = 0;
   for (i=0; i<ng; ++i)
   {
      b[ii]   = a[i];
      b[ii+1] = 0.0;
      ii += 2;
   }
}


/********************************
 *                              *
 *         PGrid:tcc_Mul        *
 *                              *
 ********************************/
void PGrid::tcc_Mul(const int nb, const double *a, const double *b, double *c)
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

/********************************
 *                              *
 *         PGrid:ttc_iMul       *
 *                              *
 ********************************/
void PGrid::tcc_iMul(const int nb, const double *a, const double *b, double *c)
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

/********************************
 *                              *
 *         PGrid:tc_iMul        *
 *                              *
 ********************************/
void PGrid::tc_iMul(const int nb, const double *a, double *c)
{
   int i,ii;
   int ng  = nida[nb]+nidb[nb];
   double x,y;

   ii = 0;
   for (i=0; i<ng; ++i)
   {
      x = c[ii]; y = c[ii+1];

      c[ii]   = -y*a[i];
      c[ii+1] =  x*a[i];
      ii += 2;
   }
}



/********************************
 *                              *
 *       PGrid:ttc_MulSum2      *
 *                              *
 ********************************/
void PGrid::tcc_MulSum2(const int nb, const double *a, const double *b, double *c)
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


/********************************
 *                              *
 *         PGrid:cc_Sum2        *
 *                              *
 ********************************/
void PGrid::cc_Sum2(const int nb, const double *a, double *b)
{
   int i;
   int ng  = 2*(nida[nb]+nidb[nb]);

   for (i=0; i<ng; ++i) b[i] += a[i];
}

/********************************
 *                              *
 *         PGrid:cccc_Sum       *
 *                              *
 ********************************/
void PGrid::cccc_Sum(const int nb, const double *a, const double *b, const double *c, double *d)
{
   int i;
   int ng  = 2*(nida[nb]+nidb[nb]);

   for (i=0; i<ng; ++i) d[i] = (a[i]+b[i]+c[i]);
}





/********************************
 *                              *
 *         PGrid:c_zero         *
 *                              *
 ********************************/
void PGrid::c_zero(const int nb, double *b)
{
   int i;
   int ng  = 2*(nida[nb]+nidb[nb]);

   for (i=0; i<ng; ++i) b[i] = 0.0;
}

/********************************
 *                              *
 *         PGrid:c_SMul         *
 *                              *
 ********************************/
void PGrid::c_SMul(const int nb, const double alpha, double *b)
{
   int i;
   int ng  = 2*(nida[nb]+nidb[nb]);

   for (i=0; i<ng; ++i) b[i] *= alpha;
}

/********************************
 *                              *
 *         PGrid:cc_SMul        *
 *                              *
 ********************************/
void PGrid::cc_SMul(const int nb, const double alpha, const double *a, double *b)
{
   int i;
   int ng  = 2*(nida[nb]+nidb[nb]);

   for (i=0; i<ng; ++i) b[i] = alpha*a[i];
}

/********************************
 *                              *
 *         PGrid:cc_daxpy       *
 *                              *
 ********************************/
void PGrid::cc_daxpy(const int nb, const double alpha, const double *a, double *b)
{
   int i;
   int ng  = 2*(nida[nb]+nidb[nb]);

   for (i=0; i<ng; ++i) b[i] += alpha*a[i];
}

/********************************
 *                              *
 *       PGrid:cct_iconjgMul    *
 *                              *
 ********************************/
void PGrid::cct_iconjgMul(const int nb, const double *a, const double *b, double *c)
{
   for (int i=0; i<(nida[nb]+nidb[nb]); ++i)
      c[i] = a[2*i]*b[2*i+1] - a[2*i+1]*b[2*i];
}


/********************************
 *                              *
 *       PGrid:cct_iconjgMulb   *
 *                              *
 ********************************/
void PGrid::cct_iconjgMulb(const int nb, const double *a, const double *b, double *c)
{
   for (int i=0; i<(nida[nb]+nidb[nb]); ++i)
      c[i] = a[2*i+1]*b[2*i] - a[2*i]*b[2*i+1];
}


