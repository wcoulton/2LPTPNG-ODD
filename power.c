#include <math.h>
#include "allvars.h"
#include "proto.h"


static double R8;
static double r_tophat;

static double AA, BB, CC;
static double nu;
static double Norm; 
static double klower;

static int NPowerTable;

static struct pow_table
{
  double logk, logD;
}
 *PowerTable;

 
/* added for non-gaussian, by M.Manera  */

static int NTransferTable;
static struct trans_table
{
  double logk, logT;
} *TransferTable;

// wrc
static int NTransferTablePNG;
static struct trans_table_PNG
{
  double logk, T;
} *TransferTablePNG;


static int NCdfTable;
static struct pdf_table
{
  double x, cdf;
} *CdfTable;
// end wrc


double TransferFunc(double k)
{
  double transfer;

  switch (WhichTransfer)
    {
    case 1:
      transfer = TransferFunc_EH(k);
      break;

    case 2:
      transfer = TransferFunc_Tabulated(k);
      break;

    case 3:
      printf(" TranferFunction from Pk deprecreated\n");
      FatalError(117);
      transfer = TransferFunc_FromPk(k);
      break;

    default:
      printf(" TranferFunction Efstathiou implemented but not recommended\n");
      transfer = TransferFunc_Efstathiou(k); 
      break;
    }

   /* at the moment no second species implemented 
    * or WDM implemented for transfer function
    */

  return transfer;
}


void read_transfer_table(void)
{
  FILE *fd;
  char buf[500];
  double k, t;
  double Tlower;
  int i;

  sprintf(buf, FileWithInputTransfer);

  if(!(fd = fopen(buf, "r")))
    {
      printf("can't read input transfer function  in file '%s' on task %d\n", buf, ThisTask);
      FatalError(17);
    }

  NTransferTable = 0;
  do
    {
      if(fscanf(fd, " %le %le ", &k, &t) == 2)
        NTransferTable++;
      else
        break;
    }
  while(1);

  fclose(fd);

  TransferTable = malloc(NTransferTable * sizeof(struct trans_table));

  sprintf(buf, FileWithInputTransfer);

  if(!(fd = fopen(buf, "r")))
    {
      printf("can't read input transfer function in file '%s' on task %d\n", buf, ThisTask);
      FatalError(18);
    }

  NTransferTable = 0;
  do
    {
      if(fscanf(fd, " %le %le ", &k, &t) == 2)
        {
          TransferTable[NTransferTable].logk = log10(k);
          TransferTable[NTransferTable].logT = log10(t);
          NTransferTable++;
        }
      else
        break;
    }
  while(1);

  fclose(fd);

  qsort(TransferTable, NTransferTable, sizeof(struct trans_table), compare_transfer_logk); 

  klower = pow(10.0, TransferTable[0].logk);

  if(TransferTable[0].logk >= -4.6 ) 
     {
      printf("\n WARNING: klower is %f, may be too large to normalize transfer  \n",klower);
      FatalError(1111);  
     }
  if(TransferTable[NTransferTable - 1].logk <= log10(500./8.) ) 
     {
      printf("\n WARNING: kmax is may be too small to normalize transfer  \n");
      printf("\n Values outside the input range will be taken to be zero  \n");
      // FatalError(1111);  
     }

      Tlower = TransferTable[0].logT;      
      for(i=0; i < NTransferTable ; i++ )
          {
          TransferTable[i].logT -= Tlower;
          }

}

int compare_transfer_logk(const void *a, const void *b)
{
  if(((struct trans_table *) a)->logk < (((struct trans_table *) b)->logk))
    return -1;

  if(((struct trans_table *) a)->logk > (((struct trans_table *) b)->logk))
    return +1;

  return 0;
}



void initialize_transferfunction(void)
{

  /* Transfer function is meant to be normalizet to 1 at k=0 
   * No multiple Tf implemented as in power spectrum multiple species 
   */ 
   if(WhichTransfer == 2)
    read_transfer_table();
 
}


