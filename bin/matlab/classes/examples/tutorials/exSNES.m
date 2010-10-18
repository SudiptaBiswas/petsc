%%
% Solves a linear system where the user manages the mesh--solver interactions
%
%   Set the Matlab path and initialize PETSc
path(path,'../../')
PetscInitialize({'-snes_monitor','-snes_mf_operator','-ksp_monitor'});
%%
%   Open a viewer to display PETSc objects
viewer = PetscViewer();
viewer.SetType('ascii');
%%
%   Create work vector for nonlinear solver and location for solution
b = Vec();
b.SetType('seq');
b.SetSizes(10,10);
x = b.Duplicate();
%%
%  Create a matrix for the Jacobian for Newton's method
mat = Mat();
mat.SetType('seqaij');
mat.SetSizes(10,10,10,10);
%%
%  Create the nonlinear solver 
snes = SNES();
snes.SetType('ls');
%%
%  Provide a function 
snes.SetFunction(b,'nlfunction',0);
type nlfunction.m
%%
%  Provide a function that evaluates the Jacobian
snes.SetJacobian(mat,mat,'nljacobian',0);
type nljacobian.m
%%
%  Solve the nonlinear system
snes.SetFromOptions();
snes.Solve(x);
x.View(viewer);
snes.View(viewer);
%%
%   Free PETSc objects and Shutdown PETSc
%
b.Destroy();
x.Destroy();
mat.Destroy();
snes.Destroy();
PetscFinalize();
