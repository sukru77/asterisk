/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 1999 - 2005, Digium, Inc.
 *
 * Mark Spencer <markster@digium.com>
 *
 * Includes code and algorithms from the Zapata library.
 *
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

/*! \file
 *
 * \brief FSK Modulator/Demodulator
 *
 * \author Mark Spencer <markster@digium.com>
 *
 * \arg Includes code and algorithms from the Zapata library.
 *
 */

/*** MODULEINFO
	<support_level>core</support_level>
 ***/

#include "asterisk.h"

#include "asterisk/fskmodem.h"

#define NBW	2
#define BWLIST	{75,800}
#define	NF	6
#define	FLIST {1400,1800,1200,2200,1300,2100}

#define STATE_SEARCH_STARTBIT	0
#define STATE_SEARCH_STARTBIT2	1
#define STATE_SEARCH_STARTBIT3	2
#define STATE_GET_BYTE			3

static inline int iget_sample(short **buffer, int *len)
{
	int retval;
	retval = (int) **buffer;
	(*buffer)++;
	(*len)--;
	return retval;
}

#define IGET_SAMPLE iget_sample(&buffer, len)
/*! \brief Coefficients for input filters
 * Coefficients table, generated by program "mkfilter"
 * mkfilter is part of the zapatatelephony.org distribution
 * Format: coef[IDX_FREC][IDX_BW][IDX_COEF]
 * IDX_COEF = 0	=>	1/GAIN
 * IDX_COEF = 1-6	=>	Coefficientes y[n]
*/
static double coef_in[NF][NBW][8]={
	{  { 1.8229206611e-04,-7.8997325866e-01,2.2401819940e+00,-4.6751353581e+00,5.5080745712e+00,-5.0571565772e+00,2.6215820004e+00,0.0000000000e+00,
	},  { 9.8532175289e-02,-5.6297236492e-02,3.3146713415e-01,-9.2239200436e-01,1.4844365184e+00,-2.0183258642e+00,2.0074154497e+00,0.0000000000e+00,
	},  },  {  { 1.8229206610e-04,-7.8997325866e-01,7.7191410839e-01,-2.8075643964e+00,1.6948618347e+00,-3.0367273700e+00,9.0333559408e-01,0.0000000000e+00,
	},  { 9.8531161839e-02,-5.6297236492e-02,1.1421579050e-01,-4.8122536483e-01,4.0121072432e-01,-7.4834487567e-01,6.9170822332e-01,0.0000000000e+00,
	},  },  {  { 1.8229206611e-04,-7.8997325866e-01,2.9003821430e+00,-6.1082779024e+00,7.7169345751e+00,-6.6075999680e+00,3.3941838836e+00,0.0000000000e+00,
	},  { 9.8539686961e-02,-5.6297236492e-02,4.2915323820e-01,-1.2609358633e+00,2.2399213250e+00,-2.9928879142e+00,2.5990173742e+00,0.0000000000e+00,
	},  },  {  { 1.8229206610e-04,-7.8997325866e-01,-7.7191410839e-01,-2.8075643964e+00,-1.6948618347e+00,-3.0367273700e+00,-9.0333559408e-01,0.0000000000e+00,
	},  { 9.8531161839e-02,-5.6297236492e-02,-1.1421579050e-01,-4.8122536483e-01,-4.0121072432e-01,-7.4834487567e-01,-6.9170822332e-01,0.0000000000e+00,
	},  },  {  { 1.8229206611e-04,-7.8997325866e-01,2.5782298908e+00,-5.3629717478e+00,6.5890882172e+00,-5.8012914776e+00,3.0171839130e+00,0.0000000000e+00,
	},  { 9.8534230718e-02,-5.6297236492e-02,3.8148618075e-01,-1.0848760410e+00,1.8441165168e+00,-2.4860666655e+00,2.3103384142e+00,0.0000000000e+00,
	},  },  {  { 1.8229206610e-04,-7.8997325866e-01,-3.8715051001e-01,-2.6192408538e+00,-8.3977994034e-01,-2.8329897913e+00,-4.5306444352e-01,0.0000000000e+00,
	},  { 9.8531160936e-02,-5.6297236492e-02,-5.7284484199e-02,-4.3673866734e-01,-1.9564766257e-01,-6.2028156584e-01,-3.4692356122e-01,0.0000000000e+00,
	},  },
};

