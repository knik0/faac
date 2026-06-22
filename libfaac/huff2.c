/****************************************************************************
    Huffman coding

    Copyright (C) 2017 Krzysztof Nikiel

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "coder.h"
#include "huffdata.h"
#include "huff2.h"
#include "bitstream.h"
#include "util.h"

/* Escape suffix for HCB_ESC: a magnitude |q| >= LAV_ESC is sent as the pair index
 * LAV_ESC (emitted by the caller) plus this suffix - a unary prefix of `preflen`
 * ones and a zero, then the low `preflen+4` bits of x. preflen counts how far x is
 * past the 16-window, so the suffix is 2*preflen+5 bits. */
static int escape(int x, int *code)
{
    int preflen;
    int base;

    if (x > MAX_HUFF_ESC_VAL)
    {
        fprintf(stderr, "%s(%d): x_quant > %d\n", __FILE__, __LINE__, MAX_HUFF_ESC_VAL);
        return 0;
    }

    preflen = 31 - CountLeadingZeros(x) - 4;
    base = 1 << (preflen + 4);

    *code = (1 << (preflen + 1)) - 2; /* preflen 1s and a 0 */
    *code <<= (preflen + 4);
    *code |= (x - base);

    return (preflen << 1) + 5;
}

#define arrlen(array) (sizeof(array) / sizeof(*array))

/* Code `len` coeffs under book `bnum`, returning the bit count. coder == NULL only
 * sizes (the per-band cost trials in huffbook); else also emits codewords to
 * coder->s[]. Signed books fold sign into the index and emit book[idx].data
 * verbatim; magnitude books look up the |coef| index then append one sign bit per
 * nonzero coefficient. HCB_ESC additionally spills |q| >= LAV_ESC into escape(). */
