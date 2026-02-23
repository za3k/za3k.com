#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
typedef struct {
    float x, y;
} Point2D;
typedef struct {
    float h[3][3];  /* h[2][2] is typically normalized to 1 */
} HomographyMatrix;
typedef struct {
    int version, size;
    float module_size;
    HomographyMatrix homography;
} QRTransform;
#define MAX_QR_VERSION    40
#define MAX_QR_MODULES    (17 + 4 * MAX_QR_VERSION)  /* 177 for version 40 */
#define MAX_CHUNKS        1024
#define MAX_CHUNK_SIZE    4096
#define MAX_DATA_SIZE     (MAX_CHUNKS * MAX_CHUNK_SIZE)
typedef struct {
    int width;
    int height;
    uint8_t *data;  /* 1 = black, 0 = white */
} Image;
typedef struct {
    char type;      /* 'N' for normal, 'P' for parity */
    int index;      /* chunk number (1-based) */
    int total;      /* total chunks of this type */
    uint8_t *data;  /* decoded data */
    int data_len;
} Chunk;
typedef struct {
    Chunk *chunks;
    int count;
    int capacity;
} ChunkList;
static int skip_ws(FILE *f) {
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (c == '#') while ((c = fgetc(f)) != EOF && c != '\n');
        else if (c > ' ') { ungetc(c, f); return 1; }
    }
    return 0;
}
static void adaptive_threshold(uint8_t *gray, uint8_t *binary,
                               int width, int height) {
    int bs = 32;  /* Block size */
    for (int by = 0; by < height; by += bs) {
        for (int bx = 0; bx < width; bx += bs) {
            /* Compute mean over 2x block for smoothing */
            int bw = (bx + bs * 2 < width) ? bs * 2 : width - bx;
            int bh = (by + bs * 2 < height) ? bs * 2 : height - by;
            int sum = 0;
            for (int y = by; y < by + bh; y++)
                for (int x = bx; x < bx + bw; x++) sum += gray[y * width + x];
            int thresh = sum / (bw * bh) - 10;
            if (thresh < 20) thresh = 20;
            /* Apply to 1x block */
            int ew = (bx + bs < width) ? bs : width - bx;
            int eh = (by + bs < height) ? bs : height - by;
            for (int y = by; y < by + eh; y++)
                for (int x = bx; x < bx + ew; x++) {
                    int idx = y * width + x;
                    binary[idx] = (gray[idx] < thresh) ? 1 : 0;
                }
        }
    }
}
static Image *image_read_pbm(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return NULL;
    Image *img = calloc(1, sizeof(Image));
    uint8_t *gray = NULL;
    char magic[3] = {0};
    if (fread(magic, 1, 2, f) != 2) goto fail;
    int fmt = 0;
    if (magic[1] == '1') fmt = 1; else if (magic[1] == '4') fmt = 4;
    else if (magic[1] == '2') fmt = 2; else if (magic[1] == '5') fmt = 5;
    if (magic[0] != 'P' || !fmt) goto fail;
    if (!skip_ws(f) || fscanf(f, "%d", &img->width) != 1) goto fail;
    if (!skip_ws(f) || fscanf(f, "%d", &img->height) != 1) goto fail;
    int maxval = 1;
    if (fmt == 2 || fmt == 5) {
        if (!skip_ws(f) || fscanf(f, "%d", &maxval) != 1) goto fail;
        if (maxval <= 0) maxval = 255;
    }
    fgetc(f);
    int n = img->width * img->height;
    img->data = malloc(n);
    if (fmt == 4) {
        int rb = (img->width + 7) / 8;
        uint8_t *row = malloc(rb);
        for (int y = 0; y < img->height; y++) {
            if (fread(row, 1, rb, f) != (size_t)rb) { free(row); goto fail; }
            for (int x = 0; x < img->width; x++)
                img->data[y * img->width + x] = (row[x / 8] >> (7 - x % 8)) & 1;
        }
        free(row);
    } else if (fmt == 1) {
        for (int i = 0; i < n; i++) {
            int c; while ((c = fgetc(f)) != EOF && c <= ' ');
            if (c == EOF) goto fail;
            img->data[i] = (c == '1') ? 1 : 0;
        }
    } else {  /* fmt == 2 or 5: grayscale */
        gray = malloc(n);
        if (fmt == 5) {
            if (fread(gray, 1, n, f) != (size_t)n) goto fail;
        } else {
            for (int i = 0; i < n; i++) {
                int val; if (fscanf(f, "%d", &val) != 1) goto fail;
                gray[i] = val;
            }
        }
        if (maxval != 255)
            for (int i = 0; i < n; i++) gray[i] = gray[i] * 255 / maxval;
        adaptive_threshold(gray, img->data, img->width, img->height);
        free(gray); gray = NULL;
    }
    fclose(f);
    return img;
fail:
    free(gray); if (img) free(img->data); free(img); fclose(f); return NULL;
}
static void image_free(Image *img) { if (img) { free(img->data); free(img); } }
static int image_get(Image *img, int x, int y) {
    if (x < 0 || x >= img->width || y < 0 || y >= img->height) return 0;
    return img->data[y * img->width + x];
}
typedef struct {
    int x, y;             /* Center position */
    float module_size;    /* Estimated module size (geometric mean of x/y) */
    float module_size_x;  /* Horizontal module size (for non-uniform scaling) */
    float module_size_y;  /* Vertical module size (for non-uniform scaling) */
} FinderPattern;
typedef struct {
    int pos;        /* Position of scan line (y for horiz, x for vert) */
    int bound_min;  /* Start of pattern on scan line */
    int bound_max;  /* End of pattern on scan line */
} FinderLine;
typedef struct {
    int pos_min, pos_max;       /* Range of scan positions */
    int center_min, center_max; /* Min/max of centers */
    int center_sum;             /* Sum of centers for averaging */
    int count;                  /* Number of lines in this range */
    float module_sum;           /* Sum of module sizes for averaging */
} FinderRange;
#define MAX_FINDER_LINES 16000
#define INITIAL_FINDER_RANGES 128
static float check_finder_ratio_ex(int *counts, float tolerance) {
    int total = counts[0] + counts[1] + counts[2] + counts[3] + counts[4];
    if (total < 7) return 0;
    if (!counts[0] || !counts[1] || !counts[2] || !counts[3] || !counts[4])
        return 0;
    float module = total / 7.0f, var = module * tolerance * 1.2f;
    float center_ratio = (float)counts[2] / total;
    if (center_ratio < 0.25f || center_ratio > 0.55f) return 0;
    for (int i = 0; i < 5; i += (i == 1 ? 2 : 1))  /* Check 0,1,3,4 (sides) */
        if (counts[i] < module - var || counts[i] > module + var) return 0;
    if (counts[2] < 2.0f * module || counts[2] > 4.0f * module) return 0;
    return module;
}
static int add_finder_line(FinderLine *lines, int count, int max,
                           int pos, int bmin, int bmax) {
    if (count < max) {
        lines[count].pos = pos;
        lines[count].bound_min = bmin;
        lines[count].bound_max = bmax;
    }
    return count < max ? count + 1 : count;
}
static int ranges_overlap(int a0, int a1, int b0, int b1) {
    int ov = ((a1 < b1 ? a1 : b1) - (a0 > b0 ? a0 : b0));
    int smaller = ((a1-a0) < (b1-b0)) ? (a1-a0) : (b1-b0);
    return ov >= 0 && ov >= smaller / 2;
}
static int cluster_finder_lines(FinderLine *lines, int nlines,
                                  FinderRange **ranges_ptr, int *capacity_ptr) {
    if (nlines == 0) return 0;
    FinderRange *ranges = *ranges_ptr;
    int capacity = *capacity_ptr;
    int nranges = 0;
    typedef struct {
        int range_idx;      /* Index into ranges array */
        int last_bound_min; /* Last line's bound_min */
        int last_bound_max; /* Last line's bound_max */
        int last_pos;       /* Position where this cluster was last updated */
    } ActiveCluster;
    #define MAX_GAP 3
    ActiveCluster active[128];
    int nactive = 0;
    int last_pos = -999;  /* Position of last processed line */
    for (int i = 0; i < nlines; i++) {
        FinderLine *line = &lines[i];
        if (line->pos != last_pos) {
            int write_idx = 0;
            for (int a = 0; a < nactive; a++) {
                if (line->pos - active[a].last_pos <= MAX_GAP + 1) {
                    if (write_idx != a) {
                        active[write_idx] = active[a];
                    }
                    write_idx++;
                }
            }
            nactive = write_idx;
            last_pos = line->pos;
        }
        int matched = -1;
        int line_center = (line->bound_min + line->bound_max) / 2;
        for (int a = 0; a < nactive; a++) {
            if (ranges_overlap(line->bound_min, line->bound_max,
                    active[a].last_bound_min, active[a].last_bound_max)) {
                int gap = line->pos - active[a].last_pos;
                if (gap > 1) {
                    FinderRange *r = &ranges[active[a].range_idx];
                    int cluster_center_min = r->center_min;
                    int cluster_center_max = r->center_max;
                    if (line_center < cluster_center_min - 10 ||
                        line_center > cluster_center_max + 10) {
                        continue;  /* Line center too far from cluster */
                    }
                }
                matched = a;
                break;
            }
        }
        int ri, ai;
        if (matched >= 0) {
            ri = active[matched].range_idx; ai = matched;
        } else if (nactive < 128) {
            if (nranges >= capacity) {
                capacity *= 2;
                ranges = realloc(ranges, capacity * sizeof(FinderRange));
                *ranges_ptr = ranges; *capacity_ptr = capacity;
            }
            ri = nranges++; ai = nactive++;
            active[ai].range_idx = ri;
            ranges[ri].pos_min = line->pos;
            ranges[ri].center_min = line->bound_max;
            ranges[ri].center_max = line->bound_min;
            ranges[ri].center_sum = 0;
            ranges[ri].module_sum = 0;
            ranges[ri].count = 0;
        } else continue;
        FinderRange *r = &ranges[ri];
        r->pos_max = line->pos;
        if (line->bound_min < r->center_min) r->center_min = line->bound_min;
        if (line->bound_max > r->center_max) r->center_max = line->bound_max;
        r->center_sum += (line->bound_min + line->bound_max) / 2;
        r->module_sum += (line->bound_max - line->bound_min) / 7.0f;
        r->count++;
        active[ai].last_bound_min = line->bound_min;
        active[ai].last_bound_max = line->bound_max;
        active[ai].last_pos = line->pos;
    }
    int valid = 0;
    for (int i = 0; i < nranges; i++) {
        FinderRange *r = &ranges[i];
        int min_lines = (int)(r->module_sum / r->count * 1.4f);
        if (min_lines < 3) min_lines = 3;
        if (r->count >= min_lines && r->pos_max - r->pos_min >= 4)
            ranges[valid++] = *r;
    }
    return valid;
}
static int compare_finder_lines_by_pos(const void *a, const void *b) {
    const FinderLine *la = (const FinderLine *)a;
    const FinderLine *lb = (const FinderLine *)b;
    return la->pos - lb->pos;
}
static int scan_finder_lines(Image *img, FinderLine *lines, int vert) {
    int outer = vert ? img->width : img->height;
    int inner = vert ? img->height : img->width, nlines = 0;
    for (int o = 0; o < outer; o++) {
        int counts[5] = {0}, state = 0;
        for (int i = 0; i < inner; i++) {
            int pixel = vert ? image_get(img, o, i) : image_get(img, i, o);
            int expected = (state & 1);  /* 1,3,5=black, 0,2,4=white */
            if (state == 0) { if (pixel) { state = 1; counts[0] = 1; } }
            else if (pixel == expected) { counts[state - 1]++; }
            else if (state < 5) { state++; counts[state - 1] = 1; }
            else {  /* state == 5, got white */
                if (check_finder_ratio_ex(counts, 0.8f) > 0) {
                    int w = counts[0]+counts[1]+counts[2]+counts[3]+counts[4];
                    nlines = add_finder_line(lines, nlines,
                        MAX_FINDER_LINES, o, i - w, i - 1);
                }
                counts[0] = counts[2]; counts[1] = counts[3];
                counts[2] = counts[4]; counts[3] = 1; counts[4] = 0;
                state = 4;
            }
        }
    }
    return nlines;
}
static int find_finder_patterns(Image *img, FinderPattern *patterns,
                                int max_patterns) {
    FinderLine *h_lines = malloc(MAX_FINDER_LINES * sizeof(FinderLine));
    FinderLine *v_lines = malloc(MAX_FINDER_LINES * sizeof(FinderLine));
    int hrc = INITIAL_FINDER_RANGES, vrc = INITIAL_FINDER_RANGES;
    FinderRange *h_ranges = malloc(hrc * sizeof(FinderRange));
    FinderRange *v_ranges = malloc(vrc * sizeof(FinderRange));
    int nhl = scan_finder_lines(img, h_lines, 0);
    int nvl = scan_finder_lines(img, v_lines, 1);
    qsort(h_lines, nhl, sizeof(FinderLine), compare_finder_lines_by_pos);
    qsort(v_lines, nvl, sizeof(FinderLine), compare_finder_lines_by_pos);
    int nhr = cluster_finder_lines(h_lines, nhl, &h_ranges, &hrc);
    int nvr = cluster_finder_lines(v_lines, nvl, &v_ranges, &vrc);
    int count = 0;
    for (int hi = 0; hi < nhr && count < max_patterns; hi++) {
        FinderRange *hr = &h_ranges[hi];
        int fx = hr->center_sum / hr->count;
        float hm = hr->module_sum / hr->count;
        for (int vi = 0; vi < nvr && count < max_patterns; vi++) {
            FinderRange *vr = &v_ranges[vi];
            int fy = vr->center_sum / vr->count;
            float vm = vr->module_sum / vr->count;
            if (fx < vr->pos_min - hm*1.5f || fx > vr->pos_max + hm*1.5f)
                continue;
            if (fy < hr->pos_min - vm*1.5f || fy > hr->pos_max + vm*1.5f)
                continue;
            /* Check module size ratio */
            float ratio = (hm > vm) ? hm / vm : vm / hm;
            if (ratio > ((hm > 3.0f || vm > 3.0f) ? 3.0f : 2.5f)) continue;
            /* Check for duplicates */
            float fm = sqrtf(hm * vm);
            int is_dup = 0;
            for (int i = 0; i < count && !is_dup; i++) {
                int dx = patterns[i].x - fx, dy = patterns[i].y - fy;
                is_dup = dx*dx + dy*dy < fm * fm * 16;
            }
            if (!is_dup) {
                patterns[count].x = fx;
                patterns[count].y = fy;
                patterns[count].module_size = fm;
                patterns[count].module_size_x = hm;
                patterns[count].module_size_y = vm;
                count++;
            }
        }
    }
    free(h_lines);
    free(v_lines);
    free(h_ranges);
    free(v_ranges);
    return count;
}
static int qr_version_to_size(int v) { return 17 + 4 * v; }
typedef struct {
    uint8_t modules[MAX_QR_MODULES][MAX_QR_MODULES];
    int size;
    int version;
} QRCode;
static void get_alignment_positions(int version, int pos[8]) {
    if (version == 1) { pos[0] = 0; return; }
    int size = 17 + version * 4, last = size - 7, num = version / 7 + 2;
    int total = last - 6, iv = num - 1;
    int step = ((total * 2 + iv) / (iv * 2) + 1) & ~1;
    pos[0] = 6;
    for (int i = num - 1; i >= 1; i--) pos[i] = last - (num - 1 - i) * step;
    pos[num] = 0;
}
static int is_function_module(int version, int x, int y) {
    int s = qr_version_to_size(version);
    /* Finder patterns and format info */
    if ((x < 9 && y < 9) || (x < 8 && y >= s-8) || (x >= s-8 && y < 9))
        return 1;
    if ((y == 8 && (x < 9 || x >= s-8)) || (x == 8 && (y < 9 || y >= s-8)))
        return 1;
    if (x == 6 || y == 6) return 1;  /* Timing patterns */
    /* Alignment patterns */
    if (version >= 2) {
        int pos[8]; get_alignment_positions(version, pos);
        for (int i = 0; pos[i]; i++)
            for (int j = 0; pos[j]; j++) {
                int ax = pos[i], ay = pos[j];
                if ((ax < 9 && ay < 9) || (ax < 9 && ay >= s-8) ||
                    (ax >= s-8 && ay < 9)) continue;
                if (x >= ax-2 && x <= ax+2 && y >= ay-2 && y <= ay+2) return 1;
            }
    }
    /* Version info */
    if (version >= 7) {
        if ((x < 6 && y >= s-11 && y < s-8) ||
            (y < 6 && x >= s-11 && x < s-8)) return 1;
    }
    return 0;
}
static int sample_module_transform_ex(Image *img, const QRTransform *transform,
                                      int mx, int my, int use_aa);
