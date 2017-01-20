
#include <petsc/private/vecimpl.h> /*I "petscvec.h" I*/
#include "../src/vec/vec/utils/tagger/impls/simple.h"

typedef struct {
  VecTagger_Simple smpl;
  PetscReal atol;
  PetscReal rtol;
  PetscInt  maxit;
  PetscInt  numMoments;
  PetscBool gather;
} VecTagger_Cumulative;

static PetscErrorCode VecTaggerComputeIntervals_Cumulative(VecTagger tagger,Vec vec,PetscInt *numIntervals,PetscScalar (**intervals)[2])
{
  VecTagger_Simple *smpl = (VecTagger_Simple *)tagger->data;
  PetscInt          bs, i, j, k, n;
  PetscScalar       (*ints) [2];
  const PetscScalar *vArray;
  PetscErrorCode    ierr;
  
  PetscFunctionBegin;
  ierr = VecTaggerGetBlockSize(tagger,&bs);CHKERRQ(ierr);
  *numIntervals = 1;
  ierr = PetscMalloc1(bs,&ints);CHKERRQ(ierr);
  ierr = VecGetLocalSize(vec,&n);CHKERRQ(ierr);
  n /= bs;
  for (i = 0; i < bs; i++) {
#if !defined(PETSC_USE_COMPLEX)
    ints[i][0] = PETSC_MAX_REAL;
    ints[i][1] = PETSC_MIN_REAL;
#else
    ints[i][0] = PETSC_MAX_REAL + PETSC_i * PETSC_MAX_REAL;
    ints[i][1] = PETSC_MIN_REAL + PETSC_i * PETSC_MIN_REAL;
#endif
  }
  ierr = VecGetArrayRead(vec, &vArray);CHKERRQ(ierr);
  for (i = 0, k = 0; i < n; i++) {
    for (j = 0; j < bs; j++, k++) {
#if !defined(PETSC_USE_COMPLEX)
      ints[j][0] = PetscMin(ints[j][0],vArray[k]);
      ints[j][1] = PetscMax(ints[j][1],vArray[k]);
#else
      ints[j][0] = PetscMin(PetscRealPart(ints[j][0]),PetscRealPart(vArray[k])) + PETSC_i *
                   PetscMin(PetscImaginaryPart(ints[j][0]),PetscImaginaryPart(vArray[k]))
      ints[j][1] = PetscMax(PetscRealPart(ints[j][1]),PetscRealPart(vArray[k])) + PETSC_i *
                   PetscMax(PetscImaginaryPart(ints[j][1]),PetscImaginaryPart(vArray[k]))
#endif
    }
  }
  for (i = 0; i < bs; i++) ints[i][1] = -ints[i][1];
  ierr = VecRestoreArrayRead(vec, &vArray);CHKERRQ(ierr);
  ierr = MPI_Allreduce(MPI_IN_PLACE,ints,2*bs,MPIU_SCALAR,MPIU_MIN,PetscObjectComm((PetscObject)tagger));CHKERRQ(ierr);
  for (i = 0; i < bs; i++) {
    PetscScalar mins = ints[i][0];
    PetscScalar difs = -ints[i][1] - mins;
    ints[i][0] = mins + smpl->interval[i][0] * difs;
    ints[i][1] = mins + smpl->interval[i][1] * difs;
  }
  *intervals = ints;
  PetscFunctionReturn(0);
}

/*@C
  VecTaggerCumulativeSetInterval - Set the cumulative interval (multi-dimensional box) defining the values to be tagged by the tagger, where cumulative intervals are subsets of [0,1], where 0 indicates the smallest value present in the vector and 1 indicates the largest.

  Logically Collective

  Input Arguments:
+ tagger - the VecTagger context
- interval - the interval: a blocksize list of [min,max] pairs

  Level: advanced

.seealso: VecTaggerCumulativeGetInterval()
@*/
PetscErrorCode VecTaggerCumulativeSetInterval(VecTagger tagger,PetscScalar (*interval)[2])
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = VecTaggerSetInterval_Simple(tagger,interval);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*@C
  VecTaggerCumulativeGetInterval - Get the cumulative interval (multi-dimensional box) defining the values to be tagged by the tagger, where cumulative intervals are subsets of [0,1], where 0 indicates the smallest value present in the vector and 1 indicates the largest.

  Logically Collective

  Input Arguments:
. tagger - the VecTagger context

  Output Arguments:
. interval - the interval: a blocksize list of [min,max] pairs

  Level: advanced

.seealso: VecTaggerCumulativeSetInterval()
@*/
PetscErrorCode VecTaggerCumulativeGetInterval(VecTagger tagger,const PetscScalar (**interval)[2])
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = VecTaggerGetInterval_Simple(tagger,interval);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

PetscErrorCode VecTaggerCreate_Cumulative(VecTagger tagger)
{
  VecTagger_Cumulative *cuml;
  PetscErrorCode       ierr;

  PetscFunctionBegin;
  ierr = VecTaggerCreate_Simple(tagger);CHKERRQ(ierr);
  ierr = PetscNewLog(tagger,&cuml);CHKERRQ(ierr);
  ierr = PetscMemcpy(&(cuml->simpl),tagger->data,sizeof(VecTagger_Simple));CHKERRQ(ierr);
  ierr = PetscFree(tagger->data);CHKERRQ(ierr);
  tagger->data = cuml;
  tagger->ops->view             = VecTaggerView_Cumulative;
  tagger->ops->setfromoptions   = VecTaggerSetFromOptions_Cumulative;
  tagger->ops->computeintervals = VecTaggerComputeIntervals_Cumulative;
  PetscFunctionReturn(0);
}
