#pragma once
#include <windows.h>
#include <malloc.h>

typedef unsigned char u8;
typedef unsigned int u32;
#define IC inline
#define xr_malloc malloc
#define xr_free free
#define xr_realloc realloc

#define N 4096 
#define F 60 
#define THRESHOLD 2
#define NIL N 
#define N_CHAR (256 - THRESHOLD + F) 
#define T (N_CHAR * 2 - 1) 
#define R (T - 1) 
#define MAX_FREQ 0x4000 

static u8 text_buf[N + F];
static unsigned freq[T + 1]; 
static int prnt[T + N_CHAR + 1]; 
static int son[T]; 

static u8 d_code[256] = {
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
    0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,
    0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,
    0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,
    0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,
    0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x09,0x09,0x09,0x09,0x09,0x09,0x09,0x09,
    0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0B,0x0B,0x0B,0x0B,0x0B,0x0B,0x0B,0x0B,
    0x0C,0x0C,0x0C,0x0C,0x0D,0x0D,0x0D,0x0D,0x0E,0x0E,0x0E,0x0E,0x0F,0x0F,0x0F,0x0F,
    0x10,0x10,0x10,0x10,0x11,0x11,0x11,0x11,0x12,0x12,0x12,0x12,0x13,0x13,0x13,0x13,
    0x14,0x14,0x14,0x14,0x15,0x15,0x15,0x15,0x16,0x16,0x16,0x16,0x17,0x17,0x17,0x17,
    0x18,0x18,0x19,0x19,0x1A,0x1A,0x1B,0x1B,0x1C,0x1C,0x1D,0x1D,0x1E,0x1E,0x1F,0x1F,
    0x20,0x20,0x21,0x21,0x22,0x22,0x23,0x23,0x24,0x24,0x25,0x25,0x26,0x26,0x27,0x27,
    0x28,0x28,0x29,0x29,0x2A,0x2A,0x2B,0x2B,0x2C,0x2C,0x2D,0x2D,0x2E,0x2E,0x2F,0x2F,
    0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F
};

static u8 d_len[256] = {
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
    4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
    5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
    5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8
};

class LZfs {
public:
    unsigned getbuf = 0;
    unsigned getlen = 0;
    u8* in_start = 0;
    u8* in_end = 0;
    u8* in_iterator = 0;
    u8* out_start = 0;
    u8* out_end = 0;
    u8* out_iterator = 0;

    IC int _getb() {
        if (in_iterator == in_end) return -1;
        return *in_iterator++;
    }

    IC void _putb(int c) {
        if (out_iterator == out_end) {
            u32 out_size = u32(out_end - out_start);
            out_start = (u8*)xr_realloc(out_start, out_size + 1024);
            out_iterator = out_start + out_size;
            out_end = out_iterator + 1024;
        }
        *out_iterator++ = (u8)(c & 0xFF);
    }

    LZfs() {}

    IC void Init_Input(u8* _start, u8* _end) {
        in_start = _start; in_end = _end; in_iterator = in_start;
        getbuf = getlen = 0;
    }

    IC void Init_Output(int _rsize) {
        if (out_start) xr_free(out_start);
        out_start = (u8*)xr_malloc(_rsize);
        out_end = out_start + _rsize;
        out_iterator = out_start;
    }

    IC u32 OutSize() { return u32(out_iterator - out_start); }
    IC u8* OutPointer() { return out_start; }
    
    IC void OutRelease() {
        if (out_start) xr_free(out_start);
        out_start = out_end = out_iterator = 0;
    }

    IC int GetBit(void) {
        unsigned i;
        while (getlen <= 8) {
            if ((int)(i = _getb()) < 0) i = 0;
            getbuf |= i << (8 - getlen);
            getlen += 8;
        }
        i = getbuf;
        getbuf <<= 1;
        getlen--;
        return (int)((i & 0x8000) >> 15);
    }

