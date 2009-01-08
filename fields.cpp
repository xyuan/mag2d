#ifndef FIELD_H
#define FIELD_H
//#define SQR(x) ((x)*(x))
#define MAX(x,y) ((x)>(y)?(x):(y))
#define MIN(x,y) ((x)>(y)?(y):(x))


#include <suitesparse/umfpack.h>
#include "param.cpp"
#include "matrix.cpp"
#include <cmath>
#include <iostream>
using namespace std;

//#define SQR(x) ((x)*(x))
inline double SQR(double x){return x*x;};

#define FIXED 0
#define FREE 1
#define BOUNDARY 3



typedef struct{
            double l, r, t, b;
}t_umf_metrika;



class t_grid
//definuje geometrii experimentu
{
    public:
	int M;
	int N;
	double dr,dz;
	Matrix<char> mask;
	Matrix<t_umf_metrika> metrika;
	Matrix<double> voltage;
	t_grid(Param &param);
	Param *p_param;
};
class Field : public Matrix<double>
{
    public:
	Field(Param &param) : Matrix<double>(param.r_sampl, param.z_sampl), idr(param.idr), idz(param.idz) {};
	inline void accumulate(double charge, double x, double y);
	void print( ostream & out = cout , double factor = 1.0)
	{
	    double dr=1.0/idr, dz=1.0/idz;
	    for(int j=0; j<jmax; j++)
	    {
		for(int l=0; l<lmax; l++)
		    out << j*dr <<"\t"<< l*dz <<"\t"<< data[j][l]*factor <<endl;
		out << endl;
	    }
	}
	void print( const char * filename , double factor = 1.0)
	{
	    ofstream out(filename);
	    print(out, factor);
	}
    private:
	double idr, idz;
};

class Fields
//stara se o reseni Poissonovy rce na danem gridu
{
    public:
	t_grid grid;
	Field u;
	Field uAvg;
	Field rho;
	Fields(Param &param);
	void solve();
	void boundary_solve();
	void reset();
	void grad(double x, double y, double &grad_x, double &grad_y) ;
	inline void accumulate(double charge, double x, double y);
	~Fields();
    private:
	double idr, idz;
	Param *p_param;
	int *Ap;
	int *Ai;
	double *Ax;
	void *Symbolic, *Numeric ;
    public:
	void u_sample();
	void u_reset();
	void u_print(const char * fname);
    private:
	int nsampl;
};
void Fields::u_sample()
{
    uAvg.add(u);
    nsampl++;
}
void Fields::u_reset()
{
    uAvg.reset();
    nsampl = 0;
}
void Fields::u_print(const char * fname)
{
    uAvg.print(fname,1.0/nsampl);
}



t_grid::t_grid(Param &param) :  M(param.r_sampl), N(param.z_sampl), 
    dr(param.dr), dz(param.dz),
    mask(param.r_sampl,param.z_sampl), metrika(param.r_sampl,param.z_sampl), voltage(param.r_sampl,param.z_sampl)
{
    int i, j;
    p_param = &param;
    double dr=p_param->dr;
    double dz=p_param->dz;
    double idr=p_param->idr;
    //double idz=p_param->idz;
    double r_max=p_param->r_max;
    double z_max=p_param->z_max;
    double PROBE_RADIUS=p_param->probe_radius;
			

    /*
     * Vytvoreni sondy
     */
    for(i=0; i<M; i++)
	for(j=0; j<N; j++)
	{
	    double r, z;
	    r = i*dr;
	    z = j*dz;
	    //if(i==0 || i==M-1 || j==0 || j==N-1)
	    if(i==M-1 || j==0 || j==N-1)
	    {
		mask[i][j] = FIXED;
		voltage[i][j] = -i*p_param->dr*p_param->extern_field + p_param->r_max*p_param->extern_field*0.5;
		voltage[i][j] = 0.0;
		/*
	    }else if(SQR(i*dr-p_param->r_max/2) + SQR(j*dz-p_param->z_max/2) <= SQR(p_param->probe_radius))
	    {
		mask[i][j] = FIXED;
		voltage[i][j] = p_param->u_probe;
		*/
	    /*
	    }
	    else if(r>1.57e-2/2.0 && r<1.67e-2/2.0 && z>1e-3 && z<25e-3) // retarding electrode
	    {
		mask[i][j] = FIXED;
		voltage[i][j] = -5.0;
	    }
	    else if((r<1.46e-2/2 && z>15e-3 && z<16e-3)
		    || (r<7e-3/2 && z>12e-3 && z<19e-3)
		    || (r<1.9e-3 && z>1e-3 && z<12e-3)) // collector
	    {
		mask[i][j] = FIXED;
		voltage[i][j] = 10.0;
		*/
	    }else
	    {
		mask[i][j] = FREE;
	    }
	}

    for(i=2;i<M-2;i++)
	for(j=2;j<N-2;j++)
	{
	    if( ( mask[i-1][j] == FIXED ||
			mask[i+1][j] == FIXED ||
			mask[i][j-1] == FIXED ||
			mask[i][j+1] == FIXED ) &&
		    mask[i][j] != FIXED )
		mask[i][j] = BOUNDARY;
	}


}