static int huffcode(int *qs /* quantized spectrum */,
                    int len,
                    int bnum,
                    CoderInfo *coder)
{
    static hcode16_t * const hmap[12] = {0, book01, book02, book03, book04,
      book05, book06, book07, book08, book09, book10, book11};
    hcode16_t *book;
    int cnt;
    int bits = 0, blen;
    int ofs, *qp;
    int data = 0;
    int idx;
    int datacnt;

    if (coder)
        datacnt = coder->datacnt;
    else
        datacnt = 0;

    book = hmap[bnum];
    switch (bnum)
    {
    case HCB_1:
    case HCB_2:
        for(ofs = 0; ofs < len; ofs += 4)
        {
            qp = qs+ofs;
            idx = DIM_S4*DIM_S4*DIM_S4 * qp[0] + DIM_S4*DIM_S4 * qp[1] + DIM_S4 * qp[2] + qp[3] + 40;
            if (idx < 0 || idx >= arrlen(book01))
            {
                return -1;
            }
            blen = book[idx].len;
            if (coder)
            {
                data = book[idx].data;
                coder->s[datacnt].data = data;
                coder->s[datacnt++].len = blen;
            }
            bits += blen;
        }
        break;
    case HCB_3:
    case HCB_4:
        for(ofs = 0; ofs < len; ofs += 4)
        {
            qp = qs+ofs;
            idx = DIM_M4*DIM_M4*DIM_M4 * abs(qp[0]) + DIM_M4*DIM_M4 * abs(qp[1]) + DIM_M4 * abs(qp[2]) + abs(qp[3]);
            if (idx < 0 || idx >= arrlen(book03))
            {
                return -1;
            }
            blen = book[idx].len;
            if (!coder)
            {
                // add sign bits
                for(cnt = 0; cnt < 4; cnt++)
                    if(qp[cnt])
                        blen++;
            }
            else
            {
                data = book[idx].data;
                // add sign bits
                for(cnt = 0; cnt < 4; cnt++)
                {
                    if(qp[cnt])
                    {
                        blen++;
                        data <<= 1;
                        if (qp[cnt] < 0)
                            data |= 1;
                    }
                }
                coder->s[datacnt].data = data;
                coder->s[datacnt++].len = blen;
            }
            bits += blen;
        }
        break;
    case HCB_5:
    case HCB_6:
        for(ofs = 0; ofs < len; ofs += 2)
        {
            qp = qs+ofs;
            idx = DIM_S2 * qp[0] + qp[1] + 40;
            if (idx < 0 || idx >= arrlen(book05))
            {
                return -1;
            }
            blen = book[idx].len;
            if (coder)
            {
                data = book[idx].data;
                coder->s[datacnt].data = data;
                coder->s[datacnt++].len = blen;
            }
            bits += blen;
        }
        break;
    case HCB_7:
    case HCB_8:
        for(ofs = 0; ofs < len; ofs += 2)
        {
            qp = qs+ofs;
            idx = DIM_M2_7 * abs(qp[0]) + abs(qp[1]);
            if (idx < 0 || idx >= arrlen(book07))
            {
                return -1;
            }
            blen = book[idx].len;
            if (!coder)
            {
                for(cnt = 0; cnt < 2; cnt++)
                    if(qp[cnt])
                        blen++;
            }
            else
            {
                data = book[idx].data;
                for(cnt = 0; cnt < 2; cnt++)
                {
                    if(qp[cnt])
                    {
                        blen++;
                        data <<= 1;
                        if (qp[cnt] < 0)
                            data |= 1;
                    }
                }
                coder->s[datacnt].data = data;
                coder->s[datacnt++].len = blen;
            }
            bits += blen;
        }
        break;
    case HCB_9:
    case HCB_10:
        for(ofs = 0; ofs < len; ofs += 2)
        {
            qp = qs+ofs;
            idx = DIM_M2_12 * abs(qp[0]) + abs(qp[1]);
            if (idx < 0 || idx >= arrlen(book09))
            {
                return -1;
            }
            blen = book[idx].len;
            if (!coder)
            {
                for(cnt = 0; cnt < 2; cnt++)
                    if(qp[cnt])
                        blen++;
            }
            else
            {
                data = book[idx].data;
                for(cnt = 0; cnt < 2; cnt++)
                {
                    if(qp[cnt])
                    {
                        blen++;
                        data <<= 1;
                        if (qp[cnt] < 0)
                            data |= 1;
                    }
                }
                coder->s[datacnt].data = data;
                coder->s[datacnt++].len = blen;
            }
            bits += blen;
        }
        break;
    case HCB_ESC:
        for(ofs = 0; ofs < len; ofs += 2)
        {
            int x0, x1;

            qp = qs+ofs;

            x0 = abs(qp[0]);
            x1 = abs(qp[1]);
            if (x0 > LAV_ESC)
                x0 = LAV_ESC;
            if (x1 > LAV_ESC)
                x1 = LAV_ESC;
            idx = DIM_ESC * x0 + x1;
            if (idx < 0 || idx >= arrlen(book11))
            {
                return -1;
            }

            blen = book[idx].len;
            if (!coder)
            {
                for(cnt = 0; cnt < 2; cnt++)
                    if(qp[cnt])
                        blen++;
            }
            else
            {
                data = book[idx].data;
                for(cnt = 0; cnt < 2; cnt++)
                {
                    if(qp[cnt])
                    {
                        blen++;
                        data <<= 1;
                        if (qp[cnt] < 0)
                            data |= 1;
                    }
                }
                coder->s[datacnt].data = data;
                coder->s[datacnt++].len = blen;
            }
            bits += blen;

            if (x0 >= LAV_ESC)
            {
                blen = escape(abs(qp[0]), &data);
                if (coder)
                {
                    coder->s[datacnt].data = data;
                    coder->s[datacnt++].len = blen;
                }
                bits += blen;
            }

            if (x1 >= LAV_ESC)
            {
                blen = escape(abs(qp[1]), &data);
                if (coder)
                {
                    coder->s[datacnt].data = data;
                    coder->s[datacnt++].len = blen;
                }
                bits += blen;
            }
        }
        break;
    default:
        fprintf(stderr, "%s(%d) book %d out of range\n", __FILE__, __LINE__, bnum);
        return -1;
    }

    if (coder)
        coder->datacnt = datacnt;

    return bits;
}


/* Per-band codebook selection: scan |maxq| to pick the lowest range-pair that can
 * represent it, keep whichever book of the pair {base, base+1} codes the band
 * shorter, emit it, and record the choice in coder->book[]. */
int huffbook(CoderInfo *coder,
             int *qs /* quantized spectrum */,
             int len)
{
    int cnt;
    int maxq = 0;
    int bookmin, lenmin;

    for (cnt = 0; cnt < len; cnt++)
    {
        int q = abs(qs[cnt]);
        if (maxq < q)
            maxq = q;
    }

    /* Size the band under both books of the covering pair; keep the cheaper. */
#define BOOKMIN(n)bookmin=n;lenmin=huffcode(qs,len,bookmin,0);if(huffcode(qs,len,bookmin+1,0)<lenmin)bookmin++;

    if (maxq < 1)
    {
        bookmin = HCB_ZERO;
        lenmin = 0;
    }
    else if (maxq < LAV_1 + 1)
    {
        BOOKMIN(HCB_1);
    }
    else if (maxq < LAV_2 + 1)
    {
        BOOKMIN(HCB_3);
    }
    else if (maxq < LAV_4 + 1)
    {
        BOOKMIN(HCB_5);
    }
    else if (maxq < LAV_7 + 1)
    {
        BOOKMIN(HCB_7);
    }
    else if (maxq < LAV_12 + 1)
    {
        BOOKMIN(HCB_9);
    }
    else
    {
        bookmin = HCB_ESC;
    }

    if (bookmin > HCB_ZERO)
        huffcode(qs, len, bookmin, coder);
    coder->book[coder->bandcnt] = bookmin;

    return 0;
}

