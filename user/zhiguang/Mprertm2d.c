/* 2-D prestack reverse time migration and its adjoint */
#include <rsf.h>
#include <mpi.h>

#ifdef _OPENMP
#include <omp.h>
#endif

static bool verb, snap;
static int nz, nx, nt, nr, ns;
static int jt, padx, padz, padnx, padnz;
static int dr_v, ds_v, r0_v, s0_v, zr_v, zs_v;
static float c0, c11, c12, c21, c22;
static double idx2, idz2;
static float **padvv, *ww;
static sf_file snapshot;
static int rank, numprocs;

void laplacian(bool adj, float **u0, float **u1, float **u2)
{
    int ix, iz;
    
    if(adj){
#ifdef _OPENMP
#pragma omp parallel for  \
    private(ix, iz)       \
    shared(padnx, padnz, u0, u1, u2, padvv, c0, c11, c12, c21, c22)
#endif
        for(ix=2; ix<padnx-2; ix++){
            for(iz=2; iz<padnz-2; iz++){
                u2[ix][iz]=
                (c11*(u1[ix][iz-1]+u1[ix][iz+1])+
                 c12*(u1[ix][iz-2]+u1[ix][iz+2])+
                 c0*u1[ix][iz]+
                 c21*(u1[ix-1][iz]+u1[ix+1][iz])+
                 c22*(u1[ix-2][iz]+u1[ix+2][iz]))
                *padvv[ix][iz]+2*u1[ix][iz]-u0[ix][iz];
            }
        }
    }else{
#ifdef _OPENMP
#pragma omp parallel for  \
    private(ix, iz)       \
    shared(padnx, padnz, u0, u1, u2, padvv, c0, c11, c12, c21, c22)
#endif
        for(ix=2; ix<padnx-2; ix++){
            for(iz=2; iz<padnz-2; iz++){
                u2[ix][iz]=
                (c11*(u1[ix][iz-1]*padvv[ix][iz-1]+u1[ix][iz+1]*padvv[ix][iz+1])+
                 c12*(u1[ix][iz-2]*padvv[ix][iz-2]+u1[ix][iz+2]*padvv[ix][iz+2])+
                 c0*u1[ix][iz]*padvv[ix][iz]+
                 c21*(u1[ix-1][iz]*padvv[ix-1][iz]+u1[ix+1][iz]*padvv[ix+1][iz])+
                 c22*(u1[ix-2][iz]*padvv[ix-2][iz]+u1[ix+2][iz]*padvv[ix+2][iz]))
                +2*u1[ix][iz]-u0[ix][iz];
            }
        }
    }
}

