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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "coder.h"
#include "huffdata.h"
#include "huff2.h"
#include "bitstream.h"


static int escape(int x, int *code)
{
    int preflen = 0;
    int base = 32;

    if (x >= 8192)
    {
        fprintf(stderr, "%s(%d): x_quant >= 8192\n", __FILE__, __LINE__);
        return 0;
    }

    *code = 0;
    while (base <= x)
    {
        base <<= 1;
        *code <<= 1;
        *code |= 1;
        preflen++;
    }
    base >>= 1;

    // separator
    *code <<= 1;

    *code <<= (preflen + 4);
    *code |= (x - base);

    return (preflen << 1) + 5;
}

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
    int data;
    int idx;
    int datacnt;

    if (coder)
        datacnt = coder->datacnt;
    else
        datacnt = 0;

    if ((bnum < 1) || (bnum > 11))
    {
        fprintf(stderr, "%s(%d) book %d out of range\n", __FILE__, __LINE__, bnum);
        return -1;
    }
    book = hmap[bnum];
    switch (bnum)
    {
    case 1:
    case 2:
        for(ofs = 0; ofs < len; ofs += 4)
        {
            qp = qs+ofs;
            idx = 27 * qp[0] + 9 * qp[1] + 3 * qp[2] + qp[3] + 40;
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
    case 3:
    case 4:
        for(ofs = 0; ofs < len; ofs += 4)
        {
            qp = qs+ofs;
            idx = 27 * abs(qp[0]) + 9 * abs(qp[1]) + 3 * abs(qp[2]) + abs(qp[3]);
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
    case 5:
    case 6:
        for(ofs = 0; ofs < len; ofs += 2)
        {
            qp = qs+ofs;
            idx = 9 * qp[0] + qp[1] + 40;
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
    case 7:
    case 8:
        for(ofs = 0; ofs < len; ofs += 2)
        {
            qp = qs+ofs;
            idx = 8 * abs(qp[0]) + abs(qp[1]);
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
    case 9:
    case 10:
        for(ofs = 0; ofs < len; ofs += 2)
        {
            qp = qs+ofs;
            idx = 13 * abs(qp[0]) + abs(qp[1]);
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
    case 11:
        for(ofs = 0; ofs < len; ofs += 2)
        {
            int x0, x1;

            qp = qs+ofs;

            x0 = abs(qp[0]);
            x1 = abs(qp[1]);
            if (x0 > 16)
                x0 = 16;
            if (x1 > 16)
                x1 = 16;
            idx = 17 * x0 + x1;

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

            if (x0 >= 16)
            {
                blen = escape(abs(qp[0]), &data);
                if (coder)
                {
                    coder->s[datacnt].data = data;
                    coder->s[datacnt++].len = blen;
                }
                bits += blen;
            }

            if (x1 >= 16)
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
    }

    if (coder)
        coder->datacnt = datacnt;

    return bits;
}


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

#define BOOKMIN(n)bookmin=n;lenmin=huffcode(qs,len,bookmin,0);if(huffcode(qs,len,bookmin+1,0)<lenmin)bookmin++;

    if (maxq < 1)
    {
        bookmin = ZERO_HCB;
        lenmin = 0;
    }
    else if (maxq < 2)
    {
        BOOKMIN(1);
    }
    else if (maxq < 3)
    {
        BOOKMIN(3);
    }
    else if (maxq < 5)
    {
        BOOKMIN(5);
    }
    else if (maxq < 8)
    {
        BOOKMIN(7);
    }
    else if (maxq < 13)
    {
        BOOKMIN(9);
    }
    else
    {
        bookmin = ESC_HCB;
        lenmin = huffcode(qs, len, bookmin, 0);
    }

    coder->book[coder->bandcnt] = bookmin;
    if (bookmin > ZERO_HCB)
        huffcode(qs, len, bookmin, coder);

    return 0;
}

int writebooks(CoderInfo *coder, BitStream *stream, int write)
{
    int cnt;
    int bookcnt;
    int bits = 0;
    int previous;
    int maxcnt, cntbits;
    int group;
    int bookbits = 4;

#ifdef DRM
    bookbits = 5; /* 5 bits in case of VCB11 */
#endif

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
        int book = coder->book[band];

        previous = book;
        bookcnt = 1;

        if (write) {
            PutBit(stream, book, bookbits);
        }
        bits += bookbits;

        for (cnt = band + 1; cnt < (band + coder->sfbn); cnt++)
        {
            book = coder->book[cnt];
#ifdef DRM
            /* sect_len is not transmitted in case the codebook for a */
            /* section is 11 or in the range of 16 and 31 */
            if ((previous == 11) ||
                ((previous >= 16) && (previous <= 32)))
            {
                if (write)
                    PutBit(stream, book, bookbits);
                bits += bookbits;
                previous = book;
                bookcnt=1;
            } else
#endif
            if (book != previous)
            {
                if (write) {
                    PutBit(stream, bookcnt, cntbits);
                }
                bits += cntbits;

                if (bookcnt >= maxcnt)
                {
                    if (write)
                        PutBit(stream, 0, cntbits);
                    bits += cntbits;
                }

                if (write)
                    PutBit(stream, book, bookbits);
                bits += bookbits;
                previous = book;
                bookcnt = 1;
                continue;
            }
            if (bookcnt >= maxcnt)
            {
                if (write) {
                    PutBit(stream, bookcnt, cntbits);
                }
                bits += cntbits;
                bookcnt = 1;
            }
            else {
                bookcnt++;
            }
        }

#ifdef DRM
        if (!((previous == 11) || ((previous >= 16) && (previous <= 32))))
#endif
        {
            if (write)
                PutBit(stream, bookcnt, cntbits);
            bits += cntbits;

            if (bookcnt >= maxcnt)
            {
                if (write)
                    PutBit(stream, 0, cntbits);
                bits += cntbits;
            }
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

    lastsf = coder->global_gain;
    lastis = 0;

    // fixme: move range check to quantizer
    for (cnt = 0; cnt < coder->bandcnt; cnt++)
    {
        int book = coder->book[cnt];

        if ((book == INTENSITY_HCB) || (book== INTENSITY_HCB2))
        {
            diff = coder->sf[cnt] - lastis;
            if (diff > 60)
                diff = 60;
            if (diff < -60)
                diff = -60;
            length = book12[60 + diff].len;

            bits += length;

            lastis += diff;

            if (write)
                PutBit(stream, book12[60 + diff].data, length);
        }
        else if (book)
        {
            diff = coder->sf[cnt] - lastsf;
            if (diff > 60)
                diff = 60;
            if (diff < -60)
                diff = -60;
            length = book12[60 + diff].len;

            bits += length;
            lastsf += diff;

            if (write)
                PutBit(stream, book12[60 + diff].data, length);
        }

    }
    return bits;
}