static int read_format_info(QRCode *qr, Image *img,
                            const QRTransform *transform) {
    static const int xs[15] = {8, 8, 8, 8, 8, 8, 8, 8, 7, 5, 4, 3, 2, 1, 0};
    static const int ys[15] = {0, 1, 2, 3, 4, 5, 7, 8, 8, 8, 8, 8, 8, 8, 8};
    uint32_t format1 = 0;
    for (int i = 14; i >= 0; i--) {
        int bit;
        if (img) bit = sample_module_transform_ex(img, transform,
                                                  xs[i], ys[i], 0);
        else bit = qr->modules[ys[i]][xs[i]] & 1;
        format1 = (format1 << 1) | bit;
    }
    return format1 ^ 0x5412;
}
static const uint16_t valid_format_unmasked[32] = {
    0x0000, 0x0537, 0x0A6E, 0x0F59, 0x11EB, 0x14DC, 0x1B85, 0x1EB2,
    0x23D6, 0x26E1, 0x29B8, 0x2C8F, 0x323D, 0x370A, 0x3853, 0x3D64,
    0x429B, 0x47AC, 0x48F5, 0x4DC2, 0x5370, 0x5647, 0x591E, 0x5C29,
    0x614D, 0x647A, 0x6B23, 0x6E14, 0x70A6, 0x7591, 0x7AC8, 0x7FFF
};
static int validate_format_info_with_correction(int format_info) {
    int remainder = format_info;
    for (int i = 14; i >= 10; i--) {
        if (remainder & (1 << i))
            remainder ^= (0x537 << (i - 10));
    }
    if (remainder == 0) return format_info;  /* Already valid */
    int best_match = -1, best_distance = 5;  /* Accept distance <= 4 */
    for (int i = 0; i < 32; i++) {
        int diff = format_info ^ valid_format_unmasked[i];
        int distance = 0;
        while (diff) {
            distance += diff & 1;
            diff >>= 1;
        }
        if (distance < best_distance) {
            best_distance = distance;
            best_match = valid_format_unmasked[i];
        }
    }
    return best_match;  /* Returns -1 if no match within distance 3 */
}
static uint8_t gf_exp[512], gf_log[256];
static void gf_init(void) {
    static int done = 0; if (done) return; done = 1;
    int x = 1;
    for (int i = 0; i < 255; i++) {
        gf_exp[i] = x; gf_log[x] = i;
        x = (x << 1) ^ ((x >> 7) * 0x11d);
    }
    for (int i = 255; i < 512; i++) gf_exp[i] = gf_exp[i - 255];
}
static uint8_t gf_mul(uint8_t a, uint8_t b) {
    return (a && b) ? gf_exp[gf_log[a] + gf_log[b]] : 0;
}
static uint8_t gf_div(uint8_t a, uint8_t b) {
    return (a && b) ? gf_exp[(gf_log[a] + 255 - gf_log[b]) % 255] : 0;
}
static uint8_t gf_inv(uint8_t a) {
    return a ? gf_exp[255 - gf_log[a]] : 0;
}
static void rs_calc_syndromes(uint8_t *data, int total_len, int ecc_len,
                              uint8_t *syndromes) {
    gf_init();
    for (int i = 0; i < ecc_len; i++) {
        uint8_t s = 0;
        for (int j = 0; j < total_len; j++) {
            s = gf_mul(s, gf_exp[i]) ^ data[j];
        }
        syndromes[i] = s;
    }
}
static int rs_berlekamp_massey(uint8_t *syndromes, int ecc_len,
                               uint8_t *sigma) {
    gf_init();
    uint8_t C[256] = {0};  /* Connection polynomial */
    uint8_t B[256] = {0};  /* Previous connection polynomial */
    C[0] = 1;
    B[0] = 1;
    int L = 0, m = 1;  /* L=LFSR len, m=iterations since L update */
    uint8_t b = 1;  /* Previous discrepancy */
    for (int n = 0; n < ecc_len; n++) {
        uint8_t d = syndromes[n];
        for (int i = 1; i <= L; i++) {
            d ^= gf_mul(C[i], syndromes[n - i]);
        }
        if (d == 0) {
            m++;
        } else if (2 * L <= n) {
            uint8_t T[256];
            memcpy(T, C, sizeof(T));
            uint8_t coef = gf_div(d, b);
            for (int i = 0; i < ecc_len - m; i++) {
                C[i + m] ^= gf_mul(coef, B[i]);
            }
            memcpy(B, T, sizeof(B));
            L = n + 1 - L;
            b = d;
            m = 1;
        } else {
            uint8_t coef = gf_div(d, b);
            for (int i = 0; i < ecc_len - m; i++) {
                C[i + m] ^= gf_mul(coef, B[i]);
            }
            m++;
        }
    }
    memcpy(sigma, C, ecc_len + 1);
    return L;
}
static int rs_chien_search(uint8_t *sigma, int num_errors, int n,
                           int *positions) {
    gf_init();
    int found = 0;
    for (int i = 0; i < n; i++) {
        uint8_t sum = sigma[0];  /* = 1 */
        for (int j = 1; j <= num_errors; j++) {
            uint8_t power = ((255 - i) * j) % 255;
            sum ^= gf_mul(sigma[j], gf_exp[power]);
        }
        if (sum == 0)
            positions[found++] = n - 1 - i;
    }
    return (found == num_errors) ? found : -1;
}
static void rs_forney(uint8_t *syndromes, uint8_t *sigma, int num_errors,
                      int *positions, int n, uint8_t *values) {
    gf_init();
    uint8_t omega[256] = {0};
    for (int i = 0; i < num_errors; i++) {
        uint8_t v = 0;
        for (int j = 0; j <= i; j++) {
            v ^= gf_mul(syndromes[i - j], sigma[j]);
        }
        omega[i] = v;
    }
    uint8_t sigma_prime[256] = {0};
    for (int i = 1; i <= num_errors; i += 2) {
        sigma_prime[i - 1] = sigma[i];
    }
    for (int i = 0; i < num_errors; i++) {
        int pos = positions[i];
        int Xi_inv_power = (n - 1 - pos) % 255;  /* Power of alpha for X_i^-1 */
        uint8_t omega_val = 0;
        for (int j = 0; j < num_errors; j++) {
            omega_val ^= gf_mul(omega[j], gf_exp[(Xi_inv_power * j) % 255]);
        }
        uint8_t sigma_prime_val = 0;
        for (int j = 0; j < num_errors; j++) {
            int p = (Xi_inv_power * j) % 255;
            sigma_prime_val ^= gf_mul(sigma_prime[j], gf_exp[p]);
        }
        if (sigma_prime_val != 0) {
            values[i] = gf_div(omega_val, sigma_prime_val);
        } else {
            values[i] = 0;
        }
    }
}
static int rs_correct_errors(uint8_t *data, int data_len, int ecc_len) {
    gf_init();
    int total_len = data_len + ecc_len, max_errors = ecc_len / 2;
    uint8_t syndromes[256];
    rs_calc_syndromes(data, total_len, ecc_len, syndromes);
    int all_zero = 1;
    for (int i = 0; i < ecc_len; i++)
        if (syndromes[i] != 0) { all_zero = 0; break; }
    if (all_zero) return 0;  /* No errors */
    uint8_t sigma[256] = {0};
    int num_errors = rs_berlekamp_massey(syndromes, ecc_len, sigma);
    if (num_errors > max_errors) return -1;
    int positions[256];
    int found = rs_chien_search(sigma, num_errors, total_len, positions);
    if (found != num_errors) return -1;
    uint8_t values[256];
    rs_forney(syndromes, sigma, num_errors, positions, total_len, values);
    for (int i = 0; i < num_errors; i++) {
        if (positions[i] >= 0 && positions[i] < total_len)
            data[positions[i]] ^= values[i];
    }
    rs_calc_syndromes(data, total_len, ecc_len, syndromes);
    for (int i = 0; i < ecc_len; i++)
        if (syndromes[i] != 0) return -1;
    return num_errors;
}
static int mask_bit(int m, int x, int y) {
    switch (m) {
        case 0: return (y+x) % 2 == 0;     case 1: return y % 2 == 0;
        case 2: return x % 3 == 0;          case 3: return (y+x) % 3 == 0;
        case 4: return (y/2 + x/3) % 2 == 0;
        case 5: return (y*x) % 2 + (y*x) % 3 == 0;
        case 6: return ((y*x) % 2 + (y*x) % 3) % 2 == 0;
        default: return ((y+x) % 2 + (y*x) % 3) % 2 == 0;
    }
}
static void unmask_qr(QRCode *qr, int mask) {
    for (int y = 0; y < qr->size; y++)
        for (int x = 0; x < qr->size; x++)
            if (!is_function_module(qr->version, x, y) && mask_bit(mask, x, y))
                qr->modules[y][x] ^= 1;
}
typedef struct { int bs, dw, ns; } RSBlockParams;
typedef struct { int data_bytes; RSBlockParams ecc[4]; } VersionInfo;
static const VersionInfo version_info[41] = {{0},
{26,{{26,16,1},{26,19,1},{26,9,1},{26,13,1}}},
{44,{{44,28,1},{44,34,1},{44,16,1},{44,22,1}}},
{70,{{70,44,1},{70,55,1},{35,13,2},{35,17,2}}},
{100,{{50,32,2},{100,80,1},{25,9,4},{50,24,2}}},
{134,{{67,43,2},{134,108,1},{33,11,2},{33,15,2}}},
{172,{{43,27,4},{86,68,2},{43,15,4},{43,19,4}}},
{196,{{49,31,4},{98,78,2},{39,13,4},{32,14,2}}},
{242,{{60,38,2},{121,97,2},{40,14,4},{40,18,4}}},
{292,{{58,36,3},{146,116,2},{36,12,4},{36,16,4}}},
{346,{{69,43,4},{86,68,2},{43,15,6},{43,19,6}}},
{404,{{80,50,1},{101,81,4},{36,12,3},{50,22,4}}},
{466,{{58,36,6},{116,92,2},{42,14,7},{46,20,4}}},
{532,{{59,37,8},{133,107,4},{33,11,12},{44,20,8}}},
{581,{{64,40,4},{145,115,3},{36,12,11},{36,16,11}}},
{655,{{65,41,5},{109,87,5},{36,12,11},{54,24,5}}},
{733,{{73,45,7},{122,98,5},{45,15,3},{43,19,15}}},
{815,{{74,46,10},{135,107,1},{42,14,2},{50,22,1}}},
{901,{{69,43,9},{150,120,5},{42,14,2},{50,22,17}}},
{991,{{70,44,3},{141,113,3},{39,13,9},{47,21,17}}},
{1085,{{67,41,3},{135,107,3},{43,15,15},{54,24,15}}},
{1156,{{68,42,17},{144,116,4},{46,16,19},{50,22,17}}},
{1258,{{74,46,17},{139,111,2},{37,13,34},{54,24,7}}},
{1364,{{75,47,4},{151,121,4},{45,15,16},{54,24,11}}},
{1474,{{73,45,6},{147,117,6},{46,16,30},{54,24,11}}},
{1588,{{75,47,8},{132,106,8},{45,15,22},{54,24,7}}},
{1706,{{74,46,19},{142,114,10},{46,16,33},{50,22,28}}},
{1828,{{73,45,22},{152,122,8},{45,15,12},{53,23,8}}},
{1921,{{73,45,3},{147,117,3},{45,15,11},{54,24,4}}},
{2051,{{73,45,21},{146,116,7},{45,15,19},{53,23,1}}},
{2185,{{75,47,19},{145,115,5},{45,15,23},{54,24,15}}},
{2323,{{74,46,2},{145,115,13},{45,15,23},{54,24,42}}},
{2465,{{74,46,10},{145,115,17},{45,15,19},{54,24,10}}},
{2611,{{74,46,14},{145,115,17},{45,15,11},{54,24,29}}},
{2761,{{74,46,14},{145,115,13},{46,16,59},{54,24,44}}},
{2876,{{75,47,12},{151,121,12},{45,15,22},{54,24,39}}},
{3034,{{75,47,6},{151,121,6},{45,15,2},{54,24,46}}},
{3196,{{74,46,29},{152,122,17},{45,15,24},{54,24,49}}},
{3362,{{74,46,13},{152,122,4},{45,15,42},{54,24,48}}},
{3532,{{75,47,40},{147,117,20},{45,15,10},{54,24,43}}},
{3706,{{75,47,18},{148,118,19},{45,15,20},{54,24,34}}}};
static int deinterleave_block(uint8_t *raw, int total_bytes, int bc,
                              int dw, int bs, int ns, int block_idx,
                              uint8_t *block_out) {
    int is_long = (block_idx >= ns);
    int block_dw = is_long ? dw + 1 : dw;
    int block_ecc = bs - dw;
    int block_bs = is_long ? bs + 1 : bs;
    int pos = 0;
    for (int j = 0; j < block_dw; j++) {
        int src_idx;
        if (j < dw) {
            src_idx = j * bc + block_idx;
        } else {
            src_idx = dw * bc + (block_idx - ns);
        }
        if (src_idx < total_bytes) {
            block_out[pos++] = raw[src_idx];
        }
    }
    int data_total = dw * bc + (bc - ns);  /* Total data codewords */
    for (int j = 0; j < block_ecc; j++) {
        int src_idx = data_total + j * bc + block_idx;
        if (src_idx < total_bytes) {
            block_out[pos++] = raw[src_idx];
        }
    }
    return block_bs;
}
static int deinterleave_and_correct(uint8_t *raw, int total_bytes,
                int version, int ecc_level, uint8_t *data_out, int max_out,
                int *errors_corrected, int *uncorrectable_out) {
    if (version < 1 || version > 40) return 0;
    const RSBlockParams *sb = &version_info[version].ecc[ecc_level];
    int ns = sb->ns;           /* Number of short blocks */
    int dw = sb->dw;           /* Data codewords per short block */
    int bs = sb->bs;           /* Total codewords per short block */
    int short_total = ns * bs;
    int remaining = total_bytes - short_total;
    int nl = (remaining > 0) ? remaining / (bs + 1) : 0;
    int bc = ns + nl;          /* Total block count */
    int block_ecc = bs - dw;   /* ECC codewords per block */
    int data_pos = 0, total_errors = 0, uncorrectable_blocks = 0;
    for (int i = 0; i < bc && data_pos < max_out; i++) {
        uint8_t block[256];
        (void)deinterleave_block(raw, total_bytes, bc, dw, bs, ns, i, block);
        int is_long = (i >= ns);
        int block_dw = is_long ? dw + 1 : dw;
        int corrected = rs_correct_errors(block, block_dw, block_ecc);
        if (corrected < 0) {
            uncorrectable_blocks++;
        } else if (corrected > 0) {
            total_errors += corrected;
        }
        for (int j = 0; j < block_dw && data_pos < max_out; j++) {
            data_out[data_pos++] = block[j];
        }
    }
    if (errors_corrected) *errors_corrected = total_errors;
    if (uncorrectable_out) *uncorrectable_out = uncorrectable_blocks;
    if (uncorrectable_blocks == bc) {
        return -1;
    }
    return data_pos;
}
static int extract_qr_data(QRCode *qr, uint8_t *data, int max_len) {
    int size = qr->size, bi = 0, bit = 7, up = 1;
    memset(data, 0, max_len);
    for (int col = size - 1; col >= 0; col -= 2) {
        if (col == 6) col--;
        int rstart = up ? size-1 : 0;
        int rend = up ? -1 : size;
        int rstep = up ? -1 : 1;
        for (int row = rstart; row != rend; row += rstep)
            for (int c = 0; c < 2 && col-c >= 0; c++)
                if (!is_function_module(qr->version, col-c, row) &&
                    bi < max_len) {
                    if (qr->modules[row][col-c]) data[bi] |= (1 << bit);
                    if (--bit < 0) { bit = 7; bi++; }
                }
        up = !up;
    }
    return bi + (bit < 7 ? 1 : 0);
}
static int decode_qr_content(uint8_t *data, int data_len, int version,
                             char *output, int max_output) {
    int bit_pos = 0, out_pos = 0;
    #define READ_BITS(n) ({ \
        int val = 0; \
        for (int i = 0; i < (n); i++) { \
            int byte_idx = bit_pos / 8; \
            int bit_idx = 7 - (bit_pos % 8); \
            if (byte_idx < data_len) { \
                val = (val << 1) | ((data[byte_idx] >> bit_idx) & 1); \
            } \
            bit_pos++; \
        } \
        val; \
    })
    while (bit_pos < data_len * 8 && out_pos < max_output - 1) {
        int mode = READ_BITS(4);
        if (mode == 0) break;  /* Terminator */
        if (mode == 4) {  /* Byte mode - only mode qr-backup uses */
            int count_bits = version <= 9 ? 8 : 16;
            int count = READ_BITS(count_bits);
            for (int i = 0; i < count && out_pos < max_output - 1; i++)
                output[out_pos++] = READ_BITS(8);
        } else break;
    }
    output[out_pos] = '\0';
    return out_pos;
    #undef READ_BITS
}
static int distance_sq(int x1, int y1, int x2, int y2) {
    int dx = x2 - x1, dy = y2 - y1; return dx * dx + dy * dy;
}
static int cross_product(int x1, int y1, int x2, int y2, int x3, int y3) {
    return (x2 - x1) * (y3 - y1) - (y2 - y1) * (x3 - x1);
}
static int identify_finder_roles(FinderPattern *fp, int *tl, int *tr, int *bl) {
    int d[3] = {distance_sq(fp[0].x, fp[0].y, fp[1].x, fp[1].y),
                distance_sq(fp[0].x, fp[0].y, fp[2].x, fp[2].y),
                distance_sq(fp[1].x, fp[1].y, fp[2].x, fp[2].y)};
    /* Corner opposite longest side is top-left */
    int corner = (d[0] >= d[1] && d[0] >= d[2]) ? 2 : (d[1] >= d[2]) ? 1 : 0;
    int p1 = (corner == 0) ? 1 : 0, p2 = (corner == 2) ? 1 : 2;
    *tl = corner;
    int cp = cross_product(fp[corner].x, fp[corner].y,
                           fp[p1].x, fp[p1].y, fp[p2].x, fp[p2].y);
    *tr = (cp > 0) ? p1 : p2;
    *bl = (cp > 0) ? p2 : p1;
    return 1;
}
/* Closed-form homography from unit square to quadrilateral dst[0..3] */
static void calculate_homography(Point2D dst[4], HomographyMatrix *H) {
    float x0 = dst[0].x, y0 = dst[0].y, x1 = dst[1].x, y1 = dst[1].y;
    float x2 = dst[2].x, y2 = dst[2].y, x3 = dst[3].x, y3 = dst[3].y;
    float dx1 = x1 - x3, dy1 = y1 - y3, dx2 = x2 - x3, dy2 = y2 - y3;
    float sx = x0 - x1 + x3 - x2, sy = y0 - y1 + y3 - y2;
    float det = dx1 * dy2 - dx2 * dy1;
    if (fabsf(det) < 1e-10f) det = 1e-10f;
    float g = (sx * dy2 - dx2 * sy) / det;
    float h = (dx1 * sy - sx * dy1) / det;
    H->h[0][0] = x1 - x0 + g * x1;
    H->h[0][1] = x2 - x0 + h * x2;
    H->h[0][2] = x0;
    H->h[1][0] = y1 - y0 + g * y1;
    H->h[1][1] = y2 - y0 + h * y2;
    H->h[1][2] = y0;
    H->h[2][0] = g; H->h[2][1] = h; H->h[2][2] = 1.0f;
}
static Point2D apply_homography(const HomographyMatrix *H, float x, float y) {
    Point2D result;
    float w = H->h[2][0] * x + H->h[2][1] * y + H->h[2][2];
    if (fabs(w) < 1e-10) w = 1e-10;
    result.x = (H->h[0][0] * x + H->h[0][1] * y + H->h[0][2]) / w;
    result.y = (H->h[1][0] * x + H->h[1][1] * y + H->h[1][2]) / w;
    return result;
}
static QRTransform calculate_qr_transform(FinderPattern *fp, int tl,
                int tr, int bl, int version_override) {
    QRTransform transform = {0};
    float dx_tr = fp[tr].x - fp[tl].x, dy_tr = fp[tr].y - fp[tl].y;
    float dx_bl = fp[bl].x - fp[tl].x, dy_bl = fp[bl].y - fp[tl].y;
    float dist_tr = sqrtf(dx_tr*dx_tr + dy_tr*dy_tr);
    float dist_bl = sqrtf(dx_bl*dx_bl + dy_bl*dy_bl);
    float gm = cbrtf(fp[tl].module_size * fp[tr].module_size *
                     fp[bl].module_size);
    if (gm < 1.0f) gm = 1.0f;
    float modules_est = (dist_tr + dist_bl) / (2.0f * gm);
    int version = version_override > 0 ? version_override :
                  ((int)(modules_est + 7.5f) - 17 + 2) / 4;
    if (version < 1) version = 1;
    if (version > 40) version = 40;
    int size = 17 + 4 * version;
    transform.version = version;
    transform.size = size;
    transform.module_size = gm;
    float span = size - 7;
    float a = dx_tr / span, b = dx_bl / span;
    float c = dy_tr / span, d = dy_bl / span;
    float tx = fp[tl].x - 3.5f * (a + b);
    float ty = fp[tl].y - 3.5f * (c + d);
    Point2D dst[4] = {{tx, ty}, {a*size+tx, c*size+ty},
                      {b*size+tx, d*size+ty},
                      {(a+b)*size+tx, (c+d)*size+ty}};
    calculate_homography(dst, &transform.homography);
    return transform;
}
static int sample_module_transform_ex(Image *img, const QRTransform *tf,
                                      int mx, int my, int use_aa) {
    float u = (mx + 0.5f) / tf->size, v = (my + 0.5f) / tf->size;
    Point2D p = apply_homography(&tf->homography, u, v);
    if (!use_aa)
        return image_get(img, (int)(p.x + 0.5f), (int)(p.y + 0.5f)) ? 1 : 0;
    int black = 0;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            int ix = (int)(p.x + dx + 0.5f), iy = (int)(p.y + dy + 0.5f);
            black += image_get(img, ix, iy) * ((dx|dy) ? 1 : 2);
        }
    return black > 5 ? 1 : 0;
}