/*! \brief Coefficients for output filter
 * Coefficients table, generated by program "mkfilter"
 * Format: coef[IDX_BW][IDX_COEF]
 * IDX_COEF = 0	=>	1/GAIN
 * IDX_COEF = 1-6	=>	Coefficientes y[n]
*/
static double coef_out[NBW][8]={
	{ 1.3868644653e-08,-6.3283665042e-01,4.0895057217e+00,-1.1020074592e+01,1.5850766191e+01,-1.2835109292e+01,5.5477477340e+00,0.0000000000e+00,
	},  { 3.1262119724e-03,-7.8390522307e-03,8.5209627801e-02,-4.0804129163e-01,1.1157139955e+00,-1.8767603680e+00,1.8916395224e+00,0.0000000000e+00
	},
};

/*! Integer Pass Band demodulator filter  */
static inline int ibpdfilter(struct filter_struct * fs, int in)
{
	int i,j;
	int s;
	int64_t s_interim;

	/* integer filter */
	s =  in * fs->icoefs[0];
	fs->ixv[(fs->ip + 6) & 7] = s;

	s =      (fs->ixv[fs->ip]           + fs->ixv[(fs->ip + 6) & 7]) +
		6  * (fs->ixv[(fs->ip + 1) & 7] + fs->ixv[(fs->ip + 5) & 7]) +
		15 * (fs->ixv[(fs->ip + 2) & 7] + fs->ixv[(fs->ip + 4) & 7]) +
		20 *  fs->ixv[(fs->ip + 3) & 7];

	for (i = 1, j = fs->ip; i < 7; i++, j++) {
		/* Promote operation to 64 bit to prevent overflow that occurred in 32 bit) */
		s_interim = (int64_t)(fs->iyv[j & 7]) *
				(int64_t)(fs->icoefs[i]) /
				(int64_t)(1024);
		s += (int) s_interim;
	}
	fs->iyv[j & 7] = s;
	fs->ip++;
	fs->ip &= 7;
	return s;
}

/*! Integer Band Pass filter */
static inline int ibpfilter(struct filter_struct * fs, int in)
{
	int i, j;
	int s;
	int64_t s_interim;

	/* integer filter */
	s =  in * fs->icoefs[0] / 256;
	fs->ixv[(fs->ip + 6) & 7] = s;

	s = (fs->ixv[(fs->ip + 6) & 7] - fs->ixv[fs->ip])
		+ 3 * (fs->ixv[(fs->ip + 2) & 7] - fs->ixv[(fs->ip + 4) & 7]);

	for (i = 1, j = fs->ip; i < 7; i++, j++) {
		s_interim = (int64_t)(fs->iyv[j & 7]) *
				(int64_t)(fs->icoefs[i]) /
				(int64_t)(256);
		s += (int) s_interim;
	}
	fs->iyv[j & 7] = s;
	fs->ip++;
	fs->ip &= 7;
	return s;
}

static inline int idemodulator(fsk_data *fskd, int *retval, int x)
{
	int is, im, id;
	int ilin2;

	is = ibpfilter(&fskd->space_filter, x);
	im = ibpfilter(&fskd->mark_filter, x);

	ilin2 = ((im * im) - (is * is)) / (256 * 256);

	id = ibpdfilter(&fskd->demod_filter, ilin2);

	*retval = id;
	return 0;
}

static int get_bit_raw(fsk_data *fskd, short *buffer, int *len)
{
	/* This function implements a DPLL to synchronize with the bits */
	int f;

	int ix;
	/* PLL coeffs are set up in callerid_new */
	for (f = 0;;) {
		if (idemodulator(fskd, &ix, IGET_SAMPLE)) return(-1);
		if ((ix * fskd->xi0) < 0) { /* Transicion */
			if (!f) {
				if (fskd->icont < (fskd->pllispb2)) {
					fskd->icont += fskd->pllids;
				} else {
					fskd->icont -= fskd->pllids;
				}
				f = 1;
			}
		}
		fskd->xi0 = ix;
		fskd->icont += 32;
		if (fskd->icont > fskd->pllispb) {
			fskd->icont -= fskd->pllispb;
			break;
		}
	}
	f = (ix > 0) ? 0x80 : 0;
	return f;
}

int fskmodem_init(fsk_data *fskd)
{
	int i;

	fskd->space_filter.ip  = 0;
	fskd->mark_filter.ip   = 0;
	fskd->demod_filter.ip  = 0;

	for ( i = 0 ; i < 7 ; i++ ) {
		fskd->space_filter.icoefs[i] =
			coef_in[fskd->f_space_idx][fskd->bw][i] * 256;
		fskd->space_filter.ixv[i] = 0;;
		fskd->space_filter.iyv[i] = 0;;

		fskd->mark_filter.icoefs[i] =
			coef_in[fskd->f_mark_idx][fskd->bw][i] * 256;
		fskd->mark_filter.ixv[i] = 0;;
		fskd->mark_filter.iyv[i] = 0;;

		fskd->demod_filter.icoefs[i] =
			coef_out[fskd->bw][i] * 1024;
		fskd->demod_filter.ixv[i] = 0;;
		fskd->demod_filter.iyv[i] = 0;;
	}
	return 0;
}

