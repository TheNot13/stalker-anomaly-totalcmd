#pragma once
#include <vector>
#include <memory>
#include <cstring>

typedef unsigned char u8;
typedef unsigned int u32;

class LzhDecoder {
private:
    static const int N = 4096;
    static const int F = 60;
    static const int THRESHOLD = 2;
    static const int N_CHAR = (256 - THRESHOLD + F);
    static const int T = (N_CHAR * 2 - 1);
    static const int R = (T - 1);
    static const int MAX_FREQ = 0x4000;

    u8 text_buf[N + F];
    unsigned freq[T + 1];
    int prnt[T + N_CHAR + 1];
    int son[T];

    unsigned getbuf = 0;
    unsigned getlen = 0;
    const u8* in_start;
    const u8* in_end;
    const u8* in_iterator;

    u8 d_code[256] = {
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
        3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,
        6,6,6,6,6,6,6,6,7,7,7,7,7,7,7,7,8,8,8,8,8,8,8,8,9,9,9,9,9,9,9,9,
        10,10,10,10,10,10,10,10,11,11,11,11,11,11,11,11,12,12,12,12,13,13,13,13,14,14,14,14,15,15,15,15,
        16,16,16,16,17,17,17,17,18,18,18,18,19,19,19,19,20,20,20,20,21,21,21,21,22,22,22,22,23,23,23,23,
        24,24,25,25,26,26,27,27,28,28,29,29,30,30,31,31,32,32,33,33,34,34,35,35,36,36,37,37,38,38,39,39,
        40,40,41,41,42,42,43,43,44,44,45,45,46,46,47,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63
    };

    u8 d_len[256] = {
        3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
        4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
        4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
        5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
        6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
        6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8
    };

    inline int _getb() {
        if (in_iterator == in_end) return -1;
        return *in_iterator++;
    }

    inline int GetBit() {
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

    inline int GetByte() {
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

    void StartHuff() {
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

    void reconst() {
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

    void update(int c) {
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

    int DecodeChar() {
        unsigned c = son[R];
        while (c < T) {
            c += GetBit();
            c = son[c];
        }
        c -= T;
        update(c);
        return (int)c;
    }

    int DecodePosition() {
        unsigned i, j, c;
        i = GetByte();
        c = (unsigned)d_code[i] << 6;
        j = d_len[i];
        j -= 2;
        while (j--) i = (i << 1) + GetBit();
        return (int)(c | (i & 0x3f));
    }

public:
    std::vector<u8> Decode(const u8* src, u32 src_sz) {
        in_start = src;
        in_end = src + src_sz;
        in_iterator = in_start;
        getbuf = getlen = 0;

        int i, j, k, r, c;
        unsigned int count;
        
        unsigned int textsize = _getb();
        textsize |= (_getb() << 8);
        textsize |= (_getb() << 16);
        textsize |= (_getb() << 24);
        
        if (textsize == 0 || textsize == (unsigned int)-1) return {};

        std::vector<u8> out_buf;
        out_buf.reserve(textsize);

        StartHuff();
        memset(text_buf, 0x20, N - F);
        r = N - F;
        
        for (count = 0; count < textsize;) {
            c = DecodeChar();
            if (c < 256) {
                out_buf.push_back((u8)c);
                text_buf[r++] = (u8)c;
                r &= (N - 1);
                count++;
            } else {
                i = (r - DecodePosition() - 1) & (N - 1);
                j = c - 255 + THRESHOLD;
                for (k = 0; k < j; k++) {
                    c = text_buf[(i + k) & (N - 1)];
                    out_buf.push_back((u8)c);
                    text_buf[r++] = (u8)c;
                    r &= (N - 1);
                    count++;
                }
            }
        }
        return out_buf;
    }
};
