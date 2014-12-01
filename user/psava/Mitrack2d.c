/* Datuming by 2D Green functions in constant media */

#include <rsf.h>
#include <math.h>

#ifdef _OPENMP
#include <omp.h>
#endif

static float mymem=0;
void memreport(float addmem)
{
    mymem += addmem;
    sf_warning("MEMORY=%5d Mb",(int)mymem);
}

/*------------------------------------------------------------*/
int main(int argc, char* argv[])
{
    bool  verb; /* verbosity flag */
    float velo; /* medium velocity */
    bool  fast;

    sf_file  Fnn,     Fff; /* coordinates */
    pt2d    *nn=NULL,*ff=NULL;
    sf_axis  an,      af;
    int      jn,      jf;
    
    sf_file     Fwin,       Fwou; /* wavefield */
    sf_complex **win=NULL, **wou=NULL, **tmp=NULL;
    sf_axis  aw;
    int      jw;

    float slow; /* slowness */
    float dist, **tim=NULL,**amp=NULL; /* time & amplitude */
    float   d,t,a;
    sf_complex iomega;

    float ox,oz;
    pt2d oo;
    vc2d vecON, vecOF;
    float angFON,angMAX;
    float gauANG, g;

    int ompnth=1,ompith=0;

    size_t eseek;

    /*------------------------------------------------------------*/
    sf_init(argc,argv);
#ifdef _OPENMP
    ompnth=omp_init();
#endif

    if(! sf_getbool("verb",&verb)) verb=false; /* verbosity flag */
    if(!sf_getfloat("velo",&velo)) velo=1.0;   /* medium velocity */
    slow=1./velo; /* slowness */
    if(! sf_getbool("fast",&fast)) fast=true;  /* fast execution */

    /*------------------------------------------------------------*/
    if(! sf_getfloat("ox",&ox)) ox=0.0; oo.x=ox;
    if(! sf_getfloat("oz",&oz)) oz=0.0; oo.z=oz;
    if(! sf_getfloat("angMAX",&angMAX)) angMAX=90.0;
    if(! sf_getfloat("gauANG",&gauANG)) gauANG=0.3*angMAX;
    gauANG = 1./ (2.*pow(gauANG,2));

    /*------------------------------------------------------------*/
    /* coordinates */
    Fnn = sf_input ("cnn"); /* "near" coordinates  */
    Fff = sf_input ("cff"); /* "far" array */

    an = sf_iaxa(Fnn,2); sf_setlabel(an,"n"); if(verb) sf_raxa(an);
    af = sf_iaxa(Fff,2); sf_setlabel(af,"f"); if(verb) sf_raxa(af);

    nn = (pt2d*) sf_alloc(sf_n(an),sizeof(*nn)); 
    ff = (pt2d*) sf_alloc(sf_n(af),sizeof(*ff)); 

    memreport(sf_n(an)/1024./1024.*sizeof(*nn));
    memreport(sf_n(af)/1024./1024.*sizeof(*ff));

    pt2dread1(Fnn,nn,sf_n(an),2); /* read (x,z) */
    pt2dread1(Fff,ff,sf_n(af),2); /* read (x,z) */
      
    /*------------------------------------------------------------*/
    /* wavefield */
    Fwin = sf_input ("in" ); 
    Fwou = sf_output("out"); 
    aw = sf_iaxa(Fwin,2); sf_setlabel(aw,"w"); if(verb) sf_raxa(aw);  /* freq axis */

    sf_warning("allocate memory");
    win = sf_complexalloc2(sf_n(an),ompnth);
    memreport(sf_n(an)/1024.*ompnth/1024.*sizeof(sf_complex));    

    wou = sf_complexalloc2(sf_n(an),ompnth);
    memreport(sf_n(an)/1024.*ompnth/1024.*sizeof(sf_complex));

    tmp = sf_complexalloc2(sf_n(af),ompnth);
    memreport(sf_n(af)/1024.*ompnth/1024.*sizeof(sf_complex));
    sf_warning("OK");

    /*------------------------------------------------------------*/
    if(fast) {
	sf_warning("precompute T & A");

	/* precompute time */
	tim = sf_floatalloc2(sf_n(af),sf_n(an));
	memreport(sf_n(af)/1024.*sf_n(an)/1024*sizeof(float));
	
	/* precompute amplitude */
	amp = sf_floatalloc2(sf_n(af),sf_n(an));
	memreport(sf_n(af)/1024.*sf_n(an)/1024*sizeof(float));
	
	for(jn=0; jn<sf_n(an); jn++) {
	    for(jf=0; jf<sf_n(af); jf++) {
		dist = sqrt( pow((ff[jf].x-nn[jn].x),2) + 
			     pow((ff[jf].z-nn[jn].z),2) ); 
		amp[jn][jf] = dist==0?1.0:1.0/(2*SF_PI*dist);  
		tim[jn][jf] = dist*slow;                   
	    }
	}
	
	sf_warning("OK");
    }
    
    /*------------------------------------------------------------*/
    sf_warning("reserve output");
    /* reserve output binary */
    for(jn=0; jn<sf_n(an); jn++)
	wou[0][jn] = sf_cmplx(0.0,0.0);
    for(jw=0; jw<sf_n(aw); jw++)
	sf_complexwrite(wou[0],sf_n(an),Fwou);
    sf_seek(Fwou,0,SEEK_SET);
    sf_warning("OK");

    /*------------------------------------------------------------*/
#ifdef _OPENMP
#pragma omp parallel for schedule(dynamic)				\
    private(ompith,jw,jn,jf,iomega,d,t,a,g,vecON,vecOF,angFON)		\
    shared (       aw,an,af,win,wou,tmp,tim,amp,oo,nn,ff)
#endif
    for(jw=0; jw<sf_n(aw); jw++) {
#ifdef _OPENMP
        ompith = omp_get_thread_num();
#endif
#ifdef _OPENMP
#pragma omp critical
#endif
	{
	    if(verb) sf_warning ("(ith=%2d) ... <jw=%4d of %4d>",ompith,jw,sf_n(aw)-1);
	    eseek =  jw*sf_n(an); /* seek elements */
	    sf_seek(Fwin,(off_t)(eseek*sizeof(sf_complex)),SEEK_SET);
	    sf_complexread (win[ompith],sf_n(an),Fwin); /* read win */
	}
	for(jn=0; jn<sf_n(an); jn++)        /* init wou */
	    wou[ompith][jn] = sf_cmplx(0.0,0.0);

	iomega = sf_cmplx(0.0, 2.*SF_PI* (sf_o(aw) + jw*sf_d(aw)));

	if(fast) {

	    /* spray nn -> ff */
	    for(jf=0; jf<sf_n(af); jf++)
		tmp[ompith][jf] = sf_cmplx(0.0,0.0);
	    for(jn=0; jn<sf_n(an); jn++) {
		vecON = vec2d(&oo, &nn[jn]);          /* vector O-N */
		for(jf=0; jf<sf_n(af); jf++) {
		    vecOF = vec2d(&oo, &ff[jf]);      /* vector O-F */
		    angFON = ang2d( &vecON, &vecOF);  /* angle F-O-N */
	    
		    if( angFON < angMAX ) {
			g = exp( - pow(angFON,2) * gauANG ); /* gaussian scaling */
			tmp[ompith][jf] += win[ompith][jn] * g * amp[jn][jf]*cexpf(-iomega*tim[jn][jf]);
		    }
		}
	    }
    
	    /* stack nn <- ff */
	    for(jn=0; jn<sf_n(an); jn++) {
		wou[ompith][jn] = sf_cmplx(0.0,0.0);
		vecON = vec2d(&oo, &nn[jn]);          /* vector O-N */
		for(jf=0; jf<sf_n(af); jf++) {
		    vecOF = vec2d(&oo, &ff[jf]);      /* vector O-F */
		    angFON = ang2d( &vecON, &vecOF);  /* angle F-O-N */
	    
		    if( angFON < angMAX ) {
			g = exp( - pow(angFON,2) * gauANG ); /* gaussian scaling */
			wou[ompith][jn] += tmp[ompith][jf] * g * amp[jn][jf]*cexpf(+iomega*tim[jn][jf]);
		    }
		}
	    }

	} else {

	    /* spray nn -> ff */
	    for(jf=0; jf<sf_n(af); jf++)
		tmp[ompith][jf] = sf_cmplx(0.0,0.0);
	    for(jn=0; jn<sf_n(an); jn++) {

		vecON = vec2d(&oo, &nn[jn]);         /* vector O-N */
		for(jf=0; jf<sf_n(af); jf++) {
		    vecOF = vec2d(&oo, &ff[jf]);     /* vector O-F */
		    angFON = ang2d(&vecON, &vecOF);  /* angle F-O-N */

		    if( angFON < angMAX ) {
			g = exp( - pow(angFON,2) * gauANG ); /* gaussian scaling */
			d = sqrt( pow((ff[jf].x-nn[jn].x),2) +
				  pow((ff[jf].z-nn[jn].z),2) );
			a = d==0?1.0:1.0/(2*SF_PI*d);     /* spherical divergence */
			t = d*slow;                       /* traveltime */
			tmp[ompith][jf] += win[ompith][jn] * g * a*cexpf(-iomega*t);
		    }
		}
	    }

	    /* stack nn <- ff */
	    for(jn=0; jn<sf_n(an); jn++) {
		wou[ompith][jn] = sf_cmplx(0.0,0.0);

		vecON = vec2d(&oo, &nn[jn]);          /* vector O-N */
		for(jf=0; jf<sf_n(af); jf++) {
		    vecOF = vec2d(&oo, &ff[jf]);      /* vector O-F */
		    angFON = ang2d( &vecON, &vecOF);  /* angle F-O-N */
	
		    if( angFON < angMAX ) {
			g = exp( - pow(angFON,2) * gauANG ); /* gaussian scaling */
			d = sqrt( pow((ff[jf].x-nn[jn].x),2) +
				  pow((ff[jf].z-nn[jn].z),2) );
			a = d==0?1.0:1.0/(2*SF_PI*d);        /* spherical divergence */
			t = d*slow;                          /* traveltime */
			wou[ompith][jn] += tmp[ompith][jf] * g * a*cexpf(+iomega*t);
		    }
		}
	    }

	}

#ifdef _OPENMP
#pragma omp critical
#endif
	{
	    eseek = jw*sf_n(an);
	    sf_seek(Fwou,(off_t)(eseek*sizeof(sf_complex)),SEEK_SET);
	    sf_complexwrite(wou[ompith],sf_n(an),Fwou); /* write wou */
	}
    }

    /*------------------------------------------------------------*/
    /* deallocate arrays */

    if(fast) {
	free(*amp); free(amp); memreport(-sf_n(af)/1024.*sf_n(an)/1024.*sizeof(float));
	free(*tim); free(tim); memreport(-sf_n(af)/1024.*sf_n(an)/1024.*sizeof(float));
    }
    free(*wou); free(wou); memreport(-ompnth/1024.*sf_n(an)/1024.*sizeof(sf_complex));
    free(*win); free(win); memreport(-ompnth/1024.*sf_n(an)/1024.*sizeof(sf_complex));
    free(*tmp); free(tmp); memreport(-ompnth/1024.*sf_n(af)/1024.*sizeof(sf_complex));
    free(ff);              memreport(-             sf_n(af)/1024./1024.*sizeof(*ff));
    free(nn);              memreport(-             sf_n(an)/1024./1024.*sizeof(*nn));

    exit (0);
}
