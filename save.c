#include <math.h>
#include <stdlib.h>

#include "allvars.h"
#include "proto.h"



//wrc
void write_phi(fftw_complex * cpot, int isNonGaus)
{
    // int i,j,k;
    // for(i = 0; i < Local_nx; i++)
    //   for(j = 0; j < Nmesh; j++)
    //     for(k = 0; k <= Nmesh / 2 ; k++)
    //       {
    //         coord = (i * Nmesh + j) * (Nmesh / 2 + 1) + k;

    //         cpot_will[coord].re = cpot[coord].re;
    //         cpot_will[coord].im = cpot[coord].im;
    //       }

    char buf_will[400];
    FILE *write_ptr;

    if (isNonGaus==0)
    {
      if(NTaskWithN > 1)
        sprintf(buf_will, "%s/%s_gaus_potential_%d_NumX_%d_StartX_%d", OutputDir, FileBase,Seed,Local_nx, Local_x_start);
      else
        sprintf(buf_will, "%s/%s_gaus_potential_%d", OutputDir, FileBase,Seed);
      if(!(write_ptr = fopen(buf_will, "w")))
        {
          printf("Error. Can't write in file '%s'\n", buf_will);
          FatalError(10);
        }
    }
    else
    {
      if(NTaskWithN > 1)
        sprintf(buf_will, "%s/%s_nonGaus_potential_%d_NumX_%d_StartX_%d", OutputDir, FileBase,Seed,Local_nx, Local_x_start);
      else
        sprintf(buf_will, "%s/%s_nonGaus_potential_%d", OutputDir, FileBase,Seed);
#ifdef LOCAL_FNL  
      sprintf(buf_will, "%s_local",buf_will);
#endif
#ifdef EQUIL_FNL 
      sprintf(buf_will, "%s_equil",buf_will);
#endif
#ifdef ORTOG_FNL 
      sprintf(buf_will, "%s_ortho",buf_will);
#endif
//wrc
#ifdef ORTOG_LSS_FNL 
      sprintf(buf_will, "%s_ortho_lss",buf_will);
#endif
#ifdef PAR_ODD_FNL 
      sprintf(buf_will, "%s_par_odd",buf_will);
#endif

      if(!(write_ptr = fopen(buf_will, "w")))
        {
          printf("Error. Can't write in file '%s'\n", buf_will);
          FatalError(10);
        }
    }
    // printf("NumX_%d_StartX_%d", Local_nx, Local_x_start);
    // printf("%e %e  \n",cpot[0].re,cpot[0].im);
    // printf("%e %e  \n",cpot[1].re,cpot[1].im);
    // printf("%e %e  \n",cpot[Nmesh/2-1].re,cpot[Nmesh/2-1].im);
    // printf("%e %e  \n",cpot[Nmesh/2].re,cpot[Nmesh/2].im);
    // printf("%e %e  \n",cpot[Nmesh/2+1].re,cpot[Nmesh/2+1].im);
    // printf("%e %e  \n",cpot[Nmesh+10].re,cpot[Nmesh+10].im);
    printf("Size grid: %d  \n",TotalSizePlusAdditional);
    my_fwrite(&TotalSizePlusAdditional,sizeof(TotalSizePlusAdditional), 1, write_ptr);
    my_fwrite(&Nmesh,sizeof(Nmesh),1,write_ptr);
    my_fwrite(&Box,sizeof(Box),1,write_ptr);
    my_fwrite(cpot, sizeof(fftw_real)*TotalSizePlusAdditional, 1, write_ptr);
    fclose(write_ptr);

}


void write_particle_data(void)
{
  int nprocgroup, groupTask, masterTask;

  if((NTask < NumFilesWrittenInParallel))
    {
      printf
	("Fatal error.\nNumber of processors must be a smaller or equal than `NumFilesWrittenInParallel'.\n");
      FatalError(24131);
    }


  nprocgroup = NTask / NumFilesWrittenInParallel;

  if((NTask % NumFilesWrittenInParallel))
    nprocgroup++;

  masterTask = (ThisTask / nprocgroup) * nprocgroup;


  for(groupTask = 0; groupTask < nprocgroup; groupTask++)
    {
      if(ThisTask == (masterTask + groupTask))	/* ok, it's this processor's turn */
	save_local_data();

      /* wait inside the group */
      MPI_Barrier(MPI_COMM_WORLD);
    }

}