inline void Field::accumulate(double charge, double r, double z)
{
    int i = (int)(r * idr);
    int j = (int)(z * idz);

    double u = r*idr - i;
    double v = z*idz - j;

    if(i<0 || i>jmax-1 || j<0 || j>lmax-1)
	throw std::runtime_error("Field::accumulate() outside of range\n");
    data[i][j] += (1-u)*(1-v)*charge;
    data[i+1][j] += u*(1-v)*charge;
    data[i][j+1] += (1-u)*v*charge;
    data[i+1][j+1] += u*v*charge;

}
inline void Fields::accumulate(double charge, double r, double z)
{
    int i = (int)(r * idr);
    int j = (int)(z * idz);

    double u = r*idr - i;
    double v = z*idz - j;

    //if(i<0 || i>p_param->r_sampl-1 || j<0 || j>p_param->z_sampl-1)
//	throw std::runtime_error("Field::accumulate() outside of range\n");
    rho[i][j] += (1-u)*(1-v)*charge;
    rho[i+1][j] += u*(1-v)*charge;
    rho[i][j+1] += (1-u)*v*charge;
    rho[i+1][j+1] += u*v*charge;

}
Fields::Fields(Param &param) : grid(param), u(param), uAvg(param), rho(param), nsampl(0)
{
    if(param.selfconsistent == false)
	return;
    p_param = &param;
    int i, j, k, l;
    int n;
    idr=p_param->idr;
    idz=p_param->idz;
    double dr = p_param->dr;
    double dz = p_param->dz;

    n = param.r_sampl * param.z_sampl;
    
    // r_i = i*dr
    Ap = new int [n+1];
    Ai = new int [n*5]; //horni odhad pro petibodove diff schema
    Ax = new double [n*5];


    l = 0;
    Ap[0] = 0;
    for(i=0; i<param.r_sampl; i++)	//cislo radku - radialni souradnice
	for(j=0; j<param.z_sampl; j++)	//cislo sloupce
	{
	    k = j + param.z_sampl*i;
	    if(grid.mask[i][j]==FIXED)
	    {
		Ax[l] = 1;
		Ai[l] = k;
		Ap[k+1] = Ap[k]+1;
		l++;
	    }
	    else if(i==0 && j>0 && j<param.z_sampl-1)
	    {
		double k2, k3;
		// psi[j,k-1]
		k2 = 1.0/(dz*dz);
		Ax[l] = k2;
		Ai[l] = k-1;

		// psi[j,k]
		k3 = 1.0/(dr*dr*0.25);
		Ax[l+1] = -2.0*k2 - k3;
		Ai[l+1] = k;

		// psi[j,k+1]
		Ax[l+2] = k2;
		Ai[l+2] = k+1;

		// psi[j+1,k]
		Ax[l+3] = k3;
		Ai[l+3] = k+param.z_sampl;

		Ap[k+1] = Ap[k]+4;
		l += 4;
	    }
	    else if(grid.mask[i][j]!=FIXED )
	    {
		double k1, k2, k3;
		// psi[j-1,k]
		k1 = (i-0.5)/(dr*dr*i);
		Ax[l] = k1;
		Ai[l] = k-param.z_sampl;

		// psi[j,k-1]
		k2 = 1.0/(dz*dz);
		Ax[l+1] = k2;
		Ai[l+1] = k-1;

		// psi[j,k]
		k3 = (i+0.5)/(dr*dr*i);
		Ax[l+2] = -2.0*k2 - k1 - k3;
		Ai[l+2] = k;

		// psi[j,k+1]
		Ax[l+3] = k2;
		Ai[l+3] = k+1;

		// psi[j+1,k]
		Ax[l+4] = k3;
		Ai[l+4] = k+param.z_sampl;

		Ap[k+1] = Ap[k]+5;
		l += 5;
	    }
	}
    /*
    for(i=0;i<param.r_sampl;i++)
	for(j=0;j<param.z_sampl;j++)
	{
	    if( SQR(i*p_param->dr-p_param->r_max/2) + SQR(j*p_param->dz-p_param->z_max/2) <= SQR(p_param->probe_radius))
	    {
		u[i][j] = p_param->u_probe;
	    }else
	    {
		u[i][j] = 0;
	    }
	}
*/
    (void) umfpack_di_symbolic (n, n, Ap, Ai, Ax, &Symbolic, NULL, NULL) ;
    (void) umfpack_di_numeric (Ap, Ai, Ax, Symbolic, &Numeric, NULL, NULL) ;
    umfpack_di_free_symbolic (&Symbolic) ;
}
    