/* Write (or size) the section layout: a 4-bit book + run length per maximal run of
 * equal adjacent books. The count field is cntbits wide (5 long / 3 short), so a
 * run past maxcnt splits into repeated full sections under the same book. */
int writebooks(CoderInfo *coder, BitStream *stream, int write)
{
    int bits = 0;
    int maxcnt, cntbits;
    int group;
    int bookbits = 4;

    if (coder->block_type == ONLY_SHORT_WINDOW){
        maxcnt = 7;
        cntbits = 3;
    } else {
        maxcnt = 31;
        cntbits = 5;
    }

    for (group = 0; group < coder->groups.n; group++)
    {
        int band = group * coder->sfbn;
        int maxband = band + coder->sfbn;

        while (band < maxband)
        {
            int book = coder->book[band++];
            int bookcnt = 1;
            if (write) {
                PutBit(stream, book, bookbits);
            }
            bits += bookbits;

            if (band < maxband)
            {
                while (book == coder->book[band])
                {
                    band++;
                    bookcnt++;
                    if (band >= maxband)
                        break;
                }
            }

            while (bookcnt >= maxcnt)
            {
                if (write)
                    PutBit(stream, maxcnt, cntbits);
                bits += cntbits;
                bookcnt -= maxcnt;
            }
            if (write)
                PutBit(stream, bookcnt, cntbits);
            bits += cntbits;
        }
    }

    return bits;
}

int writesf(CoderInfo *coder, BitStream *stream, int write)
{
    int cnt;
    int bits = 0;
    int diff, length;
    int lastsf;
    int lastis;
    int lastpns;
    int initpns = 1;

    /* Three independent DPCM chains share HCB_DELTA, each seeded so its first delta
     * is self-contained: intensity positions from 0, scalefactors from global_gain
     * (itself the first regular sf), PNS energies from global_gain - SF_PNS_OFFSET.
     * The decoder rebuilds each by the same running sum, so deltas match. */
    lastsf = coder->global_gain;
    lastis = 0;
    lastpns = coder->global_gain - SF_PNS_OFFSET;

    /* qlevel() bounds every stored sf[] and global_gain to [0, SF_MAX_ABS], so
     * the running reconstruction below cannot leave the decoder's range. */
    for (cnt = 0; cnt < coder->bandcnt; cnt++)
    {
        int book = coder->book[cnt];

        if ((book == HCB_INTENSITY) || (book== HCB_INTENSITY2))
        {
            diff = coder->sf[cnt] - lastis;
            diff = clamp_sf_diff(diff);
            length = book12[SF_DELTA + diff].len;

            bits += length;

            lastis += diff;

            if (write)
                PutBit(stream, book12[SF_DELTA + diff].data, length);
        }
        else if (book == HCB_PNS)
        {
            diff = coder->sf[cnt] - lastpns;

            if (initpns)
            {
                /* First PNS band has no prior energy to delta against, so the spec
                 * sends a raw 9-bit value (diff + 256) instead of an HCB_DELTA code;
                 * later PNS bands delta off this one. */
                initpns = 0;

                length = 9;
                bits += length;
                lastpns += diff;

                if (write)
                    PutBit(stream, diff + 256, length);
                continue;
            }

            diff = clamp_sf_diff(diff);

            length = book12[SF_DELTA + diff].len;
            bits += length;
            lastpns += diff;

            if (write)
                PutBit(stream, book12[SF_DELTA + diff].data, length);
        }
        else if ((book != HCB_ZERO) && (book != HCB_NONE))
        {
            diff = coder->sf[cnt] - lastsf;
            diff = clamp_sf_diff(diff);
            length = book12[SF_DELTA + diff].len;

            bits += length;
            lastsf += diff;

            if (write)
                PutBit(stream, book12[SF_DELTA + diff].data, length);
        }

    }
    return bits;
}