double TransferFunc_Tabulated(double k)
{
  double logk, logT, T, u, dlogk; 
  int binlow, binhigh, binmid;

  k *= (InputSpectrum_UnitLength_in_cm / UnitLength_in_cm);     /* convert to h/Mpc */

  logk = log10(k);

  if(logk < TransferTable[0].logk || logk > TransferTable[NTransferTable - 1].logk)
    return 0;

  binlow = 0;
  binhigh = NTransferTable - 1;

  while(binhigh - binlow > 1)
    {
      binmid = (binhigh + binlow) / 2;
      if(logk < TransferTable[binmid].logk)
        binhigh = binmid;
      else
        binlow = binmid;
    }

  dlogk = TransferTable[binhigh].logk - TransferTable[binlow].logk;

  if(dlogk == 0)
    FatalError(777);

  u = (logk - TransferTable[binlow].logk) / dlogk;

  logT = (1 - u) * TransferTable[binlow].logT + u * TransferTable[binhigh].logT;

  T = pow(10.0, logT);

  return T;
}


double TransferFunc_Efstathiou(double k)
{
  return tk_Efstathiou(k);
}

double TransferFunc_EH(double k)   /* Eisenstein & Hu */
{
  /*redundant function*/
  return tk_eh(k);
}

double TransferFunc_FromPk(double k)   /* From Normalized Power */
{
  printf("TranferFunction_FromPk deprecrated\n");
  exit(2);
  return sqrt( PowerSpec(k) * exp ( -PrimordialIndex * log(k)) / Anorm );    /* assuming ns equal one */  
}

/**** end of additions ***/

double PowerSpec(double k)
{
  double power, alpha, Tf;

  switch (WhichSpectrum)
    {
    case 0:
      power = Norm * exp ( PrimordialIndex * log (k) ) * TransferFunc(k) * TransferFunc(k);
      break;

    case 1:
      power = PowerSpec_EH(k);
      power *= pow(k, PrimordialIndex - 1.0);
      break;

    case 2:
      power = PowerSpec_Tabulated(k);
      power *= pow(k, PrimordialIndex - 1.0);
      break;

    default:
      power = PowerSpec_Efstathiou(k);
      power *= pow(k, PrimordialIndex - 1.0);
      break;
    }


  if(WDM_On == 1)
    {
      /* Eqn. (A9) in Bode, Ostriker & Turok (2001), assuming gX=1.5  */
      alpha =
	0.048 * pow((Omega - OmegaBaryon) / 0.4, 0.15) * pow(HubbleParam / 0.65,
							     1.3) * pow(1.0 / WDM_PartMass_in_kev, 1.15);
      Tf = pow(1 + pow(alpha * k * (3.085678e24 / UnitLength_in_cm), 2 * 1.2), -5.0 / 1.2);
      power *= Tf * Tf;
    }

#if defined(MULTICOMPONENTGLASSFILE) && defined(DIFFERENT_TRANSFER_FUNC)

  if(Type == 2)
    {
      power = PowerSpec_DM_2ndSpecies(k);
    }

#endif


  return power;
}


double PowerSpec_DM_2ndSpecies(double k)
{
  /* at the moment, we simply call the Eistenstein & Hu spectrum
   * for the second DM species, but this could be replaced with
   * something more physical, say for neutrinos
   */

  double power;

  power = Norm * k * pow(tk_eh(k), 2);

  return power;
}