void Fields::boundary_solve()
{
    int k;
    for(int i=0; i<grid.M; i++)		//cislo radku
	for(int j=0; j<grid.N; j++)	//cislo sloupce
	{
	    k = j + grid.N*i;
	    double dr = p_param->dr;
	    double dz = p_param->dz;
	    if(grid.mask[i][j] == FIXED)
		rho[i][j] = grid.voltage[i][j];
	    else if(i>0)
		rho[i][j] *= -1.0/p_param->eps_0/(M_PI*dr*dr*2.0*i*dz)*p_param->macroparticle_factor;
	    else
		rho[i][j] *= -1.0/p_param->eps_0/(M_PI*dr*dr*0.25*dz)*p_param->macroparticle_factor;
	}

    (void) umfpack_di_solve (UMFPACK_At, Ap, Ai, Ax, u[0], rho[0], Numeric, NULL, NULL) ;
}

void Fields::solve()
{
    (void) umfpack_di_solve (UMFPACK_At, Ap, Ai, Ax, u[0], rho[0], Numeric, NULL, NULL) ;
}
void Fields::reset()
{
    for(int i=0; i<grid.M; i++)		//cislo radku
	for(int j=0; j<grid.N; j++)	//cislo sloupce
	    rho[i][j] = 0;
}


Fields::~Fields()
{
    umfpack_di_free_numeric (&Numeric) ;
    delete [] Ap;
    delete [] Ai;
    delete [] Ax;
}



/*
 * vypocte gradient 2d potencialu bilinearni interpolaci
 */
void Fields::grad(double x, double y, double &grad_x, double &grad_y)
{
    int i,j;
    double g1,g2,g3,g4;
    double fx,fy;
    double idr = p_param->idr;
    double idz = p_param->idz;

    /*
     * nejprve x-ova slozka
     */
    i = (int)(x*idr+0.5);
    j = (int)(y*idz);
    j = MIN(j,p_param->z_sampl-2);
    if(i>0 && i<p_param->r_sampl-1)
    {
	//vypocet gradientu v mrizovych bodech
	g1 = (u[i][j]-u[i-1][j])*idr;
	g2 = (u[i][j+1]-u[i-1][j+1])*idr;
	g3 = (u[i+1][j+1]-u[i][j+1])*idr;
	g4 = (u[i+1][j]-u[i][j])*idr;

	//interpolace
	fx = x*idr-i+.5;
	fy = y*idz-j;
	grad_x = g1*(1-fx)*(1-fy) + g2*(1-fx)*fy + g3*fx*fy + g4*fx*(1-fy);
    }else if(i==p_param->r_sampl-1)
    {
	//vypocet gradientu v mrizovych bodech
	g1 = (u[i][j]-u[i-1][j])*idr;
	g2 = (u[i][j+1]-u[i-1][j+1])*idr;

	//interpolace
	fy = y*idz-j;
	grad_x = g1*(1-fy) + g2*fy;
    }else if(i==0)
    {
	//vypocet gradientu v mrizovych bodech
	g3 = (u[i+1][j+1]-u[i][j+1])*idr;
	g4 = (u[i+1][j]-u[i][j])*idr;

	//interpolace
	fy = y*idz-j;
	grad_x = g3*fy + g4*(1-fy);
    }



    /*
     * ted y-ova slozka
     */
    i = (int)(x*idr);
    j = (int)(y*idz+0.5);
    i = MIN(i,p_param->r_sampl-2);


    /*
       =if(pot_mask[i][j]==OKRAJ || pot_mask[i][j+1]==OKRAJ)
       {
       }else
       */
    if(j>0 && j<p_param->z_sampl-1)
    {
	//vypocet gradientu v mrizovych bodech
	g1 = (u[i][j]-u[i][j-1])*idz;
	g2 = (u[i+1][j]-u[i+1][j-1])*idz;
	g3 = (u[i+1][j+1]-u[i+1][j])*idz;
	g4 = (u[i][j+1]-u[i][j])*idz;

	//interpolace
	fx = x*idr-i;
	fy = y*idz-j+0.5;
	grad_y = g1*(1-fx)*(1-fy) + g2*(1-fy)*fx + g3*fx*fy + g4*fy*(1-fx);
    }else if(j==p_param->z_sampl-1)
    {
	//vypocet gradientu v mrizovych bodech
	g1 = (u[i][j]-u[i][j-1])*idz;
	g2 = (u[i+1][j]-u[i+1][j-1])*idz;

	//interpolace
	fx = x*idr-i;
	grad_y = g1*(1-fx) + g2*fx;
    }else if(j==0)
    {
	//vypocet gradientu v mrizovych bodech
	g3 = (u[i+1][j+1]-u[i+1][j])*idz;
	g4 = (u[i][j+1]-u[i][j])*idz;

	//interpolace
	fx = x*idr-i;
	grad_y = g3*fx + g4*(1-fx);
    }

}

#endif