static int decode_qr_from_finders(Image *img, FinderPattern *fp, int nfp,
                                  char *output, int max_output) {
    if (nfp < 3) {
        return 0;
    }
    int tl, tr, bl;
    if (!identify_finder_roles(fp, &tl, &tr, &bl)) {
        return 0;
    }
    QRTransform initial_transform = calculate_qr_transform(fp, tl, tr, bl, 0);
    int estimated_version = initial_transform.version;
    int versions_to_try[10];  /* estimated + nearby + fallback */
    int num_versions = 0;
    versions_to_try[num_versions++] = estimated_version;
    float module_size = initial_transform.module_size;
    if (estimated_version >= 25 || module_size < 8.0f) {
        for (int delta = -2; delta <= 2; delta++) {
            int v = estimated_version + delta;
            if (v >= 1 && v <= 40 && v != estimated_version) {
                versions_to_try[num_versions++] = v;
            }
        }
    }
    for (int vi = 0; vi < num_versions; vi++) {
        int try_version = versions_to_try[vi];
    QRTransform transform = calculate_qr_transform(fp, tl, tr, bl, try_version);
    QRCode qr;
    qr.version = transform.version;
    qr.size = transform.size;
    for (int my = 0; my < transform.size; my++) {
        for (int mx = 0; mx < transform.size; mx++) {
            qr.modules[my][mx] = sample_module_transform_ex(img,
                                    &transform, mx, my, 1);
        }
    }
    int format_info = read_format_info(NULL, img, &transform);
    int corrected_format = validate_format_info_with_correction(format_info);
    if (corrected_format < 0) {
        format_info = read_format_info(&qr, NULL, NULL);
        corrected_format = validate_format_info_with_correction(format_info);
    }
    if (corrected_format < 0) {
        continue;  /* Try next version */
    }
    format_info = corrected_format;
    int mask_pattern = (format_info >> 10) & 0x07;
    int ecc_level = (format_info >> 13) & 0x03;
    unmask_qr(&qr, mask_pattern);
    uint8_t raw_codewords[4096];
    int raw_len = extract_qr_data(&qr, raw_codewords, sizeof(raw_codewords));
    uint8_t codewords[4096];
    int rs_errors = 0, rs_uncorrectable = 0;
    int data_len = deinterleave_and_correct(raw_codewords, raw_len,
            transform.version, ecc_level, codewords, sizeof(codewords),
            &rs_errors, &rs_uncorrectable);
    int result = decode_qr_content(codewords, data_len, transform.version,
                                   output, max_output);
    if (result > 0) {
        if (num_versions > 1 && rs_uncorrectable > 0) {
            continue;  /* Try next version */
        }
        return result;
    }
    }  /* end version loop */
    return 0;
}
static int scan_image_for_qr(Image *img, ChunkList *chunks);
static void chunk_list_init(ChunkList *cl) {
    cl->chunks = NULL; cl->count = 0; cl->capacity = 0;
}
static void chunk_list_free(ChunkList *cl) {
    for (int i = 0; i < cl->count; i++) free(cl->chunks[i].data);
    free(cl->chunks);
    cl->chunks = NULL; cl->count = 0; cl->capacity = 0;
}
static int chunk_list_add(ChunkList *cl, char type, int index, int total,
                          const uint8_t *data, int data_len) {
    if (cl->count >= cl->capacity) {
        int new_cap = cl->capacity ? cl->capacity * 2 : 64;
        Chunk *new_chunks = realloc(cl->chunks, new_cap * sizeof(Chunk));
        if (!new_chunks) return 0;
        cl->chunks = new_chunks;
        cl->capacity = new_cap;
    }
    Chunk *c = &cl->chunks[cl->count];
    c->type = type;
    c->index = index;
    c->total = total;
    c->data = malloc(data_len);
    memcpy(c->data, data, data_len);
    c->data_len = data_len;
    cl->count++;
    return 1;
}
static int parse_chunk_label(const char *content, char *type,
                             int *index, int *total,
                             const char **data_start) {
    if (content[0] != 'N' && content[0] != 'P') return 0;
    *type = content[0];
    int n = 0;
    if (sscanf(content + 1, "%d/%d%n", index, total, &n) != 2) return 0;
    const char *p = content + 1 + n;
    while (*p == ':' || *p == ' ') p++;
    *data_start = p;
    return 1;
}
static int chunk_compare(const void *a, const void *b) {
    const Chunk *ca = a, *cb = b;
    return (ca->type != cb->type) ? ca->type - cb->type : ca->index - cb->index;
}
static void chunk_list_sort_dedupe(ChunkList *cl) {
    if (cl->count > 1)
        qsort(cl->chunks, cl->count, sizeof(Chunk), chunk_compare);
    if (cl->count < 2) return;
    int w = 1;
    for (int r = 1; r < cl->count; r++) {
        if (cl->chunks[r].type != cl->chunks[w-1].type ||
            cl->chunks[r].index != cl->chunks[w-1].index)
            cl->chunks[w++] = cl->chunks[r];
        else free(cl->chunks[r].data);
    }
    cl->count = w;
}
static int8_t b64_decode_table[256];
static void b64_init(void) {
    static int done = 0; if (done) return; done = 1;
    memset(b64_decode_table, -1, 256);
    for (int i = 0; i < 26; i++) {
        b64_decode_table['A'+i] = i;
        b64_decode_table['a'+i] = 26+i;
    }
    for (int i = 0; i < 10; i++) b64_decode_table['0'+i] = 52+i;
    b64_decode_table['+'] = 62;
    b64_decode_table['/'] = 63;
}
static int base64_decode(const char *input, int input_len,
                         uint8_t *output, int max_output) {
    b64_init();
    int out_len = 0, bits = 0; uint32_t accum = 0;
    for (int i = 0; i < input_len; i++) {
        int c = (unsigned char)input[i], val = b64_decode_table[c];
        if (c == '=' || c == '\n' || c == '\r' || c == ' ' || val < 0)
            continue;
        accum = (accum << 6) | val;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (out_len < max_output)
                output[out_len++] = (accum >> bits) & 0xFF;
        }
    }
    return out_len;
}
static int erasure_recover(ChunkList *cl, int total_normal, int total_parity) {
    gf_init();
    int result = 0, missing = 0, chunk_size = 0, parity_count = 0;
    int *n_present = calloc(total_normal + 1, sizeof(int));
    int *p_present = calloc(total_parity + 1, sizeof(int));
    int *missing_indices = calloc(total_normal, sizeof(int));
    int *parity_used = calloc(total_parity, sizeof(int));
    uint8_t **matrix = NULL, **augmented = NULL;
    /* Mark present chunks and find chunk_size */
    for (int i = 0; i < cl->count; i++) {
        Chunk *c = &cl->chunks[i];
        if (c->type == 'N' && c->index <= total_normal)
            n_present[c->index] = 1;
        else if (c->type == 'P' && c->index <= total_parity)
            p_present[c->index] = 1;
        if (!chunk_size) chunk_size = c->data_len;
    }
    /* Find missing normal chunks */
    for (int i = 1; i <= total_normal; i++)
        if (!n_present[i]) missing_indices[missing++] = i;
    if (missing == 0) { result = 1; goto cleanup; }
    /* Select parity chunks to use */
    for (int p = 1; p <= total_parity && parity_count < missing; p++)
        if (p_present[p]) parity_used[parity_count++] = p;
    if (parity_count < missing) {
        fprintf(stderr,
            "Warning: %d missing, only %d parity\n", missing, parity_count);
        goto cleanup;
    }
    /* Allocate matrix: rows=parity+known, cols=normal+parity */
    int n_known = total_normal - missing;
    int n_rows = parity_count + n_known;
    int n_cols = total_normal + parity_count;
    matrix = malloc(n_rows * sizeof(uint8_t *));
    augmented = malloc(n_rows * sizeof(uint8_t *));
    for (int i = 0; i < n_rows; i++) {
        matrix[i] = calloc(n_cols, 1);
        augmented[i] = calloc(chunk_size, 1);
    }
    /* Parity equations: coef*N[1] + coef*N[2] + ... + P = 0 */
    for (int i = 0; i < parity_count; i++) {
        int p_idx = parity_used[i];
        for (int d = 1; d <= total_normal; d++)
            matrix[i][d - 1] = gf_exp[((d - 1) * (p_idx - 1)) % 255];
        matrix[i][total_normal + i] = 1;  /* coefficient for P itself */
    }
    /* Known chunk equations: N[d] = data, or P[p] = data */
    int row = parity_count;
    for (int i = 0; i < cl->count; i++) {
        Chunk *c = &cl->chunks[i];
        int col;
        if (c->type == 'N' && c->index <= total_normal)
            col = c->index - 1;
        else if (c->type == 'P' && c->index <= total_parity) {
            int pi = -1;
            for (int j = 0; j < parity_count; j++)
                if (parity_used[j] == c->index) { pi = j; break; }
            if (pi < 0) continue;
            col = total_normal + pi;
        } else continue;
        matrix[row][col] = 1;
        memcpy(augmented[row], c->data, chunk_size);
        row++;
    }
    /* Gaussian elimination: pivot on known columns first, then missing */
    for (int col = 0; col < n_cols; col++) {
        int pivot = -1;
        for (int r = 0; r < n_rows; r++)
            if (matrix[r][col]) { pivot = r; break; }
        if (pivot < 0) continue;
        /* Swap pivot row to stable position if needed */
        /* Eliminate this column from all other rows */
        uint8_t inv = gf_inv(matrix[pivot][col]);
        for (int j = 0; j < n_cols; j++)
            matrix[pivot][j] = gf_mul(matrix[pivot][j], inv);
        for (int b = 0; b < chunk_size; b++)
            augmented[pivot][b] = gf_mul(augmented[pivot][b], inv);
        for (int r = 0; r < n_rows; r++) {
            if (r != pivot && matrix[r][col]) {
                uint8_t f = matrix[r][col];
                for (int j = 0; j < n_cols; j++)
                    matrix[r][j] ^= gf_mul(f, matrix[pivot][j]);
                for (int b = 0; b < chunk_size; b++)
                    augmented[r][b] ^= gf_mul(f, augmented[pivot][b]);
            }
        }
    }
    /* Find rows that have solved for missing chunks */
    for (int i = 0; i < missing; i++) {
        int col = missing_indices[i] - 1;
        for (int r = 0; r < n_rows; r++) {
            if (matrix[r][col] == 1) {
                /* Check this row has only this column set */
                int solo = 1;
                for (int j = 0; j < n_cols && solo; j++)
                    if (j != col && matrix[r][j]) solo = 0;
                if (solo) {
                    chunk_list_add(cl, 'N', missing_indices[i],
                        total_normal, augmented[r], chunk_size);
                    break;
                }
            }
        }
    }
    result = 1;
cleanup:
    if (matrix) for (int i = 0; i < missing; i++) free(matrix[i]);
    if (augmented) for (int i = 0; i < missing; i++) free(augmented[i]);
    free(matrix); free(augmented); free(parity_used);
    free(n_present); free(p_present); free(missing_indices);
    return result;
}
static uint8_t *assemble_data(ChunkList *cl, int *out_len) {
    int total = 0, pos = 0;
    for (int i = 0; i < cl->count; i++)
        if (cl->chunks[i].type == 'N') total += cl->chunks[i].data_len;
    if (total == 0) { *out_len = 0; return NULL; }
    uint8_t *data = malloc(total);
    for (int i = 0; i < cl->count; i++)
        if (cl->chunks[i].type == 'N') {
            memcpy(data + pos, cl->chunks[i].data, cl->chunks[i].data_len);
            pos += cl->chunks[i].data_len;
        }
    /* Parse length prefix: "1234 data..." */
    int len_end = 0;  uint32_t real_len = 0;
    while (len_end < pos && data[len_end] >= '0' && data[len_end] <= '9')
        real_len = real_len * 10 + (data[len_end++] - '0');
    if (len_end < pos && data[len_end] == ' ') len_end++;
    if (len_end == 0) { *out_len = pos; return data; }
    int actual = (int)real_len > pos - len_end ? pos - len_end : (int)real_len;
    memmove(data, data + len_end, actual);
    *out_len = actual;
    return data;
}
static const uint8_t aes_sbox[256] = {
    99,124,119,123,242,107,111,197,48,1,103,43,254,215,171,118,
    202,130,201,125,250,89,71,240,173,212,162,175,156,164,114,192,
    183,253,147,38,54,63,247,204,52,165,229,241,113,216,49,21,
    4,199,35,195,24,150,5,154,7,18,128,226,235,39,178,117,
    9,131,44,26,27,110,90,160,82,59,214,179,41,227,47,132,
    83,209,0,237,32,252,177,91,106,203,190,57,74,76,88,207,
    208,239,170,251,67,77,51,133,69,249,2,127,80,60,159,168,
    81,163,64,143,146,157,56,245,188,182,218,33,16,255,243,210,
    205,12,19,236,95,151,68,23,196,167,126,61,100,93,25,115,
    96,129,79,220,34,42,144,136,70,238,184,20,222,94,11,219,
    224,50,58,10,73,6,36,92,194,211,172,98,145,149,228,121,
    231,200,55,109,141,213,78,169,108,86,244,234,101,122,174,8,
    186,120,37,46,28,166,180,198,232,221,116,31,75,189,139,138,
    112,62,181,102,72,3,246,14,97,53,87,185,134,193,29,158,
    225,248,152,17,105,217,142,148,155,30,135,233,206,85,40,223,
    140,161,137,13,191,230,66,104,65,153,45,15,176,84,187,22};