void read_power_table(void)
{
  FILE *fd;
  char buf[500];
  double k, p;


  sprintf(buf, FileWithInputSpectrum);

  if(!(fd = fopen(buf, "r")))
    {
      printf("can't read input spectrum in file '%s' on task %d\n", buf, ThisTask);
      FatalError(17);
    }

  NPowerTable = 0;
  do
    {
      if(fscanf(fd, " %lg %lg ", &k, &p) == 2)
	NPowerTable++;
      else
	break;
    }
  while(1);

  fclose(fd);

  PowerTable = malloc(NPowerTable * sizeof(struct pow_table));

  sprintf(buf, FileWithInputSpectrum);

  if(!(fd = fopen(buf, "r")))
    {
      printf("can't read input spectrum in file '%s' on task %d\n", buf, ThisTask);
      FatalError(18);
    }

  NPowerTable = 0;
  do
    {
      if(fscanf(fd, " %lg %lg ", &k, &p) == 2)
	{
	  PowerTable[NPowerTable].logk = k;
	  PowerTable[NPowerTable].logD = p;
	  NPowerTable++;
	}
      else
	break;
    }
  while(1);

  fclose(fd);

  qsort(PowerTable, NPowerTable, sizeof(struct pow_table), compare_logk);

  klower = pow(10.0, PowerTable[0].logk) * 1.0000001;

  if(PowerTable[0].logk >= -4.6 ) 
     {
      printf("\n WARNING: klower is %f, may be too large to normalize power  \n",klower);
      FatalError(1111);  
     }
  if(PowerTable[NTransferTable].logk <= log10(500./8.) ) 
     {
      printf("\n WARNING: kmax is may be too small to normalize power  \n");
      printf("\n Values outside the input range will be taken to be zero  \n");
      // FatalError(1111);  
     }



}

int compare_logk(const void *a, const void *b)
{
  if(((struct pow_table *) a)->logk < (((struct pow_table *) b)->logk))
    return -1;

  if(((struct pow_table *) a)->logk > (((struct pow_table *) b)->logk))
    return +1;

  return 0;
}

void initialize_powerspectrum(void)
{
  double res;

  InitTime = 1 / (1 + Redshift);

  AA = 6.4 / ShapeGamma * (3.085678e24 / UnitLength_in_cm);
  BB = 3.0 / ShapeGamma * (3.085678e24 / UnitLength_in_cm);
  CC = 1.7 / ShapeGamma * (3.085678e24 / UnitLength_in_cm);
  nu = 1.13;

  R8 = 8 * (3.085678e24 / UnitLength_in_cm);	/* 8 Mpc/h */


  if(WhichSpectrum == 2)
    read_power_table();

#ifdef DIFFERENT_TRANSFER_FUNC
  Type = 1;
#endif

  Norm = 1.0;
  res = TopHatSigma2(R8);

  if(ThisTask == 0 && WhichSpectrum == 2)
    printf("\nNormalization of spectrum in file:  Sigma8 = %g\n", sqrt(res));

  Norm = Sigma8 * Sigma8 / res;

  if(ThisTask == 0 && WhichSpectrum == 2)
    printf("Normalization adjusted to  Sigma8=%g   (Normfac=%g)\n\n", Sigma8, Norm);

  D0 = GrowthFactor(1.e-8, 1.0) * 1.e-8; /* Linnear growth factor at redshift z=0.0 normalized to D = a during matter domination */
  Dplus = GrowthFactor(InitTime, 1.0);

 switch (WhichSpectrum)
    {
    case 0:
      Anorm = Norm;
      break;  /* do not use power spectrum, only  transfer function file which must be set */

    case 1:
      // Anorm = Norm; /* --- not used in this version--*/ 
      break;

    case 2:
 
      // printf("\n klower, Pklower, Anorm, Primordial Index %f %f %f %f\n", klower,PowerSpec(klower),Anorm,PrimordialIndex);

      // Anorm = PowerSpec(klower) * exp( PrimordialIndex * log(klower)) ;  /* --- not used in this version--- */
      break;

    default:
      // Anorm = Norm;  /*--- not used in this version ----*/
      break;
    }

}

double PowerSpec_Tabulated(double k)
{
  double logk, logD, P, kold, u, dlogk, Delta2;
  int binlow, binhigh, binmid;

  kold = k;

  k *= (InputSpectrum_UnitLength_in_cm / UnitLength_in_cm);	/* convert to h/Mpc */

  logk = log10(k);

  if(logk < PowerTable[0].logk || logk > PowerTable[NPowerTable - 1].logk)
    return 0;

  binlow = 0;
  binhigh = NPowerTable - 1;

  while(binhigh - binlow > 1)
    {
      binmid = (binhigh + binlow) / 2;
      if(logk < PowerTable[binmid].logk)
	binhigh = binmid;
      else
	binlow = binmid;
    }

  dlogk = PowerTable[binhigh].logk - PowerTable[binlow].logk;

  if(dlogk == 0)
    FatalError(777);

  u = (logk - PowerTable[binlow].logk) / dlogk;

  logD = (1 - u) * PowerTable[binlow].logD + u * PowerTable[binhigh].logD;

  Delta2 = pow(10.0, logD);

  P = Norm * Delta2 / (4 * M_PI * kold * kold * kold); 
 
 // Norm should be normalized this way if Transfer Allowef rom power spectrum 
 // --currently deprecated-- so I can recover more standard convention
 // P = Norm * Delta2 / (kold * kold * kold);  //* Norm has to flollow a convention now it is externalized and used i main.c 
                                               //* P = Norm k * T^2, where Norm will be fitted
                                               //* therefore absorving constants. kold is to go from Delta2=4pi k^3 P(k)  to P(k)
                                               //*
  return P;
}

