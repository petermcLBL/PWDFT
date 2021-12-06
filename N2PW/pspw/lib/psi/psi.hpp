#ifndef _PSIGETHEADER_H_
#define _PSIGETHEADER_H_

#include	"compressed_io.hpp"
#include	"control.hpp"

#include	"Parallel.hpp"
#include	"Pneb.hpp"

extern void psi_get_header(Parallel *, int *, int *, double *, int *, int *);
extern void psi_read(Pneb *, int *, int *, double *, int *, int *,double *);

#endif