void save_local_data(void)
{
#define BUFFER 10
  size_t bytes;
  float *block;
  int *blockid;
#ifndef NO64BITID
  long long *blocklongid;
#endif
  int blockmaxlen, maxlongidlen;
  int4byte dummy;
  FILE *fd;
  char buf[300];
  int i, k, pc;
#ifdef  PRODUCEGAS
  double meanspacing, shift_gas, shift_dm;
#endif


  if(NumPart == 0)
    return;

  if(NTaskWithN > 1)
    sprintf(buf, "%s/%s.%d", OutputDir, FileBase, ThisTask);
  else
    sprintf(buf, "%s/%s", OutputDir, FileBase);

  if(!(fd = fopen(buf, "w")))
    {
      printf("Error. Can't write in file '%s'\n", buf);
      FatalError(10);
    }

  for(i = 0; i < 6; i++)
    {
      header.npart[i] = 0;
      header.npartTotal[i] = 0;
      header.mass[i] = 0;
    }


#ifdef MULTICOMPONENTGLASSFILE
  qsort(P, NumPart, sizeof(struct part_data), compare_type);  /* sort particles by type, because that's how they should be stored in a gadget binary file */

  for(i = 0; i < 3; i++)
    header.npartTotal[i] = header1.npartTotal[i + 1] * GlassTileFac * GlassTileFac * GlassTileFac;

  for(i = 0; i < NumPart; i++)
    header.npart[P[i].Type]++;

  if(header.npartTotal[0])
    header.mass[0] =
      (OmegaBaryon) * 3 * Hubble * Hubble / (8 * PI * G) * pow(Box, 3) / (header.npartTotal[0]);

  if(header.npartTotal[1])
    header.mass[1] =
      (Omega - OmegaBaryon - OmegaDM_2ndSpecies) * 3 * Hubble * Hubble / (8 * PI * G) * pow(Box,
											    3) /
      (header.npartTotal[1]);

  if(header.npartTotal[2])
    header.mass[2] =
      (OmegaDM_2ndSpecies) * 3 * Hubble * Hubble / (8 * PI * G) * pow(Box, 3) / (header.npartTotal[2]);


#else

  header.npart[1] = NumPart;
  header.npartTotal[1] = TotNumPart;
  header.npartTotal[2] = (TotNumPart >> 32);
  header.mass[1] = (Omega) * 3 * Hubble * Hubble / (8 * PI * G) * pow(Box, 3) / TotNumPart;

#ifdef  PRODUCEGAS
  header.npart[0] = NumPart;
  header.npartTotal[0] = TotNumPart;
  header.mass[0] = (OmegaBaryon) * 3 * Hubble * Hubble / (8 * PI * G) * pow(Box, 3) / TotNumPart;
  header.mass[1] = (Omega - OmegaBaryon) * 3 * Hubble * Hubble / (8 * PI * G) * pow(Box, 3) / TotNumPart;
#endif
#endif


  header.time = InitTime;
  header.redshift = 1.0 / InitTime - 1;

  header.flag_sfr = 0;
  header.flag_feedback = 0;
  header.flag_cooling = 0;
  header.flag_stellarage = 0;
  header.flag_metals = 0;

  header.num_files = NTaskWithN;

  header.BoxSize = Box;
  header.Omega0 = Omega;
  header.OmegaLambda = OmegaLambda;
  header.HubbleParam = HubbleParam;

  header.flag_stellarage = 0;
  header.flag_metals = 0;
  header.hashtabsize = 0;

  dummy = sizeof(header);
  my_fwrite(&dummy, sizeof(dummy), 1, fd);
  my_fwrite(&header, sizeof(header), 1, fd);
  my_fwrite(&dummy, sizeof(dummy), 1, fd);

#ifdef  PRODUCEGAS
  meanspacing = Box / pow(TotNumPart, 1.0 / 3);
  shift_gas = -0.5 * (Omega - OmegaBaryon) / (Omega) * meanspacing;
  shift_dm = +0.5 * OmegaBaryon / (Omega) * meanspacing;
#endif


  if(!(block = malloc(bytes = BUFFER * 1024 * 1024)))
    {
      printf("failed to allocate memory for `block' (%g bytes).\n", (double)bytes);
      FatalError(24);
    }

  blockmaxlen = bytes / (3 * sizeof(float));

  blockid = (int *) block;
#ifndef NO64BITID
  blocklongid = (long long *) block;
#endif
  maxlongidlen = bytes / (sizeof(long long));

  /* write coordinates */
  dummy = sizeof(float) * 3 * NumPart;
#ifdef  PRODUCEGAS
  dummy *= 2;
#endif
  my_fwrite(&dummy, sizeof(dummy), 1, fd);
  for(i = 0, pc = 0; i < NumPart; i++)
    {
      for(k = 0; k < 3; k++)
	{
	  block[3 * pc + k] = P[i].Pos[k];
#ifdef  PRODUCEGAS
	  block[3 * pc + k] = periodic_wrap(P[i].Pos[k] + shift_gas);
#endif
	}

      pc++;

      if(pc == blockmaxlen)
	{
	  my_fwrite(block, sizeof(float), 3 * pc, fd);
	  pc = 0;
	}
    }
  if(pc > 0)
    my_fwrite(block, sizeof(float), 3 * pc, fd);
#ifdef  PRODUCEGAS
  for(i = 0, pc = 0; i < NumPart; i++)
    {
      for(k = 0; k < 3; k++)
	{
	  block[3 * pc + k] = periodic_wrap(P[i].Pos[k] + shift_dm);
	}

      pc++;

      if(pc == blockmaxlen)
	{
	  my_fwrite(block, sizeof(float), 3 * pc, fd);
	  pc = 0;
	}
    }
  if(pc > 0)
    my_fwrite(block, sizeof(float), 3 * pc, fd);
#endif
  my_fwrite(&dummy, sizeof(dummy), 1, fd);



  /* write velocities */
  dummy = sizeof(float) * 3 * NumPart;
#ifdef  PRODUCEGAS
  dummy *= 2;
#endif
  my_fwrite(&dummy, sizeof(dummy), 1, fd);
  for(i = 0, pc = 0; i < NumPart; i++)
    {
      for(k = 0; k < 3; k++)
	block[3 * pc + k] = P[i].Vel[k];

#ifdef MULTICOMPONENTGLASSFILE
      if(WDM_On == 1 && WDM_Vtherm_On == 1 && P[i].Type == 1)
	add_WDM_thermal_speeds(&block[3 * pc]);
#else
#ifndef PRODUCEGAS
      if(WDM_On == 1 && WDM_Vtherm_On == 1)
	add_WDM_thermal_speeds(&block[3 * pc]);
#endif
#endif

      pc++;

      if(pc == blockmaxlen)
	{
	  my_fwrite(block, sizeof(float), 3 * pc, fd);
	  pc = 0;
	}
    }
  if(pc > 0)
    my_fwrite(block, sizeof(float), 3 * pc, fd);
#ifdef PRODUCEGAS
  for(i = 0, pc = 0; i < NumPart; i++)
    {
      for(k = 0; k < 3; k++)
	block[3 * pc + k] = P[i].Vel[k];

      if(WDM_On == 1 && WDM_Vtherm_On == 1)
	add_WDM_thermal_speeds(&block[3 * pc]);

      pc++;

      if(pc == blockmaxlen)
	{
	  my_fwrite(block, sizeof(float), 3 * pc, fd);
	  pc = 0;
	}
    }
  if(pc > 0)
    my_fwrite(block, sizeof(float), 3 * pc, fd);
#endif
  my_fwrite(&dummy, sizeof(dummy), 1, fd);


  /* write particle ID */
#ifdef NO64BITID
  dummy = sizeof(int) * NumPart;
#else
  dummy = sizeof(long long) * NumPart;
#endif
#ifdef  PRODUCEGAS
  dummy *= 2;
#endif
  my_fwrite(&dummy, sizeof(dummy), 1, fd);
  for(i = 0, pc = 0; i < NumPart; i++)
    {
#ifdef NO64BITID
      blockid[pc] = P[i].ID;
#else
      blocklongid[pc] = P[i].ID;
#endif

      pc++;

      if(pc == maxlongidlen)
	{
#ifdef NO64BITID
	  my_fwrite(blockid, sizeof(int), pc, fd);
#else
	  my_fwrite(blocklongid, sizeof(long long), pc, fd);
#endif
	  pc = 0;
	}
    }
  if(pc > 0)
    {
#ifdef NO64BITID
      my_fwrite(blockid, sizeof(int), pc, fd);
#else
      my_fwrite(blocklongid, sizeof(long long), pc, fd);
#endif
    }

#ifdef PRODUCEGAS
  for(i = 0, pc = 0; i < NumPart; i++)
    {
#ifdef NO64BITID
      blockid[pc] = P[i].ID + TotNumPart;
#else
      blocklongid[pc] = P[i].ID + TotNumPart;
#endif

      pc++;

      if(pc == maxlongidlen)
	{
#ifdef NO64BITID
	  my_fwrite(blockid, sizeof(int), pc, fd);
#else
	  my_fwrite(blocklongid, sizeof(long long), pc, fd);
#endif
	  pc = 0;
	}
    }
  if(pc > 0)
    {
#ifdef NO64BITID
      my_fwrite(blockid, sizeof(int), pc, fd);
#else
      my_fwrite(blocklongid, sizeof(long long), pc, fd);
#endif
    }
#endif

  my_fwrite(&dummy, sizeof(dummy), 1, fd);





  /* write zero temperatures if needed */
#ifdef  PRODUCEGAS
  dummy = sizeof(float) * NumPart;
  my_fwrite(&dummy, sizeof(dummy), 1, fd);
  for(i = 0, pc = 0; i < NumPart; i++)
    {
      block[pc] = 0;

      pc++;

      if(pc == blockmaxlen)
	{
	  my_fwrite(block, sizeof(float), pc, fd);
	  pc = 0;
	}
    }
  if(pc > 0)
    my_fwrite(block, sizeof(float), pc, fd);
  my_fwrite(&dummy, sizeof(dummy), 1, fd);
#endif


  /* write zero temperatures if needed */
#ifdef  MULTICOMPONENTGLASSFILE
  if(header.npart[0])
    {
      dummy = sizeof(float) * header.npart[0];
      my_fwrite(&dummy, sizeof(dummy), 1, fd);

      for(i = 0, pc = 0; i < header.npart[0]; i++)
	{
	  block[pc] = 0;

	  pc++;

	  if(pc == blockmaxlen)
	    {
	      my_fwrite(block, sizeof(float), pc, fd);
	      pc = 0;
	    }
	}
      if(pc > 0)
	my_fwrite(block, sizeof(float), pc, fd);
      my_fwrite(&dummy, sizeof(dummy), 1, fd);
    }
#endif



  free(block);

  fclose(fd);
}


/* This catches I/O errors occuring for my_fwrite(). In this case we better stop.
 */
size_t my_fwrite(void *ptr, size_t size, size_t nmemb, FILE * stream)
{
  size_t nwritten;

  if((nwritten = fwrite(ptr, size, nmemb, stream)) != nmemb)
    {
      printf("I/O error (fwrite) on task=%d has occured.\n", ThisTask);
      fflush(stdout);
      FatalError(777);
    }
  return nwritten;
}


/* This catches I/O errors occuring for fread(). In this case we better stop.
 */
size_t my_fread(void *ptr, size_t size, size_t nmemb, FILE * stream)
{
  size_t nread;

  if((nread = fread(ptr, size, nmemb, stream)) != nmemb)
    {
      printf("I/O error (fread) on task=%d has occured.\n", ThisTask);
      fflush(stdout);
      FatalError(778);
    }
  return nread;
}


#ifdef MULTICOMPONENTGLASSFILE
int compare_type(const void *a, const void *b)
{
  if(((struct part_data *) a)->Type < (((struct part_data *) b)->Type))
    return -1;

  if(((struct part_data *) a)->Type > (((struct part_data *) b)->Type))
    return +1;

  return 0;
}
#endif
