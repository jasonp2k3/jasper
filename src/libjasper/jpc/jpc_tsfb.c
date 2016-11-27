/*
 * Copyright (c) 1999-2000 Image Power, Inc. and the University of
 *   British Columbia.
 * Copyright (c) 2001-2004 Michael David Adams.
 * All rights reserved.
 */

/* __START_OF_JASPER_LICENSE__
 * 
 * JasPer License Version 2.0
 * 
 * Copyright (c) 2001-2006 Michael David Adams
 * Copyright (c) 1999-2000 Image Power, Inc.
 * Copyright (c) 1999-2000 The University of British Columbia
 * 
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person (the
 * "User") obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 * 
 * 1.  The above copyright notices and this permission notice (which
 * includes the disclaimer below) shall be included in all copies or
 * substantial portions of the Software.
 * 
 * 2.  The name of a copyright holder shall not be used to endorse or
 * promote products derived from the Software without specific prior
 * written permission.
 * 
 * THIS DISCLAIMER OF WARRANTY CONSTITUTES AN ESSENTIAL PART OF THIS
 * LICENSE.  NO USE OF THE SOFTWARE IS AUTHORIZED HEREUNDER EXCEPT UNDER
 * THIS DISCLAIMER.  THE SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS
 * "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS.  IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL
 * INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.  NO ASSURANCES ARE
 * PROVIDED BY THE COPYRIGHT HOLDERS THAT THE SOFTWARE DOES NOT INFRINGE
 * THE PATENT OR OTHER INTELLECTUAL PROPERTY RIGHTS OF ANY OTHER ENTITY.
 * EACH COPYRIGHT HOLDER DISCLAIMS ANY LIABILITY TO THE USER FOR CLAIMS
 * BROUGHT BY ANY OTHER ENTITY BASED ON INFRINGEMENT OF INTELLECTUAL
 * PROPERTY RIGHTS OR OTHERWISE.  AS A CONDITION TO EXERCISING THE RIGHTS
 * GRANTED HEREUNDER, EACH USER HEREBY ASSUMES SOLE RESPONSIBILITY TO SECURE
 * ANY OTHER INTELLECTUAL PROPERTY RIGHTS NEEDED, IF ANY.  THE SOFTWARE
 * IS NOT FAULT-TOLERANT AND IS NOT INTENDED FOR USE IN MISSION-CRITICAL
 * SYSTEMS, SUCH AS THOSE USED IN THE OPERATION OF NUCLEAR FACILITIES,
 * AIRCRAFT NAVIGATION OR COMMUNICATION SYSTEMS, AIR TRAFFIC CONTROL
 * SYSTEMS, DIRECT LIFE SUPPORT MACHINES, OR WEAPONS SYSTEMS, IN WHICH
 * THE FAILURE OF THE SOFTWARE OR SYSTEM COULD LEAD DIRECTLY TO DEATH,
 * PERSONAL INJURY, OR SEVERE PHYSICAL OR ENVIRONMENTAL DAMAGE ("HIGH
 * RISK ACTIVITIES").  THE COPYRIGHT HOLDERS SPECIFICALLY DISCLAIM ANY
 * EXPRESS OR IMPLIED WARRANTY OF FITNESS FOR HIGH RISK ACTIVITIES.
 * 
 * __END_OF_JASPER_LICENSE__
 */

/*
 * Tree-Structured Filter Bank (TSFB) Library
 *
 * $Id$
 */

/******************************************************************************\
* Includes.
\******************************************************************************/

#include <assert.h>

#include "jasper/jas_malloc.h"
#include "jasper/jas_seq.h"

#include "jpc_tsfb.h"
#include "jpc_cod.h"
#include "jpc_cs.h"
#include "jpc_util.h"
#include "jpc_math.h"
#include "jpc_fix.h"

static int jpc_tsfb_synthesize2(jpc_tsfb_t *tsfb, jpc_fix_t *a, int xstart, 
  int ystart, int width, int height, int stride, int numlvls);


/******************************************************************************\
*
\******************************************************************************/

jpc_tsfb_t *jpc_cod_gettsfb(int qmfbid, int numlvls)
{
	jpc_tsfb_t *tsfb;

	if (!(tsfb = malloc(sizeof(jpc_tsfb_t))))
		return 0;

	if (numlvls > 0) {
		switch (qmfbid) {
		case JPC_COX_INS:
			tsfb->qmfb = &jpc_ns_qmfb2d;
			break;
		default:
		case JPC_COX_RFT:
			tsfb->qmfb = &jpc_ft_qmfb2d;
			break;
		}
	} else {
		tsfb->qmfb = 0;
	}
	tsfb->numlvls = numlvls;
	return tsfb;
}

void jpc_tsfb_destroy(jpc_tsfb_t *tsfb)
{
	free(tsfb);
}