void prertm2_oper(bool adj, float ***dd, float **mm)
{
    int ix, iz, is, it, ir, sx, rx;
    float **u0, **u1, **u2, **sou2, **permm, **temp, ***wave;
    
    if(adj)
        memset(mm[0], 0, nz*nx*sizeof(float));
    else
        memset(dd[0][0], 0, nt*nr*ns*sizeof(float));
    
    u0=sf_floatalloc2(padnz, padnx);
    u1=sf_floatalloc2(padnz, padnx);
    u2=sf_floatalloc2(padnz, padnx);
    sou2=sf_floatalloc2(nz, nx);
    permm=sf_floatalloc2(nz, nx);
    wave=sf_floatalloc3(nz, nx, nt);
    
    if(adj){/* migration */
        
        for(is=rank; is<ns; is+=numprocs){
            if(verb) sf_warning("ishot @@@ nshot:  %d  %d", is+1, ns);
            sx=is*ds_v+s0_v;
            
            memset(u0[0], 0, padnz*padnx*sizeof(float));
            memset(u1[0], 0, padnz*padnx*sizeof(float));
            memset(u2[0], 0, padnz*padnx*sizeof(float));
            memset(sou2[0], 0, nz*nx*sizeof(float));
            memset(permm[0], 0, nz*nx*sizeof(float));
            memset(wave[0][0], 0, nz*nx*nt*sizeof(float));
            for(it=0; it<nt; it++){
                //sf_warning("RTM_Source: it=%d;",it);
                laplacian(true, u0, u1, u2);
                
                temp=u0;
                u0=u1;
                u1=u2;
                u2=temp;
                
                u1[sx][zs_v]+=ww[it];
                for(ix=0; ix<nx; ix++){
                    for(iz=0; iz<nz; iz++){
                        sou2[ix][iz]+=u1[ix+padx][iz+padz]*u1[ix+padx][iz+padz];
                        wave[it][ix][iz]=u1[ix+padx][iz+padz];
                    }
                }
                if(it%jt==0 && is==ns/2 && snap)
                    sf_floatwrite(u1[0], padnx*padnz, snapshot);
            }// end of it
            
            memset(u0[0], 0, padnz*padnx*sizeof(float));
            memset(u1[0], 0, padnz*padnx*sizeof(float));
            memset(u2[0], 0, padnz*padnx*sizeof(float));
            for(it=nt-1; it>=0; it--){
                //sf_warning("RTM_Receiver: it=%d;",it);
                laplacian(false, u0, u1, u2);
                
                temp=u0;
                u0=u1;
                u1=u2;
                u2=temp;
                
                for(ir=0; ir<nr; ir++){
                    rx=ir*dr_v+r0_v;
                    u1[rx][zr_v]+=dd[is][ir][it];
                }
                for(ix=0; ix<nx; ix++){
                    for(iz=0; iz<nz; iz++){
                        permm[ix][iz]+=wave[it][ix][iz]*u1[ix+padx][iz+padz];
                    }
                }
            }// end of it
            for(ix=0; ix<nx; ix++){
                for(iz=0; iz<nz; iz++){
                    mm[ix][iz]+=permm[ix][iz]/(sou2[ix][iz]+1e-6);
                }
            }
        }// end of shot
    }else{/* modeling */
        
        for(is=0; is<ns; is++){
            if(verb) sf_warning("ishot @@@ nshot:  %d  %d", is+1, ns);
            sx=is*ds_v+s0_v;
            
            memset(u0[0], 0, padnz*padnx*sizeof(float));
            memset(u1[0], 0, padnz*padnx*sizeof(float));
            memset(u2[0], 0, padnz*padnx*sizeof(float));
            memset(sou2[0], 0, nz*nx*sizeof(float));
            memset(permm[0], 0, nz*nx*sizeof(float));
            memset(wave[0][0], 0, nz*nx*nt*sizeof(float));
            for(it=0; it<nt; it++){
                //sf_warning("Modeling_Source: it=%d;",it);
                laplacian(true, u0, u1, u2);
                
                temp=u0;
                u0=u1;
                u1=u2;
                u2=temp;
                
                u1[sx][zs_v]+=ww[it];
                for(ix=0; ix<nx; ix++){
                    for(iz=0; iz<nz; iz++){
                        sou2[ix][iz]+=u1[ix+padx][iz+padz]*u1[ix+padx][iz+padz];
                        wave[it][ix][iz]=u1[ix+padx][iz+padz];
                    }
                }
                if(it%jt==0 && is==ns/2 && snap)
                    sf_floatwrite(u1[0], padnx*padnz, snapshot);
            }// end of it
            
            memset(u0[0], 0, padnz*padnx*sizeof(float));
            memset(u1[0], 0, padnz*padnx*sizeof(float));
            memset(u2[0], 0, padnz*padnx*sizeof(float));
            for(ix=0; ix<nx; ix++){
                for(iz=0; iz<nz; iz++){
                    permm[ix][iz]+=mm[ix][iz]/(sou2[ix][iz]+1e-6);
                }
            }
            for(it=0; it<nt; it++){
                //sf_warning("Modeling_Receiver: it=%d;",it);
                laplacian(true, u0, u1, u2);
                
                temp=u0;
                u0=u1;
                u1=u2;
                u2=temp;
                
                for(ix=0; ix<nx; ix++){
                    for(iz=0; iz<nz; iz++){
                        u1[ix+padx][iz+padz]+=wave[it][ix][iz]*permm[ix][iz];
                    }
                }
                
                for(ir=0; ir<nr; ir++){
                    rx=ir*dr_v+r0_v;
                    dd[is][ir][it]+=u1[rx][zr_v];
                }
            } //end of it
        }// end of shot
    }// end of if
}

