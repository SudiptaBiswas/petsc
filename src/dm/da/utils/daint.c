
#define PETSCDM_DLL
 
#include "../src/dm/da/daimpl.h" /*I      "petscda.h"     I*/
#include "petscmat.h"         /*I      "petscmat.h"    I*/


#undef __FUNCT__  
#define __FUNCT__ "DAGetWireBasketInterpolation"
/*
      DAGetWireBasketInterpolation - Gets the interpolation for a wirebasket based coarse space

*/
PetscErrorCode DAGetWireBasketInterpolation(DA da,Mat Aglobal,Mat *P)
{
  PetscErrorCode         ierr;
  PetscInt               dim,i,j,k,m,n,p,dof,Nint,Nface,Nwire,Nsurf,*Iint,*Isurf,cint = 0,csurf = 0,istart,jstart,kstart,*II,N,c = 0;
  PetscInt               mwidth,nwidth,pwidth,cnt,mp,np,pp,Ntotal,gl[26],*globals,Ng,*IIint,*IIsurf;
  Mat                    Xint, Xsurf,Xint_tmp;
  IS                     isint,issurf,is,row,col;
  ISLocalToGlobalMapping ltg;
  MPI_Comm               comm;
  Mat                    A,Aii,Ais,Asi,*Aholder,iAii;
  MatFactorInfo          info;
  PetscScalar            *xsurf,*xint;
#if defined(PETSC_USE_DEBUG)
  PetscScalar            tmp;
#endif
  PetscTable             ht;

  PetscFunctionBegin;
  ierr = DAGetInfo(da,&dim,0,0,0,&mp,&np,&pp,&dof,0,0,0);CHKERRQ(ierr);
  if (dof != 1) SETERRQ(PETSC_ERR_SUP,"Only for single field problems");
  if (dim != 3) SETERRQ(PETSC_ERR_SUP,"Only coded for 3d problems");
  ierr = DAGetCorners(da,0,0,0,&m,&n,&p);CHKERRQ(ierr);
  ierr = DAGetGhostCorners(da,&istart,&jstart,&kstart,&mwidth,&nwidth,&pwidth);CHKERRQ(ierr);
  istart = istart ? -1 : 0;
  jstart = jstart ? -1 : 0;
  kstart = kstart ? -1 : 0;

  /* 
    the columns of P are the interpolation of each coarse grid point (one for each vertex and edge) 
    to all the local degrees of freedom (this includes the vertices, edges and faces).

    Xint are the subset of the interpolation into the interior

    Xface are the interpolation onto faces but not into the interior 

    Xsurf are the interpolation onto the vertices and edges (the surfbasket) 
                                        Xint
    Symbolically one could write P = (  Xface  ) after interchanging the rows to match the natural ordering on the domain
                                        Xsurf
  */
  N     = (m - istart)*(n - jstart)*(p - kstart);
  Nint  = (m-2-istart)*(n-2-jstart)*(p-2-kstart);
  Nface = 2*( (m-2-istart)*(n-2-jstart) + (m-2-istart)*(p-2-kstart) + (n-2-jstart)*(p-2-kstart) ); 
  Nwire = 4*( (m-2-istart) + (n-2-jstart) + (p-2-kstart) ) + 8;
  Nsurf = Nface + Nwire;
  ierr = MatCreateSeqDense(MPI_COMM_SELF,Nint,26,PETSC_NULL,&Xint);CHKERRQ(ierr);
  ierr = MatCreateSeqDense(MPI_COMM_SELF,Nsurf,26,PETSC_NULL,&Xsurf);CHKERRQ(ierr);
  ierr = MatGetArray(Xsurf,&xsurf);CHKERRQ(ierr);

  /*
     Require that all 12 edges and 6 faces have at least one grid point. Otherwise some of the columns of 
     Xsurf will be all zero (thus making the coarse matrix singular). 
  */
  if (m-istart < 3) SETERRQ(PETSC_ERR_SUP,"Number of grid points per process in X direction must be at least 3");
  if (n-jstart < 3) SETERRQ(PETSC_ERR_SUP,"Number of grid points per process in Y direction must be at least 3");
  if (p-kstart < 3) SETERRQ(PETSC_ERR_SUP,"Number of grid points per process in Z direction must be at least 3");

  cnt = 0;
  xsurf[cnt++] = 1; for (i=1; i<m-istart-1; i++) { xsurf[cnt++ + Nsurf] = 1;} xsurf[cnt++ + 2*Nsurf] = 1;
  for (j=1;j<n-1-jstart;j++) { xsurf[cnt++ + 3*Nsurf] = 1; for (i=1; i<m-istart-1; i++) { xsurf[cnt++ + 4*Nsurf] = 1;} xsurf[cnt++ + 5*Nsurf] = 1;}
  xsurf[cnt++ + 6*Nsurf] = 1; for (i=1; i<m-istart-1; i++) { xsurf[cnt++ + 7*Nsurf] = 1;} xsurf[cnt++ + 8*Nsurf] = 1;
  for (k=1;k<p-1-kstart;k++) {
    xsurf[cnt++ + 9*Nsurf] = 1;  for (i=1; i<m-istart-1; i++) { xsurf[cnt++ + 10*Nsurf] = 1;}  xsurf[cnt++ + 11*Nsurf] = 1;
    for (j=1;j<n-1-jstart;j++) { xsurf[cnt++ + 12*Nsurf] = 1; /* these are the interior nodes */ xsurf[cnt++ + 13*Nsurf] = 1;}
    xsurf[cnt++ + 14*Nsurf] = 1;  for (i=1; i<m-istart-1; i++) { xsurf[cnt++ + 15*Nsurf] = 1;} xsurf[cnt++ + 16*Nsurf] = 1;
  }
  xsurf[cnt++ + 17*Nsurf] = 1; for (i=1; i<m-istart-1; i++) { xsurf[cnt++ + 18*Nsurf] = 1;} xsurf[cnt++ + 19*Nsurf] = 1;
  for (j=1;j<n-1-jstart;j++) { xsurf[cnt++ + 20*Nsurf] = 1;  for (i=1; i<m-istart-1; i++) { xsurf[cnt++ + 21*Nsurf] = 1;} xsurf[cnt++ + 22*Nsurf] = 1;}
  xsurf[cnt++ + 23*Nsurf] = 1; for (i=1; i<m-istart-1; i++) { xsurf[cnt++ + 24*Nsurf] = 1;} xsurf[cnt++ + 25*Nsurf] = 1;

#if defined(PETSC_USE_DEBUG)
  for (i=0; i<Nsurf; i++) {
    tmp = 0.0;
    for (j=0; j<26; j++) {
      tmp += xsurf[i+j*Nsurf];
    }
    if (PetscAbsScalar(tmp-1.0) > 1.e-10) SETERRQ2(PETSC_ERR_PLIB,"Wrong Xsurf interpolation at i %D value %G",i,PetscAbsScalar(tmp));
  }
#endif
  ierr = MatRestoreArray(Xsurf,&xsurf);CHKERRQ(ierr);
  /* ierr = MatView(Xsurf,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);*/


  /* 
       I are the indices for all the needed vertices (in global numbering)
       Iint are the indices for the interior values, I surf for the surface values
            (This is just for the part of the global matrix obtained with MatGetSubMatrix(), it 
             is NOT the local DA ordering.)
       IIint and IIsurf are the same as the Iint, Isurf except they are in the global numbering
  */
#define Endpoint(a,start,b) (a == 0 || a == (b-1-start))
  ierr = PetscMalloc3(N,PetscInt,&II,Nint,PetscInt,&Iint,Nsurf,PetscInt,&Isurf);CHKERRQ(ierr);
  ierr = PetscMalloc2(Nint,PetscInt,&IIint,Nsurf,PetscInt,&IIsurf);CHKERRQ(ierr);
  for (k=0; k<p-kstart; k++) {
    for (j=0; j<n-jstart; j++) {
      for (i=0; i<m-istart; i++) {
        II[c++] = i + j*mwidth + k*mwidth*nwidth; 

        if (!Endpoint(i,istart,m) && !Endpoint(j,jstart,n) && !Endpoint(k,kstart,p)) {
          IIint[cint]  = i + j*mwidth + k*mwidth*nwidth; 
          Iint[cint++] = i + j*(m-istart) + k*(m-istart)*(n-jstart);
        } else {
          IIsurf[csurf]  = i + j*mwidth + k*mwidth*nwidth; 
          Isurf[csurf++] = i + j*(m-istart) + k*(m-istart)*(n-jstart);
        } 
      }
    }
  }
  if (c != N) SETERRQ(PETSC_ERR_PLIB,"c != N");
  if (cint != Nint) SETERRQ(PETSC_ERR_PLIB,"cint != Nint");
  if (csurf != Nsurf) SETERRQ(PETSC_ERR_PLIB,"csurf != Nsurf");
  ierr = DAGetISLocalToGlobalMapping(da,&ltg);CHKERRQ(ierr);
  ierr = ISLocalToGlobalMappingApply(ltg,N,II,II);CHKERRQ(ierr);
  ierr = ISLocalToGlobalMappingApply(ltg,Nint,IIint,IIint);CHKERRQ(ierr);
  ierr = ISLocalToGlobalMappingApply(ltg,Nsurf,IIsurf,IIsurf);CHKERRQ(ierr);
  ierr = PetscObjectGetComm((PetscObject)da,&comm);CHKERRQ(ierr);
  ierr = ISCreateGeneral(comm,N,II,&is);CHKERRQ(ierr);
  ierr = ISCreateGeneral(PETSC_COMM_SELF,Nint,Iint,&isint);CHKERRQ(ierr);
  ierr = ISCreateGeneral(PETSC_COMM_SELF,Nsurf,Isurf,&issurf);CHKERRQ(ierr);
  ierr = PetscFree3(II,Iint,Isurf);CHKERRQ(ierr);

  ierr = MatGetSubMatrices(Aglobal,1,&is,&is,MAT_INITIAL_MATRIX,&Aholder);CHKERRQ(ierr);
  A    = *Aholder;
  ierr = PetscFree(Aholder);CHKERRQ(ierr);

  ierr = MatGetSubMatrix(A,isint,isint,PETSC_DECIDE,MAT_INITIAL_MATRIX,&Aii);CHKERRQ(ierr);
  ierr = MatGetSubMatrix(A,isint,issurf,PETSC_DECIDE,MAT_INITIAL_MATRIX,&Ais);CHKERRQ(ierr);
  ierr = MatGetSubMatrix(A,issurf,isint,PETSC_DECIDE,MAT_INITIAL_MATRIX,&Asi);CHKERRQ(ierr);

  /* 
     Solve for the interpolation onto the interior Xint
  */
  ierr = MatGetFactor(Aii,MAT_SOLVER_PETSC,MAT_FACTOR_LU,&iAii);CHKERRQ(ierr);
  ierr = MatFactorInfoInitialize(&info);CHKERRQ(ierr);
  ierr = MatGetOrdering(Aii,MATORDERING_ND,&row,&col);CHKERRQ(ierr);
  ierr = MatLUFactorSymbolic(iAii,Aii,row,col,&info);CHKERRQ(ierr);
  ierr = ISDestroy(row);CHKERRQ(ierr);
  ierr = ISDestroy(col);CHKERRQ(ierr);
  ierr = MatLUFactorNumeric(iAii,Aii,&info);CHKERRQ(ierr);
  ierr = MatDuplicate(Xint,MAT_DO_NOT_COPY_VALUES,&Xint_tmp);CHKERRQ(ierr);
  ierr = MatMatMult(Ais,Xsurf,MAT_REUSE_MATRIX,PETSC_DETERMINE,&Xint_tmp);CHKERRQ(ierr);
  ierr = MatScale(Xint_tmp,-1.0);CHKERRQ(ierr);
  ierr = MatMatSolve(iAii,Xint_tmp,Xint);CHKERRQ(ierr);
  ierr = MatDestroy(Xint_tmp);CHKERRQ(ierr);
  ierr = MatDestroy(iAii);CHKERRQ(ierr);

#if defined(PETSC_USE_DEBUG)
  ierr = MatGetArray(Xint,&xint);CHKERRQ(ierr);
  for (i=0; i<Nint; i++) {
    tmp = 0.0;
    for (j=0; j<26; j++) {
      tmp += xint[i+j*Nint];
    }
    if (PetscAbsScalar(tmp-1.0) > 1.e-10) SETERRQ2(PETSC_ERR_PLIB,"Wrong Xint interpolation at i %D value %G",i,PetscAbsScalar(tmp));
  }
  ierr = MatRestoreArray(Xint,&xint);CHKERRQ(ierr);
  /* ierr =MatView(Xint,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr); */
#endif


  /*         total vertices             total faces                                  total edges */
  Ntotal = (mp + 1)*(np + 1)*(pp + 1) + mp*np*(pp+1) + mp*pp*(np+1) + np*pp*(mp+1) + mp*(np+1)*(pp+1) + np*(mp+1)*(pp+1) +  pp*(mp+1)*(np+1);

  /*
      For each vertex, edge, face on process (in the same orderings as used above) determine its local number including ghost points 
  */
  cnt = 0;
  gl[cnt++] = 0;  { gl[cnt++] = 1;} gl[cnt++] = m-istart-1;
  { gl[cnt++] = mwidth;  { gl[cnt++] = mwidth+1;} gl[cnt++] = mwidth + m-istart-1;}
  gl[cnt++] = mwidth*(n-jstart-1);  { gl[cnt++] = mwidth*(n-jstart-1)+1;} gl[cnt++] = mwidth*(n-jstart-1) + m-istart-1;
  {
    gl[cnt++] = mwidth*nwidth;  { gl[cnt++] = mwidth*nwidth+1;}  gl[cnt++] = mwidth*nwidth+ m-istart-1;
    { gl[cnt++] = mwidth*nwidth + mwidth; /* these are the interior nodes */ gl[cnt++] = mwidth*nwidth + mwidth+m-istart-1;}
    gl[cnt++] = mwidth*nwidth+ mwidth*(n-jstart-1);   { gl[cnt++] = mwidth*nwidth+mwidth*(n-jstart-1)+1;} gl[cnt++] = mwidth*nwidth+mwidth*(n-jstart-1) + m-istart-1;
  }
  gl[cnt++] = mwidth*nwidth*(p-kstart-1); { gl[cnt++] = mwidth*nwidth*(p-kstart-1)+1;} gl[cnt++] = mwidth*nwidth*(p-kstart-1) +  m-istart-1;
  { gl[cnt++] = mwidth*nwidth*(p-kstart-1) + mwidth;   { gl[cnt++] = mwidth*nwidth*(p-kstart-1) + mwidth+1;} gl[cnt++] = mwidth*nwidth*(p-kstart-1)+mwidth+m-istart-1;}
  gl[cnt++] = mwidth*nwidth*(p-kstart-1) +  mwidth*(n-jstart-1);  { gl[cnt++] = mwidth*nwidth*(p-kstart-1)+ mwidth*(n-jstart-1)+1;} gl[cnt++] = mwidth*nwidth*(p-kstart-1) + mwidth*(n-jstart-1) + m-istart-1;

  /* PetscIntView(26,gl,PETSC_VIEWER_STDOUT_WORLD); */
  /* convert that to global numbering and get them on all processes */
  ierr = ISLocalToGlobalMappingApply(ltg,26,gl,gl);CHKERRQ(ierr);
  /* PetscIntView(26,gl,PETSC_VIEWER_STDOUT_WORLD); */
  ierr = PetscMalloc(26*mp*np*pp*sizeof(PetscInt),&globals);CHKERRQ(ierr);
  ierr = MPI_Allgather(gl,26,MPIU_INT,globals,26,MPIU_INT,((PetscObject)da)->comm);CHKERRQ(ierr);

  /* Number the coarse grid points from 0 to Ntotal */
  ierr = PetscTableCreate(Ntotal/3,&ht);CHKERRQ(ierr); 
  for (i=0; i<26*mp*np*pp; i++){
    ierr = PetscTableAddCount(ht,globals[i]+1);CHKERRQ(ierr);
  }
  ierr = PetscTableGetCount(ht,&cnt);CHKERRQ(ierr);
  if (cnt != Ntotal) SETERRQ2(PETSC_ERR_PLIB,"Hash table size %D not equal to total number coarse grid points %D",cnt,Ntotal);
  ierr = PetscFree(globals);CHKERRQ(ierr);
  for (i=0; i<26; i++) {
    ierr = PetscTableFind(ht,gl[i]+1,&gl[i]);CHKERRQ(ierr);
    gl[i]--;
  }
  ierr = PetscTableDestroy(ht);CHKERRQ(ierr);
  /* PetscIntView(26,gl,PETSC_VIEWER_STDOUT_WORLD); */

  /* construct global interpolation matrix */
  ierr = MatGetLocalSize(Aglobal,&Ng,PETSC_NULL);CHKERRQ(ierr);
  ierr = MatCreateMPIAIJ(((PetscObject)da)->comm,Ng,PETSC_DECIDE,PETSC_DECIDE,Ntotal,Nint+Nsurf,PETSC_NULL,Nint+Nsurf,PETSC_NULL,P);CHKERRQ(ierr);
  ierr = MatSetOption(*P,MAT_ROW_ORIENTED,PETSC_FALSE);CHKERRQ(ierr);
  ierr = MatGetArray(Xint,&xint);CHKERRQ(ierr);
  ierr = MatSetValues(*P,Nint,IIint,26,gl,xint,INSERT_VALUES);CHKERRQ(ierr);
  ierr = MatRestoreArray(Xint,&xint);CHKERRQ(ierr);
  ierr = MatGetArray(Xsurf,&xsurf);CHKERRQ(ierr);
  ierr = MatSetValues(*P,Nsurf,IIsurf,26,gl,xsurf,INSERT_VALUES);CHKERRQ(ierr);
  ierr = MatRestoreArray(Xsurf,&xsurf);CHKERRQ(ierr);
  ierr = MatAssemblyBegin(*P,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(*P,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = PetscFree2(IIint,IIsurf);CHKERRQ(ierr);

#if defined(PETSC_USE_DEBUG)
  {
    Vec         x,y;
    PetscScalar *yy;
    ierr = VecCreateMPI(((PetscObject)da)->comm,Ng,PETSC_DETERMINE,&y);CHKERRQ(ierr);
    ierr = VecCreateMPI(((PetscObject)da)->comm,PETSC_DETERMINE,Ntotal,&x);CHKERRQ(ierr);
    ierr = VecSet(x,1.0);CHKERRQ(ierr);
    ierr = MatMult(*P,x,y);CHKERRQ(ierr);
    ierr = VecGetArray(y,&yy);CHKERRQ(ierr);
    for (i=0; i<Ng; i++) {
      if (PetscAbsScalar(yy[i]-1.0) > 1.e-10) SETERRQ2(PETSC_ERR_PLIB,"Wrong p interpolation at i %D value %G",i,PetscAbsScalar(yy[i]));
    }
    ierr = VecRestoreArray(y,&yy);CHKERRQ(ierr);
    ierr = VecDestroy(x);CHKERRQ(ierr);
    ierr = VecDestroy(y);CHKERRQ(ierr);
  }
#endif
    
  ierr = MatDestroy(Aii);CHKERRQ(ierr);
  ierr = MatDestroy(Ais);CHKERRQ(ierr);
  ierr = MatDestroy(Asi);CHKERRQ(ierr);
  ierr = MatDestroy(A);CHKERRQ(ierr);
  ierr = ISDestroy(is);CHKERRQ(ierr);
  ierr = ISDestroy(isint);CHKERRQ(ierr);
  ierr = ISDestroy(issurf);CHKERRQ(ierr);
  ierr = MatDestroy(Xint);CHKERRQ(ierr);
  ierr = MatDestroy(Xsurf);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "DAGetFaceInterpolation"
/*
      DAGetFaceInterpolation - Gets the interpolation for a face based coarse space

*/
PetscErrorCode DAGetFaceInterpolation(DA da,Mat Aglobal,Mat *P)
{
  PetscErrorCode         ierr;
  PetscInt               dim,i,j,k,m,n,p,dof,Nint,Nface,Nwire,Nsurf,*Iint,*Isurf,cint = 0,csurf = 0,istart,jstart,kstart,*II,N,c = 0;
  PetscInt               mwidth,nwidth,pwidth,cnt,mp,np,pp,Ntotal,gl[6],*globals,Ng,*IIint,*IIsurf;
  Mat                    Xint, Xsurf,Xint_tmp;
  IS                     isint,issurf,is,row,col;
  ISLocalToGlobalMapping ltg;
  MPI_Comm               comm;
  Mat                    A,Aii,Ais,Asi,*Aholder,iAii;
  MatFactorInfo          info;
  PetscScalar            *xsurf,*xint;
#if defined(PETSC_USE_DEBUG)
  PetscScalar            tmp;
#endif
  PetscTable             ht;

  PetscFunctionBegin;
  ierr = DAGetInfo(da,&dim,0,0,0,&mp,&np,&pp,&dof,0,0,0);CHKERRQ(ierr);
  if (dof != 1) SETERRQ(PETSC_ERR_SUP,"Only for single field problems");
  if (dim != 3) SETERRQ(PETSC_ERR_SUP,"Only coded for 3d problems");
  ierr = DAGetCorners(da,0,0,0,&m,&n,&p);CHKERRQ(ierr);
  ierr = DAGetGhostCorners(da,&istart,&jstart,&kstart,&mwidth,&nwidth,&pwidth);CHKERRQ(ierr);
  istart = istart ? -1 : 0;
  jstart = jstart ? -1 : 0;
  kstart = kstart ? -1 : 0;

  /* 
    the columns of P are the interpolation of each coarse grid point (one for each vertex and edge) 
    to all the local degrees of freedom (this includes the vertices, edges and faces).

    Xint are the subset of the interpolation into the interior

    Xface are the interpolation onto faces but not into the interior 

    Xsurf are the interpolation onto the vertices and edges (the surfbasket) 
                                        Xint
    Symbolically one could write P = (  Xface  ) after interchanging the rows to match the natural ordering on the domain
                                        Xsurf
  */
  N     = (m - istart)*(n - jstart)*(p - kstart);
  Nint  = (m-2-istart)*(n-2-jstart)*(p-2-kstart);
  Nface = 2*( (m-2-istart)*(n-2-jstart) + (m-2-istart)*(p-2-kstart) + (n-2-jstart)*(p-2-kstart) ); 
  Nwire = 4*( (m-2-istart) + (n-2-jstart) + (p-2-kstart) ) + 8;
  Nsurf = Nface + Nwire;
  ierr = MatCreateSeqDense(MPI_COMM_SELF,Nint,6,PETSC_NULL,&Xint);CHKERRQ(ierr);
  ierr = MatCreateSeqDense(MPI_COMM_SELF,Nsurf,6,PETSC_NULL,&Xsurf);CHKERRQ(ierr);
  ierr = MatGetArray(Xsurf,&xsurf);CHKERRQ(ierr);

  /*
     Require that all 12 edges and 6 faces have at least one grid point. Otherwise some of the columns of 
     Xsurf will be all zero (thus making the coarse matrix singular). 
  */
  if (m-istart < 3) SETERRQ(PETSC_ERR_SUP,"Number of grid points per process in X direction must be at least 3");
  if (n-jstart < 3) SETERRQ(PETSC_ERR_SUP,"Number of grid points per process in Y direction must be at least 3");
  if (p-kstart < 3) SETERRQ(PETSC_ERR_SUP,"Number of grid points per process in Z direction must be at least 3");

  cnt = 0;
  for (j=1;j<n-1-jstart;j++) {  for (i=1; i<m-istart-1; i++) { xsurf[cnt++ + 0*Nsurf] = 1;} }
   for (k=1;k<p-1-kstart;k++) {
    for (i=1; i<m-istart-1; i++) { xsurf[cnt++ + 1*Nsurf] = 1;} 
    for (j=1;j<n-1-jstart;j++) { xsurf[cnt++ + 2*Nsurf] = 1; /* these are the interior nodes */ xsurf[cnt++ + 3*Nsurf] = 1;}
    for (i=1; i<m-istart-1; i++) { xsurf[cnt++ + 4*Nsurf] = 1;} 
  }
  for (j=1;j<n-1-jstart;j++) {for (i=1; i<m-istart-1; i++) { xsurf[cnt++ + 5*Nsurf] = 1;} }

#if defined(PETSC_USE_DEBUG_foo)
  for (i=0; i<Nsurf; i++) {
    tmp = 0.0;
    for (j=0; j<6; j++) {
      tmp += xsurf[i+j*Nsurf];
    }
    if (PetscAbsScalar(tmp-1.0) > 1.e-10) SETERRQ2(PETSC_ERR_PLIB,"Wrong Xsurf interpolation at i %D value %G",i,PetscAbsScalar(tmp));
  }
#endif
  ierr = MatRestoreArray(Xsurf,&xsurf);CHKERRQ(ierr);
  /* ierr = MatView(Xsurf,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);*/


  /* 
       I are the indices for all the needed vertices (in global numbering)
       Iint are the indices for the interior values, I surf for the surface values
            (This is just for the part of the global matrix obtained with MatGetSubMatrix(), it 
             is NOT the local DA ordering.)
       IIint and IIsurf are the same as the Iint, Isurf except they are in the global numbering
  */
#define Endpoint(a,start,b) (a == 0 || a == (b-1-start))
  ierr = PetscMalloc3(N,PetscInt,&II,Nint,PetscInt,&Iint,Nsurf,PetscInt,&Isurf);CHKERRQ(ierr);
  ierr = PetscMalloc2(Nint,PetscInt,&IIint,Nsurf,PetscInt,&IIsurf);CHKERRQ(ierr);
  for (k=0; k<p-kstart; k++) {
    for (j=0; j<n-jstart; j++) {
      for (i=0; i<m-istart; i++) {
        II[c++] = i + j*mwidth + k*mwidth*nwidth; 

        if (!Endpoint(i,istart,m) && !Endpoint(j,jstart,n) && !Endpoint(k,kstart,p)) {
          IIint[cint]  = i + j*mwidth + k*mwidth*nwidth; 
          Iint[cint++] = i + j*(m-istart) + k*(m-istart)*(n-jstart);
        } else {
          IIsurf[csurf]  = i + j*mwidth + k*mwidth*nwidth; 
          Isurf[csurf++] = i + j*(m-istart) + k*(m-istart)*(n-jstart);
        } 
      }
    }
  }
  if (c != N) SETERRQ(PETSC_ERR_PLIB,"c != N");
  if (cint != Nint) SETERRQ(PETSC_ERR_PLIB,"cint != Nint");
  if (csurf != Nsurf) SETERRQ(PETSC_ERR_PLIB,"csurf != Nsurf");
  ierr = DAGetISLocalToGlobalMapping(da,&ltg);CHKERRQ(ierr);
  ierr = ISLocalToGlobalMappingApply(ltg,N,II,II);CHKERRQ(ierr);
  ierr = ISLocalToGlobalMappingApply(ltg,Nint,IIint,IIint);CHKERRQ(ierr);
  ierr = ISLocalToGlobalMappingApply(ltg,Nsurf,IIsurf,IIsurf);CHKERRQ(ierr);
  ierr = PetscObjectGetComm((PetscObject)da,&comm);CHKERRQ(ierr);
  ierr = ISCreateGeneral(comm,N,II,&is);CHKERRQ(ierr);
  ierr = ISCreateGeneral(PETSC_COMM_SELF,Nint,Iint,&isint);CHKERRQ(ierr);
  ierr = ISCreateGeneral(PETSC_COMM_SELF,Nsurf,Isurf,&issurf);CHKERRQ(ierr);
  ierr = PetscFree3(II,Iint,Isurf);CHKERRQ(ierr);

  ierr = MatGetSubMatrices(Aglobal,1,&is,&is,MAT_INITIAL_MATRIX,&Aholder);CHKERRQ(ierr);
  A    = *Aholder;
  ierr = PetscFree(Aholder);CHKERRQ(ierr);

  ierr = MatGetSubMatrix(A,isint,isint,PETSC_DECIDE,MAT_INITIAL_MATRIX,&Aii);CHKERRQ(ierr);
  ierr = MatGetSubMatrix(A,isint,issurf,PETSC_DECIDE,MAT_INITIAL_MATRIX,&Ais);CHKERRQ(ierr);
  ierr = MatGetSubMatrix(A,issurf,isint,PETSC_DECIDE,MAT_INITIAL_MATRIX,&Asi);CHKERRQ(ierr);

  /* 
     Solve for the interpolation onto the interior Xint
  */
  ierr = MatGetFactor(Aii,MAT_SOLVER_PETSC,MAT_FACTOR_LU,&iAii);CHKERRQ(ierr);
  ierr = MatFactorInfoInitialize(&info);CHKERRQ(ierr);
  ierr = MatGetOrdering(Aii,MATORDERING_ND,&row,&col);CHKERRQ(ierr);
  ierr = MatLUFactorSymbolic(iAii,Aii,row,col,&info);CHKERRQ(ierr);
  ierr = ISDestroy(row);CHKERRQ(ierr);
  ierr = ISDestroy(col);CHKERRQ(ierr);
  ierr = MatLUFactorNumeric(iAii,Aii,&info);CHKERRQ(ierr);
  ierr = MatDuplicate(Xint,MAT_DO_NOT_COPY_VALUES,&Xint_tmp);CHKERRQ(ierr);
  ierr = MatMatMult(Ais,Xsurf,MAT_REUSE_MATRIX,PETSC_DETERMINE,&Xint_tmp);CHKERRQ(ierr);
  ierr = MatScale(Xint_tmp,-1.0);CHKERRQ(ierr);
  ierr = MatMatSolve(iAii,Xint_tmp,Xint);CHKERRQ(ierr);
  ierr = MatDestroy(Xint_tmp);CHKERRQ(ierr);
  ierr = MatDestroy(iAii);CHKERRQ(ierr);

#if defined(PETSC_USE_DEBUG_foo)
  ierr = MatGetArray(Xint,&xint);CHKERRQ(ierr);
  for (i=0; i<Nint; i++) {
    tmp = 0.0;
    for (j=0; j<6; j++) {
      tmp += xint[i+j*Nint];
    }
    if (PetscAbsScalar(tmp-1.0) > 1.e-10) SETERRQ2(PETSC_ERR_PLIB,"Wrong Xint interpolation at i %D value %G",i,PetscAbsScalar(tmp));
  }
  ierr = MatRestoreArray(Xint,&xint);CHKERRQ(ierr);
  /* ierr =MatView(Xint,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr); */
#endif


  /*         total faces    */
  Ntotal =  mp*np*(pp+1) + mp*pp*(np+1) + np*pp*(mp+1);

  /*
      For each vertex, edge, face on process (in the same orderings as used above) determine its local number including ghost points 
  */
  cnt = 0;
  { gl[cnt++] = mwidth+1;}
  {
    { gl[cnt++] = mwidth*nwidth+1;}  
    { gl[cnt++] = mwidth*nwidth + mwidth; /* these are the interior nodes */ gl[cnt++] = mwidth*nwidth + mwidth+m-istart-1;}
    { gl[cnt++] = mwidth*nwidth+mwidth*(n-jstart-1)+1;}
  }
  { gl[cnt++] = mwidth*nwidth*(p-kstart-1) + mwidth+1;} 

  /* PetscIntView(6,gl,PETSC_VIEWER_STDOUT_WORLD); */
  /* convert that to global numbering and get them on all processes */
  ierr = ISLocalToGlobalMappingApply(ltg,6,gl,gl);CHKERRQ(ierr);
  /* PetscIntView(6,gl,PETSC_VIEWER_STDOUT_WORLD); */
  ierr = PetscMalloc(6*mp*np*pp*sizeof(PetscInt),&globals);CHKERRQ(ierr);
  ierr = MPI_Allgather(gl,6,MPIU_INT,globals,6,MPIU_INT,((PetscObject)da)->comm);CHKERRQ(ierr);

  /* Number the coarse grid points from 0 to Ntotal */
  ierr = PetscTableCreate(Ntotal/3,&ht);CHKERRQ(ierr); 
  for (i=0; i<6*mp*np*pp; i++){
    ierr = PetscTableAddCount(ht,globals[i]+1);CHKERRQ(ierr);
  }
  ierr = PetscTableGetCount(ht,&cnt);CHKERRQ(ierr);
  if (cnt != Ntotal) SETERRQ2(PETSC_ERR_PLIB,"Hash table size %D not equal to total number coarse grid points %D",cnt,Ntotal); 
  ierr = PetscFree(globals);CHKERRQ(ierr);
  for (i=0; i<6; i++) {
    ierr = PetscTableFind(ht,gl[i]+1,&gl[i]);CHKERRQ(ierr);
    gl[i]--;
  }
  ierr = PetscTableDestroy(ht);CHKERRQ(ierr);
  /* PetscIntView(6,gl,PETSC_VIEWER_STDOUT_WORLD); */

  /* construct global interpolation matrix */
  ierr = MatGetLocalSize(Aglobal,&Ng,PETSC_NULL);CHKERRQ(ierr);
  ierr = MatCreateMPIAIJ(((PetscObject)da)->comm,Ng,PETSC_DECIDE,PETSC_DECIDE,Ntotal,Nint+Nsurf,PETSC_NULL,Nint,PETSC_NULL,P);CHKERRQ(ierr);
  ierr = MatSetOption(*P,MAT_ROW_ORIENTED,PETSC_FALSE);CHKERRQ(ierr);
  ierr = MatGetArray(Xint,&xint);CHKERRQ(ierr);
  ierr = MatSetValues(*P,Nint,IIint,6,gl,xint,INSERT_VALUES);CHKERRQ(ierr);
  ierr = MatRestoreArray(Xint,&xint);CHKERRQ(ierr);
  ierr = MatGetArray(Xsurf,&xsurf);CHKERRQ(ierr);
  ierr = MatSetValues(*P,Nsurf,IIsurf,6,gl,xsurf,INSERT_VALUES);CHKERRQ(ierr);
  ierr = MatRestoreArray(Xsurf,&xsurf);CHKERRQ(ierr);
  ierr = MatAssemblyBegin(*P,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(*P,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = PetscFree2(IIint,IIsurf);CHKERRQ(ierr);

#if defined(PETSC_USE_DEBUG_foo)
  {
    Vec         x,y;
    PetscScalar *yy;
    ierr = VecCreateMPI(((PetscObject)da)->comm,Ng,PETSC_DETERMINE,&y);CHKERRQ(ierr);
    ierr = VecCreateMPI(((PetscObject)da)->comm,PETSC_DETERMINE,Ntotal,&x);CHKERRQ(ierr);
    ierr = VecSet(x,1.0);CHKERRQ(ierr);
    ierr = MatMult(*P,x,y);CHKERRQ(ierr);
    ierr = VecGetArray(y,&yy);CHKERRQ(ierr);
    for (i=0; i<Ng; i++) {
      if (PetscAbsScalar(yy[i]-1.0) > 1.e-10) SETERRQ2(PETSC_ERR_PLIB,"Wrong p interpolation at i %D value %G",i,PetscAbsScalar(yy[i]));
    }
    ierr = VecRestoreArray(y,&yy);CHKERRQ(ierr);
    ierr = VecDestroy(x);CHKERRQ(ierr);
    ierr = VecDestroy(y);CHKERRQ(ierr);
  }
#endif
    
  ierr = MatDestroy(Aii);CHKERRQ(ierr);
  ierr = MatDestroy(Ais);CHKERRQ(ierr);
  ierr = MatDestroy(Asi);CHKERRQ(ierr);
  ierr = MatDestroy(A);CHKERRQ(ierr);
  ierr = ISDestroy(is);CHKERRQ(ierr);
  ierr = ISDestroy(isint);CHKERRQ(ierr);
  ierr = ISDestroy(issurf);CHKERRQ(ierr);
  ierr = MatDestroy(Xint);CHKERRQ(ierr);
  ierr = MatDestroy(Xsurf);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