double PowerSpec_Efstathiou(double k)
{
  return Norm * k / pow(1 + pow(AA * k + pow(BB * k, 1.5) + CC * CC * k * k, nu), 2 / nu);
}


double tk_Efstathiou(double k)
{
  return 1.0; // (double) 1.0 / pow(1 + pow(AA * k + pow(BB * k, 1.5) + CC * CC * k * k, nu), 2 / nu);
}



double PowerSpec_EH(double k)	/* Eisenstein & Hu */
{
  return Norm * k * pow(tk_eh(k), 2);
}




double tk_eh(double k)		/* from Martin White */
{
  double q, theta, ommh2, a, s, gamma, L0, C0;
  double tmp;
  double omegam, ombh2, hubble;

  /* other input parameters */
  hubble = HubbleParam;

  omegam = Omega;
  ombh2 = OmegaBaryon * HubbleParam * HubbleParam;

  if(OmegaBaryon == 0)
    ombh2 = 0.04 * HubbleParam * HubbleParam;

  k *= (3.085678e24 / UnitLength_in_cm);	/* convert to h/Mpc */

  theta = 2.728 / 2.7;
  ommh2 = omegam * hubble * hubble;
  s = 44.5 * log(9.83 / ommh2) / sqrt(1. + 10. * exp(0.75 * log(ombh2))) * hubble;
  a = 1. - 0.328 * log(431. * ommh2) * ombh2 / ommh2
    + 0.380 * log(22.3 * ommh2) * (ombh2 / ommh2) * (ombh2 / ommh2);
  gamma = a + (1. - a) / (1. + exp(4 * log(0.43 * k * s)));
  gamma *= omegam * hubble;
  q = k * theta * theta / gamma;
  L0 = log(2. * exp(1.) + 1.8 * q);
  C0 = 14.2 + 731. / (1. + 62.5 * q);
  tmp = L0 / (L0 + C0 * q * q);
  return (tmp);
}



double TopHatSigma2(double R)
{
  r_tophat = R;

  return qromb(sigma2_int, 0, 500.0 * 1 / R);	/* note: 500/R is here chosen as 
						   integration boundary (infinity) */
}


double sigma2_int(double k)
{
  double kr, kr3, kr2, w, x;

  kr = r_tophat * k;
  kr2 = kr * kr;
  kr3 = kr2 * kr;

  if(kr < 1e-8)
    return 0;

  w = 3 * (sin(kr) / kr3 - cos(kr) / kr2);
  x = 4 * PI * k * k * w * w * PowerSpec(k);

  return x;
}


double GrowthFactor(double astart, double aend)
{
  return growth(aend) / growth(astart);
}


double growth(double a)
{
  double hubble_a;

  hubble_a = sqrt(Omega / (a * a * a) + (1 - Omega - OmegaLambda) / (a * a) + OmegaLambda);

  return hubble_a * qromb(growth_int, 0, a);
}


double growth_int(double a)
{
  return pow(a / (Omega + (1 - Omega - OmegaLambda) * a + OmegaLambda * a * a * a), 1.5);
}


double F_Omega(double a)
{
  double omega_a;

  omega_a = Omega / (Omega + a * (1 - Omega - OmegaLambda) + a * a * a * OmegaLambda);

  return pow(omega_a, 0.6);
}