    IC int GetByte(void) {
        unsigned i;
        while (getlen <= 8) {
            if ((int)(i = _getb()) < 0) i = 0;
            getbuf |= i << (8 - getlen);
            getlen += 8;
        }
        i = getbuf;
        getbuf <<= 8;
        getlen -= 8;
        return (int)((i & 0xff00) >> 8);
    }
};

static LZfs fs;

inline void StartHuff(void) {
    int i, j;
    for (i = 0; i < N_CHAR; i++) {
        freq[i] = 1;
        son[i] = i + T;
        prnt[i + T] = i;
    }
    i = 0; j = N_CHAR;
    while (j <= R) {
        freq[j] = freq[i] + freq[i + 1];
        son[j] = i;
        prnt[i] = prnt[i + 1] = j;
        i += 2; j++;
    }
    freq[T] = 0xffff;
    prnt[R] = 0;
}

inline void reconst(void) {
    int i, j, k;
    unsigned f, l;
    j = 0;
    for (i = 0; i < T; i++) {
        if (son[i] >= T) {
            freq[j] = (freq[i] + 1) / 2;
            son[j] = son[i];
            j++;
        }
    }
    for (i = 0, j = N_CHAR; j < T; i += 2, j++) {
        k = i + 1;
        f = freq[j] = freq[i] + freq[k];
        for (k = j - 1; f < freq[k]; k--);
        k++;
        l = (j - k) * sizeof(unsigned);
        memmove(&freq[k + 1], &freq[k], l);
        freq[k] = f;
        memmove(&son[k + 1], &son[k], l);
        son[k] = i;
    }
    for (i = 0; i < T; i++) {
        if ((k = son[i]) >= T) prnt[k] = i;
        else prnt[k] = prnt[k + 1] = i;
    }
}

inline void update(int c) {
    int i, j, k, l;
    if (freq[R] == MAX_FREQ) reconst();
    c = prnt[c + T];
    do {
        k = ++freq[c];
        if ((unsigned)k > freq[l = c + 1]) {
            while ((unsigned)k > freq[++l]);
            l--;
            freq[c] = freq[l];
            freq[l] = k;
            i = son[c];
            prnt[i] = l;
            if (i < T) prnt[i + 1] = l;
            j = son[l];
            son[l] = i;
            prnt[j] = c;
            if (j < T) prnt[j + 1] = c;
            son[c] = j;
            c = l;
        }
    } while ((c = prnt[c]) != 0);
}

inline int DecodeChar(void) {
    unsigned c = son[R];
    while (c < T) {
        c += fs.GetBit();
        c = son[c];
    }
    c -= T;
    update(c);
    return (int)c;
}

inline int DecodePosition(void) {
    unsigned i, j, c;
    i = fs.GetByte();
    c = (unsigned)d_code[i] << 6;
    j = d_len[i];
    j -= 2;
    while (j--) i = (i << 1) + fs.GetBit();
    return (int)(c | (i & 0x3f));
}

inline void Decode(void) {
    int i, j, k, r, c;
    unsigned int count;
    unsigned int textsize = (fs._getb());
    textsize |= (fs._getb() << 8);
    textsize |= (fs._getb() << 16);
    textsize |= (fs._getb() << 24);
    if (textsize == 0 || textsize > 100 * 1024 * 1024) return;

    fs.Init_Output(textsize);
    StartHuff();
    for (i = 0; i < N - F; i++) text_buf[i] = 0x20;
    r = N - F;
    
    for (count = 0; count < textsize;) {
        c = DecodeChar();
        if (c < 256) {
            fs._putb(c);
            text_buf[r++] = (unsigned char)c;
            r &= (N - 1);
            count++;
        } else {
            i = (r - DecodePosition() - 1) & (N - 1);
            j = c - 255 + THRESHOLD;
            for (k = 0; k < j; k++) {
                c = text_buf[(i + k) & (N - 1)];
                fs._putb(c);
                text_buf[r++] = (unsigned char)c;
                r &= (N - 1);
                count++;
            }
        }
    }
}