int fsk_serial(fsk_data *fskd, short *buffer, int *len, int *outbyte)
{
	int a;
	int i, j, n1, r;
	int samples = 0;
	int olen;
	int beginlen = *len;
	int beginlenx;

	switch (fskd->state) {
		/* Pick up where we left off */
	case STATE_SEARCH_STARTBIT2:
		goto search_startbit2;
	case STATE_SEARCH_STARTBIT3:
		goto search_startbit3;
	case STATE_GET_BYTE:
		goto getbyte;
	}
	/* We await for start bit	*/
	do {
		/* this was jesus's nice, reasonable, working (at least with RTTY) code
		to look for the beginning of the start bit. Unfortunately, since TTY/TDD's
		just start sending a start bit with nothing preceding it at the beginning
		of a transmission (what a LOSING design), we cant do it this elegantly */
		/* NOT USED
				if (demodulator(zap,&x1))
					return -1;
				for(;;) {
					if (demodulator(zap,&x2))
						return -1;
					if (x1>0 && x2<0) break;
					x1=x2;
				}
		*/
		/* this is now the imprecise, losing, but functional code to detect the
		beginning of a start bit in the TDD sceanario. It just looks for sufficient
		level to maybe, perhaps, guess, maybe that its maybe the beginning of
		a start bit, perhaps. This whole thing stinks! */
		beginlenx = beginlen; /* just to avoid unused war warnings */
		if (idemodulator(fskd, &fskd->xi1, IGET_SAMPLE))
			return -1;
		samples++;
		for(;;) {
search_startbit2:
			if (*len <= 0) {
				fskd->state = STATE_SEARCH_STARTBIT2;
				return 0;
			}
			samples++;
			if (idemodulator(fskd, &fskd->xi2, IGET_SAMPLE))
				return -1;
#if 0
			printf("xi2 = %d ", fskd->xi2);
#endif
			if (fskd->xi2 < 512) {
				break;
			}
		}
search_startbit3:
		/* We await for 0.5 bits before using DPLL */
		i = fskd->ispb / 2;
		if (*len < i) {
			fskd->state = STATE_SEARCH_STARTBIT3;
			return 0;
		}
		for (; i > 0; i--) {
			if (idemodulator(fskd, &fskd->xi1, IGET_SAMPLE))
				return(-1);
#if 0
			printf("xi1 = %d ", fskd->xi1);
#endif
			samples++;
		}

		/* x1 must be negative (start bit confirmation) */

	} while (fskd->xi1 > 0);
	fskd->state = STATE_GET_BYTE;

getbyte:

	/* Need at least 80 samples (for 1200) or
		1320 (for 45.5) to be sure we'll have a byte */
	if (fskd->nbit < 8) {
		if (*len < 1320)
			return 0;
	} else {
		if (*len < 80)
			return 0;
	}

	/* Now we read the data bits */
	j = fskd->nbit;
	for (a = n1 = 0; j; j--) {
		olen = *len;
		i = get_bit_raw(fskd, buffer, len);
		buffer += (olen - *len);
		if (i == -1)
			return -1;
		if (i)
			n1++;
		a >>= 1;
		a |= i;
	}
	j = 8 - fskd->nbit;
	a >>= j;

	/* We read parity bit (if exists) and check parity */
	if (fskd->parity) {
		olen = *len;
		i = get_bit_raw(fskd, buffer, len);
		buffer += (olen - *len);
		if (i == -1)
			return -1;
		if (i)
			n1++;
		if (fskd->parity == 1) {	/* parity=1 (even) */
			if (n1 & 1)
				a |= 0x100;			/* error */
		} else {					/* parity=2 (odd) */
			if (!(n1 & 1))
				a |= 0x100;			/* error */
		}
	}

	/* We read STOP bits. All of them must be 1 */

	for (j = fskd->instop; j; j--) {
		r = get_bit_raw(fskd, buffer, len);
		if (r == -1)
			return -1;
		if (!r)
			a |= 0x200;
	}

	/* And finally we return
	 * Bit 8 : Parity error
	 * Bit 9 : Framing error
	*/

	*outbyte = a;
	fskd->state = STATE_SEARCH_STARTBIT;
	return 1;
}