double F2_Omega(double a)
{
  double omega_a;

  omega_a = Omega / (Omega + a * (1 - Omega - OmegaLambda) + a * a * a * OmegaLambda);

  return 2 * pow(omega_a, 4./7.);
}


/*  Here comes the stuff to compute the thermal WDM velocity distribution */


#define LENGTH_FERMI_DIRAC_TABLE 2000
#define MAX_FERMI_DIRAC          20.0

double fermi_dirac_vel[LENGTH_FERMI_DIRAC_TABLE];
double fermi_dirac_cumprob[LENGTH_FERMI_DIRAC_TABLE];

double WDM_V0 = 0;

double fermi_dirac_kernel(double x)
{
  return x * x / (exp(x) + 1);
}

void fermi_dirac_init(void)
{
  int i;

  for(i = 0; i < LENGTH_FERMI_DIRAC_TABLE; i++)
    {
      fermi_dirac_vel[i] = MAX_FERMI_DIRAC * i / (LENGTH_FERMI_DIRAC_TABLE - 1.0);
      fermi_dirac_cumprob[i] = qromb(fermi_dirac_kernel, 0, fermi_dirac_vel[i]);
    }

  for(i = 0; i < LENGTH_FERMI_DIRAC_TABLE; i++)
    fermi_dirac_cumprob[i] /= fermi_dirac_cumprob[LENGTH_FERMI_DIRAC_TABLE - 1];

  WDM_V0 = 0.012 * (1 + Redshift) * pow((Omega - OmegaBaryon) / 0.3, 1.0 / 3) * pow(HubbleParam / 0.65,
										    2.0 / 3) * pow(1.0 /
												   WDM_PartMass_in_kev,
												   4.0 / 3);

  if(ThisTask == 0)
    printf("\nWarm dark matter rms velocity dispersion at starting redshift = %g km/sec\n\n",
	   3.59714 * WDM_V0);

  WDM_V0 *= 1.0e5 / UnitVelocity_in_cm_per_s;

  /* convert from peculiar velocity to gadget's cosmological velocity */
  WDM_V0 *= sqrt(1 + Redshift);
}



double get_fermi_dirac_vel(void)
{
  int i;
  double p, u;

  p = drand48();
  i = 0;

  while(i < LENGTH_FERMI_DIRAC_TABLE - 2)
    if(p > fermi_dirac_cumprob[i + 1])
      i++;
    else
      break;

  u = (p - fermi_dirac_cumprob[i]) / (fermi_dirac_cumprob[i + 1] - fermi_dirac_cumprob[i]);

  return fermi_dirac_vel[i] * (1 - u) + fermi_dirac_vel[i + 1] * u;
}



void add_WDM_thermal_speeds(float *vel)
{
  double v, phi, theta, vx, vy, vz;

  if(WDM_V0 == 0)
    fermi_dirac_init();

  v = WDM_V0 * get_fermi_dirac_vel();

  phi = 2 * M_PI * drand48();
  theta = acos(2 * drand48() - 1);

  vx = v * sin(theta) * cos(phi);
  vy = v * sin(theta) * sin(phi);
  vz = v * cos(theta);

  vel[0] += vx;
  vel[1] += vy;
  vel[2] += vz;
}




// wrc below

int compare_transfer_logk_PNG(const void *a, const void *b)
{
  if(((struct trans_table_PNG *) a)->logk < (((struct trans_table_PNG *) b)->logk))
    return -1;

  if(((struct trans_table_PNG *) a)->logk > (((struct trans_table_PNG *) b)->logk))
    return +1;

  return 0;
}

