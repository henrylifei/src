/* 2D Local prediction filter. */
/*
  Copyright (C) 2015 Jilin University
   
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

int main(int argc, char* argv[])
{
    bool verb;
    int n[SF_MAX_DIM], m[SF_MAX_DIM], rect[SF_MAX_DIM];
    int ndim, mdim, nd, ns, n12, i, j, niter, na, ia, i4, n4=1;
    float *d, *f, *g, mean, a0;
    char key[6], *peffile, *lagfile;
    sf_filter aa;
    sf_file dat, flt, mat, pre, pef, lag;

    sf_init(argc,argv);

    dat = sf_input("in");
    mat = sf_input("match");
    flt = sf_output("out");

    if (NULL != sf_getstring("pred")) {
	pre = sf_output("pred");
    } else {
	pre = NULL;
    }

    ndim = 3;
    mdim = 2;


    if (!sf_histint(dat,"n1",&n[0])) sf_error("No n1= in input");
    if (!sf_histint(dat,"n2",&n[1])) sf_error("No n2= in input");
    if (!sf_histint(dat,"n3",&n[2])) sf_error("No n3= in input");
    n[3] = sf_leftsize(dat,3);

    if (!sf_histint(mat,"n1",&m[0])) sf_error("No n1= in match");
    if (!sf_histint(mat,"n2",&m[1])) sf_error("No n2= in match");
    m[2] = sf_leftsize(mat,2);

    if (mdim > ndim) 
	sf_error("Wrong dimensions: %d > %d",mdim,ndim);
 
    if (m[2] != n[3]) {
	sf_error("Size mismatch [n%d] : %d != %d",3,m[2],n[3]);
    } else {
	n4 = n[3];
    }

    nd = 1;
    for (j=0; j < mdim; j++) {
	if (m[j] != n[j]) 
	    sf_error("Size mismatch [n%d]: %d != %d",
		     j+1,m[j],n[j]);
	nd *= m[j];
	snprintf(key,6,"rect%d",j+1);
	if (!sf_getint(key,rect+j)) rect[j]=1;
    }
    for (ns = 1; j < ndim; j++) {
	ns *= n[j];
	if (pre) {
	    snprintf(key,6,"n%d",j+1);
	    sf_putint(pre,key,n4);
	    snprintf(key,6,"n%d",j+2);
	    sf_putint(pre,key,1);
	}
    }
    n12 = nd*ns;

    if (!sf_getint("niter",&niter)) niter=100;
    /* number of iterations */

    if (!sf_getbool("verb",&verb)) verb=true;
    /* verbosity flag */

    if (NULL == (peffile = sf_getstring("pef"))) { 
        /* signal PEF file (optional) */
	aa = NULL;
    } else {
	pef = sf_input(peffile);
	if (!sf_histint(pef,"n1",&na)) 
	    sf_error("No n1= in pef");
	aa = sf_allocatehelix (na);
	
	if (!sf_histfloat(pef,"a0",&a0)) a0=1.;
	sf_floatread (aa->flt,na,pef);
	for( ia=0; ia < na; ia++) {
	    aa->flt[ia] /= a0;
	}
	if (NULL != (lagfile = sf_getstring("lag")) 
	    || 
	    NULL != (lagfile = sf_histstring(pef,"lag"))) {
            /* file with PEF lags (optional) */
	    lag = sf_input(lagfile);
	    sf_intread(aa->lag,na,lag);
	    sf_fileclose(lag);
	} else {
	    for( ia=0; ia < na; ia++) {
		aa->lag[ia] = ia+1;
	    }
	}
    }

    d = sf_floatalloc(n12);
    f = sf_floatalloc(n12);
    g = sf_floatalloc(nd);

    for(i4=0; i4 < n4; i4++) {
	fprintf(stderr,"slice %d of %d\n",i4+1,n4);	
	for (i=0; i < n12; i++) {
	    d[i] = 0.;
	}
	sf_multidivn_init(ns, mdim, nd, m, rect, d, aa, verb); 
    
	sf_floatread(d,n12,dat);
	sf_floatread(g,nd,mat);

	mean = 0.;
	for(i=0; i < n12; i++) {
	    mean += d[i]*d[i];
	}
	if (mean == 0.) {
	    sf_floatwrite(d,n12,flt);
	    continue;
	}
	
	mean = sqrtf (mean/n12);
	
	for(i=0; i < n12; i++) {
	    d[i] /= mean;
	}
	for(i=0; i < nd; i++) {
	    g[i] /= mean;
	}
	
	sf_multidivn (g,f,niter);
	sf_floatwrite(f,n12,flt);
	
	if (pre) {
	    for(i=0; i < n12; i++) {
		d[i] *= mean;
	    }
	    
	    sf_weight2_lop(false,false,n12,nd,f,g);
	    sf_floatwrite(g,nd,pre);
	}
    }
    
    exit(0);
}
