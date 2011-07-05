#include "sapt2.h"

namespace psi { namespace sapt {

SAPT2::SAPT2(Options& options, boost::shared_ptr<PSIO> psio, 
  boost::shared_ptr<Chkpt> chkpt) : SAPT(options, psio, chkpt)
{
  psio_->open(PSIF_SAPT_AA_DF_INTS,PSIO_OPEN_NEW);
  psio_->open(PSIF_SAPT_BB_DF_INTS,PSIO_OPEN_NEW);
  psio_->open(PSIF_SAPT_AB_DF_INTS,PSIO_OPEN_NEW);
  psio_->open(PSIF_SAPT_AMPS,PSIO_OPEN_NEW);

  maxiter_ = options_.get_int("MAXITER");
  e_conv_ = pow(10.0,-options_.get_int("E_CONVERGE"));
  d_conv_ = pow(10.0,-options_.get_int("D_CONVERGE"));

  ioff_ = (int *) malloc (sizeof(int) * (nso_*(nso_+1)/2));
  index2i_ = (int *) malloc (sizeof(int) * (nso_*(nso_+1)/2));
  index2j_ = (int *) malloc (sizeof(int) * (nso_*(nso_+1)/2));

  ioff_[0] = 0;

  for (int i=1; i < (nso_*(nso_+1)/2); i++) {
    ioff_[i] = ioff_[i-1] + i;
  }

  for (int i=0, ij=0; i<nso_; i++) {
    for (int j=0; j<=i; j++, ij++) {
      index2i_[ij] = i;
      index2j_[ij] = j;
  }}

  wBAR_ = NULL;
  wABS_ = NULL;
}

SAPT2::~SAPT2()
{
  if (wBAR_ != NULL) free_block(wBAR_);
  if (wABS_ != NULL) free_block(wABS_);
  psio_->close(PSIF_SAPT_AA_DF_INTS,1);
  psio_->close(PSIF_SAPT_BB_DF_INTS,1);
  psio_->close(PSIF_SAPT_AB_DF_INTS,1);
  psio_->close(PSIF_SAPT_AMPS,1);
}

double SAPT2::compute_energy()
{
  print_header();

  timer_on("DF Integrals       ");
    df_integrals();
  timer_off("DF Integrals       ");
  timer_on("Omega Integrals    ");
    w_integrals();
  timer_off("Omega Integrals    ");
  timer_on("Elst10             ");
    elst10();
  timer_off("Elst10             ");
  timer_on("Exch10 S^2         ");
    exch10_s2();
  timer_off("Exch10 S^2         ");
  timer_on("Exch10             ");
    exch10();
  timer_off("Exch10             ");
  timer_on("Ind20,r            ");
    ind20r();
  timer_off("Ind20,r            ");
  timer_on("Exch-Ind20,r       ");
    exch_ind20r();
  timer_off("Exch-Ind20,r       ");

  print_results();

  return (e_sapt0_);
}

void SAPT2::print_header()
{
  fprintf(outfile,"        SAPT2  \n");
  fprintf(outfile,"    Ed Hohenstein\n") ;
  fprintf(outfile,"     6 June 2009\n") ;
  fprintf(outfile,"\n");
  fprintf(outfile,"      Orbital Information\n");
  fprintf(outfile,"  --------------------------\n");
  fprintf(outfile,"    NSO        = %9d\n",nso_);
  fprintf(outfile,"    NMO        = %9d\n",nmo_);
  fprintf(outfile,"    NRI        = %9d\n",ndf_);
  fprintf(outfile,"    NOCC A     = %9d\n",noccA_);
  fprintf(outfile,"    NOCC B     = %9d\n",noccB_);
  fprintf(outfile,"    FOCC A     = %9d\n",foccA_);
  fprintf(outfile,"    FOCC B     = %9d\n",foccB_);
  fprintf(outfile,"    NVIR A     = %9d\n",nvirA_);
  fprintf(outfile,"    NVIR B     = %9d\n",nvirB_);
  fprintf(outfile,"\n");
  fflush(outfile);
}

void SAPT2::print_results()
{
  e_sapt0_ = eHF_ + e_disp20_ + e_exch_disp20_;
  double dHF = eHF_ - (e_elst10_ + e_exch10_ + e_ind20_ + e_exch_ind20_);

  fprintf(outfile,"\n    SAPT Results  \n");
  fprintf(outfile,"  ------------------------------------------------------------------\n");
  fprintf(outfile,"    E_HF          %16.8lf mH %16.8lf kcal mol^-1\n",
    eHF_*1000.0,eHF_*627.5095);
  fprintf(outfile,"    Elst10,r      %16.8lf mH %16.8lf kcal mol^-1\n",
    e_elst10_*1000.0,e_elst10_*627.5095);
  fprintf(outfile,"    Exch10        %16.8lf mH %16.8lf kcal mol^-1\n",
    e_exch10_*1000.0,e_exch10_*627.5095);
  fprintf(outfile,"    Exch10(S^2)   %16.8lf mH %16.8lf kcal mol^-1\n",
    e_exch10_s2_*1000.0,e_exch10_s2_*627.5095);
  fprintf(outfile,"    Ind20,r       %16.8lf mH %16.8lf kcal mol^-1\n",
    e_ind20_*1000.0,e_ind20_*627.5095);
  fprintf(outfile,"    Exch-Ind20,r  %16.8lf mH %16.8lf kcal mol^-1\n",
    e_exch_ind20_*1000.0,e_exch_ind20_*627.5095);
  fprintf(outfile,"    delta HF,r    %16.8lf mH %16.8lf kcal mol^-1\n",
    dHF*1000.0,dHF*627.5095);
  fprintf(outfile,"    Disp20        %16.8lf mH %16.8lf kcal mol^-1\n",
    e_disp20_*1000.0,e_disp20_*627.5095);
  fprintf(outfile,"    Exch-Disp20   %16.8lf mH %16.8lf kcal mol^-1\n\n",
    e_exch_disp20_*1000.0,e_exch_disp20_*627.5095);
  fprintf(outfile,"    Total SAPT0   %16.8lf mH %16.8lf kcal mol^-1\n",
    e_sapt0_*1000.0,e_sapt0_*627.5095);

  double tot_elst = e_elst10_;
  double tot_exch = e_exch10_;
  double tot_ind = e_ind20_ + e_exch_ind20_ + dHF;
  double tot_disp = e_disp20_ + e_exch_disp20_;

  Process::environment.globals["SAPT ELST ENERGY"] = tot_elst;
  Process::environment.globals["SAPT EXCH ENERGY"] = tot_exch;
  Process::environment.globals["SAPT IND ENERGY"] = tot_ind;
  Process::environment.globals["SAPT DISP ENERGY"] = tot_disp;
  Process::environment.globals["SAPT SAPT0 ENERGY"] = e_sapt0_;
  Process::environment.globals["SAPT ENERGY"] = e_sapt0_;
}

void SAPT2::df_integrals()
{
  // Get fitting metric
  boost::shared_ptr<FittingMetric> metric = boost::shared_ptr<FittingMetric>(
    new FittingMetric(ribasis_));
  metric->form_eig_inverse();
  double **J_temp = metric->get_metric()->pointer();
  double **J_mhalf = block_matrix(ndf_,ndf_);
  C_DCOPY(ndf_*ndf_,J_temp[0],1,J_mhalf[0],1);
  metric.reset();

  boost::shared_ptr<IntegralFactory> rifactory_J(new IntegralFactory(ribasis_,
    zero_, ribasis_, zero_));
  boost::shared_ptr<TwoBodyAOInt> Jint = boost::shared_ptr<TwoBodyAOInt>(
    rifactory_J->eri());
  const double *Jbuffer = Jint->buffer();

  boost::shared_ptr<IntegralFactory> ao_eri_factory(new IntegralFactory(
    basisset_, basisset_, basisset_, basisset_));
  boost::shared_ptr<TwoBodyAOInt> ao_eri = boost::shared_ptr<TwoBodyAOInt>(
    ao_eri_factory->eri());
  const double *ao_buffer = ao_eri->buffer();

  double *Schwartz = init_array(basisset_->nshell()*(basisset_->nshell()+1)/2);
  double *DFSchwartz = init_array(ribasis_->nshell());

  for(int P=0,PQ=0;P<basisset_->nshell();P++) {
    int numw = basisset_->shell(P)->nfunction();
    for(int Q=0;Q<=P;Q++,PQ++) {
      int numx = basisset_->shell(Q)->nfunction();
      double tei, max=0.0;

      ao_eri->compute_shell(P, Q, P, Q);

      for(int w=0;w<numw;w++) {
        for(int x=0;x<numx;x++) {
          int index = ( (w*numx + x) * numw + w) * numx + x;
          tei = ao_buffer[index];
          if(fabs(tei) > max) max = fabs(tei);
        }
      }
      Schwartz[PQ] = max;
    }
  }

  for(int P=0;P<ribasis_->nshell();P++) {
    int numw = ribasis_->shell(P)->nfunction();
    double tei, max=0.0;

    Jint->compute_shell(P, 0, P, 0);

    for(int w=0;w<numw;w++) {
      tei = Jbuffer[w];
      if(fabs(tei) > max) max = fabs(tei);
    }
    DFSchwartz[P] = max;
  }

  boost::shared_ptr<IntegralFactory> rifactory(new IntegralFactory(ribasis_,
    zero_, basisset_, basisset_));

  int nthreads = 1;
  #ifdef _OPENMP
    nthreads = omp_get_max_threads();
  #endif
  int rank = 0;

  boost::shared_ptr<TwoBodyAOInt> *eri = 
    new boost::shared_ptr<TwoBodyAOInt>[nthreads];
  const double **buffer = new const double*[nthreads];
  for(int i = 0;i < nthreads;++i){
    eri[i] = boost::shared_ptr<TwoBodyAOInt>(rifactory->eri());
    buffer[i] = eri[i]->buffer();
  }

  int mn;
  int maxPshell = 0;
  for (int Pshell=0; Pshell < ribasis_->nshell(); ++Pshell) {
    int numPshell = ribasis_->shell(Pshell)->nfunction();
    if (numPshell > maxPshell) maxPshell = numPshell;
  }

  psio_->open(PSIF_SAPT_TEMP,0);

  double** AO_RI = block_matrix(maxPshell,nso_*nso_);
  double* halftrans = init_array(nmo_*nso_);
  double** MO_RI = block_matrix(maxPshell,nmo_*nmo_);

  psio_address next_DF_MO = PSIO_ZERO;
  psio_address next_bare_AR = PSIO_ZERO;

  int nshelltri = basisset_->nshell()*(basisset_->nshell()+1)/2;

  for (int Pshell=0; Pshell < ribasis_->nshell(); ++Pshell) {
    int numPshell = ribasis_->shell(Pshell)->nfunction();

    #pragma omp for private(mn) schedule(guided)
    for (mn=0; mn < nshelltri; ++mn) {
      #ifdef _OPENMP
        rank = omp_get_thread_num();
      #endif
      int MU = index2i_[mn];
      int NU = index2j_[mn];
      int nummu = basisset_->shell(MU)->nfunction();
      int numnu = basisset_->shell(NU)->nfunction();

      if (sqrt(Schwartz[mn]*DFSchwartz[Pshell])>schwarz_) {
        eri[rank]->compute_shell(Pshell, 0, MU, NU);

        for (int P=0, index=0; P < numPshell; ++P) {

          for (int mu=0; mu < nummu; ++mu) {
            int omu = basisset_->shell(MU)->function_index() + mu;

            for (int nu=0; nu < numnu; ++nu, ++index) {
              int onu = basisset_->shell(NU)->function_index() + nu;

              AO_RI[P][omu*nso_+onu] = buffer[rank][index];
              AO_RI[P][onu*nso_+omu] = buffer[rank][index];
            }
          }
        }

      } // end Schwartz inequality
    } // end loop over MU,NU shells

    for (int P=0; P < numPshell; ++P) {
      C_DGEMM('T', 'N', nmo_, nso_, nso_, 1.0, CA_[0], nmo_, AO_RI[P], nso_, 
        0.0, halftrans, nso_);
      C_DGEMM('N', 'N', nmo_, nmo_, nso_, 1.0, halftrans, nso_, CA_[0], nmo_, 
        0.0, MO_RI[P], nmo_);
    }

    psio_->write(PSIF_SAPT_TEMP,"MO RI Integrals",(char *) &(MO_RI[0][0]),
      sizeof(double)*numPshell*nmo_*nmo_,next_DF_MO,&next_DF_MO);

  }

  free_block(AO_RI);
  free_block(MO_RI);

  zero_disk(PSIF_SAPT_AA_DF_INTS,"AA RI Integrals",noccA_*noccA_,ndf_+3);
  zero_disk(PSIF_SAPT_AA_DF_INTS,"AR RI Integrals",noccA_*nvirA_,ndf_+3);
  zero_disk(PSIF_SAPT_AA_DF_INTS,"RR RI Integrals",nvirA_*nvirA_,ndf_+3);

  long int numP;
  long int temp_size = mem_ / (2*ndf_);

  if (temp_size > nmo_*nmo_)
    temp_size = nmo_*nmo_;

  double** temp;
  double** temp_J;

  psio_address next_DF_AA = PSIO_ZERO;
  psio_address next_DF_AR = PSIO_ZERO;
  psio_address next_DF_RR = PSIO_ZERO;

  for (int i=0,oP=0; oP < nmo_*nmo_; ++i, oP += numP) {
    if ((i+1)*temp_size < nmo_*nmo_) numP = temp_size;
    else numP = nmo_*nmo_ - oP;

    temp = block_matrix(ndf_,numP);

    next_DF_MO = psio_get_address(PSIO_ZERO,sizeof(double)*oP);
    for (int P=0; P < ndf_; ++P) {
      psio_->read(PSIF_SAPT_TEMP,"MO RI Integrals",(char *) &(temp[P][0]),
        sizeof(double)*numP,next_DF_MO,&next_DF_MO);
      next_DF_MO = psio_get_address(next_DF_MO,sizeof(double)*(nmo_*nmo_-numP));
    }

    temp_J = block_matrix(numP,ndf_+3);

    C_DGEMM('T','N',numP,ndf_,ndf_,1.0,temp[0],numP,
      J_mhalf[0],ndf_,0.0,temp_J[0],ndf_+3);

    free_block(temp);

    for (int ij=0; ij<numP; ij++) {
      int i = (ij+oP)/nmo_;
      int j = (ij+oP)%nmo_;
      if (i < noccA_ && j < noccA_) {
        psio_->write(PSIF_SAPT_AA_DF_INTS,"AA RI Integrals",(char *)
          &(temp_J[ij][0]),sizeof(double)*(ndf_+3),next_DF_AA,&next_DF_AA);
      }
    }

    for (int ij=0; ij<numP; ij++) {
      int i = (ij+oP)/nmo_;
      int j = (ij+oP)%nmo_;
      if (i < noccA_ && j >= noccA_) {
        psio_->write(PSIF_SAPT_AA_DF_INTS,"AR RI Integrals",(char *)
          &(temp_J[ij][0]),sizeof(double)*(ndf_+3),next_DF_AR,&next_DF_AR);
      }
    }

    for (int ij=0; ij<numP; ij++) {
      int i = (ij+oP)/nmo_;
      int j = (ij+oP)%nmo_;
      if (i >= noccA_ && j >= noccA_) {
        psio_->write(PSIF_SAPT_AA_DF_INTS,"RR RI Integrals",(char *)
          &(temp_J[ij][0]),sizeof(double)*(ndf_+3),next_DF_RR,&next_DF_RR);
      }
    }

    free_block(temp_J);
  }

  AO_RI = block_matrix(maxPshell,nso_*nso_);
  MO_RI = block_matrix(maxPshell,nmo_*nmo_);

  next_DF_MO = PSIO_ZERO;
  psio_address next_bare_BS = PSIO_ZERO;

  for (int Pshell=0; Pshell < ribasis_->nshell(); ++Pshell) {
    int numPshell = ribasis_->shell(Pshell)->nfunction();

    #pragma omp for private(mn) schedule(guided)
    for (mn=0; mn < nshelltri; ++mn) {
      #ifdef _OPENMP
        rank = omp_get_thread_num();
      #endif
      int MU = index2i_[mn];
      int NU = index2j_[mn];
      int nummu = basisset_->shell(MU)->nfunction();
      int numnu = basisset_->shell(NU)->nfunction();

      if (sqrt(Schwartz[mn]*DFSchwartz[Pshell])>schwarz_) {
        eri[rank]->compute_shell(Pshell, 0, MU, NU);

        for (int P=0, index=0; P < numPshell; ++P) {

          for (int mu=0; mu < nummu; ++mu) {
            int omu = basisset_->shell(MU)->function_index() + mu;

            for (int nu=0; nu < numnu; ++nu, ++index) {
              int onu = basisset_->shell(NU)->function_index() + nu;

              AO_RI[P][omu*nso_+onu] = buffer[rank][index];
              AO_RI[P][onu*nso_+omu] = buffer[rank][index];
            }
          }
        }

      } // end Schwartz inequality
    } // end loop over MU,NU shells

    for (int P=0; P < numPshell; ++P) {
      C_DGEMM('T', 'N', nmo_, nso_, nso_, 1.0, CB_[0], nmo_, AO_RI[P], nso_, 
        0.0, halftrans, nso_);
      C_DGEMM('N', 'N', nmo_, nmo_, nso_, 1.0, halftrans, nso_, CB_[0], nmo_, 
        0.0, MO_RI[P], nmo_);
    }

    psio_->write(PSIF_SAPT_TEMP,"MO RI Integrals",(char *) &(MO_RI[0][0]),
      sizeof(double)*numPshell*nmo_*nmo_,next_DF_MO,&next_DF_MO);

  }

  free_block(AO_RI);
  free_block(MO_RI);

  zero_disk(PSIF_SAPT_BB_DF_INTS,"BB RI Integrals",noccB_*noccB_,ndf_+3);
  zero_disk(PSIF_SAPT_BB_DF_INTS,"BS RI Integrals",noccB_*nvirB_,ndf_+3);
  zero_disk(PSIF_SAPT_BB_DF_INTS,"SS RI Integrals",nvirB_*nvirB_,ndf_+3);

  psio_address next_DF_BB = PSIO_ZERO;
  psio_address next_DF_BS = PSIO_ZERO;
  psio_address next_DF_SS = PSIO_ZERO;

  for (int i=0,oP=0; oP < nmo_*nmo_; ++i, oP += numP) {
    if ((i+1)*temp_size < nmo_*nmo_) numP = temp_size;
    else numP = nmo_*nmo_ - oP;

    temp = block_matrix(ndf_,numP);

    next_DF_MO = psio_get_address(PSIO_ZERO,sizeof(double)*oP);
    for (int P=0; P < ndf_; ++P) {
      psio_->read(PSIF_SAPT_TEMP,"MO RI Integrals",(char *) &(temp[P][0]),
        sizeof(double)*numP,next_DF_MO,&next_DF_MO);
      next_DF_MO = psio_get_address(next_DF_MO,sizeof(double)*(nmo_*nmo_
        -numP));
    }

    temp_J = block_matrix(numP,ndf_+3);

    C_DGEMM('T','N',numP,ndf_,ndf_,1.0,temp[0],numP,J_mhalf[0],ndf_,0.0,
      temp_J[0],ndf_+3);

    free_block(temp);

    for (int ij=0; ij<numP; ij++) {
      int i = (ij+oP)/nmo_;
      int j = (ij+oP)%nmo_;
      if (i < noccB_ && j < noccB_) {
        psio_->write(PSIF_SAPT_BB_DF_INTS,"BB RI Integrals",(char *)
          &(temp_J[ij][0]),sizeof(double)*(ndf_+3),next_DF_BB,&next_DF_BB);
      }
    }

    for (int ij=0; ij<numP; ij++) {
      int i = (ij+oP)/nmo_;
      int j = (ij+oP)%nmo_;
      if (i < noccB_ && j >= noccB_) {
        psio_->write(PSIF_SAPT_BB_DF_INTS,"BS RI Integrals",(char *)
          &(temp_J[ij][0]),sizeof(double)*(ndf_+3),next_DF_BS,&next_DF_BS);
      }
    }

    for (int ij=0; ij<numP; ij++) {
      int i = (ij+oP)/nmo_;
      int j = (ij+oP)%nmo_;
      if (i >= noccB_ && j >= noccB_) {
        psio_->write(PSIF_SAPT_BB_DF_INTS,"SS RI Integrals",(char *)
          &(temp_J[ij][0]),sizeof(double)*(ndf_+3),next_DF_SS,&next_DF_SS);
      }
    }

    free_block(temp_J);
  }

  AO_RI = block_matrix(maxPshell,nso_*nso_);
  MO_RI = block_matrix(maxPshell,nmo_*nmo_);

  next_DF_MO = PSIO_ZERO;

  for (int Pshell=0; Pshell < ribasis_->nshell(); ++Pshell) {
    int numPshell = ribasis_->shell(Pshell)->nfunction();

    #pragma omp for private(mn) schedule(guided)
    for (mn=0; mn < nshelltri; ++mn) {
      #ifdef _OPENMP
        rank = omp_get_thread_num();
      #endif
      int MU = index2i_[mn];
      int NU = index2j_[mn];
      int nummu = basisset_->shell(MU)->nfunction();
      int numnu = basisset_->shell(NU)->nfunction();

      if (sqrt(Schwartz[mn]*DFSchwartz[Pshell])>schwarz_) {
        eri[rank]->compute_shell(Pshell, 0, MU, NU);

        for (int P=0, index=0; P < numPshell; ++P) {

          for (int mu=0; mu < nummu; ++mu) {
            int omu = basisset_->shell(MU)->function_index() + mu;

            for (int nu=0; nu < numnu; ++nu, ++index) {
              int onu = basisset_->shell(NU)->function_index() + nu;

              AO_RI[P][omu*nso_+onu] = buffer[rank][index];
              AO_RI[P][onu*nso_+omu] = buffer[rank][index];
            }
          }
        }

      } // end Schwartz inequality
    } // end loop over MU,NU shells

    for (int P=0; P < numPshell; ++P) {
      C_DGEMM('T', 'N', nmo_, nso_, nso_, 1.0, CA_[0], nmo_, AO_RI[P], nso_, 
        0.0, halftrans, nso_);
      C_DGEMM('N', 'N', nmo_, nmo_, nso_, 1.0, halftrans, nso_, CB_[0], nmo_, 
        0.0, MO_RI[P], nmo_);
    }

    psio_->write(PSIF_SAPT_TEMP,"MO RI Integrals",(char *) &(MO_RI[0][0]),
      sizeof(double)*numPshell*nmo_*nmo_,next_DF_MO,&next_DF_MO);

  }

  free_block(AO_RI);
  free_block(MO_RI);

  zero_disk(PSIF_SAPT_AB_DF_INTS,"AB RI Integrals",noccA_*noccB_,ndf_+3);
  zero_disk(PSIF_SAPT_AB_DF_INTS,"AS RI Integrals",noccA_*nvirB_,ndf_+3);
  zero_disk(PSIF_SAPT_AB_DF_INTS,"RB RI Integrals",nvirA_*noccB_,ndf_+3);

  psio_address next_DF_AB = PSIO_ZERO;
  psio_address next_DF_AS = PSIO_ZERO;
  psio_address next_DF_RB = PSIO_ZERO;

  for (int i=0,oP=0; oP < nmo_*nmo_; ++i, oP += numP) {
    if ((i+1)*temp_size < nmo_*nmo_) numP = temp_size;
    else numP = nmo_*nmo_ - oP;

    temp = block_matrix(ndf_,numP);

    next_DF_MO = psio_get_address(PSIO_ZERO,sizeof(double)*oP);
    for (int P=0; P < ndf_; ++P) {
      psio_->read(PSIF_SAPT_TEMP,"MO RI Integrals",(char *) &(temp[P][0]),
        sizeof(double)*numP,next_DF_MO,&next_DF_MO);
      next_DF_MO = psio_get_address(next_DF_MO,sizeof(double)*(nmo_*nmo_
        -numP));
    }

    temp_J = block_matrix(numP,ndf_+3);

    C_DGEMM('T','N',numP,ndf_,ndf_,1.0,temp[0],numP,J_mhalf[0],ndf_,
      0.0,temp_J[0],ndf_+3);

    free_block(temp);

    for (int ij=0; ij<numP; ij++) {
      int i = (ij+oP)/nmo_;
      int j = (ij+oP)%nmo_;
      if (i < noccA_ && j < noccB_) {
        psio_->write(PSIF_SAPT_AB_DF_INTS,"AB RI Integrals",(char *)
          &(temp_J[ij][0]),sizeof(double)*(ndf_+3),next_DF_AB,&next_DF_AB);
      }
    }

    for (int ij=0; ij<numP; ij++) {
      int i = (ij+oP)/nmo_;
      int j = (ij+oP)%nmo_;
      if (i < noccA_ && j >= noccB_) {
        psio_->write(PSIF_SAPT_AB_DF_INTS,"AS RI Integrals",(char *)
          &(temp_J[ij][0]),sizeof(double)*(ndf_+3),next_DF_AS,&next_DF_AS);
      }
    }

    for (int ij=0; ij<numP; ij++) {
      int i = (ij+oP)/nmo_;
      int j = (ij+oP)%nmo_;
      if (i >= noccA_ && j < noccB_) {
        psio_->write(PSIF_SAPT_AB_DF_INTS,"RB RI Integrals",(char *)
          &(temp_J[ij][0]),sizeof(double)*(ndf_+3),next_DF_RB,&next_DF_RB);
      }
    }

    free_block(temp_J);
  }

  free(halftrans);

  free_block(J_mhalf);

  psio_->close(PSIF_SAPT_TEMP,0);

  free(Schwartz);
  free(DFSchwartz);

  fflush(outfile);
}

void SAPT2::w_integrals()
{
  double **B_p_A = get_diag_AA_ints(1);
  double **B_p_B = get_diag_BB_ints(1);

  diagAA_ = init_array(ndf_+3);
  diagBB_ = init_array(ndf_+3);

  for(int a=0; a<noccA_; a++){
    C_DAXPY(ndf_+3,1.0,&(B_p_A[a][0]),1,diagAA_,1);
  }

  for(int b=0; b<noccB_; b++){
    C_DAXPY(ndf_+3,1.0,&(B_p_B[b][0]),1,diagBB_,1);
  }

  free_block(B_p_A);
  free_block(B_p_B);

  wBAR_ = block_matrix(noccA_,nvirA_);

  for(int a=0; a<noccA_; a++){
    C_DAXPY(nvirA_,1.0,&(vBAA_[a][noccA_]),1,&(wBAR_[a][0]),1);
  }

  double **B_p_AR = get_AR_ints(0);

  C_DGEMV('n',noccA_*nvirA_,ndf_,2.0,&(B_p_AR[0][0]),ndf_+3,diagBB_,1,1.0,
    &(wBAR_[0][0]),1);

  free_block(B_p_AR);

  wABS_ = block_matrix(noccB_,nvirB_);

  for(int b=0; b<noccB_; b++){
    C_DAXPY(nvirB_,1.0,&(vABB_[b][noccB_]),1,&(wABS_[b][0]),1);
  }

  double **B_p_BS = get_BS_ints(0);

  C_DGEMV('n',noccB_*nvirB_,ndf_,2.0,&(B_p_BS[0][0]),ndf_+3,diagAA_,1,1.0,
    &(wABS_[0][0]),1);

  free_block(B_p_BS);

  wBAA_ = block_matrix(noccA_,noccA_);

  for(int a=0; a<noccA_; a++){
    C_DAXPY(noccA_,1.0,&(vBAA_[a][0]),1,&(wBAA_[a][0]),1);
  }

  double **B_p_AA = get_AA_ints(0);

  C_DGEMV('n',noccA_*noccA_,ndf_,2.0,&(B_p_AA[0][0]),ndf_+3,diagBB_,1,1.0,
    &(wBAA_[0][0]),1);

  free_block(B_p_AA);

  wABB_ = block_matrix(noccB_,noccB_);

  for(int b=0; b<noccB_; b++){
    C_DAXPY(noccB_,1.0,&(vABB_[b][0]),1,&(wABB_[b][0]),1);
  }

  double **B_p_BB = get_BB_ints(0);

  C_DGEMV('n',noccB_*noccB_,ndf_,2.0,&(B_p_BB[0][0]),ndf_+3,diagAA_,1,1.0,
    &(wABB_[0][0]),1);

  free_block(B_p_BB);

  wBRR_ = block_matrix(nvirA_,nvirA_);

  for(int r=0; r<nvirA_; r++){
    C_DAXPY(nvirA_,1.0,&(vBAA_[r+noccA_][noccA_]),1,&(wBRR_[r][0]),1);
  }

  double **B_p_RR = get_RR_ints(0);

  C_DGEMV('n',nvirA_*nvirA_,ndf_,2.0,&(B_p_RR[0][0]),ndf_+3,diagBB_,1,1.0,
    &(wBRR_[0][0]),1);

  free_block(B_p_RR);

  wASS_ = block_matrix(nvirB_,nvirB_);

  for(int s=0; s<nvirB_; s++){
    C_DAXPY(nvirB_,1.0,&(vABB_[s+noccB_][noccB_]),1,&(wASS_[s][0]),1);
  }

  double **B_p_SS = get_SS_ints(0);

  C_DGEMV('n',nvirB_*nvirB_,ndf_,2.0,&(B_p_SS[0][0]),ndf_+3,diagAA_,1,1.0,
    &(wASS_[0][0]),1);

  free_block(B_p_SS);
}

}}