void read_transfer_PNG_table(void)
{
  FILE *fd;
  char buf[500];
  double k, t;

  if(ThisTask == 0)
  {
  printf("reading transfer file: %s \n",FileWithInputTransferPNG);
  }
  sprintf(buf, FileWithInputTransferPNG);

  if(!(fd = fopen(buf, "r")))
    {
      printf("can't read input transfer function  in file '%s' on task %d\n", buf, ThisTask);
      FatalError(17);
    }

  NTransferTablePNG = 0;
  do
    {
      if(fscanf(fd, " %le %le ", &k, &t) == 2)
        NTransferTablePNG++;
      else
        break;
    }
  while(1);

  fclose(fd);

  TransferTablePNG = malloc(NTransferTablePNG * sizeof(struct trans_table_PNG));

  sprintf(buf, FileWithInputTransferPNG);

  if(!(fd = fopen(buf, "r")))
    {
      printf("can't read input transfer function in file '%s' on task %d\n", buf, ThisTask);
      FatalError(18);
    }

  NTransferTablePNG = 0;
  do
    {
      if(fscanf(fd, " %le %le ", &k, &t) == 2)
        {
          TransferTablePNG[NTransferTablePNG].logk = log10(k);
          TransferTablePNG[NTransferTablePNG].T = (t);
          NTransferTablePNG++;
        }
      else
        break;
    }
  while(1);

  fclose(fd);

  qsort(TransferTablePNG, NTransferTablePNG, sizeof(struct trans_table_PNG), compare_transfer_logk_PNG); 

  klower = pow(10.0, TransferTablePNG[0].logk);

  // if(TransferTablePNG[0].logk >= -4.6 ) 
  //    {
  //     printf("\n WARNING: klower is %f, may be too large to normalize transfer  \n",klower);
  //     FatalError(1111);  
  //    }
  // if(TransferTablePNG[NTransferTablePNG - 1].logk <= log10(500./8.) ) 
  //    {
  //     printf("\n WARNING: kmax is may be too small to normalize transfer  \n");
  //     printf("\n Values outside the input range will be taken to be zero  \n");
  //     // FatalError(1111);  
  //    }

      // Tlower = TransferTablePNG[0].logT;      
      // for(i=0; i < NTransferTablePNG ; i++ )
      //     {
      //     TransferTablePNG[i].logT -= Tlower;
      //     }
}




double TransferFunc_PNG_Tabulated(double k)
{
  double logk, T, u, dlogk; 
  int binlow, binhigh, binmid;

  k *= (InputSpectrum_UnitLength_in_cm / UnitLength_in_cm);     /* convert to h/Mpc */

  logk = log10(k);

  if(logk < TransferTablePNG[0].logk || logk > TransferTablePNG[NTransferTablePNG - 1].logk)
    return 0;

  binlow = 0;
  binhigh = NTransferTablePNG - 1;

  while(binhigh - binlow > 1)
    {
      binmid = (binhigh + binlow) / 2;
      if(logk < TransferTablePNG[binmid].logk)
        binhigh = binmid;
      else
        binlow = binmid;
    }

  dlogk = TransferTablePNG[binhigh].logk - TransferTablePNG[binlow].logk;

  if(dlogk == 0)
    FatalError(777);

  u = (logk - TransferTablePNG[binlow].logk) / dlogk;

  T = (1 - u) * TransferTablePNG[binlow].T + u * TransferTablePNG[binhigh].T;
  //logT = (1 - u) * TransferTablePNG[binlow].logT + u * TransferTablePNG[binhigh].logT;

  //T = pow(10.0, logT);

  return T;
}




double pdf_with_tails(double x) // p(x) for pdf
{
    double alpha_new,normG,normE,prb;
    double TailsNewDistStd = sqrt(TailsNewDistVariance);


    alpha_new = TailsSwitchSigma/(TailsNewDistStd*TailsExponentPow*pow( TailsSwitchSigma*TailsNewDistStd,TailsExponentPow-1) );
    
    normG = sqrt(1./TailsNewDistVariance/2./M_PI);
    if ( fabs(x)>TailsSwitchSigma*TailsNewDistStd)
    {
        normE = normG;
        normE *= exp(-1./2*pow(TailsSwitchSigma,2));
        normE *= exp(alpha_new*pow( TailsSwitchSigma*TailsNewDistStd , TailsExponentPow ) );
        prb = normE*exp(-alpha_new*pow(fabs(x),TailsExponentPow));
    }
    else
    {

        prb = normG*exp(-.5*pow(x,2)/TailsNewDistVariance);
    }
     
    return prb;
}



