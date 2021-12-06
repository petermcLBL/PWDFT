#ifndef _BALANCE_H_
#define _BALANCE_H_
/* Balance.h
   Author - Eric Bylaska

*/

#include	"Parallel.hpp"

#define	MAXSIZE_MAX	2

class Balance  {

   Parallel *parall;

   int maxsize;
   int *proc_to_list[MAXSIZE_MAX];
   int *proc_from_list[MAXSIZE_MAX];
   int *packet_size_list[MAXSIZE_MAX];
   int *indx_start_list[MAXSIZE_MAX];

   int npacket_list[MAXSIZE_MAX];
   int receiver_list[MAXSIZE_MAX];
   int sender_list[MAXSIZE_MAX];

public:

   /* Constructors */
   Balance(Parallel *, const int, const int *, int *);
           
   /* destructor */
   ~Balance();

   void c_unbalance(const int, double *a);
   void c_balance(const int, double *a);
   void t_balance(const int, double *a);
   void i_balance(const int, int    *a);
};

#endif