int jpc_tsfb_analyze(jpc_tsfb_t *tsfb, jas_seq2d_t *a)
{
	int xstart = jas_seq2d_xstart(a);
	int ystart = jas_seq2d_ystart(a);
	int width = jas_seq2d_width(a);
	int height = jas_seq2d_height(a);
	int stride = jas_seq2d_rowstep(a);
	jpc_fix_t *px = jas_seq2d_getref(a, xstart, ystart);
	int i;

	for (i = 0; width && height && i < tsfb->numlvls; ++i) {
		if ((*tsfb->qmfb->analyze)(px, xstart, ystart, width, height, stride))
			return -1;

		width = JPC_CEILDIVPOW2(xstart + width, 1) - 
		  JPC_CEILDIVPOW2(xstart, 1);
		height = JPC_CEILDIVPOW2(ystart + height, 1) - 
		  JPC_CEILDIVPOW2(ystart, 1);
		xstart = JPC_CEILDIVPOW2(xstart, 1);
		ystart = JPC_CEILDIVPOW2(ystart, 1);
	}
	return 0;
}

int jpc_tsfb_synthesize(jpc_tsfb_t *tsfb, jas_seq2d_t *a)
{
	return (tsfb->numlvls > 0 && jas_seq2d_size(a)) ?
	  jpc_tsfb_synthesize2(tsfb,
	  jas_seq2d_getref(a, jas_seq2d_xstart(a), jas_seq2d_ystart(a)),
	  jas_seq2d_xstart(a), jas_seq2d_ystart(a), jas_seq2d_width(a),
	  jas_seq2d_height(a), jas_seq2d_rowstep(a), tsfb->numlvls - 1) : 0;
}

static int jpc_tsfb_synthesize2(jpc_tsfb_t *tsfb, jpc_fix_t *a, int xstart, int ystart,
  int width, int height, int stride, int numlvls)
{
	if (numlvls > 0) {
		if (jpc_tsfb_synthesize2(tsfb, a, JPC_CEILDIVPOW2(xstart, 1),
		  JPC_CEILDIVPOW2(ystart, 1), JPC_CEILDIVPOW2(xstart + width,
		  1) - JPC_CEILDIVPOW2(xstart, 1), JPC_CEILDIVPOW2(ystart +
		  height, 1) - JPC_CEILDIVPOW2(ystart, 1), stride, numlvls -
		  1)) {
			return -1;
		}
	}
	if (width > 0 && height > 0) {
		if ((*tsfb->qmfb->synthesize)(a, xstart, ystart, width, height, stride)) {
			return -1;
		}
	}
	return 0;
}

int jpc_tsfb_getbands(jpc_tsfb_t *tsfb, uint_fast32_t xstart,
  uint_fast32_t ystart, uint_fast32_t xend, uint_fast32_t yend,
  jpc_tsfb_band_t *bands)
{
	int numlvls = tsfb->numlvls;
	jpc_tsfb_band_t * band = bands + 3 * numlvls;
	int cxstart = xstart;
	int cystart = ystart;
	int cxend = xend;
	int cyend = yend;
	double hpwt = 0.0;
	double lpwt = 1.0;
	int i;

	for (i = numlvls; i; --i)
	{
		int fxstart = JPC_FLOORDIVPOW2(cxstart, 1);
		int fystart = JPC_FLOORDIVPOW2(cystart, 1);
		int fxend = JPC_FLOORDIVPOW2(cxend, 1);
		int fyend = JPC_FLOORDIVPOW2(cyend, 1);

		cxstart -= fxstart;
		cystart -= fystart;
		cxend -= fxend;
		cyend -= fyend;
		hpwt = tsfb->qmfb->hpenergywts[numlvls - i];
		lpwt = tsfb->qmfb->lpenergywts[numlvls - i];

		band->xstart = fxstart;
		band->ystart = fystart;
		band->xend = fxend;
		band->yend = fyend;
		band->locxstart = xstart + cxend - cxstart;
		band->locystart = ystart + cyend - cystart;
		band->locxend = band->locxstart + band->xend - band->xstart;
		band->locyend = band->locystart + band->yend - band->ystart;
		band->orient = JPC_TSFB_HH;
		band->synenergywt = jpc_dbltofix(hpwt * hpwt);
		--band;

		band->xstart = cxstart;
		band->ystart = fystart;
		band->xend = cxend;
		band->yend = fyend;
		band->locxstart = xstart;
		band->locystart = ystart + cyend - cystart;
		band->locxend = band->locxstart + band->xend - band->xstart;
		band->locyend = band->locystart + band->yend - band->ystart;
		band->orient = JPC_TSFB_LH;
		band->synenergywt = jpc_dbltofix(lpwt * hpwt);
		--band;

		band->xstart = fxstart;
		band->ystart = cystart;
		band->xend = fxend;
		band->yend = cyend;
		band->locxstart = xstart + cxend - cxstart;
		band->locystart = ystart;
		band->locxend = band->locxstart + band->xend - band->xstart;
		band->locyend = band->locystart + band->yend - band->ystart;
		band->orient = JPC_TSFB_HL;
		band->synenergywt = jpc_dbltofix(hpwt * lpwt);
		--band;
	}

	band->xstart = cxstart;
	band->ystart = cystart;
	band->xend = cxend;
	band->yend = cyend;
	band->locxstart = xstart;
	band->locystart = ystart;
	band->locxend = band->locxstart + band->xend - band->xstart;
	band->locyend = band->locystart + band->yend - band->ystart;
	band->orient = JPC_TSFB_LL;
	band->synenergywt = jpc_dbltofix(lpwt * lpwt);
	return 3 * numlvls + 1;
}

