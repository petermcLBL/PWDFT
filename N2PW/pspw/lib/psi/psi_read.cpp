
#include        "compressed_io.hpp"
#include        "control.hpp"

#include        "Parallel.hpp"
#include	"Pneb.hpp"

void psi_header(Pneb *mypneb,int *version, int nfft[], 
                double unita[], int *ispin, int ne[],
                double *psi)
{
   int occupation;
   Parallel *myparall = mypneb->d3db::parall;

   if (myparall->is_master())
   {
      openfile(4,control_input_movecs_filename(),"r");
      iread(4,version,1);
      iread(4,nfft,3);
      dread(4,unita,9);
      iread(4,ispin,1);
      iread(4,ne,2);
      iread(4,&occupation,1);
   }
   myparall->Brdcst_iValue(0,0,version);
   myparall->Brdcst_iValues(0,0,3,nfft);
   myparall->Brdcst_Values(0,0,9,unita);
   myparall->Brdcst_iValue(0,0,ispin);
   myparall->Brdcst_iValues(0,0,2,ne);

   //printf("fhera taskid=%d\n",taskid);

   mypneb->g_read(4,psi);

   if (myparall->is_master()) closefile(4);
}