int main(int argc, char* argv[])
{
    bool adj;
    int ix, iz;
    float dz, dx, dt, dr, ds;
    float z0, x0, t0, r0, s0;
    float zr, zs, padx0, padz0;
    float dt2;
 
    float ***dd, **mm, **vv, **reducemm;
    sf_file in, out, vel, wavelet;
    
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
    if(rank==0) sf_warning("numprocs=%d", numprocs);
    
    sf_init(argc, argv);
    omp_init();
    
    if(!sf_getbool("adj", &adj)) adj=true;
    if(!sf_getbool("verb", &verb)) verb=false;
    if(!sf_getbool("snap", &snap)) snap=false;
    
    in=sf_input("in");
    out=sf_output("out");
    vel=sf_input("velocity");
    wavelet=sf_input("wavelet");
    
    if(!sf_histint(vel, "n1", &nz)) sf_error("No n1= in velocity");
    if(!sf_histfloat(vel, "d1", &dz)) sf_error("No d1= in velocity");
    if(!sf_histfloat(vel, "o1", &z0)) sf_error("No o1= in velocity");
    
    if(!sf_histint(vel, "n2", &nx)) sf_error("No n2= in velocity");
    if(!sf_histfloat(vel, "d2", &dx)) sf_error("No d2= in velocity");
    if(!sf_histfloat(vel, "o2", &x0)) sf_error("No o2= in velocity");
    
    if(!sf_histint(wavelet, "n1", &nt)) sf_error("No n1= in wavelet");
    if(!sf_histfloat(wavelet, "d1", &dt)) sf_error("No d1= in wavelet");
    if(!sf_histfloat(wavelet, "o1", &t0)) sf_error("No o1= in wavelet");
    
    if(adj){/* migration */
        if(!sf_histint(in, "n2", &nr)) sf_error("No n2= in input");
        if(!sf_histfloat(in, "d2", &dr)) sf_error("No d2= in input");
        if(!sf_histfloat(in, "o2", &r0)) sf_error("No o2= in input");
        
        if(!sf_histint(in, "n3", &ns)) sf_error("No n3= in input");
        if(!sf_histfloat(in, "d3", &ds)) sf_error("No d3= in input");
        if(!sf_histfloat(in, "o3", &s0)) sf_error("No o3= in input");
        
        sf_putint(out, "n1", nz);
        sf_putfloat(out, "d1", dz);
        sf_putfloat(out, "o1", z0);
        sf_putint(out, "n2", nx);
        sf_putfloat(out, "d2", dx);
        sf_putfloat(out, "o2", x0);
        sf_putint(out, "n3", 1);
        sf_putstring(out, "label1", "Depth");
        sf_putstring(out, "unit1", "m");
        sf_putstring(out, "label2", "Distance");
        sf_putstring(out, "unit2", "m");
    }else{/* modeling */
        if(!sf_getint("nr", &nr)) sf_error("No nr=");
        if(!sf_getfloat("dr", &dr)) sf_error("No dr=");
        if(!sf_getfloat("r0", &r0)) sf_error("No r0=");
        
        if(!sf_getint("ns", &ns)) sf_error("No ns=");
        if(!sf_getfloat("ds", &ds)) sf_error("No ds=");
        if(!sf_getfloat("s0", &s0)) sf_error("No s0=");
        
        sf_putint(out, "n1", nt);
        sf_putfloat(out, "d1", dt);
        sf_putfloat(out, "o1", t0);
        sf_putint(out, "n2", nr);
        sf_putfloat(out, "d2", dr);
        sf_putfloat(out, "o2", r0);
        sf_putint(out, "n3", ns);
        sf_putfloat(out, "d3", ds);
        sf_putfloat(out, "o3", s0);
        sf_putstring(out, "label1", "Time");
        sf_putstring(out, "unit1", "s");
        sf_putstring(out, "label2", "Distance");
        sf_putstring(out, "unit2", "m");
        sf_putstring(out, "label3", "Shot");
        sf_putstring(out, "unit3", "m");
    }
    
    if(!sf_getfloat("zr", &zr)) zr=0.0;
    if(!sf_getfloat("zs", &zs)) zs=0.0;
    if(!sf_getint("jt", &jt)) jt=100;
    if(!sf_getint("padz", &padz)) padz=50;
    if(!sf_getint("padx", &padx)) padx=50;
    
    padnx=nx+2*padx;
    padnz=nz+2*padz;
    padx0=x0-dx*padx;
    padz0=z0-dz*padz;
    
    dd=sf_floatalloc3(nt, nr, ns);
    mm=sf_floatalloc2(nz, nx);
    vv=sf_floatalloc2(nz, nx);
    padvv=sf_floatalloc2(padnz, padnx);
    ww=sf_floatalloc(nt);
    reducemm=sf_floatalloc2(nz, nx);
    
    sf_floatread(ww, nt, wavelet);
    sf_floatread(vv[0], nz*nx, vel);
    
    dt2=dt*dt;
    for(ix=0; ix<nx; ix++)
        for(iz=0; iz<nz; iz++)
            padvv[ix+padx][iz+padz]=vv[ix][iz]*vv[ix][iz]*dt2;
    
    for(iz=0; iz<padz; iz++){
        for(ix=padx; ix<nx+padx; ix++){
            padvv[ix][iz]=padvv[ix][padz];
            padvv[ix][iz+nz+padz]=padvv[ix][nz+padz-1];
        }
    }
    
    for(ix=0; ix<padx; ix++){
        for(iz=0; iz<padnz; iz++){
            padvv[ix][iz]=padvv[padx][iz];
            padvv[ix+nx+padx][iz]=padvv[nx+padx-1][iz];
        }
    }
    
    if(snap){
        snapshot=sf_output("snapshot");
        
        sf_putint(snapshot, "n1", padnz);
        sf_putfloat(snapshot, "d1", dz);
        sf_putfloat(snapshot, "o1", padz0);
        sf_putint(snapshot, "n2", padnx);
        sf_putfloat(snapshot, "d2", dx);
        sf_putfloat(snapshot, "o2", padx0);
        sf_putint(snapshot, "n3", 1+(nt-1)/jt);
        sf_putfloat(snapshot, "d3", jt*dt);
        sf_putfloat(snapshot, "o3", t0);
        sf_putstring(snapshot, "label1", "Depth");
        sf_putstring(snapshot, "unit1", "m");
        sf_putstring(snapshot, "label2", "Distance");
        sf_putstring(snapshot, "unit2", "m");
        sf_putstring(snapshot, "label3", "Time");
        sf_putstring(snapshot, "unit3", "s");
    }
    
    dr_v=(dr/dx)+0.5;
    r0_v=(r0-padx0)/dx+0.5;
    zr_v=(zr-padz0)/dz+0.5;
    
    ds_v=(ds/dx)+0.5;
    s0_v=(s0-padx0)/dx+0.5;
    zs_v=(zs-padz0)/dz+0.5;
    
    sf_warning("FLT_EPSILON=%f", 1e-6);
    sf_warning("nx=%d padnx=%d nz=%d padnz=%d nt=%d nr=%d ns=%d", nx, padnx, nz, padnz, nt, nr, ns);
    sf_warning("dx=%.3f dz=%.3f dt=%.3f dr=%.3f ds=%.3f", dx, dz, dt, dr, ds);
    sf_warning("x0=%.3f z0=%.3f t0=%.3f r0=%.3f s0=%.3f", x0, z0, t0, r0, s0);
    sf_warning("dr_v=%d r0_v=%d zr_v=%d ds_v=%d s0_v=%d zs_v=%d", dr_v, r0_v, zr_v, ds_v, s0_v, zs_v);
    
    idz2=1./(dz*dz);
    idx2=1./(dx*dx);
//    c0=-5.0/2.;
//    c1=4.0/3.;
//    c2=-1.0/12.;
    
    c11=4.0*idz2/3.0;
    c12=-idz2/12.0;
    c21=4.0*idx2/3.0;
    c22=-idx2/12.0;
    c0=-2*(c11+c12+c21+c22);
    
    if(adj){/* migration */
        sf_floatread(dd[0][0], nt*nr*ns, in);
        prertm2_oper(adj, dd, mm);
        MPI_Reduce(mm[0], reducemm[0], nz*nx, MPI_FLOAT, MPI_SUM, 0, MPI_COMM_WORLD);
        if(rank==0) sf_floatwrite(reducemm[0], nz*nx, out);
    }else{
        sf_floatread(mm[0], nz*nx, in);
        prertm2_oper(adj, dd, mm);
        sf_floatwrite(dd[0][0], nt*nr*ns, out);
    }
    
    free(*padvv); free(padvv);
    free(ww);
    MPI_Finalize();
    
    exit(0);
}