static const uint8_t aes_rcon[11] = {
    0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36
};
static uint8_t xtime(uint8_t x) {
    return (x << 1) ^ ((x >> 7) * 0x1b);
}
static void aes_key_expand(const uint8_t *key, uint8_t *rk, int klen) {
    int nk = klen / 4, nr = nk + 6, nb = 4;  /* nk=words, nr=rounds */
    uint8_t *round_keys = rk;
    memcpy(round_keys, key, klen);
    uint8_t temp[4];
    int i = nk;
    while (i < nb * (nr + 1)) {
        memcpy(temp, round_keys + (i - 1) * 4, 4);
        if (i % nk == 0) {
            uint8_t t = temp[0];
            temp[0] = temp[1]; temp[1] = temp[2];
            temp[2] = temp[3]; temp[3] = t;
            for (int j = 0; j < 4; j++) temp[j] = aes_sbox[temp[j]];
            temp[0] ^= aes_rcon[i / nk];
        } else if (nk > 6 && i % nk == 4)
            for (int j = 0; j < 4; j++) temp[j] = aes_sbox[temp[j]];
        for (int j = 0; j < 4; j++)
            round_keys[i*4 + j] = round_keys[(i - nk)*4 + j] ^ temp[j];
        i++;
    }
}
static void sha256(const uint8_t *data, size_t len, uint8_t *hash);
static uint8_t *gzip_decompress(const uint8_t *data, int len, int *out);
static void sha1_transform(uint32_t *state, const uint8_t *data) {
    uint32_t a, b, c, d, e, t, w[80];
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)data[i*4] << 24) | ((uint32_t)data[i*4+1] << 16) |
               ((uint32_t)data[i*4+2] << 8) | data[i*4+3];
    }
    for (int i = 16; i < 80; i++) {
        t = w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16];
        w[i] = (t << 1) | (t >> 31);
    }
    a = state[0]; b = state[1]; c = state[2]; d = state[3]; e = state[4];
    #define SHA1_ROL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }
        t = SHA1_ROL(a, 5) + f + e + k + w[i];
        e = d; d = c; c = SHA1_ROL(b, 30); b = a; a = t;
    }
    #undef SHA1_ROL
    state[0] += a; state[1] += b; state[2] += c;
    state[3] += d; state[4] += e;
}
static void sha1(const uint8_t *data, size_t len, uint8_t *hash) {
    uint32_t state[5] = {
        0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    uint8_t buffer[64];
    size_t pos = 0;
    while (pos + 64 <= len) {
        sha1_transform(state, data + pos);
        pos += 64;
    }
    size_t remaining = len - pos;
    memcpy(buffer, data + pos, remaining);
    buffer[remaining++] = 0x80;
    if (remaining > 56) {
        memset(buffer + remaining, 0, 64 - remaining);
        sha1_transform(state, buffer);
        memset(buffer, 0, 56);
    } else {
        memset(buffer + remaining, 0, 56 - remaining);
    }
    uint64_t bits = len * 8;
    buffer[56] = bits >> 56; buffer[57] = bits >> 48;
    buffer[58] = bits >> 40; buffer[59] = bits >> 32;
    buffer[60] = bits >> 24; buffer[61] = bits >> 16;
    buffer[62] = bits >> 8;  buffer[63] = bits;
    sha1_transform(state, buffer);
    for (int i = 0; i < 5; i++) {
        hash[i*4] = state[i] >> 24; hash[i*4+1] = state[i] >> 16;
        hash[i*4+2] = state[i] >> 8; hash[i*4+3] = state[i];
    }
}
#define S2K_SIMPLE    0
#define S2K_SALTED    1
#define S2K_ITERATED  3
#define CIPHER_AES128 7
#define CIPHER_AES192 8
#define CIPHER_AES256 9
#define HASH_SHA1   2
#define HASH_SHA256 8
static uint32_t s2k_decode_count(uint8_t c) {
    return (16 + (c & 15)) << ((c >> 4) + 6);
}
static void s2k_derive_key(const char *password, int pass_len,
                           const uint8_t *salt, int s2k_type, uint8_t hash_algo,
                           uint32_t count, uint8_t *key, int key_len) {
    int hash_len = (hash_algo == HASH_SHA256) ? 32 : 20, pos = 0, preload = 0;
    while (pos < key_len) {
        uint8_t *hash_input = NULL;
        int input_len = 0;
        if (s2k_type == S2K_SIMPLE) {
            input_len = preload + pass_len;
            hash_input = malloc(input_len);
            memset(hash_input, 0, preload);
            memcpy(hash_input + preload, password, pass_len);
        } else if (s2k_type == S2K_SALTED) {
            input_len = preload + 8 + pass_len;
            hash_input = malloc(input_len);
            memset(hash_input, 0, preload);
            memcpy(hash_input + preload, salt, 8);
            memcpy(hash_input + preload + 8, password, pass_len);
        } else {  /* S2K_ITERATED */
            int sp_len = 8 + pass_len;  /* salt + password length */
            uint32_t actual_count = count;
            if (actual_count < (uint32_t)sp_len) actual_count = sp_len;
            input_len = preload + actual_count;
            hash_input = malloc(input_len);
            memset(hash_input, 0, preload);
            int fill_pos = preload;
            while (fill_pos < input_len) {
                int chunk = sp_len;
                if (fill_pos + chunk > input_len) chunk = input_len - fill_pos;
                if (chunk <= 8) {
                    memcpy(hash_input + fill_pos, salt, chunk);
                } else {
                    memcpy(hash_input + fill_pos, salt, 8);
                    int pass_chunk = chunk - 8;
                    if (pass_chunk > pass_len) pass_chunk = pass_len;
                    memcpy(hash_input + fill_pos + 8, password, pass_chunk);
                }
                fill_pos += sp_len;
            }
        }
        uint8_t hash[32];
        if (hash_algo == HASH_SHA256) {
            sha256(hash_input, input_len, hash);
        } else {
            sha1(hash_input, input_len, hash);
        }
        int copy_len = hash_len;
        if (pos + copy_len > key_len) copy_len = key_len - pos;
        memcpy(key + pos, hash, copy_len);
        pos += copy_len;
        free(hash_input);
        preload++;  /* Add another zero byte for next round if needed */
    }
}
static void aes_encrypt_block(const uint8_t *in, uint8_t *out,
                              const uint8_t *round_keys, int nr) {
    uint8_t state[16];
    memcpy(state, in, 16);
    for (int i = 0; i < 16; i++) state[i] ^= round_keys[i];
    for (int round = 1; round <= nr; round++) {
        for (int i = 0; i < 16; i++) state[i] = aes_sbox[state[i]];
        uint8_t temp;
        temp = state[1]; state[1] = state[5]; state[5] = state[9];
        state[9] = state[13]; state[13] = temp;
        temp = state[2]; state[2] = state[10]; state[10] = temp;
        temp = state[6]; state[6] = state[14]; state[14] = temp;
        temp = state[15]; state[15] = state[11]; state[11] = state[7];
        state[7] = state[3]; state[3] = temp;
        if (round < nr) {
            for (int c = 0; c < 4; c++) {
                uint8_t a[4];
                for (int i = 0; i < 4; i++) a[i] = state[c * 4 + i];
                state[c*4 + 0] = xtime(a[0]) ^ xtime(a[1])
                    ^ a[1] ^ a[2] ^ a[3];
                state[c*4 + 1] = a[0] ^ xtime(a[1]) ^ xtime(a[2])
                    ^ a[2] ^ a[3];
                state[c*4 + 2] = a[0] ^ a[1] ^ xtime(a[2])
                    ^ xtime(a[3]) ^ a[3];
                state[c*4 + 3] = xtime(a[0]) ^ a[0] ^ a[1]
                    ^ a[2] ^ xtime(a[3]);
            }
        }
        for (int i = 0; i < 16; i++) state[i] ^= round_keys[round * 16 + i];
    }
    memcpy(out, state, 16);
}
static void aes_cfb_decrypt(const uint8_t *data, int data_len,
                            const uint8_t *key, int key_len,
                            uint8_t *output) {
    int nr = (key_len == 16) ? 10 : (key_len == 24) ? 12 : 14;
    uint8_t round_keys[240];
    aes_key_expand(key, round_keys, key_len);
    uint8_t fr[16] = {0}, fre[16];  /* fr = Feedback reg, fre = encrypted */
    int pos = 0;
    while (pos < data_len) {
        aes_encrypt_block(fr, fre, round_keys, nr);
        int block_len = 16;
        if (pos + block_len > data_len) block_len = data_len - pos;
        for (int i = 0; i < block_len; i++) {
            output[pos + i] = data[pos + i] ^ fre[i];
        }
        if (block_len == 16) {
            memcpy(fr, data + pos, 16);
        } else {
            memmove(fr, fr + block_len, 16 - block_len);
            memcpy(fr + 16 - block_len, data + pos, block_len);
        }
        pos += block_len;
    }
}
static int pgp_parse_header(const uint8_t *data, int len,
                            int *body_len, int *header_len) {
    if (len < 2) return -1;
    uint8_t tag_byte = data[0];
    if ((tag_byte & 0x80) == 0) return -1;  /* Not a PGP packet */
    int packet_tag, pos = 1;
    if (tag_byte & 0x40) {
        packet_tag = tag_byte & 0x3f;
        if (data[pos] < 192) {
            *body_len = data[pos];
            pos++;
        } else if (data[pos] < 224) {
            if (len < pos + 2) return -1;
            *body_len = ((data[pos] - 192) << 8) + data[pos+1] + 192;
            pos += 2;
        } else if (data[pos] == 255) {
            if (len < pos + 5) return -1;
            *body_len = ((uint32_t)data[pos+1] << 24) |
                ((uint32_t)data[pos+2] << 16) |
                ((uint32_t)data[pos+3] << 8) | data[pos+4];
            pos += 5;
        } else {
            *body_len = 1 << (data[pos] & 0x1f);
            pos++;
        }
    } else {
        packet_tag = (tag_byte >> 2) & 0x0f;
        int length_type = tag_byte & 0x03;
        if (length_type == 0) {
            *body_len = data[pos];
            pos++;
        } else if (length_type == 1) {
            if (len < pos + 2) return -1;
            *body_len = (data[pos] << 8) | data[pos+1];
            pos += 2;
        } else if (length_type == 2) {
            if (len < pos + 4) return -1;
            *body_len = ((uint32_t)data[pos] << 24) |
                ((uint32_t)data[pos+1] << 16) |
                ((uint32_t)data[pos+2] << 8) | data[pos+3];
            pos += 4;
        } else {
            *body_len = len - pos;
        }
    }
    *header_len = pos;
    return packet_tag;
}
static int is_gpg_encrypted(const uint8_t *data, int len) {
    if (len < 2) return 0;
    if ((data[0] & 0x80) == 0) return 0;
    int packet_tag = (data[0] & 0x40)
        ? (data[0] & 0x3f) : ((data[0] >> 2) & 0x0f);
    return (packet_tag == 3 || packet_tag == 9 || packet_tag == 18);
}
static uint8_t *gpg_decrypt(const uint8_t *data, int data_len,
                            const char *password, int *out_len) {
    *out_len = 0;
    if (!is_gpg_encrypted(data, data_len)) {
        return NULL;
    }
    int pos = 0;
    uint8_t session_key[32];
    int session_key_len = 0, cipher_algo = CIPHER_AES128;
    const uint8_t *encrypted_data = NULL;  int encrypted_len = 0, has_mdc = 0;
    while (pos < data_len) {
        int body_len, header_len;
        int packet_tag = pgp_parse_header(data + pos,
            data_len - pos, &body_len, &header_len);
        if (packet_tag < 0) break;
        const uint8_t *body = data + pos + header_len;
        if (packet_tag == 3) {
            if (body_len < 4) return NULL;
            int version = body[0];
            if (version != 4) {
                fprintf(stderr, "GPG: Unsupported SKESK v%d\n", version);
                return NULL;
            }
            cipher_algo = body[1];
            int s2k_type = body[2], s2k_pos = 3;
            uint8_t hash_algo = body[s2k_pos++], salt[8] = {0};
            uint32_t count = 65536;
            if (s2k_type == S2K_SALTED || s2k_type == S2K_ITERATED) {
                memcpy(salt, body + s2k_pos, 8); s2k_pos += 8;
                if (s2k_type == S2K_ITERATED)
                    count = s2k_decode_count(body[s2k_pos++]);
            } else if (s2k_type != S2K_SIMPLE) {
                fprintf(stderr, "GPG: S2K type %d\n", s2k_type);
                return NULL;
            }
            session_key_len = (cipher_algo == CIPHER_AES256) ? 32 :
                (cipher_algo == CIPHER_AES192) ? 24 : 16;
            if (cipher_algo < CIPHER_AES128 || cipher_algo > CIPHER_AES256) {
                fprintf(stderr, "GPG: cipher %d\n", cipher_algo);
                return NULL;
            }
            s2k_derive_key(password, strlen(password), salt, s2k_type,
                          hash_algo, count, session_key, session_key_len);
        } else if (packet_tag == 9) {
            encrypted_data = body;
            encrypted_len = body_len;
            has_mdc = 0;
        } else if (packet_tag == 18) {
            if (body_len < 1 || body[0] != 1) {
                fprintf(stderr, "GPG: Unsupported SEIPD version\n");
                return NULL;
            }
            encrypted_data = body + 1;
            encrypted_len = body_len - 1;
            has_mdc = 1;
        }
        pos += header_len + body_len;
    }
    if (!encrypted_data || session_key_len == 0) {
        fprintf(stderr, "GPG: Missing data or key\n");
        return NULL;
    }
    uint8_t *decrypted = malloc(encrypted_len);
    aes_cfb_decrypt(encrypted_data, encrypted_len,
        session_key, session_key_len, decrypted);
    int block_size = 16;  /* AES block size */
    if (encrypted_len < block_size + 2) {
        free(decrypted);
        return NULL;
    }
    if (decrypted[block_size - 2] != decrypted[block_size] ||
        decrypted[block_size - 1] != decrypted[block_size + 1]) {
        fprintf(stderr, "GPG: prefix check failed (bad password?)\n");
        free(decrypted);
        return NULL;
    }
    int payload_start = block_size + 2;
    int payload_len = encrypted_len - payload_start;
    if (has_mdc) {
        if (payload_len < 22) { free(decrypted); return NULL; }
        int mdc_pos = payload_start + payload_len - 22;
        if (decrypted[mdc_pos] != 0xD3 || decrypted[mdc_pos + 1] != 0x14) {
            fprintf(stderr, "GPG: MDC not found\n");
            free(decrypted); return NULL;
        }
        uint8_t *mdc_input = malloc(payload_start + payload_len - 20);
        memcpy(mdc_input, decrypted, payload_start + payload_len - 20);
        uint8_t computed_hash[20];
        sha1(mdc_input, payload_start + payload_len - 20, computed_hash);
        free(mdc_input);
        if (memcmp(computed_hash, decrypted + mdc_pos + 2, 20) != 0) {
            fprintf(stderr, "GPG: MDC failed\n");
            free(decrypted); return NULL;
        }
        payload_len -= 22;  /* Remove MDC from output */
    }
    int lit_blen, lit_hlen, lit_tag = pgp_parse_header(
        decrypted + payload_start, payload_len, &lit_blen, &lit_hlen);
    uint8_t *result = NULL;
    if (lit_tag == 11) {
        const uint8_t *lit_body = decrypted + payload_start + lit_hlen;
        if (lit_blen < 6) { free(decrypted); return NULL; }
        int filename_len = lit_body[1], data_off = 2 + filename_len + 4;
        int data_len_final = lit_blen - data_off;
        result = malloc(data_len_final);
        if (result) {
            memcpy(result, lit_body + data_off, data_len_final);
            *out_len = data_len_final;
        }
    } else if (lit_tag == 8) {
        const uint8_t *cb = decrypted + payload_start + lit_hlen;
        int comp_algo = cb[0];
        if (comp_algo == 0) {
            result = malloc(lit_blen - 1);
            if (result) {
                memcpy(result, cb + 1, lit_blen - 1);
                *out_len = lit_blen - 1;
            }
        } else if (comp_algo == 1 || comp_algo == 2) {
            /* Wrap DEFLATE/ZLIB in gzip header for decompression */
            int skip = (comp_algo == 2) ? 3 : 1;  /* ZLIB: 2-byte hdr */
            int trim = (comp_algo == 2) ? 6 : 0;
            int comp_len = lit_blen - 1 - trim;
            if (comp_len > 0) {
                uint8_t *wrapped = malloc(comp_len + 18);
                memcpy(wrapped, "\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff", 10);
                memcpy(wrapped + 10, cb + skip, comp_len);
                memset(wrapped + 10 + comp_len, 0, 8);
                result = gzip_decompress(wrapped, comp_len + 18, out_len);
                free(wrapped);
            }
        } else {
            fprintf(stderr, "GPG: compression %d\n", comp_algo);
        }
    } else {
        fprintf(stderr, "GPG: packet type %d\n", lit_tag);
    }
    free(decrypted);
    return result;
}
typedef struct {
    uint16_t sym;   /* Symbol or subtable offset */
    uint8_t bits;   /* Number of bits */
} HuffEntry;
typedef struct {
    const uint8_t *in;
    int in_len;
    int in_pos;
    uint32_t bit_buf;
    int bit_cnt;
    uint8_t *out;
    int out_len;
    int out_pos;
    int out_cap;
} InflateState;
static uint32_t inf_read_bits(InflateState *s, int n) {
    while (s->bit_cnt < n) {
        if (s->in_pos >= s->in_len) return 0;
        s->bit_buf |= (uint32_t)s->in[s->in_pos++] << s->bit_cnt;
        s->bit_cnt += 8;
    }
    uint32_t val = s->bit_buf & ((1 << n) - 1);
    s->bit_buf >>= n;
    s->bit_cnt -= n;
    return val;
}
static int inf_output(InflateState *s, uint8_t b) {
    if (s->out_pos >= s->out_cap) {
        int new_cap = s->out_cap ? s->out_cap * 2 : 4096;
        s->out = realloc(s->out, new_cap);
        s->out_cap = new_cap;
    }
    s->out[s->out_pos++] = b;
    return 1;
}
static void get_fixed_lit_lengths(uint8_t *lengths) {
    for (int i = 0; i < 144; i++) lengths[i] = 8;
    for (int i = 144; i < 256; i++) lengths[i] = 9;
    for (int i = 256; i < 280; i++) lengths[i] = 7;
    for (int i = 280; i < 288; i++) lengths[i] = 8;
}
static int huff_build(const uint8_t *lengths, int count,
                      HuffEntry *table, int *table_bits) {
    int bl_count[16] = {0};
    int max_len = 0;
    for (int i = 0; i < count; i++) {
        if (lengths[i]) {
            bl_count[lengths[i]]++;
            if (lengths[i] > max_len) max_len = lengths[i];
        }
    }
    *table_bits = max_len > 9 ? 9 : max_len;
    int code = 0;
    int next_code[16];
    next_code[0] = 0;
    for (int bits = 1; bits <= max_len; bits++) {
        code = (code + bl_count[bits - 1]) << 1;
        next_code[bits] = code;
    }
    for (int i = 0; i < count; i++) {
        int len = lengths[i];
        if (len) {
            int c = next_code[len]++;
            int rev = 0;
            for (int j = 0; j < len; j++) {
                rev = (rev << 1) | (c & 1);
                c >>= 1;
            }
            if (len <= *table_bits) {
                int fill = 1 << (*table_bits - len);
                for (int j = 0; j < fill; j++) {
                    int idx = rev | (j << len);
                    table[idx].sym = i;
                    table[idx].bits = len;
                }
            }
        }
    }
    return 1;
}
static int huff_decode(InflateState *s, HuffEntry *table, int table_bits) {
    while (s->bit_cnt < table_bits) {
        if (s->in_pos >= s->in_len) return -1;
        s->bit_buf |= (uint32_t)s->in[s->in_pos++] << s->bit_cnt;
        s->bit_cnt += 8;
    }
    int idx = s->bit_buf & ((1 << table_bits) - 1);
    int bits = table[idx].bits;
    int sym = table[idx].sym;
    s->bit_buf >>= bits;
    s->bit_cnt -= bits;
    return sym;
}
static const int len_base[] = {
    3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,
    35,43,51,59,67,83,99,115,131,163,195,227,258};
static const int len_extra[] = {
    0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
static const int dist_base[] = {
    1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,
    257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
static const int dist_extra[] = {
    0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
static const int codelen_order[19] = {16, 17, 18, 0, 8, 7, 9, 6,
    10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
static int inflate_with_tables(InflateState *s, HuffEntry *lt, int lb,
                               HuffEntry *dt, int db) {
    HuffEntry *lit_table = lt, *dist_table = dt;
    int lit_bits = lb, dist_bits = db;
    while (1) {
        int sym = huff_decode(s, lit_table, lit_bits);
        if (sym < 0) return 0;
        if (sym < 256) {
            if (!inf_output(s, sym)) return 0;
        } else if (sym == 256) {
            return 1;
        } else {
            sym -= 257;
            if (sym >= 29) return 0;
            int len = len_base[sym] + inf_read_bits(s, len_extra[sym]);
            int dist_sym = huff_decode(s, dist_table, dist_bits);
            if (dist_sym < 0 || dist_sym >= 30) return 0;
            int dist = dist_base[dist_sym]
                + inf_read_bits(s, dist_extra[dist_sym]);
            for (int i = 0; i < len; i++) {
                int src = s->out_pos - dist;
                if (src < 0) return 0;
                if (!inf_output(s, s->out[src])) return 0;
            }
        }
    }
}
static int inflate_block_fixed(InflateState *s) {
    HuffEntry lit_table[512], dist_table[32];
    int lit_bits, dist_bits;
    uint8_t lit_len[288], dist_len[32];
    get_fixed_lit_lengths(lit_len);
    huff_build(lit_len, 288, lit_table, &lit_bits);
    for (int i = 0; i < 32; i++) dist_len[i] = 5;
    huff_build(dist_len, 32, dist_table, &dist_bits);
    return inflate_with_tables(s, lit_table, lit_bits, dist_table, dist_bits);
}
static int inflate_block_dynamic(InflateState *s) {
    int hlit = inf_read_bits(s, 5) + 257;   /* Literal/length codes */
    int hdist = inf_read_bits(s, 5) + 1;    /* Distance codes */
    int hclen = inf_read_bits(s, 4) + 4;    /* Code length codes */
    if (hlit > 286 || hdist > 30) return 0;
    uint8_t codelen_lengths[19] = {0};
    for (int i = 0; i < hclen; i++) {
        codelen_lengths[codelen_order[i]] = inf_read_bits(s, 3);
    }
    HuffEntry codelen_table[128];
    int codelen_bits;
    if (!huff_build(codelen_lengths, 19, codelen_table, &codelen_bits))
        return 0;
    uint8_t lengths[286 + 30];
    int total_codes = hlit + hdist;
    int i = 0;
    while (i < total_codes) {
        int sym = huff_decode(s, codelen_table, codelen_bits);
        if (sym < 0) return 0;
        if (sym < 16) {
            lengths[i++] = sym;
        } else if (sym == 16) {
            if (i == 0) return 0;
            int repeat = 3 + inf_read_bits(s, 2);
            uint8_t prev = lengths[i - 1];
            while (repeat-- && i < total_codes) lengths[i++] = prev;
        } else if (sym == 17) {
            int repeat = 3 + inf_read_bits(s, 3);
            while (repeat-- && i < total_codes) lengths[i++] = 0;
        } else if (sym == 18) {
            int repeat = 11 + inf_read_bits(s, 7);
            while (repeat-- && i < total_codes) lengths[i++] = 0;
        } else {
            return 0;
        }
    }
    HuffEntry lit_table[32768];  /* Need larger table for dynamic codes */
    HuffEntry dist_table[32768];
    int lit_bits, dist_bits;
    if (!huff_build(lengths, hlit, lit_table, &lit_bits)) return 0;
    if (!huff_build(lengths + hlit, hdist, dist_table, &dist_bits)) return 0;
    return inflate_with_tables(s, lit_table, lit_bits, dist_table, dist_bits);
}
static uint8_t *gzip_decompress(const uint8_t *data, int data_len,
                                int *out_len) {
    if (data_len < 10) { *out_len = 0; return NULL; }
    if (data[0] != 0x1f || data[1] != 0x8b) { *out_len = 0; return NULL; }
    if (data[2] != 8) { *out_len = 0; return NULL; }
    int flags = data[3];
    int pos = 10;
    if (flags & 0x04) {
        if (pos + 2 > data_len) { *out_len = 0; return NULL; }
        int xlen = data[pos] | (data[pos+1] << 8);
        pos += 2 + xlen;
    }
    if (flags & 0x08) {
        while (pos < data_len && data[pos]) pos++;
        pos++;
    }
    if (flags & 0x10) {
        while (pos < data_len && data[pos]) pos++;
        pos++;
    }
    if (flags & 0x02) pos += 2;
    if (pos >= data_len) { *out_len = 0; return NULL; }
    InflateState s = {0};
    s.in = data + pos; s.in_len = data_len - pos - 8; s.in_pos = 0;
    s.out = NULL; s.out_pos = 0; s.out_cap = 0;
    int bfinal;
    do {
        bfinal = inf_read_bits(&s, 1);
        int btype = inf_read_bits(&s, 2);
        if (btype == 0) {
            s.bit_buf = 0; s.bit_cnt = 0;
            if (s.in_pos + 4 > s.in_len) break;
            int len = s.in[s.in_pos] | (s.in[s.in_pos+1] << 8);
            s.in_pos += 4;
            for (int i = 0; i < len && s.in_pos < s.in_len; i++) {
                if (!inf_output(&s, s.in[s.in_pos++])) break;
            }
        } else if (btype == 1) {
            if (!inflate_block_fixed(&s)) break;
        } else if (btype == 2) {
            if (!inflate_block_dynamic(&s)) break;
        } else {
            break;  /* Invalid block type */
        }
    } while (!bfinal);
    *out_len = s.out_pos;
    return s.out;
}
static const uint32_t sha256_k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,
    0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
    0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,
    0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,
    0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,
    0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,
    0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,
    0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,
    0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define EP1(x) (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define SIG0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^ ((x) >> 3))
#define SIG1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ ((x) >> 10))
static void sha256_transform(uint32_t *state, const uint8_t *block) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i*4] << 24) |
               ((uint32_t)block[i*4+1] << 16) |
               ((uint32_t)block[i*4+2] << 8) |
               (uint32_t)block[i*4+3];
    }
    for (int i = 16; i < 64; i++) {
        w[i] = SIG1(w[i-2]) + w[i-7] + SIG0(w[i-15]) + w[i-16];
    }
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
    for (int i = 0; i < 64; i++) {
        uint32_t t1 = h + EP1(e) + CH(e, f, g) + sha256_k[i] + w[i];
        uint32_t t2 = EP0(a) + MAJ(a, b, c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}
static void sha256(const uint8_t *data, size_t len, uint8_t *hash) {
    uint32_t state[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };
    size_t i;
    for (i = 0; i + 64 <= len; i += 64) {
        sha256_transform(state, data + i);
    }
    uint8_t block[64];
    size_t remaining = len - i;
    memcpy(block, data + i, remaining);
    block[remaining] = 0x80;
    if (remaining >= 56) {
        memset(block + remaining + 1, 0, 63 - remaining);
        sha256_transform(state, block);
        memset(block, 0, 56);
    } else {
        memset(block + remaining + 1, 0, 55 - remaining);
    }
    uint64_t bit_len = len * 8;
    for (int j = 0; j < 8; j++) {
        block[63 - j] = bit_len & 0xff;
        bit_len >>= 8;
    }
    sha256_transform(state, block);
    for (int j = 0; j < 8; j++) {
        hash[j*4] = (state[j] >> 24) & 0xff;
        hash[j*4+1] = (state[j] >> 16) & 0xff;
        hash[j*4+2] = (state[j] >> 8) & 0xff;
        hash[j*4+3] = state[j] & 0xff;
    }
}

static int valid_qr_triple(FinderPattern *p0, FinderPattern *p1,
                           FinderPattern *p2) {
    float ms[3] = {p0->module_size, p1->module_size, p2->module_size};
    float avg_ms = (ms[0] + ms[1] + ms[2]) / 3.0f;
    for (int i = 0; i < 3; i++)
        if (ms[i] < avg_ms * 0.75f || ms[i] > avg_ms * 1.25f) return 0;
    int d[3] = {distance_sq(p0->x, p0->y, p1->x, p1->y),
                distance_sq(p0->x, p0->y, p2->x, p2->y),
                distance_sq(p1->x, p1->y, p2->x, p2->y)};
    /* Sort distances */
    if (d[0] > d[1]) { int t = d[0]; d[0] = d[1]; d[1] = t; }
    if (d[1] > d[2]) { int t = d[1]; d[1] = d[2]; d[2] = t; }
    if (d[0] > d[1]) { int t = d[0]; d[0] = d[1]; d[1] = t; }
    /* Check: two sides similar, hypotenuse ~= sqrt(2) * side */
    if (d[1] > d[0] * 2) return 0;
    float hyp_ratio = (float)d[2] / (d[0] + d[1]);
    if (hyp_ratio < 0.75f || hyp_ratio > 1.25f) return 0;
    float version = (sqrtf((float)d[0]) / avg_ms - 10.0f) / 4.0f;
    return version <= 50;
}
static int scan_image_for_qr(Image *img, ChunkList *chunks) {
    FinderPattern patterns[500];
    int npatterns = find_finder_patterns(img, patterns, 500);
    if (npatterns < 3) return 0;  /* Need >= 3 finder patterns */
    int *used = calloc(npatterns, sizeof(int));
    int decoded_count = 0;
    for (int i = 0; i < npatterns - 2 && decoded_count < MAX_CHUNKS; i++) {
        if (used[i]) continue;
        for (int j = i + 1; j < npatterns - 1; j++) {
            if (used[j]) continue;
            for (int k = j + 1; k < npatterns; k++) {
                if (used[k]) continue;
                if (!valid_qr_triple(&patterns[i], &patterns[j],
                                     &patterns[k])) {
                    continue;
                }
                FinderPattern triple[3] = {
                    patterns[i], patterns[j], patterns[k]};
                char qr_content[4096];
                int len = decode_qr_from_finders(img, triple, 3,
                    qr_content, sizeof(qr_content));
                if (len > 0) {
                    char type;
                    int index, total;
                    const char *data_start;
                    if (parse_chunk_label(qr_content, &type, &index,
                                          &total, &data_start)) {
                        uint8_t decoded[MAX_CHUNK_SIZE];
                        int decoded_len = base64_decode(data_start,
                            strlen(data_start), decoded, sizeof(decoded));
                        if (decoded_len > 0) {
                            chunk_list_add(chunks, type, index, total,
                                decoded, decoded_len);
                            decoded_count++;
                            used[i] = used[j] = used[k] = 1;
                            goto next_i;  /* Move to next starting pattern */
                        }
                    }
                }
            }
        }
        next_i:;
    }
    free(used);
    return decoded_count;
}
static int parse_args(int argc, char **argv, const char **password,
                      const char **output_file, int *first_file) {
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            if (!*first_file) *first_file = i;
            continue;
        }
        if (!strcmp(argv[i], "-p") && i+1 < argc) *password = argv[++i];
        else if (!strcmp(argv[i], "-o") && i+1 < argc)
            *output_file = argv[++i];
        else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "-vv")
                 || !strcmp(argv[i], "--debug")) continue;
        else {
            fprintf(stderr, "Usage: %s [-p pass] [-o out] img.pbm\n",
                argv[0]);
            return argv[i][1] == 'h' ? 0 : -1;
        }
    }
    if (!*first_file) {
        fprintf(stderr, "Usage: %s [-p pass] [-o out] img.pbm\n", argv[0]);
        return -1;
    }
    return 1;
}
static int load_images(int argc, char **argv, int first_file,
                       ChunkList *chunks) {
    for (int i = first_file; i < argc; i++) {
        if (argv[i][0] == '-') continue;
        Image *img = image_read_pbm(argv[i]);
        if (img) { scan_image_for_qr(img, chunks); image_free(img); }
    }
    return chunks->count > 0;
}
static uint8_t *collect_chunks(ChunkList *chunks, int *data_len) {
    chunk_list_sort_dedupe(chunks);
    int tn = 0, tp = 0;
    for (int i = 0; i < chunks->count; i++) {
        Chunk *c = &chunks->chunks[i];
        if (c->type == 'N' && c->total > tn) tn = c->total;
        if (c->type == 'P' && c->total > tp) tp = c->total;
    }
    if (!erasure_recover(chunks, tn, tp))
        fprintf(stderr, "Warning: recovery incomplete\n");
    return assemble_data(chunks, data_len);
}
static uint8_t *decrypt_if_needed(uint8_t *data, int *len,
                                  const char *password) {
    if (!is_gpg_encrypted(data, *len)) return data;
    if (!password) {
        fprintf(stderr, "Error: GPG encrypted, need -p\n");
        free(data); return NULL;
    }
    int dec_len;
    uint8_t *dec = gpg_decrypt(data, *len, password, &dec_len);
    if (!dec) {
        fprintf(stderr, "GPG decryption failed\n");
        free(data); return NULL;
    }
    free(data); *len = dec_len; return dec;
}
static uint8_t *decompress_if_needed(uint8_t *data, int *len) {
    if (*len < 2 || data[0] != 0x1f || data[1] != 0x8b) return data;
    int dec_len;
    uint8_t *dec = gzip_decompress(data, *len, &dec_len);
    if (!dec) return data;
    free(data); *len = dec_len; return dec;
}
static int write_output(uint8_t *data, int len, const char *output_file) {
    uint8_t hash[32]; sha256(data, len, hash);
    fprintf(stderr, "SHA256: ");
    for (int i = 0; i < 32; i++) fprintf(stderr, "%02x", hash[i]);
    fprintf(stderr, "\n");
    FILE *out = output_file ? fopen(output_file, "wb") : stdout;
    if (!out) return 0;
    fwrite(data, 1, len, out); if (output_file) fclose(out);
    return 1;
}
int main(int argc, char **argv) {
    const char *password = NULL, *output_file = NULL;
    int first_file = 0;
    int r = parse_args(argc, argv, &password, &output_file, &first_file);
    if (r <= 0) return r < 0 ? 1 : 0;
    ChunkList chunks; chunk_list_init(&chunks);
    if (!load_images(argc, argv, first_file, &chunks))
        { fprintf(stderr, "No QR codes found\n"); return 1; }
    int len;
    uint8_t *data = collect_chunks(&chunks, &len);
    chunk_list_free(&chunks);
    if (!data) { fprintf(stderr, "Failed to assemble data\n"); return 1; }
    data = decrypt_if_needed(data, &len, password);  if (!data) return 1;
    data = decompress_if_needed(data, &len);
    (void)xtime;
    int ok = write_output(data, len, output_file);
    free(data);
    return ok ? 0 : 1;
}
