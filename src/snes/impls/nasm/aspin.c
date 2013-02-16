#include <petsc-private/snesimpl.h>             /*I   "petscsnes.h"   I*/
#include <petscdm.h>

#undef __FUNCT__
#define __FUNCT__ "MatMultASPIN"
PetscErrorCode MatMultASPIN(Mat m,Vec X,Vec Y)
{
  PetscErrorCode ierr;
  void           *ctx;
  SNES           snes;
  PetscInt       n,i;
  VecScatter     *oscatter;
  SNES           *subsnes;
  PetscBool      match;
  MPI_Comm       comm;
  KSP            ksp;
  Vec            *x,*b;
  SNES           npc;

  PetscFunctionBegin;
  ierr = MatShellGetContext(m,&ctx);CHKERRQ(ierr);
  snes = (SNES)ctx;
  ierr = SNESGetPC(snes,&npc);CHKERRQ(ierr);
  ierr = PetscObjectTypeCompare((PetscObject)npc,SNESNASM,&match);CHKERRQ(ierr);
  if (!match) {
    ierr = PetscObjectGetComm((PetscObject)snes,&comm);
    SETERRQ(comm,PETSC_ERR_ARG_WRONGSTATE,"MatMultASPIN requires that the nonlinear preconditioner be Nonlinear additive Schwarz");
  }
  ierr = SNESNASMGetSubdomains(npc,&n,&subsnes,NULL,&oscatter,NULL);CHKERRQ(ierr);
  ierr = SNESNASMGetSubdomainVecs(npc,&n,&x,&b,NULL,NULL);CHKERRQ(ierr);
  ierr = MatMult(npc->jacobian_pre,X,Y);CHKERRQ(ierr);
  for (i=0;i<n;i++) {
    ierr = VecScatterBegin(oscatter[i],Y,b[i],INSERT_VALUES,SCATTER_FORWARD);CHKERRQ(ierr);
    ierr = VecScatterEnd(oscatter[i],Y,b[i],INSERT_VALUES,SCATTER_FORWARD);CHKERRQ(ierr);
  }
  ierr = MPI_Barrier(PetscObjectComm((PetscObject)snes));CHKERRQ(ierr);
  for (i=0;i<n;i++) {
    ierr = VecSet(x[i],0);CHKERRQ(ierr);
    ierr = SNESGetKSP(subsnes[i],&ksp);CHKERRQ(ierr);
    ierr = KSPSolve(ksp,b[i],x[i]);CHKERRQ(ierr);
  }
  ierr = VecSet(Y,0);CHKERRQ(ierr);
  ierr = MPI_Barrier(PetscObjectComm((PetscObject)snes));CHKERRQ(ierr);
  for (i=0;i<n;i++) {
    ierr = VecScatterBegin(oscatter[i],x[i],Y,ADD_VALUES,SCATTER_REVERSE);CHKERRQ(ierr);
    ierr = VecScatterEnd(oscatter[i],x[i],Y,ADD_VALUES,SCATTER_REVERSE);CHKERRQ(ierr);
  }

  ierr = MPI_Barrier(PetscObjectComm((PetscObject)snes));CHKERRQ(ierr);

  PetscFunctionReturn(0);
}

EXTERN_C_BEGIN
#undef __FUNCT__
#define __FUNCT__ "SNESCreate_ASPIN"
PetscErrorCode SNESCreate_ASPIN(SNES snes)
{
  PetscErrorCode ierr;
  SNES           npc;
  KSP            ksp;
  PC             pc;
  Mat            aspinmat;
  MPI_Comm       comm;
  Vec            F;
  PetscInt       n;

  PetscFunctionBegin;
  /* set up the solver */
  ierr = SNESSetType(snes,SNESNEWTONLS);CHKERRQ(ierr);
  ierr = SNESSetPCSide(snes,PC_LEFT);CHKERRQ(ierr);
  ierr = SNESGetPC(snes,&npc);CHKERRQ(ierr);
  ierr = SNESSetType(npc,SNESNASM);CHKERRQ(ierr);
  ierr = SNESNASMSetComputeFinalJacobian(npc,PETSC_TRUE);CHKERRQ(ierr);
  ierr = SNESGetKSP(snes,&ksp);CHKERRQ(ierr);
  ierr = KSPGetPC(ksp,&pc);CHKERRQ(ierr);
  ierr = PCSetType(pc,PCNONE);CHKERRQ(ierr);
  ierr = PetscObjectGetComm((PetscObject)snes,&comm);

  /* set up the shell matrix */
  ierr = SNESGetFunction(snes,&F,NULL,NULL);CHKERRQ(ierr);
  ierr = VecGetLocalSize(F,&n);CHKERRQ(ierr);
  ierr = MatCreateShell(comm,n,n,PETSC_DECIDE,PETSC_DECIDE,snes,&aspinmat);CHKERRQ(ierr);
  ierr = MatSetType(aspinmat,MATSHELL);CHKERRQ(ierr);
  ierr = MatShellSetOperation(aspinmat,MATOP_MULT,(void(*)(void))MatMultASPIN);CHKERRQ(ierr);

  ierr = SNESSetJacobian(snes,aspinmat,NULL,NULL,NULL);CHKERRQ(ierr);
  ierr = MatDestroy(&aspinmat);CHKERRQ(ierr);

  PetscFunctionReturn(0);
}
EXTERN_C_END
