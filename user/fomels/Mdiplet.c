/* Seislet frame */
/*
  Copyright (C) 2004 University of Texas at Austin
   
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
   
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
   
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <rsf.h>

#include "diplet.h"

int main(int argc, char *argv[])
{
    int n1, n2, i3, n3, n12, np, n12p, ncycle;
    bool inv, adj;
    char *type;
    float *pp, *qq, ***dd, eps;
    sf_file in, out, dip;

    sf_init(argc,argv);

    in = sf_input("in");
    out = sf_output("out");
    dip = sf_input("dips");

    if (!sf_histint(in,"n1",&n1)) sf_error("No n1= in input");
    if (!sf_histint(in,"n2",&n2)) sf_error("No n2= in input");
    n12 = n1*n2;

    if (!sf_histint(dip,"n3",&np)) np=1;
    n12p = n12*np;

    pp = sf_floatalloc(n12);
    qq = sf_floatalloc(n12p);
    dd = sf_floatalloc3(n1,n2,np);

    if (!sf_getbool("inv",&inv)) inv=false;
    /* if y, do inverse transform */

    if (!sf_getfloat("eps",&eps)) eps=0.01;
    /* regularization */

    if (!sf_getint("niter",&ncycle)) ncycle=0;
    /* number of iterations */

    if (adj) {
	n3 = sf_leftsize(in,2);
	sf_putint(out,"n3",np);
	sf_putint(out,"n4",n3);
    } else {
	n3 = sf_leftsize(in,3);
	sf_putint(out,"n3",n3);
    }

    if (NULL == (type=sf_getstring("type"))) type="linear";
    /* wavelet type (haar,linear,biorthogonal), default is linear */
    
    diplet_init(n1,n2,np,dd,true,eps,type[0]);
    
    for (i3=0; i3 < n3; i3++) {
	sf_floatread(dd[0][0],n12p,dip);	
	
	if (!inv) {
    	    sf_floatread(pp,n12,in);
	    diplet_lop(adj,false,n12p,n12,qq,pp);
	    sf_floatwrite(qq,n12p,out);
	} else {
	    sf_floatread(qq,n12p,in);
	    diplet_lop(adj,false,n12p,n12,qq,pp);
	    sf_floatwrite(pp,n12,out);
	}
    }
    
    exit(0);
}