void initalize_cdf_table(void)
{

  int i,NPkInt;
  double x_min,x_max,x_step,fac,Beta;
  double k0,k01,k1,dk,k2Pk0,k2Pk01,k2Pk1;



  // Pk normalization from main.c
  //fac = pow(2 * PI / Box, 1.5);
  fac = pow(2*PI,1.5);
  Beta = 1.5 * Omega / (2998. * 2998. / UnitLength_in_cm / UnitLength_in_cm * 3.085678e24 * 3.085678e24 ) / D0 ;  



  theoryGausVar =0;

  // Number of ks and kmin,kmax (implicit)
  NPkInt = 100000;
  k0 =  2*PI / Box;
  dk = (PI/Box*Nsample-k0)/NPkInt;

  // In the loop k0 is updated as k1 from the previous iteration. Need to set this manually for i=0

  k2Pk0 = Anorm * exp( PrimordialIndex * log(k0) );   /* initial normalized power */                      
  k2Pk0 *= pow(fac * Beta / k0,2);    /* amplitude of the initial gaussian potential */

  for(i=0; i < NPkInt ; i++ )
      {
        k1 = k0+dk;

        k01 = (k1+k0)/2;


        k2Pk01 = Anorm * exp( PrimordialIndex * log(k01) );   /* initial normalized power */                      
        k2Pk01 *= pow(fac * Beta / k01,2);    /* amplitude of the initial potential */
        k2Pk1 = Anorm * exp( PrimordialIndex * log(k1) );   /* initial normalized power */                      
        k2Pk1 *= pow(fac * Beta / k1,2);    /* amplitude of the initial potential */

        // simps 1/3
        theoryGausVar+= dk/6.*(k2Pk0+4*k2Pk01+k2Pk1);

        k0=k1;
        k2Pk0=k2Pk1;
      }

  theoryGausVar /= 2*pow(M_PI,2);
  //theoryGausVar*=1.0224997103866253e+18;
  if(ThisTask == 0)
  {
  printf("Theoretical Variance set to: %e \n", theoryGausVar);
  }
  TailsNewDistVariance = theoryGausVar;//*TailsVarianceRatio;
 
  NCdfTable  = 10000;
  
  x_min = -15*pow(TailsNewDistVariance,0.5);
  x_max = 15*pow(TailsNewDistVariance,0.5);
  x_step = (x_max-x_min)/(NCdfTable);

  CdfTable = malloc(NCdfTable * sizeof(struct pdf_table));

  CdfTable[0].x = x_min;
  CdfTable[0].cdf = pdf_with_tails(x_min);

  for(i=1; i < NCdfTable ; i++ )
  {
    CdfTable[i].x =  CdfTable[i-1].x+x_step;
    CdfTable[i].cdf = CdfTable[i-1].cdf+ pdf_with_tails(CdfTable[i].x);
  }

  for(i=0; i < NCdfTable ; i++ )
  {
    CdfTable[i].cdf /= CdfTable[NCdfTable-1].cdf;
  }

}




double inverse_cdf_tabulated(double cdf_test)
{
  double  x, u, dCdf; 
  int binlow, binhigh, binmid;


  if(cdf_test < CdfTable[0].cdf)
    return CdfTable[0].x;
  else if (cdf_test == CdfTable[NCdfTable-1].cdf)
    return CdfTable[NCdfTable-1].x;

  binlow = 0;
  binhigh = NCdfTable - 1;

  while(binhigh - binlow > 1)
    {
      binmid = (binhigh + binlow) / 2;
      if(cdf_test < CdfTable[binmid].cdf)
        binhigh = binmid;
      else
        binlow = binmid;
    }

  dCdf = CdfTable[binhigh].cdf - CdfTable[binlow].cdf;

  if(dCdf == 0)
    FatalError(777);

  u = (cdf_test - CdfTable[binlow].cdf) / dCdf;

  x = (1 - u) * CdfTable[binlow].x + u * CdfTable[binhigh].x;
  return x;
}



double transform_to_tails(double phi)
{
  double  cdf_value; 
  cdf_value = (.5*(1.+erf(phi/(sqrt(theoryGausVar)*sqrt(2)))));
  return inverse_cdf_tabulated(cdf_value);
}