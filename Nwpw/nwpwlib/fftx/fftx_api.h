#ifndef _MY_FFTX_API
#define _MY_FFTX_API

void init_fftx_dftbat_1_60_1d();
void fftx_dftbat_1_60_1d(double *Y, double *X);
void destroy_fftx_dftbat_1_60_1d();
void init_fftx_idftbat_1_60_1d();
void fftx_idftbat_1_60_1d(double *Y, double *X);
void destroy_fftx_idftbat_1_60_1d();

void init_fftx_prdftbat_1_60_1d();
void fftx_prdftbat_1_60_1d(double *Y, double *X);
void destroy_fftx_prdftbat_1_60_1d();
void init_fftx_iprdftbat_1_60_1d();
void fftx_iprdftbat_1_60_1d(double *Y, double *X);
void destroy_fftx_iprdftbat_1_60_1d();

#endif
