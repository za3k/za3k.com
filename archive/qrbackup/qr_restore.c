/*
 * qr_restore.c - Paper backup restore program for qr-backup
 * 
 * A standalone C program to restore qr-backup paper backups from PBM images.
 * No dependencies other than libc (malloc, stdio, string).
 * 
 * Licensed under CC0 (Public Domain)
 * 
 * Restore phases:
 *   1. PBM image reading
 *   2. QR code detection and decoding
 *   3. Chunk organization (parse labels, sort, dedupe)
 *   4. Base64 decoding
 *   5. Reed-Solomon erasure coding recovery
 *   6. Data assembly (concat, remove length/padding)
 *   7. AES decryption (GPG symmetric mode) [optional]
 *   8. Gzip decompression [optional]
 *   9. SHA256 checksum output
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#ifdef UNIT_TEST_BUILD
/* Forward declaration for unit tests - defined in unit_tests.c */
int run_unit_tests(void);
/* Make internal functions visible for testing */
#define STATIC
#else
#define STATIC static
#endif /* UNIT_TEST_BUILD */

/* Simple 2D point structure */
typedef struct {
    float x, y;
} Point2D;

/* Homography matrix (3x3) for projective transform */
typedef struct {
    float h[3][3];  /* h[2][2] is typically normalized to 1 */
} HomographyMatrix;

/* Structure for centralized transform handling */
typedef struct {
    /* Original finder pattern positions */
    float tl_x, tl_y;  /* Top-left finder pattern */
    float tr_x, tr_y;  /* Top-right finder pattern */
    float bl_x, bl_y;  /* Bottom-left finder pattern */
    
    /* QR code properties */
    int version;       /* QR code version (1-40) */
    int size;          /* Size in modules (17 + 4*version) */
    float module_size; /* Average module size in pixels */
    
    /* Transform parameters */
    float scale_x;     /* Horizontal scaling factor */
    float scale_y;     /* Vertical scaling factor */
    float angle;       /* Rotation angle in radians */
    
    /* Corners of the QR code */
    float qr_tl_x, qr_tl_y;  /* Top-left corner */
    float qr_tr_x, qr_tr_y;  /* Top-right corner */
    float qr_bl_x, qr_bl_y;  /* Bottom-left corner */
    float qr_br_x, qr_br_y;  /* Bottom-right corner (derived) */
    
    /* Homography matrix for more accurate mapping */
    HomographyMatrix homography;
    int use_homography;     /* Flag to indicate if homography is available */
} QRTransform;

/* Simple square root for float - Newton's method with better initial guess */
static float fsqrt(float x) {
    if (x <= 0) return 0;
    /* Use bit manipulation for a good initial guess (fast inverse square root inspired) */
    union { float f; uint32_t i; } conv = { .f = x };
    conv.i = 0x1fbd1df5 + (conv.i >> 1);  /* Approximate sqrt via bit manipulation */
    float guess = conv.f;
    /* Newton-Raphson iterations for accuracy */
    for (int i = 0; i < 4; i++) {
        guess = (guess + x / guess) * 0.5f;
    }
    return guess;
}

/* ============================================================================
 * CONFIGURATION
 * ============================================================================ */

#define MAX_QR_VERSION    40
#define MAX_QR_MODULES    (17 + 4 * MAX_QR_VERSION)  /* 177 for version 40 */
#define MAX_CHUNKS        1024
#define MAX_CHUNK_SIZE    4096
#define MAX_DATA_SIZE     (MAX_CHUNKS * MAX_CHUNK_SIZE)

/* ============================================================================
 * DATA STRUCTURES
 * ============================================================================ */

/* Bitmap image */
typedef struct {
    int width;
    int height;
    uint8_t *data;  /* 1 = black, 0 = white */
} Image;

/* Decoded QR code chunk */
typedef struct {
    char type;      /* 'N' for normal, 'P' for parity */
    int index;      /* chunk number (1-based) */
    int total;      /* total chunks of this type */
    uint8_t *data;  /* decoded data */
    int data_len;
} Chunk;

/* Collection of chunks */
typedef struct {
    Chunk *chunks;
    int count;
    int capacity;
} ChunkList;

/* ============================================================================
 * PHASE 1: PBM/PGM IMAGE READER
 * ============================================================================ */

static int skip_whitespace_and_comments(FILE *f) {
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (c == '#') {
            /* Skip comment line */
            while ((c = fgetc(f)) != EOF && c != '\n');
        } else if (c > ' ') {
            ungetc(c, f);
            return 1;
        }
    }
    return 0;
}

/* Otsu's method for automatic threshold selection - can be used as alternative to adaptive */
static int otsu_threshold(uint8_t *gray, int size) __attribute__((unused));
static int otsu_threshold(uint8_t *gray, int size) {
    /* Build histogram */
    int hist[256] = {0};
    for (int i = 0; i < size; i++) {
        hist[gray[i]]++;
    }
    
    /* Total pixels and sum */
    int total = size;
    int sum = 0;
    for (int i = 0; i < 256; i++) {
        sum += i * hist[i];
    }
    
    /* Find optimal threshold */
    int sumB = 0;
    int wB = 0;
    float max_var = 0;
    int threshold = 128;
    
    for (int t = 0; t < 256; t++) {
        wB += hist[t];
        if (wB == 0) continue;
        
        int wF = total - wB;
        if (wF == 0) break;
        
        sumB += t * hist[t];
        
        float mB = (float)sumB / wB;
        float mF = (float)(sum - sumB) / wF;
        
        float var = (float)wB * (float)wF * (mB - mF) * (mB - mF);
        
        if (var > max_var) {
            max_var = var;
            threshold = t;
        }
    }
    
    return threshold;
}

/* Adaptive local thresholding for better results with uneven lighting */
static void adaptive_threshold(uint8_t *gray, uint8_t *binary, int width, int height) {
    /* Use block-based adaptive thresholding */
    int block_size = 32;  /* Size of local blocks */
    
    for (int by = 0; by < height; by += block_size) {
        for (int bx = 0; bx < width; bx += block_size) {
            /* Calculate local threshold for this block */
            int bw = (bx + block_size * 2 < width) ? block_size * 2 : width - bx;
            int bh = (by + block_size * 2 < height) ? block_size * 2 : height - by;
            
            /* Compute local mean */
            int sum = 0, count = 0;
            for (int y = by; y < by + bh && y < height; y++) {
                for (int x = bx; x < bx + bw && x < width; x++) {
                    sum += gray[y * width + x];
                    count++;
                }
            }
            int local_mean = count > 0 ? sum / count : 128;
            
            /* Threshold slightly below mean for better black detection */
            int threshold = local_mean - 10;
            if (threshold < 20) threshold = 20;
            
            /* Apply threshold to central block */
            int ew = (bx + block_size < width) ? block_size : width - bx;
            int eh = (by + block_size < height) ? block_size : height - by;
            for (int y = by; y < by + eh; y++) {
                for (int x = bx; x < bx + ew; x++) {
                    /* 1 = black (dark), 0 = white (light) */
                    binary[y * width + x] = (gray[y * width + x] < threshold) ? 1 : 0;
                }
            }
        }
    }
}

static Image *image_read_pbm(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return NULL;
    
    Image *img = malloc(sizeof(Image));
    if (!img) { fclose(f); return NULL; }
    
    /* Read magic number */
    char magic[3];
    if (fread(magic, 1, 2, f) != 2) { free(img); fclose(f); return NULL; }
    magic[2] = 0;
    
    int format = 0;  /* 1=P1, 2=P2, 4=P4, 5=P5 */
    if (strcmp(magic, "P1") == 0) {
        format = 1;  /* ASCII PBM */
    } else if (strcmp(magic, "P4") == 0) {
        format = 4;  /* Binary PBM */
    } else if (strcmp(magic, "P2") == 0) {
        format = 2;  /* ASCII PGM (greyscale) */
    } else if (strcmp(magic, "P5") == 0) {
        format = 5;  /* Binary PGM (greyscale) */
    } else {
        free(img); fclose(f); return NULL;
    }
    
    /* Read dimensions */
    if (!skip_whitespace_and_comments(f)) { free(img); fclose(f); return NULL; }
    if (fscanf(f, "%d", &img->width) != 1) { free(img); fclose(f); return NULL; }
    if (!skip_whitespace_and_comments(f)) { free(img); fclose(f); return NULL; }
    if (fscanf(f, "%d", &img->height) != 1) { free(img); fclose(f); return NULL; }
    
    /* PGM formats have maxval */
    int maxval = 1;
    if (format == 2 || format == 5) {
        if (!skip_whitespace_and_comments(f)) { free(img); fclose(f); return NULL; }
        if (fscanf(f, "%d", &maxval) != 1) { free(img); fclose(f); return NULL; }
        if (maxval <= 0) maxval = 255;
    }
    
    /* Skip single whitespace after header */
    fgetc(f);
    
    /* Allocate pixel data */
    int npixels = img->width * img->height;
    img->data = malloc(npixels);
    if (!img->data) { free(img); fclose(f); return NULL; }
    
    if (format == 4) {
        /* Binary PBM: 8 pixels per byte, MSB first */
        int row_bytes = (img->width + 7) / 8;
        uint8_t *row = malloc(row_bytes);
        if (!row) { free(img->data); free(img); fclose(f); return NULL; }
        
        for (int y = 0; y < img->height; y++) {
            if (fread(row, 1, row_bytes, f) != (size_t)row_bytes) {
                free(row); free(img->data); free(img); fclose(f); return NULL;
            }
            for (int x = 0; x < img->width; x++) {
                int byte_idx = x / 8;
                int bit_idx = 7 - (x % 8);
                img->data[y * img->width + x] = (row[byte_idx] >> bit_idx) & 1;
            }
        }
        free(row);
    } else if (format == 1) {
        /* ASCII PBM: one character per pixel */
        for (int i = 0; i < npixels; i++) {
            int c;
            while ((c = fgetc(f)) != EOF && (c == ' ' || c == '\t' || c == '\n' || c == '\r'));
            if (c == EOF) { free(img->data); free(img); fclose(f); return NULL; }
            img->data[i] = (c == '1') ? 1 : 0;
        }
    } else if (format == 5) {
        /* Binary PGM: one byte per pixel (greyscale) */
        uint8_t *gray = malloc(npixels);
        if (!gray) { free(img->data); free(img); fclose(f); return NULL; }
        
        if (fread(gray, 1, npixels, f) != (size_t)npixels) {
            free(gray); free(img->data); free(img); fclose(f); return NULL;
        }
        
        /* Scale to 0-255 if maxval != 255 */
        if (maxval != 255) {
            for (int i = 0; i < npixels; i++) {
                gray[i] = (uint8_t)((gray[i] * 255) / maxval);
            }
        }
        
        /* Apply adaptive thresholding */
        adaptive_threshold(gray, img->data, img->width, img->height);
        free(gray);
    } else if (format == 2) {
        /* ASCII PGM: one number per pixel (greyscale) */
        uint8_t *gray = malloc(npixels);
        if (!gray) { free(img->data); free(img); fclose(f); return NULL; }
        
        for (int i = 0; i < npixels; i++) {
            int val;
            if (fscanf(f, "%d", &val) != 1) {
                free(gray); free(img->data); free(img); fclose(f); return NULL;
            }
            gray[i] = (uint8_t)((val * 255) / maxval);
        }
        
        /* Apply adaptive thresholding */
        adaptive_threshold(gray, img->data, img->width, img->height);
        free(gray);
    }
    
    fclose(f);
    return img;
}

static void image_free(Image *img) {
    if (img) {
        free(img->data);
        free(img);
    }
}

STATIC int image_get(Image *img, int x, int y) {
    if (x < 0 || x >= img->width || y < 0 || y >= img->height) return 0;
    return img->data[y * img->width + x];
}

/* ============================================================================
 * PHASE 2: QR CODE DETECTION AND DECODING
 * ============================================================================ */

/* QR Code finder pattern detection */
typedef struct {
    int x, y;             /* Center position */
    float module_size;    /* Estimated module size (geometric mean of x/y) */
    float module_size_x;  /* Horizontal module size (for non-uniform scaling) */
    float module_size_y;  /* Vertical module size (for non-uniform scaling) */
} FinderPattern;

/* Finder line - a scan line segment that matches 1:1:3:1:1 pattern */
typedef struct {
    int pos;              /* Position of scan line (y for horizontal, x for vertical) */
    int bound_min;        /* Start of pattern on scan line */
    int bound_max;        /* End of pattern on scan line */
} FinderLine;

/* Finder range - cluster of consecutive parallel finder lines (same finder pattern) */
typedef struct {
    int pos_min, pos_max;       /* Range of scan positions (y range for horiz, x range for vert) */
    int center_min, center_max; /* Min/max of centers (for estimating finder extent) */
    int center_sum;             /* Sum of centers for averaging */
    int count;                  /* Number of lines in this range */
    float module_sum;           /* Sum of module sizes for averaging */
} FinderRange;

/* Finder line/range collections */
#define MAX_FINDER_LINES 16000
#define INITIAL_FINDER_RANGES 128

/* Debug flag - set by -v -v */
static int debug_finder = 0;  // DEBUG

/* Debug mode - write intermediate files to disk */
static int debug_mode = 0;  // DEBUG
static int debug_qr_counter = 0;  /* Counter for debug file naming (BASIC style: 10, 20, ...) */  // DEBUG
  // DEBUG
/* Debug file output helpers */  // DEBUG
static void debug_dump_finders(FinderPattern *patterns, int count) {  // DEBUG
    if (!debug_mode) return;  // DEBUG
    char filename[64];  // DEBUG
    snprintf(filename, sizeof(filename), "debug_%02d_finders.txt", debug_qr_counter);  // DEBUG
    FILE *f = fopen(filename, "w");  // DEBUG
    if (!f) return;  // DEBUG
  // DEBUG
    fprintf(f, "# Finder patterns found: %d\n", count);  // DEBUG
    fprintf(f, "# Format: index x y module_size\n");  // DEBUG
    for (int i = 0; i < count; i++) {  // DEBUG
        fprintf(f, "%3d  %4d %4d  %.2f\n",  // DEBUG
                i, patterns[i].x, patterns[i].y, patterns[i].module_size);  // DEBUG
    }  // DEBUG
    fclose(f);  // DEBUG
    fprintf(stderr, "  Debug: wrote %s\n", filename);  // DEBUG
}  // DEBUG

static void debug_dump_grid(uint8_t grid[MAX_QR_MODULES][MAX_QR_MODULES], int size,  // DEBUG
                            int version, const char *stage) {  // DEBUG
    if (!debug_mode) return;  // DEBUG
    char filename[64];  // DEBUG
    snprintf(filename, sizeof(filename), "debug_%02d_%s.txt", debug_qr_counter, stage);  // DEBUG
    FILE *f = fopen(filename, "w");  // DEBUG
    if (!f) return;  // DEBUG
  // DEBUG
    fprintf(f, "# QR grid %s: version=%d size=%d\n", stage, version, size);  // DEBUG
    fprintf(f, "# Legend: # = black (1), . = white (0)\n\n");  // DEBUG
  // DEBUG
    /* Header with column numbers */  // DEBUG
    fprintf(f, "    ");  // DEBUG
    for (int x = 0; x < size; x++) {  // DEBUG
        if (x % 10 == 0) fprintf(f, "%d", (x / 10) % 10);  // DEBUG
        else fprintf(f, " ");  // DEBUG
    }  // DEBUG
    fprintf(f, "\n    ");  // DEBUG
    for (int x = 0; x < size; x++) {  // DEBUG
        fprintf(f, "%d", x % 10);  // DEBUG
    }  // DEBUG
    fprintf(f, "\n\n");  // DEBUG
  // DEBUG
    for (int y = 0; y < size; y++) {  // DEBUG
        fprintf(f, "%3d ", y);  // DEBUG
        for (int x = 0; x < size; x++) {  // DEBUG
            fprintf(f, "%c", grid[y][x] ? '#' : '.');  // DEBUG
        }  // DEBUG
        fprintf(f, "\n");  // DEBUG
    }  // DEBUG
    fclose(f);  // DEBUG
    fprintf(stderr, "  Debug: wrote %s\n", filename);  // DEBUG
}  // DEBUG

static void debug_dump_bits(uint8_t *data, int len, int version, int ecc_level) {  // DEBUG
    if (!debug_mode) return;  // DEBUG
    char filename[64];  // DEBUG
    snprintf(filename, sizeof(filename), "debug_%02d_bits.txt", debug_qr_counter);  // DEBUG
    FILE *f = fopen(filename, "w");  // DEBUG
    if (!f) return;  // DEBUG
  // DEBUG
    fprintf(f, "# Raw bits extracted: %d bytes, version=%d ecc=%d\n", len, version, ecc_level);  // DEBUG
    fprintf(f, "# Format: offset hex binary\n\n");  // DEBUG
  // DEBUG
    for (int i = 0; i < len; i++) {  // DEBUG
        fprintf(f, "%4d  %02x  ", i, data[i]);  // DEBUG
        for (int b = 7; b >= 0; b--) {  // DEBUG
            fprintf(f, "%c", (data[i] >> b) & 1 ? '1' : '0');  // DEBUG
        }  // DEBUG
        if (i < 16) {  // DEBUG
            /* Annotate first bytes: mode indicator, char count, etc. */  // DEBUG
            if (i == 0) fprintf(f, "  # mode + start of count");  // DEBUG
        }  // DEBUG
        fprintf(f, "\n");  // DEBUG
    }  // DEBUG
    fclose(f);  // DEBUG
    fprintf(stderr, "  Debug: wrote %s\n", filename);  // DEBUG
}  // DEBUG

static void debug_dump_codewords(uint8_t *raw, int raw_len, uint8_t *deint, int deint_len,  // DEBUG
                                  int version, int ecc_level) {  // DEBUG
    if (!debug_mode) return;  // DEBUG
    char filename[64];  // DEBUG
    snprintf(filename, sizeof(filename), "debug_%02d_codewords.txt", debug_qr_counter);  // DEBUG
    FILE *f = fopen(filename, "w");  // DEBUG
    if (!f) return;  // DEBUG
  // DEBUG
    fprintf(f, "# Codewords: version=%d ecc=%d\n", version, ecc_level);  // DEBUG
    fprintf(f, "# Raw (interleaved): %d bytes\n", raw_len);  // DEBUG
    fprintf(f, "# Deinterleaved data: %d bytes\n\n", deint_len);  // DEBUG
  // DEBUG
    fprintf(f, "## Raw codewords (as extracted from zigzag):\n");  // DEBUG
    for (int i = 0; i < raw_len; i++) {  // DEBUG
        if (i % 16 == 0) fprintf(f, "%04d: ", i);  // DEBUG
        fprintf(f, "%02x ", raw[i]);  // DEBUG
        if (i % 16 == 15) fprintf(f, "\n");  // DEBUG
    }  // DEBUG
    if (raw_len % 16 != 0) fprintf(f, "\n");  // DEBUG
  // DEBUG
    fprintf(f, "\n## Deinterleaved data codewords:\n");  // DEBUG
    for (int i = 0; i < deint_len; i++) {  // DEBUG
        if (i % 16 == 0) fprintf(f, "%04d: ", i);  // DEBUG
        fprintf(f, "%02x ", deint[i]);  // DEBUG
        if (i % 16 == 15) fprintf(f, "\n");  // DEBUG
    }  // DEBUG
    if (deint_len % 16 != 0) fprintf(f, "\n");  // DEBUG
  // DEBUG
    fclose(f);  // DEBUG
    fprintf(stderr, "  Debug: wrote %s\n", filename);  // DEBUG
}  // DEBUG

static void debug_dump_decoded(const char *content, int len, char type, int index, int total) {  // DEBUG
    if (!debug_mode) return;  // DEBUG
    char filename[64];  // DEBUG
    snprintf(filename, sizeof(filename), "debug_%02d_decoded.txt", debug_qr_counter);  // DEBUG
    FILE *f = fopen(filename, "w");  // DEBUG
    if (!f) return;  // DEBUG
  // DEBUG
    fprintf(f, "# Decoded QR content: %d characters\n", len);  // DEBUG
    fprintf(f, "# Chunk: %c%d/%d\n\n", type, index, total);  // DEBUG
    fprintf(f, "%s\n", content);  // DEBUG
  // DEBUG
    fclose(f);  // DEBUG
    fprintf(stderr, "  Debug: wrote %s\n", filename);  // DEBUG
}  // DEBUG

/* Debug dump for chunk list (after collection/dedup) */
static void debug_dump_chunks(ChunkList *cl, const char *stage) {  // DEBUG
    if (!debug_mode) return;  // DEBUG
    char filename[64];  // DEBUG
    snprintf(filename, sizeof(filename), "debug_60_chunks_%s.txt", stage);  // DEBUG
    FILE *f = fopen(filename, "w");  // DEBUG
    if (!f) return;  // DEBUG
  // DEBUG
    fprintf(f, "# Chunks %s: %d total\n\n", stage, cl->count);  // DEBUG
    fprintf(f, "# Type  Index  Total  Size    First 16 bytes (hex)\n");  // DEBUG
    fprintf(f, "# ----  -----  -----  ----    --------------------\n");  // DEBUG
  // DEBUG
    for (int i = 0; i < cl->count; i++) {  // DEBUG
        Chunk *c = &cl->chunks[i];  // DEBUG
        fprintf(f, "  %c     %3d    %3d    %4d    ",  // DEBUG
                c->type, c->index, c->total, c->data_len);  // DEBUG
        for (int j = 0; j < 16 && j < c->data_len; j++) {  // DEBUG
            fprintf(f, "%02x ", c->data[j]);  // DEBUG
        }  // DEBUG
        if (c->data_len > 16) fprintf(f, "...");  // DEBUG
        fprintf(f, "\n");  // DEBUG
    }  // DEBUG
  // DEBUG
    fclose(f);  // DEBUG
    fprintf(stderr, "Debug: wrote %s\n", filename);  // DEBUG
}  // DEBUG

/* Debug dump for erasure recovery - extended version with recovered data */
static void debug_dump_erasure_ex(int total_n, int total_p, int have_n, int have_p,  // DEBUG
                                   int *missing_indices, int missing_count, int recovered,  // DEBUG
                                   uint8_t **recovered_data, int *recovered_lens,  // DEBUG
                                   const char *method) {  // DEBUG
    if (!debug_mode) return;  // DEBUG
    FILE *f = fopen("debug_70_erasure.txt", "w");  // DEBUG
    if (!f) return;  // DEBUG
  // DEBUG
    fprintf(f, "# Erasure Recovery Status\n\n");  // DEBUG
    fprintf(f, "Total N chunks expected: %d\n", total_n);  // DEBUG
    fprintf(f, "Total P chunks expected: %d\n", total_p);  // DEBUG
    fprintf(f, "N chunks available: %d\n", have_n);  // DEBUG
    fprintf(f, "P chunks available: %d\n", have_p);  // DEBUG
    fprintf(f, "Missing N chunks: %d\n", missing_count);  // DEBUG
  // DEBUG
    if (missing_count > 0) {  // DEBUG
        fprintf(f, "Missing indices: ");  // DEBUG
        for (int i = 0; i < missing_count; i++) {  // DEBUG
            fprintf(f, "%s%d", i > 0 ? ", " : "", missing_indices[i]);  // DEBUG
        }  // DEBUG
        fprintf(f, "\n");  // DEBUG
    }  // DEBUG
  // DEBUG
    fprintf(f, "\nRecovery: %s\n", recovered ? "SUCCEEDED" : "FAILED");  // DEBUG
    if (method) {  // DEBUG
        fprintf(f, "Method: %s\n", method);  // DEBUG
    }  // DEBUG
  // DEBUG
    /* Dump recovered chunk data */  // DEBUG
    if (recovered && recovered_data && missing_count > 0) {  // DEBUG
        fprintf(f, "\n## Recovered Chunk Data:\n\n");  // DEBUG
        for (int i = 0; i < missing_count; i++) {  // DEBUG
            if (recovered_data[i] && recovered_lens[i] > 0) {  // DEBUG
                fprintf(f, "### N%d (recovered, %d bytes):\n", missing_indices[i], recovered_lens[i]);  // DEBUG
                for (int j = 0; j < recovered_lens[i]; j++) {  // DEBUG
                    if (j % 16 == 0) fprintf(f, "%04x: ", j);  // DEBUG
                    fprintf(f, "%02x ", recovered_data[i][j]);  // DEBUG
                    if (j % 16 == 15) {  // DEBUG
                        fprintf(f, " |");  // DEBUG
                        for (int k = j - 15; k <= j; k++) {  // DEBUG
                            char c = recovered_data[i][k];  // DEBUG
                            fprintf(f, "%c", (c >= 32 && c < 127) ? c : '.');  // DEBUG
                        }  // DEBUG
                        fprintf(f, "|\n");  // DEBUG
                    }  // DEBUG
                }  // DEBUG
                if (recovered_lens[i] % 16 != 0) fprintf(f, "\n");  // DEBUG
                fprintf(f, "\n");  // DEBUG
            }  // DEBUG
        }  // DEBUG
    }  // DEBUG
  // DEBUG
    fclose(f);  // DEBUG
    fprintf(stderr, "Debug: wrote debug_70_erasure.txt\n");  // DEBUG
}  // DEBUG

/* Simple version for initial state / failure cases */
static void debug_dump_erasure(int total_n, int total_p, int have_n, int have_p,  // DEBUG
                                int *missing_indices, int missing_count, int recovered) {  // DEBUG
    debug_dump_erasure_ex(total_n, total_p, have_n, have_p, missing_indices,  // DEBUG
                          missing_count, recovered, NULL, NULL, NULL);  // DEBUG
}  // DEBUG

/* Debug dump for assembled binary data */
static void debug_dump_assembled(uint8_t *data, int len) {  // DEBUG
    if (!debug_mode) return;  // DEBUG
  // DEBUG
    /* Write binary dump */  // DEBUG
    FILE *f = fopen("debug_80_assembled.bin", "wb");  // DEBUG
    if (f) {  // DEBUG
        fwrite(data, 1, len, f);  // DEBUG
        fclose(f);  // DEBUG
        fprintf(stderr, "Debug: wrote debug_80_assembled.bin (%d bytes)\n", len);  // DEBUG
    }  // DEBUG
  // DEBUG
    /* Write hex dump for inspection */  // DEBUG
    f = fopen("debug_80_assembled.txt", "w");  // DEBUG
    if (f) {  // DEBUG
        fprintf(f, "# Assembled data: %d bytes\n", len);  // DEBUG
        fprintf(f, "# First 2 bytes: %02x %02x (%s)\n\n",  // DEBUG
                len >= 2 ? data[0] : 0, len >= 2 ? data[1] : 0,  // DEBUG
                (len >= 2 && data[0] == 0x1f && data[1] == 0x8b) ? "gzip" : "raw");  // DEBUG
  // DEBUG
        for (int i = 0; i < len; i++) {  // DEBUG
            if (i % 16 == 0) fprintf(f, "%04x: ", i);  // DEBUG
            fprintf(f, "%02x ", data[i]);  // DEBUG
            if (i % 16 == 15) {  // DEBUG
                fprintf(f, " |");  // DEBUG
                for (int j = i - 15; j <= i; j++) {  // DEBUG
                    char c = data[j];  // DEBUG
                    fprintf(f, "%c", (c >= 32 && c < 127) ? c : '.');  // DEBUG
                }  // DEBUG
                fprintf(f, "|\n");  // DEBUG
            }  // DEBUG
        }  // DEBUG
        if (len % 16 != 0) {  // DEBUG
            int pad = 16 - (len % 16);  // DEBUG
            for (int i = 0; i < pad; i++) fprintf(f, "   ");  // DEBUG
            fprintf(f, " |");  // DEBUG
            int start = len - (len % 16);  // DEBUG
            for (int j = start; j < len; j++) {  // DEBUG
                char c = data[j];  // DEBUG
                fprintf(f, "%c", (c >= 32 && c < 127) ? c : '.');  // DEBUG
            }  // DEBUG
            fprintf(f, "|\n");  // DEBUG
        }  // DEBUG
  // DEBUG
        fclose(f);  // DEBUG
        fprintf(stderr, "Debug: wrote debug_80_assembled.txt\n");  // DEBUG
    }  // DEBUG
}  // DEBUG

/* Debug dump for decompressed data */
static void debug_dump_decompressed(uint8_t *data, int len) {  // DEBUG
    if (!debug_mode) return;  // DEBUG
  // DEBUG
    /* Write binary dump */  // DEBUG
    FILE *f = fopen("debug_90_decompressed.bin", "wb");  // DEBUG
    if (f) {  // DEBUG
        fwrite(data, 1, len, f);  // DEBUG
        fclose(f);  // DEBUG
        fprintf(stderr, "Debug: wrote debug_90_decompressed.bin (%d bytes)\n", len);  // DEBUG
    }  // DEBUG
  // DEBUG
    /* Write text preview */  // DEBUG
    f = fopen("debug_90_decompressed.txt", "w");  // DEBUG
    if (f) {  // DEBUG
        fprintf(f, "# Decompressed data: %d bytes\n\n", len);  // DEBUG
  // DEBUG
        /* Check if it's mostly text */  // DEBUG
        int printable = 0;  // DEBUG
        for (int i = 0; i < len && i < 1000; i++) {  // DEBUG
            if ((data[i] >= 32 && data[i] < 127) || data[i] == '\n' || data[i] == '\r' || data[i] == '\t') {  // DEBUG
                printable++;  // DEBUG
            }  // DEBUG
        }  // DEBUG
  // DEBUG
        if (printable > len * 0.8 || printable > 800) {  // DEBUG
            fprintf(f, "## Content (text):\n");  // DEBUG
            fwrite(data, 1, len, f);  // DEBUG
        } else {  // DEBUG
            fprintf(f, "## Content (hex, first 256 bytes):\n");  // DEBUG
            for (int i = 0; i < len && i < 256; i++) {  // DEBUG
                if (i % 16 == 0) fprintf(f, "%04x: ", i);  // DEBUG
                fprintf(f, "%02x ", data[i]);  // DEBUG
                if (i % 16 == 15) fprintf(f, "\n");  // DEBUG
            }  // DEBUG
            if (len > 256) fprintf(f, "\n... (%d more bytes)\n", len - 256);  // DEBUG
        }  // DEBUG
  // DEBUG
        fclose(f);  // DEBUG
        fprintf(stderr, "Debug: wrote debug_90_decompressed.txt\n");  // DEBUG
    }  // DEBUG
}  // DEBUG

/* Check if a run of pixels matches the 1:1:3:1:1 finder pattern ratio 
 * tolerance: 0.0 to 1.0 (e.g., 0.5 = 50% tolerance)
 * Returns module size estimate or 0 if not a match */
STATIC float check_finder_ratio_ex(int *counts, float tolerance) {
    int total = 0;
    for (int i = 0; i < 5; i++) {
        if (counts[i] == 0) return 0;
        total += counts[i];
    }
    
    /* Need minimum size to be meaningful */
    if (total < 7) return 0;
    
    float module = total / 7.0f;
    
    /* Use adaptive tolerance based on the pattern size
     * For larger finder patterns, we can be more lenient with distortion
     * For smaller patterns, we need to be more strict to avoid false positives */
    float adaptive_tolerance = tolerance;
    if (module > 3.0f) {
        /* Increase tolerance for larger patterns, up to 1.5 times the base tolerance */
        adaptive_tolerance = tolerance * (1.0f + (module - 3.0f) * 0.1f);
        if (adaptive_tolerance > tolerance * 1.5f) {
            adaptive_tolerance = tolerance * 1.5f;
        }
    }
    
    float variance = module * adaptive_tolerance;
    
    /* Use a more flexible check for the center (3x) portion */
    float center_min = 2.0f * module;  /* Allow as low as 2x instead of 3x for severe distortion */
    float center_max = 4.0f * module;  /* Allow up to 4x for severe distortion */
    
    int ok = (counts[0] >= module - variance && counts[0] <= module + variance * 1.2f) &&
             (counts[1] >= module - variance && counts[1] <= module + variance * 1.2f) &&
             (counts[2] >= center_min && counts[2] <= center_max) &&
             (counts[3] >= module - variance && counts[3] <= module + variance * 1.2f) &&
             (counts[4] >= module - variance && counts[4] <= module + variance * 1.2f);
    
    /* Secondary check for aspect ratio - the 1:1:3:1:1 finder pattern should maintain 
     * a specific relationship even when distorted. The side segments (1+1+1+1=4) 
     * should be about 4/7 of the total, and the center segment should be about 3/7. */
    float side_ratio = (float)(counts[0] + counts[1] + counts[3] + counts[4]) / total;
    float center_ratio = (float)counts[2] / total;
    
    int ratio_ok = (side_ratio >= 0.4f && side_ratio <= 0.7f) &&
                   (center_ratio >= 0.25f && center_ratio <= 0.55f);
    
    /* Both the individual segment test and ratio test must pass */
    int final_ok = ok && ratio_ok;
    
    if (debug_finder >= 2) {  // DEBUG
        /* Only show patterns that are close to valid */
        float c2_expected = 3 * module;
        int roughly_valid = (counts[2] > c2_expected * 0.5 && counts[2] < c2_expected * 1.5);
        if (roughly_valid || ok) {
            fprintf(stderr, "  ratio: [%d,%d,%d,%d,%d] total=%d module=%.1f side_ratio=%.2f center_ratio=%.2f tol=%.2f -> %s\n",
                    counts[0], counts[1], counts[2], counts[3], counts[4],
                    total, module, side_ratio, center_ratio, adaptive_tolerance, 
                    final_ok ? "YES" : "no");
        }
    }
    
    return final_ok ? module : 0;
}

/* Legacy wrapper with 50% tolerance - kept for potential future use */
static int check_finder_ratio(int *counts) __attribute__((unused));
static int check_finder_ratio(int *counts) {
    return check_finder_ratio_ex(counts, 0.5f) > 0;
}

/* Perform vertical cross-check at given position and return module size (0 if failed) */
static float vertical_crosscheck(Image *img, int cx, int cy, float h_module __attribute__((unused)), int *out_cy) {
    int h = img->height;
    int vc[5] = {0};
    int vstate = 2;
    int vy_top = cy, vy_bot = cy;
    
    /* Scan upward from cy */
    for (int dy = 0; cy - dy >= 0; dy++) {
        int pix = image_get(img, cx, cy - dy);
        if (vstate == 2) {
            if (pix) { vc[2]++; vy_top = cy - dy; }
            else { vstate = 1; vc[1]++; }
        } else if (vstate == 1) {
            if (!pix) { vc[1]++; }
            else { vstate = 0; vc[0]++; vy_top = cy - dy; }
        } else {
            if (pix) { vc[0]++; vy_top = cy - dy; }
            else break;
        }
    }
    
    vstate = 2;
    /* Scan downward from cy */
    for (int dy = 1; cy + dy < h; dy++) {
        int pix = image_get(img, cx, cy + dy);
        if (vstate == 2) {
            if (pix) { vc[2]++; vy_bot = cy + dy; }
            else { vstate = 3; vc[3]++; }
        } else if (vstate == 3) {
            if (!pix) { vc[3]++; }
            else { vstate = 4; vc[4]++; vy_bot = cy + dy; }
        } else {
            if (pix) { vc[4]++; vy_bot = cy + dy; }
            else break;
        }
    }
    
            /* Use much higher tolerance for vertical checks with severely rotated codes */
            float v_module = check_finder_ratio_ex(vc, 0.8f);
            if (v_module > 0) {
                *out_cy = (vy_top + vy_bot) / 2;
                return v_module;
            }
    return 0;
}

/* Perform horizontal cross-check at given position and return module size (0 if failed) */
static float horizontal_crosscheck(Image *img, int cx, int cy, float v_module __attribute__((unused)), int *out_cx) {
    int w = img->width;
    int hc[5] = {0};
    int hstate = 2;
    int hx_left = cx, hx_right = cx;
    
    /* Scan leftward from cx */
    for (int dx = 0; cx - dx >= 0; dx++) {
        int pix = image_get(img, cx - dx, cy);
        if (hstate == 2) {
            if (pix) { hc[2]++; hx_left = cx - dx; }
            else { hstate = 1; hc[1]++; }
        } else if (hstate == 1) {
            if (!pix) { hc[1]++; }
            else { hstate = 0; hc[0]++; hx_left = cx - dx; }
        } else {
            if (pix) { hc[0]++; hx_left = cx - dx; }
            else break;
        }
    }
    
    hstate = 2;
    /* Scan rightward from cx */
    for (int dx = 1; cx + dx < w; dx++) {
        int pix = image_get(img, cx + dx, cy);
        if (hstate == 2) {
            if (pix) { hc[2]++; hx_right = cx + dx; }
            else { hstate = 3; hc[3]++; }
        } else if (hstate == 3) {
            if (!pix) { hc[3]++; }
            else { hstate = 4; hc[4]++; hx_right = cx + dx; }
        } else {
            if (pix) { hc[4]++; hx_right = cx + dx; }
            else break;
        }
    }
    
    /* Use much higher tolerance for horizontal checks with severely rotated codes */
    float h_module = check_finder_ratio_ex(hc, 0.8f);
    if (h_module > 0) {
        *out_cx = (hx_left + hx_right) / 2;
        return h_module;
    }
    return 0;
}

/* Add a finder line to a list */
static int add_finder_line(FinderLine *lines, int count, int max_lines,
                           int pos, int bound_min, int bound_max) {
    if (count < max_lines) {
        lines[count].pos = pos;
        lines[count].bound_min = bound_min;
        lines[count].bound_max = bound_max;
        return count + 1;
    }
    return count;
}

/* Check if two ranges overlap significantly.
 * Requires at least 50% of the smaller range to be in the overlap.
 * This prevents merging adjacent but distinct patterns that barely touch. */
static int ranges_overlap(int a_min, int a_max, int b_min, int b_max) {
    int overlap_min = (a_min > b_min) ? a_min : b_min;
    int overlap_max = (a_max < b_max) ? a_max : b_max;
    if (overlap_min > overlap_max) return 0;  /* No overlap */
    
    int overlap_size = overlap_max - overlap_min;
    int a_size = a_max - a_min;
    int b_size = b_max - b_min;
    int smaller_size = (a_size < b_size) ? a_size : b_size;
    
    /* Require overlap to be at least 50% of the smaller range */
    return overlap_size >= smaller_size / 2;
}

/* Cluster finder lines into ranges using consecutive-overlap approach.
 * Lines must be sorted by pos. A line joins a cluster if:
 * 1. Its pos is exactly consecutive (pos == last_pos + 1)
 * 2. Its bounds overlap with the last line's bounds
 * This naturally handles rotation (bounds drift gradually) while
 * rejecting unrelated patterns (bounds don't overlap).
 * 
 * The ranges array is dynamically grown as needed. Caller must pass pointers
 * to the array and its capacity, and must free the array when done. */
static int cluster_finder_lines(FinderLine *lines, int nlines, 
                                  FinderRange **ranges_ptr, int *capacity_ptr) {
    if (nlines == 0) return 0;
    
    FinderRange *ranges = *ranges_ptr;
    int capacity = *capacity_ptr;
    int nranges = 0;
    
    /* Track active clusters - clusters that ended at recent positions.
     * Each active cluster remembers its last line's bounds for overlap checking,
     * and how many positions ago it was last updated (gap_count). */
    typedef struct {
        int range_idx;      /* Index into ranges array */
        int last_bound_min; /* Last line's bound_min */
        int last_bound_max; /* Last line's bound_max */
        int last_pos;       /* Position where this cluster was last updated */
    } ActiveCluster;
    
    /* Allow gaps of up to MAX_GAP positions in a cluster.
     * This handles rotated finder patterns where some scan lines 
     * don't produce valid 1:1:3:1:1 ratios. */
    #define MAX_GAP 3
    
    ActiveCluster active[128];
    int nactive = 0;
    int last_pos = -999;  /* Position of last processed line */
    
    for (int i = 0; i < nlines; i++) {
        FinderLine *line = &lines[i];
        
        /* If we've moved to a new position, prune clusters that are too old */
        if (line->pos != last_pos) {
            /* Remove clusters that haven't been updated in MAX_GAP+1 positions */
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
        
        /* Try to find an active cluster whose last bounds overlap with this line.
         * For clusters with gaps, also check that the line center is close to
         * the cluster's running average center to prevent merging distant patterns. */
        int matched = -1;
        int line_center = (line->bound_min + line->bound_max) / 2;
        for (int a = 0; a < nactive; a++) {
            if (ranges_overlap(line->bound_min, line->bound_max,
                               active[a].last_bound_min, active[a].last_bound_max)) {
                /* Check if there's a gap since last update */
                int gap = line->pos - active[a].last_pos;
                if (gap > 1) {
                    /* With a gap, require the line center to be within the cluster's
                     * accumulated bounds. This prevents merging separate patterns
                     * that happen to have overlapping bounds after drifting. */
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
        
        if (matched >= 0) {
            /* Add to existing cluster */
            int ri = active[matched].range_idx;
            FinderRange *r = &ranges[ri];
            r->pos_max = line->pos;
            if (line->bound_min < r->center_min) r->center_min = line->bound_min;
            if (line->bound_max > r->center_max) r->center_max = line->bound_max;
            r->center_sum += (line->bound_min + line->bound_max) / 2;
            r->module_sum += (line->bound_max - line->bound_min) / 7.0f;
            r->count++;
            /* Update the cluster's bounds and position */
            active[matched].last_bound_min = line->bound_min;
            active[matched].last_bound_max = line->bound_max;
            active[matched].last_pos = line->pos;
        } else if (nactive < 128) {
            /* Start a new cluster - grow array if needed */
            if (nranges >= capacity) {
                capacity *= 2;
                ranges = realloc(ranges, capacity * sizeof(FinderRange));
                if (!ranges) {
                    fprintf(stderr, "Out of memory growing ranges array\n");
                    *ranges_ptr = NULL;
                    *capacity_ptr = 0;
                    return 0;
                }
                *ranges_ptr = ranges;
                *capacity_ptr = capacity;
            }
            FinderRange *r = &ranges[nranges];
            r->pos_min = r->pos_max = line->pos;
            r->center_min = line->bound_min;
            r->center_max = line->bound_max;
            r->center_sum = (line->bound_min + line->bound_max) / 2;
            r->module_sum = (line->bound_max - line->bound_min) / 7.0f;
            r->count = 1;
            /* Add to active clusters */
            active[nactive].range_idx = nranges;
            active[nactive].last_bound_min = line->bound_min;
            active[nactive].last_bound_max = line->bound_max;
            active[nactive].last_pos = line->pos;
            nactive++;
            nranges++;
        }
    }
    
    /* Validate ranges and compact - require minimum line count */
    int valid_count = 0;
    for (int i = 0; i < nranges; i++) {
        FinderRange *r = &ranges[i];
        float avg_module = r->module_sum / r->count;
        int pos_span = r->pos_max - r->pos_min + 1;
        
        /* Require enough lines to be confident this is a real finder.
         * A finder pattern is 7 modules wide, so we expect ~7*module_size lines.
         * For rotated patterns, not every scan line produces a valid 1:1:3:1:1 ratio,
         * so we only require ~1/5 of the expected lines. */
        int min_lines = (int)(avg_module * 7 * 0.2f);
        if (min_lines < 3) min_lines = 3;
        
        if (debug_finder && r->count >= 3) {  // DEBUG
            fprintf(stderr, "    Cluster: pos=[%d-%d] bounds=[%d-%d] count=%d module=%.1f min_lines=%d span=%d -> %s\n",
                    r->pos_min, r->pos_max, r->center_min, r->center_max,
                    r->count, avg_module, min_lines, pos_span,
                    (r->count >= min_lines && pos_span >= 5) ? "ACCEPT" : "REJECT");
        }
        if (r->count >= min_lines && pos_span >= 5) {
            if (debug_finder) {  // DEBUG
                fprintf(stderr, "    Range: pos=[%d-%d] bounds=[%d-%d] avg_center=%d count=%d module=%.1f\n",  // DEBUG
                        r->pos_min, r->pos_max, r->center_min, r->center_max,  // DEBUG
                        r->center_sum / r->count, r->count, avg_module);  // DEBUG
            }  // DEBUG
            if (valid_count != i) {
                ranges[valid_count] = *r;
            }
            valid_count++;
        }
    }
    
    return valid_count;
}

/* Compare function for sorting finder lines by position, then center */
static int compare_finder_lines_by_pos(const void *a, const void *b) {
    const FinderLine *la = (const FinderLine *)a;
    const FinderLine *lb = (const FinderLine *)b;
    return la->pos - lb->pos;
}

/* Compare function for sorting finder lines by center, then position */
static int compare_finder_lines_by_bound_min(const void *a, const void *b) {
    const FinderLine *la = (const FinderLine *)a;
    const FinderLine *lb = (const FinderLine *)b;
    if (la->bound_min != lb->bound_min) return la->bound_min - lb->bound_min;
    return la->pos - lb->pos;
}

/* Find all finder patterns using range intersection method (zbar-style) */
static int find_finder_patterns(Image *img, FinderPattern *patterns, int max_patterns) {
    int w = img->width;
    int h = img->height;
    
    /* Allocate line storage */
    FinderLine *h_lines = malloc(MAX_FINDER_LINES * sizeof(FinderLine));
    FinderLine *v_lines = malloc(MAX_FINDER_LINES * sizeof(FinderLine));
    int h_ranges_cap = INITIAL_FINDER_RANGES;
    int v_ranges_cap = INITIAL_FINDER_RANGES;
    FinderRange *h_ranges = malloc(h_ranges_cap * sizeof(FinderRange));
    FinderRange *v_ranges = malloc(v_ranges_cap * sizeof(FinderRange));
    
    if (!h_lines || !v_lines || !h_ranges || !v_ranges) {
        free(h_lines); free(v_lines); free(h_ranges); free(v_ranges);
        return 0;
    }
    
    int nh_lines = 0, nv_lines = 0;
    
    /* ====== PASS 1: Collect horizontal finder lines ====== */
    for (int y = 0; y < h; y++) {
        int counts[5] = {0};
        int state = 0;
        
        for (int x = 0; x < w; x++) {
            int pixel = image_get(img, x, y);
            
            if (state == 0) {
                if (pixel) { state = 1; counts[0] = 1; }
            } else if (state == 1) {
                if (pixel) counts[0]++;
                else { state = 2; counts[1] = 1; }
            } else if (state == 2) {
                if (!pixel) counts[1]++;
                else { state = 3; counts[2] = 1; }
            } else if (state == 3) {
                if (pixel) counts[2]++;
                else { state = 4; counts[3] = 1; }
            } else if (state == 4) {
                if (!pixel) counts[3]++;
                else { state = 5; counts[4] = 1; }
            } else if (state == 5) {
                if (pixel) counts[4]++;
                else {
                    float module = check_finder_ratio_ex(counts, 0.8f);
                    if (module > 0) {
                        /* Record the bounds of the pattern */
                        int total_width = counts[0] + counts[1] + counts[2] + counts[3] + counts[4];
                        int bound_min = x - total_width;
                        int bound_max = x - 1;  /* x is now on the white pixel after the pattern */
                        nh_lines = add_finder_line(h_lines, nh_lines, MAX_FINDER_LINES,
                                                   y, bound_min, bound_max);
                    }
                    counts[0] = counts[2];
                    counts[1] = counts[3];
                    counts[2] = counts[4];
                    counts[3] = 1;
                    counts[4] = 0;
                    state = 4;
                }
            }
        }
    }
    
    /* ====== PASS 2: Collect vertical finder lines ====== */
    for (int x = 0; x < w; x++) {
        int counts[5] = {0};
        int state = 0;
        
        for (int y = 0; y < h; y++) {
            int pixel = image_get(img, x, y);
            
            if (state == 0) {
                if (pixel) { state = 1; counts[0] = 1; }
            } else if (state == 1) {
                if (pixel) counts[0]++;
                else { state = 2; counts[1] = 1; }
            } else if (state == 2) {
                if (!pixel) counts[1]++;
                else { state = 3; counts[2] = 1; }
            } else if (state == 3) {
                if (pixel) counts[2]++;
                else { state = 4; counts[3] = 1; }
            } else if (state == 4) {
                if (!pixel) counts[3]++;
                else { state = 5; counts[4] = 1; }
            } else if (state == 5) {
                if (pixel) counts[4]++;
                else {
                    float module = check_finder_ratio_ex(counts, 0.8f);
                    if (module > 0) {
                        /* Record the bounds of the pattern */
                        int total_width = counts[0] + counts[1] + counts[2] + counts[3] + counts[4];
                        int bound_min = y - total_width;
                        int bound_max = y - 1;  /* y is now on the white pixel after the pattern */
                        nv_lines = add_finder_line(v_lines, nv_lines, MAX_FINDER_LINES,
                                                   x, bound_min, bound_max);
                    }
                    counts[0] = counts[2];
                    counts[1] = counts[3];
                    counts[2] = counts[4];
                    counts[3] = 1;
                    counts[4] = 0;
                    state = 4;
                }
            }
        }
    }
    
    if (debug_finder) {  // DEBUG
        fprintf(stderr, "  Collected %d horizontal lines, %d vertical lines\n", nh_lines, nv_lines);  // DEBUG
        /* Debug: show first 50 horizontal lines */  // DEBUG
        for (int i = 0; i < nh_lines && i < 50; i++) {  // DEBUG
            fprintf(stderr, "    h_line: y=%d x=[%d-%d]\n",  // DEBUG
                        h_lines[i].pos, h_lines[i].bound_min, h_lines[i].bound_max);  // DEBUG
        }  // DEBUG
    }  // DEBUG
    
    /* ====== PASS 3: Sort by pos and cluster lines into ranges ====== */
    /* Sort by pos (scan position) so lines from same finder pattern are adjacent */
    qsort(h_lines, nh_lines, sizeof(FinderLine), compare_finder_lines_by_pos);
    qsort(v_lines, nv_lines, sizeof(FinderLine), compare_finder_lines_by_pos);
    
    if (debug_finder) {  // DEBUG
        fprintf(stderr, "  Horizontal ranges:\n");  // DEBUG
    }  // DEBUG
    int nh_ranges = cluster_finder_lines(h_lines, nh_lines, &h_ranges, &h_ranges_cap);
    if (debug_finder) {  // DEBUG
        fprintf(stderr, "  Vertical ranges:\n");  // DEBUG
    }  // DEBUG
    int nv_ranges = cluster_finder_lines(v_lines, nv_lines, &v_ranges, &v_ranges_cap);
    
    if (debug_finder) {  // DEBUG
        fprintf(stderr, "  Clustered into %d horizontal ranges, %d vertical ranges\n", nh_ranges, nv_ranges);  // DEBUG
    }  // DEBUG
    
    /* ====== PASS 4: Find range intersections ====== */
    int count = 0;
    
    for (int hi = 0; hi < nh_ranges && count < max_patterns; hi++) {
        FinderRange *hr = &h_ranges[hi];
        int h_center_x = hr->center_sum / hr->count;
        float h_module = hr->module_sum / hr->count;
        
        for (int vi = 0; vi < nv_ranges && count < max_patterns; vi++) {
            FinderRange *vr = &v_ranges[vi];
            int v_center_y = vr->center_sum / vr->count;
            float v_module = vr->module_sum / vr->count;
            
            /* Check if ranges overlap spatially */
            /* Horizontal range: y from pos_min to pos_max, x around h_center_x */
            /* Vertical range: x from pos_min to pos_max, y around v_center_y */
            
            /* Use adaptive tolerance based on module size - larger patterns can be more distorted
             * But keep tolerance reasonable to avoid matching wrong finder patterns.
             * A finder pattern is 7 modules wide, so the center should be within ~3.5 modules
             * of the range. Use 1.5 modules tolerance to allow for some distortion while
             * avoiding matching wrong finders in dense multi-QR images. */
            float x_tolerance = h_module * 1.5f;
            float y_tolerance = v_module * 1.5f;
            
            /* Does the vertical range's x span include h_center_x? */
            int vx_min = vr->pos_min;
            int vx_max = vr->pos_max;
            
            if (h_center_x < vx_min - x_tolerance || h_center_x > vx_max + x_tolerance)
                continue;
            
            /* Does the horizontal range's y span include v_center_y? */
            int hy_min = hr->pos_min;
            int hy_max = hr->pos_max;
            
            if (v_center_y < hy_min - y_tolerance || v_center_y > hy_max + y_tolerance)
                continue;
            
            /* Module sizes can vary significantly due to perspective distortion
             * Allow a much wider range, especially for larger modules */
            float module_ratio = (h_module > v_module) ? h_module / v_module : v_module / h_module;
            float max_ratio = 2.5f;  /* Increased from 2.0f */
            
            /* For larger modules, allow even greater distortion ratios */
            if (h_module > 3.0f || v_module > 3.0f) {
                max_ratio = 3.0f;  /* Allow up to 3x difference for large modules */
            }
            
            if (module_ratio > max_ratio)
                continue;
            
            /* Found intersection! Compute finder center */
            /* Use weighted center calculation for more accurate center estimation
             * with distorted patterns. Weight by proximity to the center for better results
             * with perspective distortion */
            
            /* For x-coordinate, use the average center value from horizontal range */
            /* (horizontal scan finds where along x-axis the pattern center is) */
            int finder_x = hr->center_sum / hr->count;
            
            /* For y-coordinate, use the average center value from vertical range */
            /* (vertical scan finds where along y-axis the pattern center is) */
            int finder_y = vr->center_sum / vr->count;
            
            /* Store both horizontal and vertical module sizes to capture aspect ratio distortion */
            float finder_module = fsqrt(h_module * v_module);  /* Geometric mean for overall size */
            
            /* Check for duplicate */
            int is_dup = 0;
            for (int i = 0; i < count; i++) {
                int dx = patterns[i].x - finder_x;
                int dy = patterns[i].y - finder_y;
                if (dx*dx + dy*dy < finder_module * finder_module * 16) {
                    is_dup = 1;
                    break;
                }
            }
            
            if (!is_dup) {
                patterns[count].x = finder_x;
                patterns[count].y = finder_y;
                patterns[count].module_size = finder_module;
                patterns[count].module_size_x = h_module;
                patterns[count].module_size_y = v_module;
                
                if (debug_finder) {  // DEBUG
                    fprintf(stderr, "  FOUND FINDER #%d at (%d,%d) module=%.1f (x=%.1f, y=%.1f) "  // DEBUG
                            "h_range=[%d-%d] v_range=[%d-%d]\n",  // DEBUG
                            count, finder_x, finder_y, finder_module, h_module, v_module,  // DEBUG
                            hr->pos_min, hr->pos_max, vr->pos_min, vr->pos_max);  // DEBUG
                }  // DEBUG
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

/* QR code version/size info */
STATIC int qr_version_to_size(int version) {
    return 17 + 4 * version;
}



/* Extract QR code grid from image */
typedef struct {
    uint8_t modules[MAX_QR_MODULES][MAX_QR_MODULES];
    int size;
    int version;
} QRCode;

/* Alignment pattern positions for each version */
static const int alignment_positions[][8] = {
    {0},                                        /* Version 1 - no alignment */
    {6, 18, 0},                                 /* Version 2 */
    {6, 22, 0},                                 /* Version 3 */
    {6, 26, 0},                                 /* Version 4 */
    {6, 30, 0},                                 /* Version 5 */
    {6, 34, 0},                                 /* Version 6 */
    {6, 22, 38, 0},                             /* Version 7 */
    {6, 24, 42, 0},                             /* Version 8 */
    {6, 26, 46, 0},                             /* Version 9 */
    {6, 28, 50, 0},                             /* Version 10 */
    {6, 30, 54, 0},                             /* Version 11 */
    {6, 32, 58, 0},                             /* Version 12 */
    {6, 34, 62, 0},                             /* Version 13 */
    {6, 26, 46, 66, 0},                         /* Version 14 */
    {6, 26, 48, 70, 0},                         /* Version 15 */
    {6, 26, 50, 74, 0},                         /* Version 16 */
    {6, 30, 54, 78, 0},                         /* Version 17 */
    {6, 30, 56, 82, 0},                         /* Version 18 */
    {6, 30, 58, 86, 0},                         /* Version 19 */
    {6, 34, 62, 90, 0},                         /* Version 20 */
    {6, 28, 50, 72, 94, 0},                     /* Version 21 */
    {6, 26, 50, 74, 98, 0},                     /* Version 22 */
    {6, 30, 54, 78, 102, 0},                    /* Version 23 */
    {6, 28, 54, 80, 106, 0},                    /* Version 24 */
    {6, 32, 58, 84, 110, 0},                    /* Version 25 */
    {6, 30, 58, 86, 114, 0},                    /* Version 26 */
    {6, 34, 62, 90, 118, 0},                    /* Version 27 */
    {6, 26, 50, 74, 98, 122, 0},                /* Version 28 */
    {6, 30, 54, 78, 102, 126, 0},               /* Version 29 */
    {6, 26, 52, 78, 104, 130, 0},               /* Version 30 */
    {6, 30, 56, 82, 108, 134, 0},               /* Version 31 */
    {6, 34, 60, 86, 112, 138, 0},               /* Version 32 */
    {6, 30, 58, 86, 114, 142, 0},               /* Version 33 */
    {6, 34, 62, 90, 118, 146, 0},               /* Version 34 */
    {6, 30, 54, 78, 102, 126, 150, 0},          /* Version 35 */
    {6, 24, 50, 76, 102, 128, 154, 0},          /* Version 36 */
    {6, 28, 54, 80, 106, 132, 158, 0},          /* Version 37 */
    {6, 32, 58, 84, 110, 136, 162, 0},          /* Version 38 */
    {6, 26, 54, 82, 110, 138, 166, 0},          /* Version 39 */
    {6, 30, 58, 86, 114, 142, 170, 0},          /* Version 40 */
};

/* Check if a module is a function pattern (not data) */
STATIC int is_function_module(int version, int x, int y) {
    int size = qr_version_to_size(version);
    
    /* Finder patterns + separators + format info */
    if ((x < 9 && y < 9) ||                    /* Top-left */
        (x < 8 && y >= size - 8) ||            /* Bottom-left (not including format info col) */
        (x >= size - 8 && y < 9)) {            /* Top-right */
        return 1;
    }
    
    /* Format info areas */
    if ((y == 8 && (x < 9 || x >= size - 8)) ||     /* Row 8 format info */
        (x == 8 && (y < 9 || y >= size - 8))) {     /* Col 8 format info */
        return 1;
    }
    
    /* Timing patterns */
    if (x == 6 || y == 6) return 1;
    
    /* Alignment patterns */
    if (version >= 2) {
        const int *pos = alignment_positions[version - 1];
        for (int i = 0; pos[i]; i++) {
            for (int j = 0; pos[j]; j++) {
                int ax = pos[i], ay = pos[j];
                /* Skip if overlapping with finder patterns */
                if ((ax < 9 && ay < 9) ||
                    (ax < 9 && ay >= size - 8) ||
                    (ax >= size - 8 && ay < 9)) continue;
                    
                if (x >= ax - 2 && x <= ax + 2 && y >= ay - 2 && y <= ay + 2) {
                    return 1;
                }
            }
        }
    }
    
    /* Version info (versions 7+) */
    if (version >= 7) {
        if ((x < 6 && y >= size - 11 && y < size - 8) ||
            (y < 6 && x >= size - 11 && x < size - 8)) {
            return 1;
        }
    }
    
    return 0;
}

/* Read format information from QR code (from pre-sampled modules array) */
static int read_format_info(QRCode *qr) {
    /* Format info is stored around finder patterns
     * 
     * Following quirc's approach: read 15 bits MSB-first from specific positions.
     * Positions are indexed from top-left (0,0).
     * 
     * quirc reads in order i=14..0 using:
     *   xs[15] = {8, 8, 8, 8, 8, 8, 8, 8, 7, 5, 4, 3, 2, 1, 0}
     *   ys[15] = {0, 1, 2, 3, 4, 5, 7, 8, 8, 8, 8, 8, 8, 8, 8}
     *   format = (format << 1) | grid_bit(code, xs[i], ys[i])
     * 
     * So reading i from 14 to 0:
     *   i=14: (0,8)  i=13: (1,8)  i=12: (2,8)  i=11: (3,8)  i=10: (4,8)  i=9: (5,8)
     *   i=8:  (7,8)  i=7:  (8,8)  i=6:  (8,7)  i=5:  (8,5)  i=4:  (8,4)
     *   i=3:  (8,3)  i=2:  (8,2)  i=1:  (8,1)  i=0:  (8,0)
     * 
     * Note: quirc uses (col, row) = (x, y), our modules[row][col]
     */
    static const int xs[15] = {8, 8, 8, 8, 8, 8, 8, 8, 7, 5, 4, 3, 2, 1, 0};
    static const int ys[15] = {0, 1, 2, 3, 4, 5, 7, 8, 8, 8, 8, 8, 8, 8, 8};
    
    uint32_t format1 = 0;
    
    /* Read MSB-first, matching quirc's order */
    for (int i = 14; i >= 0; i--) {
        int col = xs[i];
        int row = ys[i];
        format1 = (format1 << 1) | (qr->modules[row][col] & 1);
    }
    
    if (debug_finder) {  // DEBUG
        fprintf(stderr, "  format bits (raw): row8[0-5]=%d%d%d%d%d%d row8[7]=%d row8[8]=%d row7[8]=%d\n",  // DEBUG
                qr->modules[8][0], qr->modules[8][1], qr->modules[8][2],  // DEBUG
                qr->modules[8][3], qr->modules[8][4], qr->modules[8][5],  // DEBUG
                qr->modules[8][7], qr->modules[8][8], qr->modules[7][8]);  // DEBUG
        fprintf(stderr, "  format bits cont: col8[5-0]=%d%d%d%d%d%d raw=0x%04x\n",  // DEBUG
                qr->modules[5][8], qr->modules[4][8], qr->modules[3][8],  // DEBUG
                qr->modules[2][8], qr->modules[1][8], qr->modules[0][8], format1);  // DEBUG
    }  // DEBUG
    
    /* XOR with mask pattern */
    format1 ^= 0x5412;
    
    return format1;
}

/* Forward declaration for sample_module_transform_ex */
static int sample_module_transform_ex(Image *img, const QRTransform *transform, int mx, int my, int use_antialiasing);

/* Read format information directly from image without anti-aliasing.
 * This is more accurate for format bits which can be on module boundaries. */
static int read_format_info_direct(Image *img, const QRTransform *transform) {
    static const int xs[15] = {8, 8, 8, 8, 8, 8, 8, 8, 7, 5, 4, 3, 2, 1, 0};
    static const int ys[15] = {0, 1, 2, 3, 4, 5, 7, 8, 8, 8, 8, 8, 8, 8, 8};
    
    uint32_t format1 = 0;
    
    /* Read MSB-first without anti-aliasing for better accuracy */
    for (int i = 14; i >= 0; i--) {
        int col = xs[i];
        int row = ys[i];
        int bit = sample_module_transform_ex(img, transform, col, row, 0);
        format1 = (format1 << 1) | bit;
    }
    
    /* XOR with mask pattern */
    format1 ^= 0x5412;
    
    return format1;
}

/* Valid format info values AFTER unmasking (XOR with 0x5412)
 * These are the 32 valid BCH(15,5) codewords for format info.
 * Index = (ecc_level << 3) | mask_pattern, where ecc: 0=M, 1=L, 2=H, 3=Q */
static const uint16_t valid_format_unmasked[32] = {
    /* M-0 to M-7: unmasked values */
    0x0000, 0x0537, 0x0A6E, 0x0F59, 0x11EB, 0x14DC, 0x1B85, 0x1EB2,
    /* L-0 to L-7 */
    0x23D6, 0x26E1, 0x29B8, 0x2C8F, 0x323D, 0x370A, 0x3853, 0x3D64,
    /* H-0 to H-7 */
    0x429B, 0x47AC, 0x48F5, 0x4DC2, 0x5370, 0x5647, 0x591E, 0x5C29,
    /* Q-0 to Q-7 */
    0x614D, 0x647A, 0x6B23, 0x6E14, 0x70A6, 0x7591, 0x7AC8, 0x7FFF
};

/* Validate and correct format info using BCH(15,5) code
 * BCH(15,5) can correct up to 3 bit errors
 * Input is the format info AFTER XOR with 0x5412 (unmasked)
 * Returns the corrected format info, or -1 if uncorrectable */
static int validate_format_info_with_correction(int format_info) {
    /* First check if already valid (syndrome = 0) */
    int remainder = format_info;
    for (int i = 14; i >= 10; i--) {
        if (remainder & (1 << i)) {
            remainder ^= (0x537 << (i - 10));
        }
    }
    if (remainder == 0) return format_info;  /* Already valid */
    
    /* Not valid - try to find closest valid format by Hamming distance
     * BCH(15,5) can correct up to 3 errors. We accept up to 4 errors to handle
     * sampling inaccuracies at module boundaries */
    int best_match = -1;
    int best_distance = 5;  /* Accept distance <= 4 */
    
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

/* Legacy validation function - just checks if valid, no correction */
static int validate_format_info(int format_info) {
    return validate_format_info_with_correction(format_info) == format_info;
}

/* Galois field arithmetic for Reed-Solomon */
static uint8_t gf_exp[512];
static uint8_t gf_log[256];
static int gf_initialized = 0;

STATIC void gf_init(void) {
    if (gf_initialized) return;
    
    int x = 1;
    for (int i = 0; i < 255; i++) {
        gf_exp[i] = x;
        gf_log[x] = i;
        x <<= 1;
        if (x & 0x100) x ^= 0x11d;  /* QR code polynomial */
    }
    for (int i = 255; i < 512; i++) {
        gf_exp[i] = gf_exp[i - 255];
    }
    gf_initialized = 1;
}

STATIC uint8_t gf_mul(uint8_t a, uint8_t b) {
    if (a == 0 || b == 0) return 0;
    return gf_exp[gf_log[a] + gf_log[b]];
}

/* GF division - used for Reed-Solomon error correction */
STATIC uint8_t gf_div(uint8_t a, uint8_t b) {
    if (b == 0) return 0;  /* Division by zero */
    if (a == 0) return 0;
    return gf_exp[(gf_log[a] + 255 - gf_log[b]) % 255];
}

/* Reed-Solomon syndrome calculation for QR codes 
 * Syndromes are S_i = R(alpha^i) for i = 0 to ecc_len-1
 * where R(x) is the received polynomial */
STATIC void rs_calc_syndromes(uint8_t *data, int total_len, int ecc_len, uint8_t *syndromes) {
    gf_init();
    
    for (int i = 0; i < ecc_len; i++) {
        uint8_t s = 0;
        for (int j = 0; j < total_len; j++) {
            s = gf_mul(s, gf_exp[i]) ^ data[j];
        }
        syndromes[i] = s;
    }
}

/* Check if all syndromes are zero (no errors) */
static int rs_check_syndromes(uint8_t *data, int data_len, int ecc_len) {
    gf_init();
    
    uint8_t syndromes[256];
    rs_calc_syndromes(data, data_len + ecc_len, ecc_len, syndromes);
    
    for (int i = 0; i < ecc_len; i++) {
        if (syndromes[i] != 0) return 0;
    }
    return 1;  /* Returns 1 if no errors */
}

/* Berlekamp-Massey algorithm to find the error locator polynomial
 * Returns the degree of the error locator polynomial (number of errors)
 * sigma[] contains the coefficients: sigma(x) = 1 + sigma[1]*x + sigma[2]*x^2 + ...
 */
static int rs_berlekamp_massey(uint8_t *syndromes, int ecc_len, uint8_t *sigma) {
    gf_init();
    
    uint8_t C[256] = {0};  /* Connection polynomial */
    uint8_t B[256] = {0};  /* Previous connection polynomial */
    
    C[0] = 1;
    B[0] = 1;
    
    int L = 0;  /* Current length of LFSR */
    int m = 1;  /* Number of iterations since L was updated */
    uint8_t b = 1;  /* Previous discrepancy */
    
    for (int n = 0; n < ecc_len; n++) {
        /* Calculate discrepancy */
        uint8_t d = syndromes[n];
        for (int i = 1; i <= L; i++) {
            d ^= gf_mul(C[i], syndromes[n - i]);
        }
        
        if (d == 0) {
            m++;
        } else if (2 * L <= n) {
            /* Update L */
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
            /* Just update C */
            uint8_t coef = gf_div(d, b);
            for (int i = 0; i < ecc_len - m; i++) {
                C[i + m] ^= gf_mul(coef, B[i]);
            }
            m++;
        }
    }
    
    /* Copy result to sigma */
    memcpy(sigma, C, ecc_len + 1);
    
    return L;
}

/* Chien search to find error positions
 * Returns number of errors found, or -1 if inconsistent
 * positions[] contains the error positions (as powers of alpha) */
static int rs_chien_search(uint8_t *sigma, int num_errors, int n, int *positions) {
    gf_init();
    
    int found = 0;
    
    for (int i = 0; i < n; i++) {
        /* Evaluate sigma at alpha^(-i) = alpha^(255-i) */
        uint8_t sum = sigma[0];  /* = 1 */
        for (int j = 1; j <= num_errors; j++) {
            uint8_t power = ((255 - i) * j) % 255;
            sum ^= gf_mul(sigma[j], gf_exp[power]);
        }
        
        if (sum == 0) {
            /* Found a root - error at position i */
            positions[found++] = n - 1 - i;  /* Convert to byte index */
        }
    }
    
    /* Check we found the right number of errors */
    return (found == num_errors) ? found : -1;
}

/* Forney's algorithm to calculate error values
 * omega(x) = S(x) * sigma(x) mod x^(2t)  where S is syndrome polynomial
 * e_i = omega(X_i^-1) / sigma'(X_i^-1) where X_i = alpha^(pos[i])
 */
static void rs_forney(uint8_t *syndromes, uint8_t *sigma, int num_errors, 
                      int *positions, int n, uint8_t *values) {
    gf_init();
    
    /* Calculate omega(x) = S(x) * sigma(x) truncated to num_errors terms
     * S(x) = S_0 + S_1*x + S_2*x^2 + ... */
    uint8_t omega[256] = {0};
    for (int i = 0; i < num_errors; i++) {
        uint8_t v = 0;
        for (int j = 0; j <= i; j++) {
            v ^= gf_mul(syndromes[i - j], sigma[j]);
        }
        omega[i] = v;
    }
    
    /* Calculate formal derivative of sigma: sigma'(x) = sigma_1 + sigma_3*x^2 + sigma_5*x^4 + ... */
    uint8_t sigma_prime[256] = {0};
    for (int i = 1; i <= num_errors; i += 2) {
        sigma_prime[i - 1] = sigma[i];
    }
    
    /* Calculate error values */
    for (int i = 0; i < num_errors; i++) {
        int pos = positions[i];
        int Xi_inv_power = (n - 1 - pos) % 255;  /* Power of alpha for X_i^-1 */
        
        /* Evaluate omega at X_i^-1 */
        uint8_t omega_val = 0;
        for (int j = 0; j < num_errors; j++) {
            omega_val ^= gf_mul(omega[j], gf_exp[(Xi_inv_power * j) % 255]);
        }
        
        /* Evaluate sigma' at X_i^-1 */
        uint8_t sigma_prime_val = 0;
        for (int j = 0; j < num_errors; j++) {
            sigma_prime_val ^= gf_mul(sigma_prime[j], gf_exp[(Xi_inv_power * j) % 255]);
        }
        
        /* Error value = omega / sigma' */
        if (sigma_prime_val != 0) {
            values[i] = gf_div(omega_val, sigma_prime_val);
        } else {
            values[i] = 0;
        }
    }
}

/* Full Reed-Solomon error correction
 * data: received data (data + ecc bytes)
 * data_len: number of data bytes (not including ECC)
 * ecc_len: number of ECC bytes
 * Returns: number of errors corrected, or -1 if uncorrectable */
static int rs_correct_errors(uint8_t *data, int data_len, int ecc_len) {
    gf_init();
    
    int total_len = data_len + ecc_len;
    int max_errors = ecc_len / 2;
    
    /* Calculate syndromes */
    uint8_t syndromes[256];
    rs_calc_syndromes(data, total_len, ecc_len, syndromes);
    
    /* Check if already correct */
    int all_zero = 1;
    for (int i = 0; i < ecc_len; i++) {
        if (syndromes[i] != 0) {
            all_zero = 0;
            break;
        }
    }
    if (all_zero) return 0;  /* No errors */
    
    /* Find error locator polynomial using Berlekamp-Massey */
    uint8_t sigma[256] = {0};
    int num_errors = rs_berlekamp_massey(syndromes, ecc_len, sigma);
    
    if (num_errors > max_errors) {
        return -1;  /* Too many errors */
    }
    
    /* Find error positions using Chien search */
    int positions[256];
    int found = rs_chien_search(sigma, num_errors, total_len, positions);
    
    if (found != num_errors) {
        return -1;  /* Inconsistent - uncorrectable */
    }
    
    /* Calculate error values using Forney's algorithm */
    uint8_t values[256];
    rs_forney(syndromes, sigma, num_errors, positions, total_len, values);
    
    /* Apply corrections */
    for (int i = 0; i < num_errors; i++) {
        if (positions[i] >= 0 && positions[i] < total_len) {
            data[positions[i]] ^= values[i];
        }
    }
    
    /* Verify correction by recalculating syndromes */
    rs_calc_syndromes(data, total_len, ecc_len, syndromes);
    for (int i = 0; i < ecc_len; i++) {
        if (syndromes[i] != 0) {
            return -1;  /* Correction failed */
        }
    }
    
    return num_errors;
}

/* Unmask QR code data */
static void unmask_qr(QRCode *qr, int mask_pattern) {
    int size = qr->size;
    
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            if (is_function_module(qr->version, x, y)) continue;
            
            int should_flip = 0;
            switch (mask_pattern) {
                case 0: should_flip = (y + x) % 2 == 0; break;
                case 1: should_flip = y % 2 == 0; break;
                case 2: should_flip = x % 3 == 0; break;
                case 3: should_flip = (y + x) % 3 == 0; break;
                case 4: should_flip = (y/2 + x/3) % 2 == 0; break;
                case 5: should_flip = (y*x) % 2 + (y*x) % 3 == 0; break;
                case 6: should_flip = ((y*x) % 2 + (y*x) % 3) % 2 == 0; break;
                case 7: should_flip = ((y+x) % 2 + (y*x) % 3) % 2 == 0; break;
            }
            
            if (should_flip) {
                qr->modules[y][x] ^= 1;
            }
        }
    }
}

/* Number of data codewords per version/error correction level
 * Index: [version][ecc_raw], where ecc_raw is the 2-bit value from format info:
 *   00 = M, 01 = L, 10 = H, 11 = Q
 * So array order is: [M, L, H, Q] = indices [0, 1, 2, 3] */
static const int data_codewords[41][4] = {
    {0, 0, 0, 0},        /* Version 0 (invalid) */
    {16, 19, 9, 13},     /* Version 1: M=16, L=19, H=9, Q=13 */
    {28, 34, 16, 22},     /* Version 2: M=28, L=34, H=16, Q=22 */
    {44, 55, 26, 34},     /* Version 3: M=44, L=55, H=26, Q=34 */
    {64, 80, 36, 48},     /* Version 4: M=64, L=80, H=36, Q=48 */
    {86, 108, 46, 62},     /* Version 5: M=86, L=108, H=46, Q=62 */
    {108, 136, 60, 76},     /* Version 6: M=108, L=136, H=60, Q=76 */
    {124, 156, 66, 88},     /* Version 7: M=124, L=156, H=66, Q=88 */
    {154, 194, 86, 110},     /* Version 8: M=154, L=194, H=86, Q=110 */
    {182, 232, 100, 132},     /* Version 9: M=182, L=232, H=100, Q=132 */
    {216, 274, 122, 154},     /* Version 10: M=216, L=274, H=122, Q=154 */
    {254, 324, 140, 180},     /* Version 11: M=254, L=324, H=140, Q=180 */
    {290, 370, 158, 206},     /* Version 12: M=290, L=370, H=158, Q=206 */
    {334, 428, 180, 244},     /* Version 13: M=334, L=428, H=180, Q=244 */
    {365, 461, 197, 261},     /* Version 14: M=365, L=461, H=197, Q=261 */
    {415, 523, 223, 295},     /* Version 15: M=415, L=523, H=223, Q=295 */
    {453, 589, 253, 325},     /* Version 16: M=453, L=589, H=253, Q=325 */
    {507, 647, 283, 367},     /* Version 17: M=507, L=647, H=283, Q=367 */
    {563, 721, 313, 397},     /* Version 18: M=563, L=721, H=313, Q=397 */
    {627, 795, 341, 445},     /* Version 19: M=627, L=795, H=341, Q=445 */
    {669, 861, 385, 485},     /* Version 20: M=669, L=861, H=385, Q=485 */
    {714, 932, 406, 512},     /* Version 21: M=714, L=932, H=406, Q=512 */
    {782, 1006, 442, 568},     /* Version 22: M=782, L=1006, H=442, Q=568 */
    {860, 1094, 464, 614},     /* Version 23: M=860, L=1094, H=464, Q=614 */
    {914, 1174, 514, 664},     /* Version 24: M=914, L=1174, H=514, Q=664 */
    {1000, 1276, 538, 718},     /* Version 25: M=1000, L=1276, H=538, Q=718 */
    {1062, 1370, 596, 754},     /* Version 26: M=1062, L=1370, H=596, Q=754 */
    {1128, 1468, 628, 808},     /* Version 27: M=1128, L=1468, H=628, Q=808 */
    {1193, 1531, 661, 871},     /* Version 28: M=1193, L=1531, H=661, Q=871 */
    {1267, 1631, 701, 911},     /* Version 29: M=1267, L=1631, H=701, Q=911 */
    {1373, 1735, 745, 985},     /* Version 30: M=1373, L=1735, H=745, Q=985 */
    {1455, 1843, 793, 1033},     /* Version 31: M=1455, L=1843, H=793, Q=1033 */
    {1541, 1955, 845, 1115},     /* Version 32: M=1541, L=1955, H=845, Q=1115 */
    {1631, 2071, 901, 1171},     /* Version 33: M=1631, L=2071, H=901, Q=1171 */
    {1725, 2191, 961, 1231},     /* Version 34: M=1725, L=2191, H=961, Q=1231 */
    {1812, 2306, 986, 1286},     /* Version 35: M=1812, L=2306, H=986, Q=1286 */
    {1914, 2434, 1054, 1354},     /* Version 36: M=1914, L=2434, H=1054, Q=1354 */
    {1992, 2566, 1096, 1426},     /* Version 37: M=1992, L=2566, H=1096, Q=1426 */
    {2102, 2702, 1142, 1502},     /* Version 38: M=2102, L=2702, H=1142, Q=1502 */
    {2216, 2812, 1222, 1582},     /* Version 39: M=2216, L=2812, H=1222, Q=1582 */
    {2334, 2956, 1276, 1666},     /* Version 40: M=2334, L=2956, H=1276, Q=1666 */
};

/* Reed-Solomon block parameters for deinterleaving
 * bs = block size (total codewords), dw = data codewords, ns = number of short blocks
 * Some versions have short and long blocks (long blocks have 1 extra data codeword)
 */
typedef struct {
    int bs;   /* Block size (total codewords per block) */
    int dw;   /* Data codewords per block */
    int ns;   /* Number of blocks of this size */
} RSBlockParams;

typedef struct {
    int data_bytes;                /* Total data codewords */
    RSBlockParams ecc[4];          /* ECC params for M, L, H, Q (indexed by format bits) */
} VersionInfo;

/* Version database - indexed by [version][ecc_level] where ecc_level is format bits:
 * 0=M, 1=L, 2=H, 3=Q */
static const VersionInfo version_info[41] = {
    {0},  /* Version 0 (invalid) */
    { /* Version 1 */
        .data_bytes = 26,
        .ecc = {
            {.bs = 26, .dw = 16, .ns = 1},  /* M */
            {.bs = 26, .dw = 19, .ns = 1},  /* L */
            {.bs = 26, .dw = 9, .ns = 1},   /* H */
            {.bs = 26, .dw = 13, .ns = 1}   /* Q */
        }
    },
    { /* Version 2 */
        .data_bytes = 44,
        .ecc = {
            {.bs = 44, .dw = 28, .ns = 1},
            {.bs = 44, .dw = 34, .ns = 1},
            {.bs = 44, .dw = 16, .ns = 1},
            {.bs = 44, .dw = 22, .ns = 1}
        }
    },
    { /* Version 3 */
        .data_bytes = 70,
        .ecc = {
            {.bs = 70, .dw = 44, .ns = 1},
            {.bs = 70, .dw = 55, .ns = 1},
            {.bs = 35, .dw = 13, .ns = 2},
            {.bs = 35, .dw = 17, .ns = 2}
        }
    },
    { /* Version 4 */
        .data_bytes = 100,
        .ecc = {
            {.bs = 50, .dw = 32, .ns = 2},
            {.bs = 100, .dw = 80, .ns = 1},
            {.bs = 25, .dw = 9, .ns = 4},
            {.bs = 50, .dw = 24, .ns = 2}
        }
    },
    { /* Version 5 */
        .data_bytes = 134,
        .ecc = {
            {.bs = 67, .dw = 43, .ns = 2},
            {.bs = 134, .dw = 108, .ns = 1},
            {.bs = 33, .dw = 11, .ns = 2},
            {.bs = 33, .dw = 15, .ns = 2}
        }
    },
    { /* Version 6 */
        .data_bytes = 172,
        .ecc = {
            {.bs = 43, .dw = 27, .ns = 4},
            {.bs = 86, .dw = 68, .ns = 2},
            {.bs = 43, .dw = 15, .ns = 4},
            {.bs = 43, .dw = 19, .ns = 4}
        }
    },
    { /* Version 7 */
        .data_bytes = 196,
        .ecc = {
            {.bs = 49, .dw = 31, .ns = 4},
            {.bs = 98, .dw = 78, .ns = 2},
            {.bs = 39, .dw = 13, .ns = 4},
            {.bs = 32, .dw = 14, .ns = 2}
        }
    },
    { /* Version 8 */
        .data_bytes = 242,
        .ecc = {
            {.bs = 60, .dw = 38, .ns = 2},
            {.bs = 121, .dw = 97, .ns = 2},
            {.bs = 40, .dw = 14, .ns = 4},
            {.bs = 40, .dw = 18, .ns = 4}
        }
    },
    { /* Version 9 */
        .data_bytes = 292,
        .ecc = {
            {.bs = 58, .dw = 36, .ns = 3},
            {.bs = 146, .dw = 116, .ns = 2},
            {.bs = 36, .dw = 12, .ns = 4},
            {.bs = 36, .dw = 16, .ns = 4}
        }
    },
    { /* Version 10 */
        .data_bytes = 346,
        .ecc = {
            {.bs = 69, .dw = 43, .ns = 4},
            {.bs = 86, .dw = 68, .ns = 2},
            {.bs = 43, .dw = 15, .ns = 6},
            {.bs = 43, .dw = 19, .ns = 6}
        }
    },
    { /* Version 11 */
        .data_bytes = 404,
        .ecc = {
            {.bs = 80, .dw = 50, .ns = 1},
            {.bs = 101, .dw = 81, .ns = 4},
            {.bs = 36, .dw = 12, .ns = 3},
            {.bs = 50, .dw = 22, .ns = 4}
        }
    },
    { /* Version 12 */
        .data_bytes = 466,
        .ecc = {
            {.bs = 58, .dw = 36, .ns = 6},
            {.bs = 116, .dw = 92, .ns = 2},
            {.bs = 42, .dw = 14, .ns = 7},
            {.bs = 46, .dw = 20, .ns = 4}
        }
    },
    { /* Version 13 */
        .data_bytes = 532,
        .ecc = {
            {.bs = 59, .dw = 37, .ns = 8},
            {.bs = 133, .dw = 107, .ns = 4},
            {.bs = 33, .dw = 11, .ns = 12},
            {.bs = 44, .dw = 20, .ns = 8}
        }
    },
    { /* Version 14 */
        .data_bytes = 581,
        .ecc = {
            {.bs = 64, .dw = 40, .ns = 4},
            {.bs = 145, .dw = 115, .ns = 3},
            {.bs = 36, .dw = 12, .ns = 11},
            {.bs = 36, .dw = 16, .ns = 11}
        }
    },
    { /* Version 15 */
        .data_bytes = 655,
        .ecc = {
            {.bs = 65, .dw = 41, .ns = 5},
            {.bs = 109, .dw = 87, .ns = 5},
            {.bs = 36, .dw = 12, .ns = 11},
            {.bs = 54, .dw = 24, .ns = 5}
        }
    },
    { /* Version 16 */
        .data_bytes = 733,
        .ecc = {
            {.bs = 73, .dw = 45, .ns = 7},
            {.bs = 122, .dw = 98, .ns = 5},
            {.bs = 45, .dw = 15, .ns = 3},
            {.bs = 43, .dw = 19, .ns = 15}
        }
    },
    { /* Version 17 */
        .data_bytes = 815,
        .ecc = {
            {.bs = 74, .dw = 46, .ns = 10},
            {.bs = 135, .dw = 107, .ns = 1},
            {.bs = 42, .dw = 14, .ns = 2},
            {.bs = 50, .dw = 22, .ns = 1}
        }
    },
    { /* Version 18 */
        .data_bytes = 901,
        .ecc = {
            {.bs = 69, .dw = 43, .ns = 9},
            {.bs = 150, .dw = 120, .ns = 5},
            {.bs = 42, .dw = 14, .ns = 2},
            {.bs = 50, .dw = 22, .ns = 17}
        }
    },
    { /* Version 19 */
        .data_bytes = 991,
        .ecc = {
            {.bs = 70, .dw = 44, .ns = 3},
            {.bs = 141, .dw = 113, .ns = 3},
            {.bs = 39, .dw = 13, .ns = 9},
            {.bs = 47, .dw = 21, .ns = 17}
        }
    },
    { /* Version 20 */
        .data_bytes = 1085,
        .ecc = {
            {.bs = 67, .dw = 41, .ns = 3},
            {.bs = 135, .dw = 107, .ns = 3},
            {.bs = 43, .dw = 15, .ns = 15},
            {.bs = 54, .dw = 24, .ns = 15}
        }
    },
    { /* Versions 21-40: using simplified single-block approximation for now */
        .data_bytes = 1156, .ecc = {{.bs = 68, .dw = 42, .ns = 17}, {.bs = 144, .dw = 116, .ns = 4}, {.bs = 46, .dw = 16, .ns = 19}, {.bs = 50, .dw = 22, .ns = 17}}
    },
    { .data_bytes = 1258, .ecc = {{.bs = 74, .dw = 46, .ns = 17}, {.bs = 139, .dw = 111, .ns = 2}, {.bs = 37, .dw = 13, .ns = 34}, {.bs = 54, .dw = 24, .ns = 7}} },
    { .data_bytes = 1364, .ecc = {{.bs = 75, .dw = 47, .ns = 4}, {.bs = 151, .dw = 121, .ns = 4}, {.bs = 45, .dw = 15, .ns = 16}, {.bs = 54, .dw = 24, .ns = 11}} },
    { .data_bytes = 1474, .ecc = {{.bs = 73, .dw = 45, .ns = 6}, {.bs = 147, .dw = 117, .ns = 6}, {.bs = 46, .dw = 16, .ns = 30}, {.bs = 54, .dw = 24, .ns = 11}} },
    { .data_bytes = 1588, .ecc = {{.bs = 75, .dw = 47, .ns = 8}, {.bs = 132, .dw = 106, .ns = 8}, {.bs = 45, .dw = 15, .ns = 22}, {.bs = 54, .dw = 24, .ns = 7}} },
    { .data_bytes = 1706, .ecc = {{.bs = 74, .dw = 46, .ns = 19}, {.bs = 142, .dw = 114, .ns = 10}, {.bs = 46, .dw = 16, .ns = 33}, {.bs = 50, .dw = 22, .ns = 28}} },
    { .data_bytes = 1828, .ecc = {{.bs = 73, .dw = 45, .ns = 22}, {.bs = 152, .dw = 122, .ns = 8}, {.bs = 45, .dw = 15, .ns = 12}, {.bs = 53, .dw = 23, .ns = 8}} },
    { .data_bytes = 1921, .ecc = {{.bs = 73, .dw = 45, .ns = 3}, {.bs = 147, .dw = 117, .ns = 3}, {.bs = 45, .dw = 15, .ns = 11}, {.bs = 54, .dw = 24, .ns = 4}} },
    { .data_bytes = 2051, .ecc = {{.bs = 73, .dw = 45, .ns = 21}, {.bs = 146, .dw = 116, .ns = 7}, {.bs = 45, .dw = 15, .ns = 19}, {.bs = 53, .dw = 23, .ns = 1}} },
    { .data_bytes = 2185, .ecc = {{.bs = 75, .dw = 47, .ns = 19}, {.bs = 145, .dw = 115, .ns = 5}, {.bs = 45, .dw = 15, .ns = 23}, {.bs = 54, .dw = 24, .ns = 15}} },
    { .data_bytes = 2323, .ecc = {{.bs = 74, .dw = 46, .ns = 2}, {.bs = 145, .dw = 115, .ns = 13}, {.bs = 45, .dw = 15, .ns = 23}, {.bs = 54, .dw = 24, .ns = 42}} },
    { .data_bytes = 2465, .ecc = {{.bs = 74, .dw = 46, .ns = 10}, {.bs = 145, .dw = 115, .ns = 17}, {.bs = 45, .dw = 15, .ns = 19}, {.bs = 54, .dw = 24, .ns = 10}} },
    { .data_bytes = 2611, .ecc = {{.bs = 74, .dw = 46, .ns = 14}, {.bs = 145, .dw = 115, .ns = 17}, {.bs = 45, .dw = 15, .ns = 11}, {.bs = 54, .dw = 24, .ns = 29}} },
    { .data_bytes = 2761, .ecc = {{.bs = 74, .dw = 46, .ns = 14}, {.bs = 145, .dw = 115, .ns = 13}, {.bs = 46, .dw = 16, .ns = 59}, {.bs = 54, .dw = 24, .ns = 44}} },
    { .data_bytes = 2876, .ecc = {{.bs = 75, .dw = 47, .ns = 12}, {.bs = 151, .dw = 121, .ns = 12}, {.bs = 45, .dw = 15, .ns = 22}, {.bs = 54, .dw = 24, .ns = 39}} },
    { .data_bytes = 3034, .ecc = {{.bs = 75, .dw = 47, .ns = 6}, {.bs = 151, .dw = 121, .ns = 6}, {.bs = 45, .dw = 15, .ns = 2}, {.bs = 54, .dw = 24, .ns = 46}} },
    { .data_bytes = 3196, .ecc = {{.bs = 74, .dw = 46, .ns = 29}, {.bs = 152, .dw = 122, .ns = 17}, {.bs = 45, .dw = 15, .ns = 24}, {.bs = 54, .dw = 24, .ns = 49}} },
    { .data_bytes = 3362, .ecc = {{.bs = 74, .dw = 46, .ns = 13}, {.bs = 152, .dw = 122, .ns = 4}, {.bs = 45, .dw = 15, .ns = 42}, {.bs = 54, .dw = 24, .ns = 48}} },
    { .data_bytes = 3532, .ecc = {{.bs = 75, .dw = 47, .ns = 40}, {.bs = 147, .dw = 117, .ns = 20}, {.bs = 45, .dw = 15, .ns = 10}, {.bs = 54, .dw = 24, .ns = 43}} },
    { .data_bytes = 3706, .ecc = {{.bs = 75, .dw = 47, .ns = 18}, {.bs = 148, .dw = 118, .ns = 19}, {.bs = 45, .dw = 15, .ns = 20}, {.bs = 54, .dw = 24, .ns = 34}} },
};

/* Deinterleave a single block's codewords from interleaved raw data
 * block_idx: which block (0 to bc-1)
 * is_long: whether this is a long block (has dw+1 data codewords)
 * Returns total block size (data + ecc)
 */
static int deinterleave_block(uint8_t *raw, int total_bytes, int bc, int dw, int bs, 
                              int ns, int block_idx, uint8_t *block_out) {
    int is_long = (block_idx >= ns);
    int block_dw = is_long ? dw + 1 : dw;
    int block_ecc = bs - dw;
    int block_bs = is_long ? bs + 1 : bs;
    int pos = 0;
    
    /* Extract data codewords */
    for (int j = 0; j < block_dw; j++) {
        int src_idx;
        if (j < dw) {
            /* Regular interleaved position */
            src_idx = j * bc + block_idx;
        } else {
            /* Extra codeword from long block */
            src_idx = dw * bc + (block_idx - ns);
        }
        if (src_idx < total_bytes) {
            block_out[pos++] = raw[src_idx];
        }
    }
    
    /* Extract ECC codewords - they follow data in the interleaved stream */
    int data_total = dw * bc + (bc - ns);  /* Total data codewords */
    for (int j = 0; j < block_ecc; j++) {
        int src_idx = data_total + j * bc + block_idx;
        if (src_idx < total_bytes) {
            block_out[pos++] = raw[src_idx];
        }
    }
    
    return block_bs;
}

/* Deinterleave codewords with RS error correction
 * QR codes interleave data and ECC codewords across multiple blocks.
 * This function:
 * 1. Deinterleaves each block separately
 * 2. Applies RS error correction to each block
 * 3. Extracts just the corrected data codewords
 * 
 * Returns number of data bytes extracted, or -1 if uncorrectable errors.
 */
static int deinterleave_and_correct(uint8_t *raw, int total_bytes, int version, int ecc_level,
                                     uint8_t *data_out, int max_out, int *errors_corrected,
                                     int *uncorrectable_out) {
    if (version < 1 || version > 40) return 0;
    
    const RSBlockParams *sb = &version_info[version].ecc[ecc_level];
    int ns = sb->ns;           /* Number of short blocks */
    int dw = sb->dw;           /* Data codewords per short block */
    int bs = sb->bs;           /* Total codewords per short block */
    
    /* Calculate number of long blocks */
    int short_total = ns * bs;
    int remaining = total_bytes - short_total;
    int nl = (remaining > 0) ? remaining / (bs + 1) : 0;
    int bc = ns + nl;          /* Total block count */
    int block_ecc = bs - dw;   /* ECC codewords per block */
    
    if (debug_finder) {  // DEBUG
        fprintf(stderr, "  RS correct: version=%d ecc=%d ns=%d nl=%d dw=%d bs=%d ecc_per_block=%d\n",  // DEBUG
                version, ecc_level, ns, nl, dw, bs, block_ecc);  // DEBUG
    }  // DEBUG
    
    int data_pos = 0;
    int total_errors = 0;
    int uncorrectable_blocks = 0;
    
    /* Process each block */
    for (int i = 0; i < bc && data_pos < max_out; i++) {
        uint8_t block[256];
        (void)deinterleave_block(raw, total_bytes, bc, dw, bs, ns, i, block);
        
        int is_long = (i >= ns);
        int block_dw = is_long ? dw + 1 : dw;
        
        /* Apply RS error correction */
        int corrected = rs_correct_errors(block, block_dw, block_ecc);
        
        if (corrected < 0) {
            if (debug_finder) {  // DEBUG
                fprintf(stderr, "  Block %d: uncorrectable errors\n", i);  // DEBUG
            }  // DEBUG
            uncorrectable_blocks++;
            /* Still copy the data but note the failure */
        } else if (corrected > 0) {
            if (debug_finder) {  // DEBUG
                fprintf(stderr, "  Block %d: corrected %d errors\n", i, corrected);  // DEBUG
            }  // DEBUG
            total_errors += corrected;
        }
        
        /* Copy corrected data codewords to output */
        for (int j = 0; j < block_dw && data_pos < max_out; j++) {
            data_out[data_pos++] = block[j];
        }
    }
    
    if (errors_corrected) *errors_corrected = total_errors;
    if (uncorrectable_out) *uncorrectable_out = uncorrectable_blocks;
    
    /* If ALL blocks had uncorrectable errors, the data is garbage - reject it */
    if (uncorrectable_blocks == bc) {
        if (debug_finder) {  // DEBUG
            fprintf(stderr, "  RS: all %d blocks uncorrectable, rejecting\n", bc);  // DEBUG
        }  // DEBUG
        return -1;
    }
    
    return data_pos;
}

/* Extract data from QR code modules (in bit order) */
static int extract_qr_data(QRCode *qr, uint8_t *data, int max_len) {
    int size = qr->size;
    int byte_index = 0;
    int bit_index = 7;
    int going_up = 1;
    int total_bits = 0;
    
    memset(data, 0, max_len);
    
    if (debug_finder) {  // DEBUG
        fprintf(stderr, "  extract_qr_data: first 32 bits from positions:\n  ");  // DEBUG
    }  // DEBUG
    
    /* Read data in the zigzag pattern */
    for (int col = size - 1; col >= 0; col -= 2) {
        if (col == 6) col--;  /* Skip timing pattern column */
        if (col < 0) break;
        
        int rows = going_up ? size - 1 : 0;
        int end = going_up ? -1 : size;
        int step = going_up ? -1 : 1;
        
        for (int row = rows; row != end; row += step) {
            for (int c = 0; c < 2; c++) {
                int x = col - c;
                if (x < 0) continue;
                
                if (!is_function_module(qr->version, x, row)) {
                    if (byte_index < max_len) {
                        int bit = qr->modules[row][x];
                        if (debug_finder && total_bits < 32) {  // DEBUG
                            fprintf(stderr, "(%d,%d)=%d ", x, row, bit);
                            if (total_bits % 8 == 7) fprintf(stderr, "\n  ");
                        }
                        (void)0; /* Placeholder - debug removed */
                        if (bit) {
                            data[byte_index] |= (1 << bit_index);
                        }
                        bit_index--;
                        if (bit_index < 0) {
                            bit_index = 7;
                            byte_index++;
                        }
                        total_bits++;
                    }
                }
            }
        }
        going_up = !going_up;
    }
    
    if (debug_finder) fprintf(stderr, "\n");  // DEBUG
    
    return byte_index + (bit_index < 7 ? 1 : 0);
}

/* Decode QR data content */
static int decode_qr_content(uint8_t *data, int data_len, int version, char *output, int max_output) {
    int bit_pos = 0;
    int out_pos = 0;
    
    if (debug_finder) {  // DEBUG
        fprintf(stderr, "  decode_qr_content: data_len=%d version=%d first bytes: %02x %02x %02x %02x\n",  // DEBUG
                data_len, version, data[0], data[1], data[2], data[3]);  // DEBUG
    }  // DEBUG
    
    /* Read bits helper */
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
        if (debug_finder) {  // DEBUG
            fprintf(stderr, "  mode=%d at bit_pos=%d\n", mode, bit_pos - 4);  // DEBUG
        }  // DEBUG
        if (mode == 0) break;  /* Terminator */
        
        /* Determine character count length based on version and mode */
        int count_bits;
        
        if (mode == 1) {  /* Numeric */
            count_bits = version <= 9 ? 10 : (version <= 26 ? 12 : 14);
            int count = READ_BITS(count_bits);
            
            while (count >= 3 && out_pos < max_output - 3) {
                int val = READ_BITS(10);
                output[out_pos++] = '0' + val / 100;
                output[out_pos++] = '0' + (val / 10) % 10;
                output[out_pos++] = '0' + val % 10;
                count -= 3;
            }
            if (count == 2 && out_pos < max_output - 2) {
                int val = READ_BITS(7);
                output[out_pos++] = '0' + val / 10;
                output[out_pos++] = '0' + val % 10;
            } else if (count == 1 && out_pos < max_output - 1) {
                int val = READ_BITS(4);
                output[out_pos++] = '0' + val;
            }
        } else if (mode == 2) {  /* Alphanumeric */
            static const char alphanumeric[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:";
            count_bits = version <= 9 ? 9 : (version <= 26 ? 11 : 13);
            int count = READ_BITS(count_bits);
            
            if (debug_finder) {  // DEBUG
                fprintf(stderr, "  alphanumeric mode: count_bits=%d count=%d\n", count_bits, count);  // DEBUG
            }  // DEBUG
            
            int pair_idx = 0;
            while (count >= 2 && out_pos < max_output - 2) {
                int val = READ_BITS(11);
                if (debug_finder && pair_idx < 5) {  // DEBUG
                    fprintf(stderr, "  alpha pair[%d] val=%d -> '%c%c'\n", pair_idx, val,
                            alphanumeric[val / 45], alphanumeric[val % 45]);
                    pair_idx++;
                }
                output[out_pos++] = alphanumeric[val / 45];
                output[out_pos++] = alphanumeric[val % 45];
                count -= 2;
            }
            if (count == 1 && out_pos < max_output - 1) {
                int val = READ_BITS(6);
                output[out_pos++] = alphanumeric[val];
            }
        } else if (mode == 4) {  /* Byte mode */
            count_bits = version <= 9 ? 8 : 16;
            int count = READ_BITS(count_bits);
            
            if (debug_finder) {  // DEBUG
                fprintf(stderr, "  byte mode: count_bits=%d count=%d\n", count_bits, count);  // DEBUG
            }  // DEBUG
            
            for (int i = 0; i < count && out_pos < max_output - 1; i++) {
                int start_bit = bit_pos;
                int c = READ_BITS(8);
                if (debug_finder && i >= count - 3) {  // DEBUG
                    /* Show last 3 bytes in detail with actual bit pattern */
                    int byte1 = start_bit / 8;
                    int byte2 = (start_bit + 7) / 8;
                    fprintf(stderr, "    byte[%d] = 0x%02x '%c' (bits %d-%d from bytes %d-%d: 0x%02x 0x%02x)\n", 
                            i, c, (c >= 32 && c < 127) ? c : '?',
                            start_bit, start_bit + 7, byte1, byte2,
                            byte1 < data_len ? data[byte1] : 0,
                            byte2 < data_len ? data[byte2] : 0);
                }
                output[out_pos++] = c;
            }
        } else {
            break;  /* Unsupported mode */
        }
    }
    
    output[out_pos] = '\0';
    return out_pos;
    
    #undef READ_BITS
}

/* Calculate distance squared between two points */
static int distance_sq(int x1, int y1, int x2, int y2) {
    int dx = x2 - x1;
    int dy = y2 - y1;
    return dx * dx + dy * dy;
}

/* Calculate cross product (determines orientation) */
static int cross_product(int x1, int y1, int x2, int y2, int x3, int y3) {
    return (x2 - x1) * (y3 - y1) - (y2 - y1) * (x3 - x1);
}

/* Identify the three finder patterns as top-left, top-right, bottom-left */
STATIC int identify_finder_roles(FinderPattern *fp, int *tl, int *tr, int *bl) {
    /* The top-left finder is opposite the longest side (hypotenuse) */
    int d01 = distance_sq(fp[0].x, fp[0].y, fp[1].x, fp[1].y);
    int d02 = distance_sq(fp[0].x, fp[0].y, fp[2].x, fp[2].y);
    int d12 = distance_sq(fp[1].x, fp[1].y, fp[2].x, fp[2].y);
    
    int corner;  /* The corner opposite the longest side (top-left) */
    int p1, p2;  /* The other two corners */
    
    if (d01 >= d02 && d01 >= d12) {
        corner = 2; p1 = 0; p2 = 1;
    } else if (d02 >= d01 && d02 >= d12) {
        corner = 1; p1 = 0; p2 = 2;
    } else {
        corner = 0; p1 = 1; p2 = 2;
    }
    
    *tl = corner;
    
    /* Use cross product to determine which is top-right vs bottom-left */
    int cp = cross_product(fp[corner].x, fp[corner].y, 
                           fp[p1].x, fp[p1].y, 
                           fp[p2].x, fp[p2].y);
    
    if (cp > 0) {
        *tr = p1;
        *bl = p2;
    } else {
        *tr = p2;
        *bl = p1;
    }
    
    return 1;
}

/* Gaussian elimination with partial pivoting */
static int gaussian_eliminate(float *A, float *b, float *x, int n) {
    /* Combine A|b into augmented matrix */
    float *aug = malloc((n * (n + 1)) * sizeof(float));
    if (!aug) return 0;
    
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            aug[i * (n + 1) + j] = A[i * n + j];
        }
        aug[i * (n + 1) + n] = b[i];
    }
    
    /* Forward elimination with partial pivoting */
    for (int k = 0; k < n - 1; k++) {
        /* Find pivot */
        int max_row = k;
        float max_val = fabs(aug[k * (n + 1) + k]);
        for (int i = k + 1; i < n; i++) {
            float val = fabs(aug[i * (n + 1) + k]);
            if (val > max_val) {
                max_val = val;
                max_row = i;
            }
        }
        
        /* Swap rows if needed */
        if (max_row != k) {
            for (int j = k; j <= n; j++) {
                float temp = aug[k * (n + 1) + j];
                aug[k * (n + 1) + j] = aug[max_row * (n + 1) + j];
                aug[max_row * (n + 1) + j] = temp;
            }
        }
        
        /* Check for singularity */
        if (fabs(aug[k * (n + 1) + k]) < 1e-10) {
            free(aug);
            return 0;
        }
        
        /* Eliminate below */
        for (int i = k + 1; i < n; i++) {
            float factor = aug[i * (n + 1) + k] / aug[k * (n + 1) + k];
            aug[i * (n + 1) + k] = 0.0;
            for (int j = k + 1; j <= n; j++) {
                aug[i * (n + 1) + j] -= factor * aug[k * (n + 1) + j];
            }
        }
    }
    
    /* Back substitution */
    for (int i = n - 1; i >= 0; i--) {
        float sum = aug[i * (n + 1) + n];
        for (int j = i + 1; j < n; j++) {
            sum -= aug[i * (n + 1) + j] * x[j];
        }
        if (fabs(aug[i * (n + 1) + i]) < 1e-10) {
            free(aug);
            return 0;
        }
        x[i] = sum / aug[i * (n + 1) + i];
    }
    
    free(aug);
    return 1;
}

/* Calculate homography matrix from 4 point correspondences */
static int calculate_homography(Point2D src[4], Point2D dst[4], HomographyMatrix *H) {
    /* For 4 point pairs, we need to solve an 8x8 system */
    float A[64];  /* 8x8 matrix */
    float b[8];   /* Right hand side */
    float x[8];   /* Solution */
    
    /* Fill matrices */
    for (int i = 0; i < 4; i++) {
        /* Each point gives two rows */
        int row1 = i * 2;
        int row2 = i * 2 + 1;
        
        /* First row: x equation */
        A[row1 * 8 + 0] = src[i].x;
        A[row1 * 8 + 1] = src[i].y;
        A[row1 * 8 + 2] = 1;
        A[row1 * 8 + 3] = 0;
        A[row1 * 8 + 4] = 0;
        A[row1 * 8 + 5] = 0;
        A[row1 * 8 + 6] = -src[i].x * dst[i].x;
        A[row1 * 8 + 7] = -src[i].y * dst[i].x;
        b[row1] = dst[i].x;
        
        /* Second row: y equation */
        A[row2 * 8 + 0] = 0;
        A[row2 * 8 + 1] = 0;
        A[row2 * 8 + 2] = 0;
        A[row2 * 8 + 3] = src[i].x;
        A[row2 * 8 + 4] = src[i].y;
        A[row2 * 8 + 5] = 1;
        A[row2 * 8 + 6] = -src[i].x * dst[i].y;
        A[row2 * 8 + 7] = -src[i].y * dst[i].y;
        b[row2] = dst[i].y;
    }
    
    /* Solve the system */
    if (!gaussian_eliminate(A, b, x, 8)) {
        return 0;
    }
    
    /* Fill homography matrix */
    H->h[0][0] = x[0];
    H->h[0][1] = x[1];
    H->h[0][2] = x[2];
    H->h[1][0] = x[3];
    H->h[1][1] = x[4];
    H->h[1][2] = x[5];
    H->h[2][0] = x[6];
    H->h[2][1] = x[7];
    H->h[2][2] = 1.0f;
    
    return 1;
}

/* Apply homography transform to a point */
static Point2D apply_homography(const HomographyMatrix *H, float x, float y) {
    Point2D result;
    float w = H->h[2][0] * x + H->h[2][1] * y + H->h[2][2];
    
    /* Avoid division by zero */
    if (fabs(w) < 1e-10) w = 1e-10;
    
    result.x = (H->h[0][0] * x + H->h[0][1] * y + H->h[0][2]) / w;
    result.y = (H->h[1][0] * x + H->h[1][1] * y + H->h[1][2]) / w;
    
    return result;
}

/* Refine finder center by searching for the center of the inner black region.
 * The finder pattern has a 3x3 black center that we can use to find the true center.
 * This is important for rotated codes where the initial line-scan estimate may be biased. */
static void refine_finder_center(Image *img, int *px, int *py, float module_size) {
    int x = *px, y = *py;
    int search_radius = (int)(module_size * 2 + 0.5f);
    if (search_radius < 5) search_radius = 5;
    if (search_radius > 20) search_radius = 20;
    
    /* Find the center of the black region near the initial estimate */
    int best_x = x, best_y = y;
    int max_black_count = 0;
    
    for (int dy = -search_radius; dy <= search_radius; dy++) {
        for (int dx = -search_radius; dx <= search_radius; dx++) {
            int cx = x + dx, cy = y + dy;
            
            /* Count black pixels in a small window around this candidate center */
            int black_count = 0;
            int window = (int)(module_size * 1.5f);
            if (window < 3) window = 3;
            
            for (int wy = -window; wy <= window; wy++) {
                for (int wx = -window; wx <= window; wx++) {
                    if (image_get(img, cx + wx, cy + wy)) {
                        black_count++;
                    }
                }
            }
            
            /* Prefer the candidate that's closest to our initial estimate when black counts are similar */
            int dist_sq = dx*dx + dy*dy;
            int score = black_count * 100 - dist_sq;
            
            if (black_count > max_black_count || 
                (black_count == max_black_count && dist_sq < (best_x - x)*(best_x - x) + (best_y - y)*(best_y - y))) {
                max_black_count = black_count;
                best_x = cx;
                best_y = cy;
            }
        }
    }
    
    *px = best_x;
    *py = best_y;
}

/* Calculate QR transform from finder patterns.
 * If version_override > 0, use that version instead of computing from geometry. */
static QRTransform calculate_qr_transform(FinderPattern *fp, int tl, int tr, int bl, int version_override) {
    QRTransform transform;
    memset(&transform, 0, sizeof(transform));
    
    /* Store finder pattern positions (may be refined below) */
    transform.tl_x = fp[tl].x;
    transform.tl_y = fp[tl].y;
    transform.tr_x = fp[tr].x;
    transform.tr_y = fp[tr].y;
    transform.bl_x = fp[bl].x;
    transform.bl_y = fp[bl].y;
    
    /* Calculate distances between finders */
    float dx_tr = fp[tr].x - fp[tl].x;
    float dy_tr = fp[tr].y - fp[tl].y;
    float dx_bl = fp[bl].x - fp[tl].x;
    float dy_bl = fp[bl].y - fp[tl].y;
    
    float dist_tr = fsqrt(dx_tr*dx_tr + dy_tr*dy_tr);
    float dist_bl = fsqrt(dx_bl*dx_bl + dy_bl*dy_bl);
    
    /* Calculate module sizes - geometric mean of 3 values for overall scaling */
    float geo_module = cbrtf(fp[tl].module_size * fp[tr].module_size * fp[bl].module_size);
    if (!geo_module) geo_module = 1.0f;  /* Avoid zero division */
    
    /* Use TL finder module size for corner calculation since TL finder is the reference point */
    float tl_module = fp[tl].module_size;
    
    /* Calculate rotation angle from top-left to top-right */
    transform.angle = atan2(dy_tr, dx_tr);
    
    /* Calculate QR size from finder distances.
     * The distance between finder centers is (size - 7) modules.
     * 
     * For skewed/stretched images, we need to use directional module sizes.
     * dist_tr (TL->TR) is primarily horizontal, so use module_size_x
     * dist_bl (TL->BL) is primarily vertical, so use module_size_y
     * 
     * For rotated images, we also correct for the angle-based stretch. */
    
    /* Get average directional module sizes */
    float avg_module_x = (fp[tl].module_size_x + fp[tr].module_size_x + fp[bl].module_size_x) / 3.0f;
    float avg_module_y = (fp[tl].module_size_y + fp[tr].module_size_y + fp[bl].module_size_y) / 3.0f;
    if (avg_module_x < 1.0f) avg_module_x = geo_module;
    if (avg_module_y < 1.0f) avg_module_y = geo_module;
    
    /* Calculate the angle-based stretch correction.
     * For rotated patterns, the scanned module size is inflated because
     * horizontal/vertical scanlines cut across modules at an angle. */
    float angle_abs = fabsf(transform.angle);
    /* Normalize to 0-45° range to find minimum stretch factor */
    while (angle_abs > 0.7854f) angle_abs -= 1.5708f;  /* M_PI_4, M_PI_2 */
    angle_abs = fabsf(angle_abs);
    float stretch_factor = 1.0f / cosf(angle_abs);
    if (stretch_factor < 1.0f) stretch_factor = 1.0f;  /* Safety clamp */
    
    /* Correct the directional module sizes for rotation */
    float corrected_module_x = avg_module_x / stretch_factor;
    float corrected_module_y = avg_module_y / stretch_factor;
    
    /* Calculate module count using appropriate directional modules.
     * dist_tr is mostly horizontal -> use module_x
     * dist_bl is mostly vertical -> use module_y */
    float modules_tr = dist_tr / corrected_module_x;
    float modules_bl = dist_bl / corrected_module_y;
    float modules_est = (modules_tr + modules_bl) / 2.0f;
    
    if (debug_finder) {  // DEBUG
        fprintf(stderr, "  version calc: dist_tr=%.1f dist_bl=%.1f mod_x=%.1f mod_y=%.1f angle=%.1f° stretch=%.2f modules_tr=%.1f modules_bl=%.1f modules_est=%.1f\n",  // DEBUG
                dist_tr, dist_bl, avg_module_x, avg_module_y, transform.angle * 57.2958f, stretch_factor, modules_tr, modules_bl, modules_est);  // DEBUG
    }  // DEBUG
    
    /* Add 7 modules for finder patterns and round to valid QR size */
    int qr_modules = (int)(modules_est + 7.5f);
    
    /* Round to nearest valid QR size: size = 17 + 4*version */
    int version;
    if (version_override > 0) {
        /* Use the override version for brute-force search */
        version = version_override;
    } else {
        version = (qr_modules - 17 + 2) / 4;
        if (version < 1) version = 1;
        if (version > 40) version = 40;
    }
    
    int size = 17 + 4 * version;
    
    /* Store QR code properties */
    transform.version = version;
    transform.size = size;
    transform.module_size = geo_module;
    
    /* Calculate TRUE module size from finder-to-finder geometry using geometric mean of distances */
    float avg_dist = sqrtf(dist_tr * dist_bl);
    float true_module = avg_dist / (size - 7);
    
    /* Calculate scaling factors */
    transform.scale_x = dist_tr / ((size - 7) * true_module);
    transform.scale_y = dist_bl / ((size - 7) * true_module);
    
    /* Calculate QR code corners using all three finder positions for better accuracy.
     * 
     * In module coordinates:
     * - TL finder is at (3.5, 3.5)
     * - TR finder is at (size-3.5, 3.5) 
     * - BL finder is at (3.5, size-3.5)
     *
     * We use the finder-to-finder vectors to determine the QR orientation and scale,
     * then compute corners from the centroid of all three finders for robustness. */
    
    /* Finder positions in module coordinates */
    float tl_mx = 3.5f, tl_my = 3.5f;
    float tr_mx = size - 3.5f, tr_my = 3.5f;
    float bl_mx = 3.5f, bl_my = size - 3.5f;
    
    /* Compute QR corners using affine transformation derived from the three finder positions.
     * 
     * We have three known correspondences:
     *   module (tl_mx, tl_my) -> image (fp[tl].x, fp[tl].y)
     *   module (tr_mx, tr_my) -> image (fp[tr].x, fp[tr].y)
     *   module (bl_mx, bl_my) -> image (fp[bl].x, fp[bl].y)
     *
     * An affine transform is: [x]   [a b tx] [mx]
     *                         [y] = [c d ty] [my]
     *                                        [1 ]
     *
     * We solve for a, b, c, d, tx, ty using the three points.
     * 
     * From the module-to-image mapping:
     *   fp[tl].x = a * tl_mx + b * tl_my + tx
     *   fp[tl].y = c * tl_mx + d * tl_my + ty
     *   fp[tr].x = a * tr_mx + b * tr_my + tx
     *   fp[tr].y = c * tr_mx + d * tr_my + ty
     *   fp[bl].x = a * bl_mx + b * bl_my + tx
     *   fp[bl].y = c * bl_mx + d * bl_my + ty
     */
    
    /* Use vector differences to eliminate tx, ty:
     * (fp[tr] - fp[tl]) = a * (tr_mx - tl_mx) + b * (tr_my - tl_my)  [for x]
     * (fp[bl] - fp[tl]) = a * (bl_mx - tl_mx) + b * (bl_my - tl_my)  [for x]
     *
     * Since tr_my = tl_my (both at y=3.5 in module space) and bl_mx = tl_mx:
     *   dx_tr = a * (tr_mx - tl_mx)  => a = dx_tr / (tr_mx - tl_mx)
     *   dx_bl = b * (bl_my - tl_my)  => b = dx_bl / (bl_my - tl_my)
     * Similarly for c, d.
     */
    
    float module_span_x = tr_mx - tl_mx;  /* size - 7 */
    float module_span_y = bl_my - tl_my;  /* size - 7 */
    
    /* Affine transform coefficients */
    float a = dx_tr / module_span_x;
    float b = dx_bl / module_span_y;
    float c = dy_tr / module_span_x;
    float d = dy_bl / module_span_y;
    float tx = fp[tl].x - a * tl_mx - b * tl_my;
    float ty = fp[tl].y - c * tl_mx - d * tl_my;
    
    /* Now compute QR corners using the affine transform */
    /* QR corner at module (0, 0) */
    transform.qr_tl_x = a * 0 + b * 0 + tx;
    transform.qr_tl_y = c * 0 + d * 0 + ty;
    
    /* QR corner at module (size, 0) */
    transform.qr_tr_x = a * size + b * 0 + tx;
    transform.qr_tr_y = c * size + d * 0 + ty;
    
    /* QR corner at module (0, size) */
    transform.qr_bl_x = a * 0 + b * size + tx;
    transform.qr_bl_y = c * 0 + d * size + ty;
    
    /* QR corner at module (size, size) */
    transform.qr_br_x = a * size + b * size + tx;
    transform.qr_br_y = c * size + d * size + ty;
    
    /* Calculate homography matrix */
    /* Source points in QR module space (normalized 0-1) */
    Point2D src[4] = {
        {0, 0},                 /* Top-left */
        {1, 0},                 /* Top-right */
        {0, 1},                 /* Bottom-left */
        {1, 1}                  /* Bottom-right */
    };
    
    /* Destination points in image space */
    Point2D dst[4] = {
        {transform.qr_tl_x, transform.qr_tl_y}, /* Top-left */
        {transform.qr_tr_x, transform.qr_tr_y}, /* Top-right */
        {transform.qr_bl_x, transform.qr_bl_y}, /* Bottom-left */
        {transform.qr_br_x, transform.qr_br_y}  /* Bottom-right */
    };
    
    /* Calculate the homography transform */
    transform.use_homography = calculate_homography(src, dst, &transform.homography);
    
    return transform;
}

/* Apply transform to sample module at position (mx, my) 
 * If use_antialiasing is true, uses weighted 3x3 majority vote for robustness.
 * If false, samples only the center pixel (better for format info). */
static int sample_module_transform_ex(Image *img, const QRTransform *transform, int mx, int my, int use_antialiasing) {
    /* Calculate normalized coordinates in the QR code (0.0 to 1.0) */
    float u = (mx + 0.5f) / transform->size;
    float v = (my + 0.5f) / transform->size;
    
    float px, py;
    
    if (transform->use_homography) {
        /* Use homography transform for more accurate mapping */
        Point2D p = apply_homography(&transform->homography, u, v);
        px = p.x;
        py = p.y;
    } else {
        /* Fall back to bilinear interpolation between the four corners */
        px = (1-u) * (1-v) * transform->qr_tl_x + 
             u * (1-v) * transform->qr_tr_x + 
             (1-u) * v * transform->qr_bl_x + 
             u * v * transform->qr_br_x;
               
        py = (1-u) * (1-v) * transform->qr_tl_y + 
             u * (1-v) * transform->qr_tr_y + 
             (1-u) * v * transform->qr_bl_y + 
             u * v * transform->qr_br_y;
    }
    
    if (!use_antialiasing) {
        /* Center-only sampling - more accurate for format info */
        int ix = (int)(px + 0.5f);
        int iy = (int)(py + 0.5f);
        return image_get(img, ix, iy) ? 1 : 0;
    }
    
    /* Anti-aliasing sampling - check surrounding pixels and use majority vote 
     * This helps with rotated codes where the modules may not align with pixels */
    int sample_radius = 1;  /* Sample in a 3x3 area */
    int black_count = 0;
    int total_samples = 0;
    
    for (int dy = -sample_radius; dy <= sample_radius; dy++) {
        for (int dx = -sample_radius; dx <= sample_radius; dx++) {
            /* Use weighted samples - center pixel counts more */
            float weight = (dx == 0 && dy == 0) ? 2.0f : 1.0f;
            
            int ix = (int)(px + dx + 0.5f);
            int iy = (int)(py + dy + 0.5f);
            
            if (image_get(img, ix, iy)) {
                black_count += weight;
            }
            total_samples += weight;
        }
    }
    
    /* Return 1 (black) if majority of weighted samples are black */
    return (black_count * 2 > total_samples) ? 1 : 0;
}

/* Apply transform to sample module at position (mx, my) with anti-aliasing */
static int sample_module_transform(Image *img, const QRTransform *transform, int mx, int my) {
    return sample_module_transform_ex(img, transform, mx, my, 1);
}

/* Sample module using perspective-corrected coordinates with anti-aliasing 
 * Legacy function for compatibility - forwards to the new transform-based implementation */
static int sample_module_perspective(Image *img, 
                                     float tl_x, float tl_y,
                                     float tr_x, float tr_y,
                                     float bl_x, float bl_y,
                                     int size, int mx, int my) {
    /* Create a temporary transform */
    QRTransform temp;
    
    /* Calculate bottom-right corner */
    float br_x = tr_x + bl_x - tl_x;
    float br_y = tr_y + bl_y - tl_y;
    
    /* Fill in transform corners */
    temp.qr_tl_x = tl_x;
    temp.qr_tl_y = tl_y;
    temp.qr_tr_x = tr_x;
    temp.qr_tr_y = tr_y;
    temp.qr_bl_x = bl_x;
    temp.qr_bl_y = bl_y;
    temp.qr_br_x = br_x;
    temp.qr_br_y = br_y;
    temp.size = size;
    
    /* Use the centralized transform function */
    return sample_module_transform(img, &temp, mx, my);
}

/* Detect and decode a single QR code from finder patterns */
static int decode_qr_from_finders(Image *img, FinderPattern *fp, int nfp, char *output, int max_output) {
    if (nfp < 3) {
        if (debug_finder) fprintf(stderr, "  decode_qr: nfp < 3\n");  // DEBUG
        return 0;
    }
    
    /* Identify which finder is top-left, top-right, bottom-left */
    int tl, tr, bl;
    if (!identify_finder_roles(fp, &tl, &tr, &bl)) {
        if (debug_finder) fprintf(stderr, "  decode_qr: identify_finder_roles failed\n");  // DEBUG
        return 0;
    }
    if (debug_finder) fprintf(stderr, "  decode_qr: trying fp[%d,%d,%d] -> tl=%d tr=%d bl=%d\n",  // DEBUG
                              fp[0].x, fp[0].y, fp[1].x, tl, tr, bl);  // DEBUG
    
    /* First, get the geometrically estimated version */
    QRTransform initial_transform = calculate_qr_transform(fp, tl, tr, bl, 0);
    int estimated_version = initial_transform.version;
    
    /* Build a list of versions to try.
     * For large QR codes (v25+), geometric estimation can be off by 1-2 due to
     * sub-pixel module size measurement errors. We try nearby versions (±2) for these.
     * Also for small module sizes (< 8 pixels), estimation is less accurate.
     * For other codes, the geometric estimate is accurate enough - only use it. */
    int versions_to_try[10];  /* At most: estimated + 4 nearby + potential fallback */
    int num_versions = 0;
    
    /* Always try the estimated version first */
    versions_to_try[num_versions++] = estimated_version;
    
    /* For large versions (v25+) or small module sizes (< 8px), also try nearby versions */
    float module_size = initial_transform.module_size;
    if (estimated_version >= 25 || module_size < 8.0f) {
        for (int delta = -2; delta <= 2; delta++) {
            int v = estimated_version + delta;
            if (v >= 1 && v <= 40 && v != estimated_version) {
                versions_to_try[num_versions++] = v;
            }
        }
    }
    
    if (debug_finder && num_versions > 1) {  // DEBUG
        fprintf(stderr, "  Versions to try (%d): ", num_versions);
        for (int i = 0; i < num_versions; i++) {
            fprintf(stderr, "%d ", versions_to_try[i]);
        }
        fprintf(stderr, "\n");
    }
    
    /* Try each version until one succeeds */
    for (int vi = 0; vi < num_versions; vi++) {
        int try_version = versions_to_try[vi];
    
    /* Calculate transform from finder patterns */
    QRTransform transform = calculate_qr_transform(fp, tl, tr, bl, try_version);
    
    /* Debugging output */
    if (debug_finder) {  // DEBUG
        fprintf(stderr, "  avg_modules=%.2f -> qr_modules=%d version=%d size=%d\n",  // DEBUG
                transform.size - 7.0f, transform.size, transform.version, transform.size);  // DEBUG
        fprintf(stderr, "  derived: module_size=%.2f (x=%.2f y=%.2f)\n",  // DEBUG
                transform.module_size, transform.scale_x * transform.module_size,  // DEBUG
                transform.scale_y * transform.module_size);  // DEBUG
        fprintf(stderr, "  finders: tl=(%d,%d) tr=(%d,%d) bl=(%d,%d)\n",  // DEBUG
                fp[tl].x, fp[tl].y, fp[tr].x, fp[tr].y, fp[bl].x, fp[bl].y);  // DEBUG
        fprintf(stderr, "  corners: tl=(%.1f,%.1f) tr=(%.1f,%.1f) bl=(%.1f,%.1f)\n",  // DEBUG
                transform.qr_tl_x, transform.qr_tl_y,  // DEBUG
                transform.qr_tr_x, transform.qr_tr_y,  // DEBUG
                transform.qr_bl_x, transform.qr_bl_y);  // DEBUG
  // DEBUG
        if (transform.use_homography) {  // DEBUG
            fprintf(stderr, "  homography: enabled (using full projective transform)\n");  // DEBUG
            fprintf(stderr, "  homography matrix: [%.3f %.3f %.3f; %.3f %.3f %.3f; %.3f %.3f 1.000]\n",  // DEBUG
                    transform.homography.h[0][0], transform.homography.h[0][1], transform.homography.h[0][2],  // DEBUG
                    transform.homography.h[1][0], transform.homography.h[1][1], transform.homography.h[1][2],  // DEBUG
                    transform.homography.h[2][0], transform.homography.h[2][1]);  // DEBUG
        } else {  // DEBUG
            fprintf(stderr, "  homography: disabled (falling back to bilinear interpolation)\n");  // DEBUG
        }  // DEBUG
    }  // DEBUG
    
    /* Create QR code structure */
    QRCode qr;
    qr.version = transform.version;
    qr.size = transform.size;
    
    /* Sanity check: all corners should be within image bounds (with margin) */
    float margin = -5.0f;  /* Allow slightly outside */
    if (transform.qr_tl_x < margin || transform.qr_tl_y < margin || 
        transform.qr_tr_x < margin || transform.qr_tr_y < margin ||
        transform.qr_bl_x < margin || transform.qr_bl_y < margin ||
        transform.qr_br_x < margin || transform.qr_br_y < margin ||
        transform.qr_tl_x > img->width + 5 || transform.qr_tl_y > img->height + 5 ||
        transform.qr_tr_x > img->width + 5 || transform.qr_tr_y > img->height + 5 ||
        transform.qr_bl_x > img->width + 5 || transform.qr_bl_y > img->height + 5 ||
        transform.qr_br_x > img->width + 5 || transform.qr_br_y > img->height + 5) {
        if (debug_finder) {  // DEBUG
            fprintf(stderr, "  Rejecting v%d: corners outside image bounds\n", try_version);  // DEBUG
        }  // DEBUG
        continue;  /* Try next version */
    }
    
    /* Sample all modules using the transform */
    for (int my = 0; my < transform.size; my++) {
        for (int mx = 0; mx < transform.size; mx++) {
            qr.modules[my][mx] = sample_module_transform(img, &transform, mx, my);
        }
    }
    
    /* Debug: print sampled QR code */
    if (debug_finder) {  // DEBUG
        fprintf(stderr, "  Sampled QR (%dx%d):\n", transform.size, transform.size);  // DEBUG
        for (int my = 0; my < transform.size; my++) {  // DEBUG
            fprintf(stderr, "  ");  // DEBUG
            for (int mx = 0; mx < transform.size; mx++) {  // DEBUG
                fprintf(stderr, "%c", qr.modules[my][mx] ? '#' : '.');  // DEBUG
            }  // DEBUG
            fprintf(stderr, "\n");  // DEBUG
        }  // DEBUG
    }  // DEBUG
    
    /* Debug dump: sampled grid */
    debug_dump_grid(qr.modules, transform.size, transform.version, "sampled");  // DEBUG
    
    /* Read format info directly from image without anti-aliasing.
     * Format bits can fall on module boundaries where anti-aliasing causes errors. */
    int format_info = read_format_info_direct(img, &transform);
    
    /* Try to correct format info using BCH error correction */
    int corrected_format = validate_format_info_with_correction(format_info);
    
    /* If direct read fails (even with correction), try reading from pre-sampled grid */
    if (corrected_format < 0) {
        format_info = read_format_info(&qr);
        corrected_format = validate_format_info_with_correction(format_info);
    }
    
    /* Reject if still invalid after correction attempts */
    if (corrected_format < 0) {
        if (debug_finder) {  // DEBUG
            fprintf(stderr, "  v%d: Format info 0x%04x invalid (BCH check failed), trying next version\n",  // DEBUG
                    try_version, format_info);  // DEBUG
        }  // DEBUG
        continue;  /* Try next version */
    }
    
    /* Use the corrected format info */
    format_info = corrected_format;
    
    /* After XOR with 0x5412, format info (15 bits) is:
     * Bits 0-9: BCH error correction
     * Bits 10-12: mask pattern (0-7)
     * Bits 13-14: error correction level (0=M, 1=L, 2=H, 3=Q)
     */
    int mask_pattern = (format_info >> 10) & 0x07;
    int ecc_level = (format_info >> 13) & 0x03;
    
    if (debug_finder) {  // DEBUG
        fprintf(stderr, "  QR version=%d size=%d format=0x%04x ecc=%d mask=%d\n",  // DEBUG
                transform.version, transform.size, format_info, ecc_level, mask_pattern);  // DEBUG
    }  // DEBUG
    
    unmask_qr(&qr, mask_pattern);
    
    /* Debug: print unmasked QR code */
    if (debug_finder) {  // DEBUG
        fprintf(stderr, "  Unmasked QR:\n");  // DEBUG
        for (int my = 0; my < transform.size; my++) {  // DEBUG
            fprintf(stderr, "  ");  // DEBUG
            for (int mx = 0; mx < transform.size; mx++) {  // DEBUG
                fprintf(stderr, "%c", qr.modules[my][mx] ? '#' : '.');  // DEBUG
            }  // DEBUG
            fprintf(stderr, "\n");  // DEBUG
        }  // DEBUG
        /* Verify specific modules near data start - only do this for large codes */  // DEBUG
        if (transform.size >= 53) {  // DEBUG
            fprintf(stderr, "  DEBUG: qr.modules[51][52] = %d\n", qr.modules[51][52]);  // DEBUG
            fprintf(stderr, "  DEBUG: qr.modules[52][52] = %d\n", qr.modules[52][52]);  // DEBUG
            fprintf(stderr, "  DEBUG: Row 51 cols 50-52: ");  // DEBUG
            for (int i = 50; i <= 52; i++) {  // DEBUG
                fprintf(stderr, "[%d]=%d ", i, qr.modules[51][i]);  // DEBUG
            }  // DEBUG
            fprintf(stderr, "\n");  // DEBUG
            fprintf(stderr, "  DEBUG: Row 52 cols 50-52: ");  // DEBUG
            for (int i = 50; i <= 52; i++) {  // DEBUG
                fprintf(stderr, "[%d]=%d ", i, qr.modules[52][i]);  // DEBUG
            }  // DEBUG
            fprintf(stderr, "\n");  // DEBUG
        }  // DEBUG
    }  // DEBUG
    
    /* Debug dump: unmasked grid */
    debug_dump_grid(qr.modules, transform.size, transform.version, "unmasked");  // DEBUG
    
    /* Extract raw codewords (still interleaved) */
    uint8_t raw_codewords[4096];
    int raw_len = extract_qr_data(&qr, raw_codewords, sizeof(raw_codewords));
    
    if (debug_finder) {  // DEBUG
        fprintf(stderr, "  raw bytes (first 32): ");  // DEBUG
        for (int i = 0; i < 32 && i < raw_len; i++) {  // DEBUG
            fprintf(stderr, "%02x ", raw_codewords[i]);  // DEBUG
        }  // DEBUG
        fprintf(stderr, "\n");  // DEBUG
        fprintf(stderr, "  raw bytes around 215-220: ");  // DEBUG
        for (int i = 213; i < 222 && i < raw_len; i++) {  // DEBUG
            fprintf(stderr, "[%d]=%02x ", i, raw_codewords[i]);  // DEBUG
        }  // DEBUG
        fprintf(stderr, "\n");  // DEBUG
    }  // DEBUG
    
    /* Debug dump: raw bits */
    debug_dump_bits(raw_codewords, raw_len, transform.version, ecc_level);  // DEBUG
    
    /* Deinterleave codewords and apply RS error correction */
    uint8_t codewords[4096];
    int rs_errors = 0;
    int rs_uncorrectable = 0;
    int data_len = deinterleave_and_correct(raw_codewords, raw_len, transform.version, ecc_level,
                                             codewords, sizeof(codewords), &rs_errors, &rs_uncorrectable);
    
    if (debug_finder) {  // DEBUG
        fprintf(stderr, "  raw_len=%d deinterleaved data_len=%d expected=%d rs_errors=%d\n",  // DEBUG
                raw_len, data_len, data_codewords[transform.version][ecc_level], rs_errors);  // DEBUG
        fprintf(stderr, "  deinterleaved bytes (first 32): ");  // DEBUG
        for (int i = 0; i < 32 && i < data_len; i++) {  // DEBUG
            fprintf(stderr, "%02x ", codewords[i]);  // DEBUG
        }  // DEBUG
        fprintf(stderr, "\n");  // DEBUG
        fprintf(stderr, "  deinterleaved bytes (last 10): ");  // DEBUG
        for (int i = data_len - 10; i < data_len; i++) {  // DEBUG
            if (i >= 0) fprintf(stderr, "[%d]=%02x ", i, codewords[i]);  // DEBUG
        }  // DEBUG
        fprintf(stderr, "\n");  // DEBUG
    }  // DEBUG
    
    /* Debug dump: codewords */
    debug_dump_codewords(raw_codewords, raw_len, codewords, data_len, transform.version, ecc_level);  // DEBUG
    
    /* Decode content */
    int result = decode_qr_content(codewords, data_len, transform.version, output, max_output);
    
    /* If decode succeeded, check if we should accept it */
    if (result > 0) {
        /* When doing version brute-force (trying multiple versions), reject decodes
         * with uncorrectable RS errors - this usually indicates wrong version guess.
         * Only apply this check when we have alternatives to try (num_versions > 1). */
        if (num_versions > 1 && rs_uncorrectable > 0) {
            if (debug_finder) {  // DEBUG
                fprintf(stderr, "  v%d decode rejected: %d uncorrectable RS blocks (trying alternatives)\n",  // DEBUG
                        try_version, rs_uncorrectable);  // DEBUG
            }  // DEBUG
            continue;  /* Try next version */
        }
        
        /* Increment debug counter for next QR */
        if (debug_mode) {  // DEBUG
            debug_qr_counter += 10;  /* BASIC style: 10, 20, 30, ... */  // DEBUG
        }  // DEBUG
        
        if (vi > 0 && debug_finder) {  // DEBUG
            fprintf(stderr, "  Version brute-force: estimated v%d, decoded with v%d\n",
                    estimated_version, try_version);
        }
        
        return result;
    }
    
    /* Decode failed, try next version */
    }  /* end version loop */
    
    /* All versions failed */
    if (debug_finder) {  // DEBUG
        fprintf(stderr, "  decode_qr: all %d versions failed\n", num_versions);  // DEBUG
    }  // DEBUG
    return 0;
}

/* Find and decode all QR codes in an image */
static int scan_image_for_qr(Image *img, ChunkList *chunks);

/* ============================================================================
 * PHASE 3: CHUNK ORGANIZATION
 * ============================================================================ */

STATIC void chunk_list_init(ChunkList *cl) {
    cl->chunks = NULL;
    cl->count = 0;
    cl->capacity = 0;
}

STATIC void chunk_list_free(ChunkList *cl) {
    for (int i = 0; i < cl->count; i++) {
        free(cl->chunks[i].data);
    }
    free(cl->chunks);
    cl->chunks = NULL;
    cl->count = 0;
    cl->capacity = 0;
}

STATIC int chunk_list_add(ChunkList *cl, char type, int index, int total, 
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
    if (!c->data) return 0;
    memcpy(c->data, data, data_len);
    c->data_len = data_len;
    cl->count++;
    return 1;
}

/* Parse QR content to extract chunk info: "N01/50:base64data..." */
STATIC int parse_chunk_label(const char *content, char *type, int *index, int *total, 
                             const char **data_start) {
    if (content[0] != 'N' && content[0] != 'P') return 0;
    *type = content[0];
    
    /* Parse index */
    const char *p = content + 1;
    *index = 0;
    while (*p >= '0' && *p <= '9') {
        *index = *index * 10 + (*p - '0');
        p++;
    }
    
    if (*p != '/') return 0;
    p++;
    
    /* Parse total */
    *total = 0;
    while (*p >= '0' && *p <= '9') {
        *total = *total * 10 + (*p - '0');
        p++;
    }
    
    /* Skip colon or other separator */
    while (*p && (*p == ':' || *p == ' ')) p++;
    
    *data_start = p;
    return 1;
}

/* Sort chunks by type then index */
static int chunk_compare(const void *a, const void *b) {
    const Chunk *ca = a;
    const Chunk *cb = b;
    
    if (ca->type != cb->type) return ca->type - cb->type;
    return ca->index - cb->index;
}

STATIC void chunk_list_sort(ChunkList *cl) {
    if (cl->count > 1) {
        qsort(cl->chunks, cl->count, sizeof(Chunk), chunk_compare);
    }
}

/* Remove duplicate chunks (same type and index) */
STATIC void chunk_list_dedupe(ChunkList *cl) {
    if (cl->count < 2) return;
    
    int write = 1;
    for (int read = 1; read < cl->count; read++) {
        if (cl->chunks[read].type != cl->chunks[write-1].type ||
            cl->chunks[read].index != cl->chunks[write-1].index) {
            if (write != read) {
                cl->chunks[write] = cl->chunks[read];
            }
            write++;
        } else {
            free(cl->chunks[read].data);
        }
    }
    cl->count = write;
}

/* ============================================================================
 * PHASE 4: BASE64 DECODING
 * ============================================================================ */

static const int8_t b64_decode_table[256] = {
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
    52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
    -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
    15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
    -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
    41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
};

STATIC int base64_decode(const char *input, int input_len, uint8_t *output, int max_output) {
    int out_len = 0;
    uint32_t accum = 0;
    int bits = 0;
    
    for (int i = 0; i < input_len; i++) {
        int c = (unsigned char)input[i];
        if (c == '=' || c == '\n' || c == '\r' || c == ' ') continue;
        
        int val = b64_decode_table[c];
        if (val < 0) continue;  /* Skip invalid characters */
        
        accum = (accum << 6) | val;
        bits += 6;
        
        if (bits >= 8) {
            bits -= 8;
            if (out_len < max_output) {
                output[out_len++] = (accum >> bits) & 0xFF;
            }
        }
    }
    
    return out_len;
}

/* ============================================================================
 * PHASE 5: REED-SOLOMON ERASURE CODING RECOVERY
 * ============================================================================ */

/* GF(2^8) for erasure coding - different from QR's GF */
STATIC uint8_t ec_gf_exp[512];
STATIC uint8_t ec_gf_log[256];
static int ec_gf_initialized = 0;

STATIC void ec_gf_init(void) {
    if (ec_gf_initialized) return;
    
    /* Using primitive polynomial x^8 + x^4 + x^3 + x^2 + 1 (0x11d) */
    int x = 1;
    for (int i = 0; i < 255; i++) {
        ec_gf_exp[i] = x;
        ec_gf_log[x] = i;
        x <<= 1;
        if (x & 0x100) x ^= 0x11d;
    }
    for (int i = 255; i < 512; i++) {
        ec_gf_exp[i] = ec_gf_exp[i - 255];
    }
    ec_gf_initialized = 1;
}

STATIC uint8_t ec_gf_mul(uint8_t a, uint8_t b) {
    if (a == 0 || b == 0) return 0;
    return ec_gf_exp[ec_gf_log[a] + ec_gf_log[b]];
}

STATIC uint8_t ec_gf_inv(uint8_t a) {
    if (a == 0) return 0;
    return ec_gf_exp[255 - ec_gf_log[a]];
}

/* Recover missing data chunks using parity chunks
 * This implements Reed-Solomon erasure recovery in GF(2^8)
 * 
 * qr-backup uses a systematic RS code where:
 * - N chunks are the original data
 * - P chunks are parity computed as P[j] = sum(N[i] * alpha^(i*j)) for all i
 * 
 * For recovery, we solve a system of equations in GF(2^8) using
 * Gaussian elimination.
 */
STATIC int erasure_recover(ChunkList *cl, int total_normal, int total_parity) {
    ec_gf_init();
    
    /* Count available normal and parity chunks */
    int have_normal = 0;
    int have_parity = 0;
    
    for (int i = 0; i < cl->count; i++) {
        if (cl->chunks[i].type == 'N') have_normal++;
        else if (cl->chunks[i].type == 'P') have_parity++;
    }
    
    int missing = total_normal - have_normal;
    if (missing == 0) return 1;  /* All data present */
    if (missing > have_parity || missing > total_parity) {
        fprintf(stderr, "Warning: %d chunks missing, only %d parity available\n", 
                missing, have_parity);
        return 0;  /* Cannot recover */
    }
    
    /* Find which chunks are missing and which are present */
    int *n_present = calloc(total_normal + 1, sizeof(int));
    int *p_present = calloc(total_parity + 1, sizeof(int));
    int *missing_indices = calloc(missing, sizeof(int));
    if (!n_present || !p_present || !missing_indices) {
        free(n_present); free(p_present); free(missing_indices);
        return 0;
    }
    
    for (int i = 0; i < cl->count; i++) {
        if (cl->chunks[i].type == 'N' && cl->chunks[i].index <= total_normal) {
            n_present[cl->chunks[i].index] = 1;
        } else if (cl->chunks[i].type == 'P' && cl->chunks[i].index <= total_parity) {
            p_present[cl->chunks[i].index] = 1;
        }
    }
    
    int missing_count = 0;
    for (int i = 1; i <= total_normal && missing_count < missing; i++) {
        if (!n_present[i]) {
            missing_indices[missing_count++] = i;
        }
    }
    
    if (debug_finder) {  // DEBUG
        fprintf(stderr, "  Erasure recovery: %d missing chunks [", missing);  // DEBUG
        for (int i = 0; i < missing; i++) {  // DEBUG
            fprintf(stderr, "%s%d", i > 0 ? "," : "", missing_indices[i]);  // DEBUG
        }  // DEBUG
        fprintf(stderr, "], %d parity available\n", have_parity);  // DEBUG
    }  // DEBUG
    
    /* Debug dump: erasure recovery initial state (result filled in on success paths) */
    debug_dump_erasure(total_normal, total_parity, have_normal, have_parity,  // DEBUG
                       missing_indices, missing, 0);  // DEBUG
    
    /* Simple XOR parity recovery for single erasure */
    if (missing == 1 && have_parity >= 1) {
        int missing_idx = missing_indices[0];
        
        /* Find first parity chunk (P1 is simple XOR of all N chunks) */
        Chunk *parity = NULL;
        for (int i = 0; i < cl->count; i++) {
            if (cl->chunks[i].type == 'P' && cl->chunks[i].index == 1) {
                parity = &cl->chunks[i];
                break;
            }
        }
        
        /* If no P1, try any parity chunk */
        if (!parity) {
            for (int i = 0; i < cl->count; i++) {
                if (cl->chunks[i].type == 'P') {
                    parity = &cl->chunks[i];
                    break;
                }
            }
        }
        
        if (parity && parity->data_len > 0) {
            /* Determine chunk size from parity or other chunks */
            int chunk_size = parity->data_len;
            uint8_t *recovered = malloc(chunk_size);
            
            if (recovered) {
                memcpy(recovered, parity->data, chunk_size);
                
                /* XOR with all present N chunks */
                for (int i = 0; i < cl->count; i++) {
                    if (cl->chunks[i].type == 'N') {
                        int len = cl->chunks[i].data_len;
                        if (len > chunk_size) len = chunk_size;
                        for (int j = 0; j < len; j++) {
                            recovered[j] ^= cl->chunks[i].data[j];
                        }
                    }
                }
                
                /* Add recovered chunk */
                chunk_list_add(cl, 'N', missing_idx, total_normal, recovered, chunk_size);
                
                if (debug_finder) {  // DEBUG
                    fprintf(stderr, "  Recovered N%d using XOR parity\n", missing_idx);  // DEBUG
                }  // DEBUG
                
                /* Update debug file with success and recovered data */
                uint8_t *rec_ptrs[1] = { recovered };
                int rec_lens[1] = { chunk_size };
                debug_dump_erasure_ex(total_normal, total_parity, have_normal, have_parity,  // DEBUG
                                      &missing_idx, 1, 1, rec_ptrs, rec_lens, "XOR parity");  // DEBUG
                
                free(recovered);
                free(n_present); free(p_present); free(missing_indices);
                return 1;
            }
        }
    }
    
    /* Multi-erasure recovery using Cauchy matrix in GF(2^8)
     * 
     * The generator matrix is [I | P] where P is a Cauchy matrix:
     * P[i][j] = 1 / (x[i] + y[j]) where x and y are distinct field elements
     * 
     * For qr-backup, we use x[i] = i (data indices) and y[j] = 255-j (parity indices)
     * This ensures the Vandermonde-like matrix is invertible.
     */
    if (missing > 1 && have_parity >= missing) {
        /* Find chunk size from any available chunk */
        int chunk_size = 0;
        for (int i = 0; i < cl->count && chunk_size == 0; i++) {
            chunk_size = cl->chunks[i].data_len;
        }
        
        if (chunk_size == 0) {
            free(n_present); free(p_present); free(missing_indices);
            return 0;
        }
        
        /* Build the recovery matrix
         * We need to select 'missing' parity chunks and solve for missing data
         * 
         * The system is: P_selected = M * D_missing
         * Where M is derived from the Cauchy encoding matrix
         * 
         * For simplicity, use a Vandermonde-like matrix:
         * M[i][j] = alpha^((missing_idx[j]-1) * parity_idx[i])
         */
        
        /* Select which parity chunks to use */
        int *parity_used = calloc(missing, sizeof(int));
        int parity_count = 0;
        for (int p = 1; p <= total_parity && parity_count < missing; p++) {
            if (p_present[p]) {
                parity_used[parity_count++] = p;
            }
        }
        
        if (parity_count < missing) {
            free(n_present); free(p_present); free(missing_indices); free(parity_used);
            return 0;
        }
        
        /* Allocate matrix for Gaussian elimination (missing x missing) */
        uint8_t **matrix = malloc(missing * sizeof(uint8_t *));
        uint8_t **augmented = malloc(missing * sizeof(uint8_t *));
        for (int i = 0; i < missing; i++) {
            matrix[i] = malloc(missing);
            augmented[i] = malloc(chunk_size);
        }
        
        /* Build the encoding matrix
         * For parity chunk p, its contribution from data chunk d is:
         * coef = alpha^((d-1) * (p-1))
         * 
         * We only care about the missing data chunks' contribution.
         * The parity - sum(known_data * coef) = sum(missing_data * coef)
         */
        for (int i = 0; i < missing; i++) {
            int p_idx = parity_used[i];
            
            /* Find this parity chunk */
            Chunk *parity = NULL;
            for (int c = 0; c < cl->count; c++) {
                if (cl->chunks[c].type == 'P' && cl->chunks[c].index == p_idx) {
                    parity = &cl->chunks[c];
                    break;
                }
            }
            
            if (!parity) continue;
            
            /* Start with parity data */
            memcpy(augmented[i], parity->data, chunk_size);
            
            /* Subtract contribution from known data chunks */
            for (int c = 0; c < cl->count; c++) {
                if (cl->chunks[c].type == 'N') {
                    int d_idx = cl->chunks[c].index;
                    uint8_t coef = ec_gf_exp[((d_idx - 1) * (p_idx - 1)) % 255];
                    
                    for (int b = 0; b < chunk_size && b < cl->chunks[c].data_len; b++) {
                        augmented[i][b] ^= ec_gf_mul(coef, cl->chunks[c].data[b]);
                    }
                }
            }
            
            /* Fill in the matrix coefficients for missing chunks */
            for (int j = 0; j < missing; j++) {
                int d_idx = missing_indices[j];
                matrix[i][j] = ec_gf_exp[((d_idx - 1) * (p_idx - 1)) % 255];
            }
        }
        
        /* Gaussian elimination with partial pivoting in GF(2^8) */
        int success = 1;
        for (int col = 0; col < missing && success; col++) {
            /* Find pivot */
            int pivot = -1;
            for (int row = col; row < missing; row++) {
                if (matrix[row][col] != 0) {
                    pivot = row;
                    break;
                }
            }
            
            if (pivot < 0) {
                success = 0;
                break;
            }
            
            /* Swap rows if needed */
            if (pivot != col) {
                uint8_t *tmp = matrix[col];
                matrix[col] = matrix[pivot];
                matrix[pivot] = tmp;
                
                tmp = augmented[col];
                augmented[col] = augmented[pivot];
                augmented[pivot] = tmp;
            }
            
            /* Scale pivot row */
            uint8_t pivot_inv = ec_gf_inv(matrix[col][col]);
            for (int j = col; j < missing; j++) {
                matrix[col][j] = ec_gf_mul(matrix[col][j], pivot_inv);
            }
            for (int b = 0; b < chunk_size; b++) {
                augmented[col][b] = ec_gf_mul(augmented[col][b], pivot_inv);
            }
            
            /* Eliminate column */
            for (int row = 0; row < missing; row++) {
                if (row != col && matrix[row][col] != 0) {
                    uint8_t factor = matrix[row][col];
                    for (int j = col; j < missing; j++) {
                        matrix[row][j] ^= ec_gf_mul(factor, matrix[col][j]);
                    }
                    for (int b = 0; b < chunk_size; b++) {
                        augmented[row][b] ^= ec_gf_mul(factor, augmented[col][b]);
                    }
                }
            }
        }
        
        if (success) {
            /* Add recovered chunks */
            for (int i = 0; i < missing; i++) {
                chunk_list_add(cl, 'N', missing_indices[i], total_normal, 
                               augmented[i], chunk_size);
                if (debug_finder) {  // DEBUG
                    fprintf(stderr, "  Recovered N%d using multi-erasure recovery\n",  // DEBUG
                            missing_indices[i]);  // DEBUG
                }  // DEBUG
            }
            
            /* Update debug file with success and recovered data */
            int *rec_lens = malloc(missing * sizeof(int));
            for (int i = 0; i < missing; i++) rec_lens[i] = chunk_size;
            debug_dump_erasure_ex(total_normal, total_parity, have_normal, have_parity,  // DEBUG
                                  missing_indices, missing, 1, augmented, rec_lens,  // DEBUG
                                  "Gaussian elimination in GF(2^8)");  // DEBUG
            free(rec_lens);
        }
        
        /* Cleanup */
        for (int i = 0; i < missing; i++) {
            free(matrix[i]);
            free(augmented[i]);
        }
        free(matrix);
        free(augmented);
        free(parity_used);
        
        free(n_present); free(p_present); free(missing_indices);
        return success;
    }
    
    free(n_present); free(p_present); free(missing_indices);
    
    if (missing > 0) {
        fprintf(stderr, "Warning: %d chunks missing, recovery failed\n", missing);
        return 0;
    }
    
    return 1;
}

/* ============================================================================
 * PHASE 6: DATA ASSEMBLY
 * ============================================================================ */

STATIC uint8_t *assemble_data(ChunkList *cl, int *out_len) {
    /* Find total size needed */
    int total_size = 0;
    for (int i = 0; i < cl->count; i++) {
        if (cl->chunks[i].type == 'N') {
            total_size += cl->chunks[i].data_len;
        }
    }
    
    if (total_size == 0) {
        *out_len = 0;
        return NULL;
    }
    
    uint8_t *data = malloc(total_size);
    if (!data) {
        *out_len = 0;
        return NULL;
    }
    
    /* Concatenate normal chunks in order */
    int pos = 0;
    for (int i = 0; i < cl->count; i++) {
        if (cl->chunks[i].type == 'N') {
            memcpy(data + pos, cl->chunks[i].data, cl->chunks[i].data_len);
            pos += cl->chunks[i].data_len;
        }
    }
    
    /* Parse length prefix - qr-backup uses ASCII decimal followed by space
     * e.g., "336 " for 336 bytes of data */
    int len_end = 0;
    uint32_t real_len = 0;
    
    /* Parse ASCII decimal length */
    while (len_end < pos && data[len_end] >= '0' && data[len_end] <= '9') {
        real_len = real_len * 10 + (data[len_end] - '0');
        len_end++;
    }
    
    /* Skip space separator */
    if (len_end < pos && data[len_end] == ' ') {
        len_end++;
    }
    
    if (len_end == 0) {
        /* No valid length prefix found */
        *out_len = pos;
        return data;
    }
    
    /* The length prefix indicates the COMPRESSED size (gzip data length).
     * This tells us how much of the remaining data is actual content
     * vs padding added to fill the last QR chunk. */
    int remaining = pos - len_end;
    
    /* Use the smaller of: stated length or remaining data */
    int actual_len = (int)real_len;
    if (actual_len > remaining) {
        actual_len = remaining;
    }
    
    /* Remove length prefix and truncate to actual length */
    memmove(data, data + len_end, actual_len);
    *out_len = actual_len;
    
    return data;
}

/* ============================================================================
 * PHASE 7: AES DECRYPTION (GPG Symmetric Mode)
 * ============================================================================ */

/* AES S-box */
static const uint8_t aes_sbox[256] = {
    0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
    0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
    0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
    0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
    0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
    0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
    0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
    0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
    0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
    0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
    0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
    0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
    0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
    0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
    0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
    0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
};

/* Inverse AES S-box */
static const uint8_t aes_inv_sbox[256] = {
    0x52,0x09,0x6a,0xd5,0x30,0x36,0xa5,0x38,0xbf,0x40,0xa3,0x9e,0x81,0xf3,0xd7,0xfb,
    0x7c,0xe3,0x39,0x82,0x9b,0x2f,0xff,0x87,0x34,0x8e,0x43,0x44,0xc4,0xde,0xe9,0xcb,
    0x54,0x7b,0x94,0x32,0xa6,0xc2,0x23,0x3d,0xee,0x4c,0x95,0x0b,0x42,0xfa,0xc3,0x4e,
    0x08,0x2e,0xa1,0x66,0x28,0xd9,0x24,0xb2,0x76,0x5b,0xa2,0x49,0x6d,0x8b,0xd1,0x25,
    0x72,0xf8,0xf6,0x64,0x86,0x68,0x98,0x16,0xd4,0xa4,0x5c,0xcc,0x5d,0x65,0xb6,0x92,
    0x6c,0x70,0x48,0x50,0xfd,0xed,0xb9,0xda,0x5e,0x15,0x46,0x57,0xa7,0x8d,0x9d,0x84,
    0x90,0xd8,0xab,0x00,0x8c,0xbc,0xd3,0x0a,0xf7,0xe4,0x58,0x05,0xb8,0xb3,0x45,0x06,
    0xd0,0x2c,0x1e,0x8f,0xca,0x3f,0x0f,0x02,0xc1,0xaf,0xbd,0x03,0x01,0x13,0x8a,0x6b,
    0x3a,0x91,0x11,0x41,0x4f,0x67,0xdc,0xea,0x97,0xf2,0xcf,0xce,0xf0,0xb4,0xe6,0x73,
    0x96,0xac,0x74,0x22,0xe7,0xad,0x35,0x85,0xe2,0xf9,0x37,0xe8,0x1c,0x75,0xdf,0x6e,
    0x47,0xf1,0x1a,0x71,0x1d,0x29,0xc5,0x89,0x6f,0xb7,0x62,0x0e,0xaa,0x18,0xbe,0x1b,
    0xfc,0x56,0x3e,0x4b,0xc6,0xd2,0x79,0x20,0x9a,0xdb,0xc0,0xfe,0x78,0xcd,0x5a,0xf4,
    0x1f,0xdd,0xa8,0x33,0x88,0x07,0xc7,0x31,0xb1,0x12,0x10,0x59,0x27,0x80,0xec,0x5f,
    0x60,0x51,0x7f,0xa9,0x19,0xb5,0x4a,0x0d,0x2d,0xe5,0x7a,0x9f,0x93,0xc9,0x9c,0xef,
    0xa0,0xe0,0x3b,0x4d,0xae,0x2a,0xf5,0xb0,0xc8,0xeb,0xbb,0x3c,0x83,0x53,0x99,0x61,
    0x17,0x2b,0x04,0x7e,0xba,0x77,0xd6,0x26,0xe1,0x69,0x14,0x63,0x55,0x21,0x0c,0x7d
};

/* AES round constants */
static const uint8_t aes_rcon[11] = {
    0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36
};

/* xtime - multiply by x in GF(2^8) */
static uint8_t xtime(uint8_t x) {
    return (x << 1) ^ ((x >> 7) * 0x1b);
}

/* AES key expansion */
static void aes_key_expand(const uint8_t *key, uint8_t *round_keys, int key_len) {
    int nk = key_len / 4;  /* Number of 32-bit words in key */
    int nr = nk + 6;       /* Number of rounds */
    int nb = 4;            /* Block size in words */
    
    /* Copy original key */
    memcpy(round_keys, key, key_len);
    
    uint8_t temp[4];
    int i = nk;
    
    while (i < nb * (nr + 1)) {
        memcpy(temp, round_keys + (i - 1) * 4, 4);
        
        if (i % nk == 0) {
            /* RotWord */
            uint8_t t = temp[0];
            temp[0] = temp[1]; temp[1] = temp[2]; temp[2] = temp[3]; temp[3] = t;
            /* SubWord */
            for (int j = 0; j < 4; j++) temp[j] = aes_sbox[temp[j]];
            /* XOR with Rcon */
            temp[0] ^= aes_rcon[i / nk];
        } else if (nk > 6 && i % nk == 4) {
            for (int j = 0; j < 4; j++) temp[j] = aes_sbox[temp[j]];
        }
        
        for (int j = 0; j < 4; j++) {
            round_keys[i * 4 + j] = round_keys[(i - nk) * 4 + j] ^ temp[j];
        }
        i++;
    }
}

/* AES inverse cipher (decryption) */
static void aes_decrypt_block(const uint8_t *in, uint8_t *out, const uint8_t *round_keys, int nr) {
    uint8_t state[16];
    memcpy(state, in, 16);
    
    /* Add round key */
    for (int i = 0; i < 16; i++) state[i] ^= round_keys[nr * 16 + i];
    
    for (int round = nr - 1; round >= 0; round--) {
        /* Inverse ShiftRows */
        uint8_t temp;
        temp = state[13]; state[13] = state[9]; state[9] = state[5]; state[5] = state[1]; state[1] = temp;
        temp = state[2]; state[2] = state[10]; state[10] = temp;
        temp = state[6]; state[6] = state[14]; state[14] = temp;
        temp = state[3]; state[3] = state[7]; state[7] = state[11]; state[11] = state[15]; state[15] = temp;
        
        /* Inverse SubBytes */
        for (int i = 0; i < 16; i++) state[i] = aes_inv_sbox[state[i]];
        
        /* Add round key */
        for (int i = 0; i < 16; i++) state[i] ^= round_keys[round * 16 + i];
        
        /* Inverse MixColumns (skip for round 0) */
        if (round > 0) {
            for (int c = 0; c < 4; c++) {
                uint8_t a[4];
                for (int i = 0; i < 4; i++) a[i] = state[c * 4 + i];
                
                state[c * 4 + 0] = gf_mul(0x0e, a[0]) ^ gf_mul(0x0b, a[1]) ^ gf_mul(0x0d, a[2]) ^ gf_mul(0x09, a[3]);
                state[c * 4 + 1] = gf_mul(0x09, a[0]) ^ gf_mul(0x0e, a[1]) ^ gf_mul(0x0b, a[2]) ^ gf_mul(0x0d, a[3]);
                state[c * 4 + 2] = gf_mul(0x0d, a[0]) ^ gf_mul(0x09, a[1]) ^ gf_mul(0x0e, a[2]) ^ gf_mul(0x0b, a[3]);
                state[c * 4 + 3] = gf_mul(0x0b, a[0]) ^ gf_mul(0x0d, a[1]) ^ gf_mul(0x09, a[2]) ^ gf_mul(0x0e, a[3]);
            }
        }
    }
    
    memcpy(out, state, 16);
}

/* Forward declarations */
static void sha256(const uint8_t *data, size_t len, uint8_t *hash);
STATIC uint8_t *gzip_decompress(const uint8_t *data, int data_len, int *out_len);

/* ============================================================================
 * SHA-1 Implementation (for S2K and MDC)
 * ============================================================================ */

static void sha1_transform(uint32_t *state, const uint8_t *data) {
    uint32_t a, b, c, d, e, t, w[80];
    
    /* Prepare message schedule */
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
    
    state[0] += a; state[1] += b; state[2] += c; state[3] += d; state[4] += e;
}

static void sha1(const uint8_t *data, size_t len, uint8_t *hash) {
    uint32_t state[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    uint8_t buffer[64];
    size_t pos = 0;
    
    /* Process complete blocks */
    while (pos + 64 <= len) {
        sha1_transform(state, data + pos);
        pos += 64;
    }
    
    /* Final block with padding */
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
    
    /* Append length in bits */
    uint64_t bits = len * 8;
    buffer[56] = bits >> 56; buffer[57] = bits >> 48;
    buffer[58] = bits >> 40; buffer[59] = bits >> 32;
    buffer[60] = bits >> 24; buffer[61] = bits >> 16;
    buffer[62] = bits >> 8;  buffer[63] = bits;
    sha1_transform(state, buffer);
    
    /* Output hash */
    for (int i = 0; i < 5; i++) {
        hash[i*4] = state[i] >> 24; hash[i*4+1] = state[i] >> 16;
        hash[i*4+2] = state[i] >> 8; hash[i*4+3] = state[i];
    }
}

/* ============================================================================
 * OpenPGP Packet Parsing and GPG Decryption
 * ============================================================================ */

/* S2K (String-to-Key) types */
#define S2K_SIMPLE    0
#define S2K_SALTED    1
#define S2K_ITERATED  3

/* Cipher algorithms */
#define CIPHER_AES128 7
#define CIPHER_AES192 8
#define CIPHER_AES256 9

/* Hash algorithms */
#define HASH_SHA1   2
#define HASH_SHA256 8

/* S2K count decode: count = (16 + (c & 15)) << ((c >> 4) + 6) */
static uint32_t s2k_decode_count(uint8_t c) {
    return (16 + (c & 15)) << ((c >> 4) + 6);
}

/* Iterated and Salted S2K key derivation */
static void s2k_derive_key(const char *password, int pass_len,
                           const uint8_t *salt, int s2k_type, uint8_t hash_algo,
                           uint32_t count, uint8_t *key, int key_len) {
    int hash_len = (hash_algo == HASH_SHA256) ? 32 : 20;
    int pos = 0;
    int preload = 0;  /* Number of zero bytes to prepend for key extension */
    
    while (pos < key_len) {
        /* Build data to hash */
        uint8_t *hash_input = NULL;
        int input_len = 0;
        
        if (s2k_type == S2K_SIMPLE) {
            /* Just hash the password */
            input_len = preload + pass_len;
            hash_input = malloc(input_len);
            memset(hash_input, 0, preload);
            memcpy(hash_input + preload, password, pass_len);
        } else if (s2k_type == S2K_SALTED) {
            /* Hash salt + password */
            input_len = preload + 8 + pass_len;
            hash_input = malloc(input_len);
            memset(hash_input, 0, preload);
            memcpy(hash_input + preload, salt, 8);
            memcpy(hash_input + preload + 8, password, pass_len);
        } else {  /* S2K_ITERATED */
            /* Hash salt + password repeatedly until count bytes processed */
            int sp_len = 8 + pass_len;  /* salt + password length */
            uint32_t actual_count = count;
            if (actual_count < (uint32_t)sp_len) actual_count = sp_len;
            
            input_len = preload + actual_count;
            hash_input = malloc(input_len);
            memset(hash_input, 0, preload);
            
            /* Fill with repeated salt+password */
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
        
        /* Hash and extract key bytes */
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

/* AES encryption (for CFB mode) */
static void aes_encrypt_block(const uint8_t *in, uint8_t *out, const uint8_t *round_keys, int nr) {
    uint8_t state[16];
    memcpy(state, in, 16);
    
    /* Initial round key addition */
    for (int i = 0; i < 16; i++) state[i] ^= round_keys[i];
    
    for (int round = 1; round <= nr; round++) {
        /* SubBytes */
        for (int i = 0; i < 16; i++) state[i] = aes_sbox[state[i]];
        
        /* ShiftRows */
        uint8_t temp;
        temp = state[1]; state[1] = state[5]; state[5] = state[9]; state[9] = state[13]; state[13] = temp;
        temp = state[2]; state[2] = state[10]; state[10] = temp;
        temp = state[6]; state[6] = state[14]; state[14] = temp;
        temp = state[15]; state[15] = state[11]; state[11] = state[7]; state[7] = state[3]; state[3] = temp;
        
        /* MixColumns (skip for final round) */
        if (round < nr) {
            for (int c = 0; c < 4; c++) {
                uint8_t a[4];
                for (int i = 0; i < 4; i++) a[i] = state[c * 4 + i];
                
                state[c * 4 + 0] = xtime(a[0]) ^ xtime(a[1]) ^ a[1] ^ a[2] ^ a[3];
                state[c * 4 + 1] = a[0] ^ xtime(a[1]) ^ xtime(a[2]) ^ a[2] ^ a[3];
                state[c * 4 + 2] = a[0] ^ a[1] ^ xtime(a[2]) ^ xtime(a[3]) ^ a[3];
                state[c * 4 + 3] = xtime(a[0]) ^ a[0] ^ a[1] ^ a[2] ^ xtime(a[3]);
            }
        }
        
        /* AddRoundKey */
        for (int i = 0; i < 16; i++) state[i] ^= round_keys[round * 16 + i];
    }
    
    memcpy(out, state, 16);
}

/* AES-CFB decryption (OpenPGP variant with resync) */
static void aes_cfb_decrypt(const uint8_t *data, int data_len,
                            const uint8_t *key, int key_len,
                            uint8_t *output) {
    int nr = (key_len == 16) ? 10 : (key_len == 24) ? 12 : 14;
    uint8_t round_keys[240];
    aes_key_expand(key, round_keys, key_len);
    
    uint8_t fr[16] = {0};  /* Feedback register - starts as zeros (IV) */
    uint8_t fre[16];       /* Encrypted feedback register */
    int pos = 0;
    
    while (pos < data_len) {
        /* Encrypt feedback register */
        aes_encrypt_block(fr, fre, round_keys, nr);
        
        /* XOR with ciphertext to get plaintext */
        int block_len = 16;
        if (pos + block_len > data_len) block_len = data_len - pos;
        
        for (int i = 0; i < block_len; i++) {
            output[pos + i] = data[pos + i] ^ fre[i];
        }
        
        /* Update feedback register with ciphertext */
        if (block_len == 16) {
            memcpy(fr, data + pos, 16);
        } else {
            /* Partial block: shift and fill */
            memmove(fr, fr + block_len, 16 - block_len);
            memcpy(fr + 16 - block_len, data + pos, block_len);
        }
        
        pos += block_len;
    }
}

/* Parse OpenPGP packet header
 * Returns packet type and sets *body_len and *header_len
 * Returns -1 on error */
static int pgp_parse_header(const uint8_t *data, int len, int *body_len, int *header_len) {
    if (len < 2) return -1;
    
    uint8_t tag_byte = data[0];
    if ((tag_byte & 0x80) == 0) return -1;  /* Not a PGP packet */
    
    int packet_tag;
    int pos = 1;
    
    if (tag_byte & 0x40) {
        /* New format packet */
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
            *body_len = ((uint32_t)data[pos+1] << 24) | ((uint32_t)data[pos+2] << 16) |
                        ((uint32_t)data[pos+3] << 8) | data[pos+4];
            pos += 5;
        } else {
            /* Partial body length - not fully supported */
            *body_len = 1 << (data[pos] & 0x1f);
            pos++;
        }
    } else {
        /* Old format packet */
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
            *body_len = ((uint32_t)data[pos] << 24) | ((uint32_t)data[pos+1] << 16) |
                        ((uint32_t)data[pos+2] << 8) | data[pos+3];
            pos += 4;
        } else {
            /* Indeterminate length */
            *body_len = len - pos;
        }
    }
    
    *header_len = pos;
    return packet_tag;
}

/* Check if data looks like GPG encrypted data */
STATIC int is_gpg_encrypted(const uint8_t *data, int len) {
    if (len < 2) return 0;
    
    /* Check for PGP packet tag */
    if ((data[0] & 0x80) == 0) return 0;
    
    /* Get packet type */
    int packet_tag;
    if (data[0] & 0x40) {
        packet_tag = data[0] & 0x3f;
    } else {
        packet_tag = (data[0] >> 2) & 0x0f;
    }
    
    /* Symmetric-Key Encrypted Session Key (tag 3) or
     * Symmetrically Encrypted Data (tag 9) or
     * Symmetrically Encrypted and Integrity Protected (tag 18) */
    return (packet_tag == 3 || packet_tag == 9 || packet_tag == 18);
}

/* Full GPG symmetric decryption
 * Returns decrypted data or NULL on error */
static uint8_t *gpg_decrypt(const uint8_t *data, int data_len, 
                            const char *password, int *out_len) {
    *out_len = 0;
    
    if (!is_gpg_encrypted(data, data_len)) {
        return NULL;
    }
    
    int pos = 0;
    uint8_t session_key[32];
    int session_key_len = 0;
    int cipher_algo = CIPHER_AES128;
    const uint8_t *encrypted_data = NULL;
    int encrypted_len = 0;
    int has_mdc = 0;
    
    /* Parse packets */
    while (pos < data_len) {
        int body_len, header_len;
        int packet_tag = pgp_parse_header(data + pos, data_len - pos, &body_len, &header_len);
        
        if (packet_tag < 0) break;
        
        const uint8_t *body = data + pos + header_len;
        
        if (packet_tag == 3) {
            /* Symmetric-Key Encrypted Session Key packet */
            if (body_len < 4) return NULL;
            
            int version = body[0];
            if (version != 4) {
                fprintf(stderr, "GPG: Unsupported SKESK version %d\n", version);
                return NULL;
            }
            
            cipher_algo = body[1];
            int s2k_type = body[2];
            int s2k_pos = 3;
            
            uint8_t hash_algo = HASH_SHA1;
            uint8_t salt[8] = {0};
            uint32_t count = 65536;
            
            if (s2k_type == S2K_SIMPLE) {
                hash_algo = body[s2k_pos++];
            } else if (s2k_type == S2K_SALTED) {
                hash_algo = body[s2k_pos++];
                memcpy(salt, body + s2k_pos, 8);
                s2k_pos += 8;
            } else if (s2k_type == S2K_ITERATED) {
                hash_algo = body[s2k_pos++];
                memcpy(salt, body + s2k_pos, 8);
                s2k_pos += 8;
                count = s2k_decode_count(body[s2k_pos++]);
            } else {
                fprintf(stderr, "GPG: Unsupported S2K type %d\n", s2k_type);
                return NULL;
            }
            
            /* Determine key length from cipher */
            switch (cipher_algo) {
                case CIPHER_AES128: session_key_len = 16; break;
                case CIPHER_AES192: session_key_len = 24; break;
                case CIPHER_AES256: session_key_len = 32; break;
                default:
                    fprintf(stderr, "GPG: Unsupported cipher algorithm %d\n", cipher_algo);
                    return NULL;
            }
            
            /* Derive session key from password */
            s2k_derive_key(password, strlen(password), salt, s2k_type, 
                          hash_algo, count, session_key, session_key_len);
            
            if (debug_finder) {  // DEBUG
                fprintf(stderr, "GPG: S2K type=%d hash=%d count=%u cipher=%d keylen=%d\n",  // DEBUG
                        s2k_type, hash_algo, count, cipher_algo, session_key_len);  // DEBUG
            }  // DEBUG
            
        } else if (packet_tag == 9) {
            /* Symmetrically Encrypted Data packet */
            encrypted_data = body;
            encrypted_len = body_len;
            has_mdc = 0;
            
        } else if (packet_tag == 18) {
            /* Symmetrically Encrypted Integrity Protected Data packet */
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
        fprintf(stderr, "GPG: Missing encrypted data or session key\n");
        return NULL;
    }
    
    /* Decrypt the data using AES-CFB */
    uint8_t *decrypted = malloc(encrypted_len);
    if (!decrypted) return NULL;
    
    aes_cfb_decrypt(encrypted_data, encrypted_len, session_key, session_key_len, decrypted);
    
    /* OpenPGP CFB resync: first block_size+2 bytes are prefix
     * prefix[block_size-2:block_size] should equal prefix[block_size:block_size+2] */
    int block_size = 16;  /* AES block size */
    if (encrypted_len < block_size + 2) {
        free(decrypted);
        return NULL;
    }
    
    /* Verify prefix (quick check) */
    if (decrypted[block_size - 2] != decrypted[block_size] ||
        decrypted[block_size - 1] != decrypted[block_size + 1]) {
        fprintf(stderr, "GPG: Decryption prefix check failed (wrong password?)\n");
        free(decrypted);
        return NULL;
    }
    
    /* Skip prefix */
    int payload_start = block_size + 2;
    int payload_len = encrypted_len - payload_start;
    
    /* Handle MDC if present */
    if (has_mdc) {
        /* Last 22 bytes should be MDC packet: 0xD3 0x14 + 20-byte SHA1 */
        if (payload_len < 22) {
            free(decrypted);
            return NULL;
        }
        
        int mdc_pos = payload_start + payload_len - 22;
        if (decrypted[mdc_pos] != 0xD3 || decrypted[mdc_pos + 1] != 0x14) {
            fprintf(stderr, "GPG: MDC packet not found\n");
            free(decrypted);
            return NULL;
        }
        
        /* Verify MDC: SHA1 of prefix + plaintext + 0xD3 0x14 */
        uint8_t *mdc_input = malloc(payload_start + payload_len - 20);
        memcpy(mdc_input, decrypted, payload_start + payload_len - 20);
        
        uint8_t computed_hash[20];
        sha1(mdc_input, payload_start + payload_len - 20, computed_hash);
        free(mdc_input);
        
        if (memcmp(computed_hash, decrypted + mdc_pos + 2, 20) != 0) {
            fprintf(stderr, "GPG: MDC verification failed\n");
            free(decrypted);
            return NULL;
        }
        
        payload_len -= 22;  /* Remove MDC from output */
    }
    
    /* The payload should now be a Literal Data or Compressed Data packet */
    int literal_body_len, literal_header_len;
    int literal_tag = pgp_parse_header(decrypted + payload_start, payload_len, 
                                       &literal_body_len, &literal_header_len);
    
    uint8_t *result = NULL;
    
    if (literal_tag == 11) {
        /* Literal Data packet */
        const uint8_t *lit_body = decrypted + payload_start + literal_header_len;
        
        /* Format: mode(1) + filename_len(1) + filename + date(4) + data */
        if (literal_body_len < 6) {
            free(decrypted);
            return NULL;
        }
        
        int filename_len = lit_body[1];
        int data_offset = 2 + filename_len + 4;
        int data_len_final = literal_body_len - data_offset;
        
        result = malloc(data_len_final);
        if (result) {
            memcpy(result, lit_body + data_offset, data_len_final);
            *out_len = data_len_final;
        }
        
    } else if (literal_tag == 8) {
        /* Compressed Data packet - need to decompress */
        const uint8_t *comp_body = decrypted + payload_start + literal_header_len;
        int comp_algo = comp_body[0];
        
        if (comp_algo == 0) {
            /* Uncompressed */
            result = malloc(literal_body_len - 1);
            if (result) {
                memcpy(result, comp_body + 1, literal_body_len - 1);
                *out_len = literal_body_len - 1;
            }
        } else if (comp_algo == 1 || comp_algo == 2) {
            /* ZIP or ZLIB - use gzip_decompress (handles zlib format too) */
            /* For raw deflate (ZIP), we need to wrap it */
            if (comp_algo == 1) {
                /* Raw deflate - construct minimal gzip wrapper */
                int comp_len = literal_body_len - 1;
                uint8_t *wrapped = malloc(comp_len + 18);  /* gzip header + trailer */
                /* Minimal gzip header */
                wrapped[0] = 0x1f; wrapped[1] = 0x8b; wrapped[2] = 8; wrapped[3] = 0;
                wrapped[4] = wrapped[5] = wrapped[6] = wrapped[7] = 0;  /* mtime */
                wrapped[8] = 0; wrapped[9] = 0xff;  /* xfl, os */
                memcpy(wrapped + 10, comp_body + 1, comp_len);
                /* Add dummy trailer (CRC + size will be wrong but ignored) */
                memset(wrapped + 10 + comp_len, 0, 8);
                
                result = gzip_decompress(wrapped, comp_len + 18, out_len);
                free(wrapped);
            } else {
                /* ZLIB format - skip 2-byte header and 4-byte trailer */
                int comp_len = literal_body_len - 1;
                if (comp_len > 6) {
                    uint8_t *wrapped = malloc(comp_len + 12);
                    wrapped[0] = 0x1f; wrapped[1] = 0x8b; wrapped[2] = 8; wrapped[3] = 0;
                    wrapped[4] = wrapped[5] = wrapped[6] = wrapped[7] = 0;
                    wrapped[8] = 0; wrapped[9] = 0xff;
                    memcpy(wrapped + 10, comp_body + 3, comp_len - 6);  /* Skip zlib header/trailer */
                    memset(wrapped + 10 + comp_len - 6, 0, 8);
                    
                    result = gzip_decompress(wrapped, comp_len + 6, out_len);
                    free(wrapped);
                }
            }
        } else {
            fprintf(stderr, "GPG: Unsupported compression algorithm %d\n", comp_algo);
        }
    } else {
        fprintf(stderr, "GPG: Unexpected packet type %d after decryption\n", literal_tag);
    }
    
    free(decrypted);
    return result;
}

/* Legacy functions for API compatibility */
static uint8_t *aes_cbc_decrypt(const uint8_t *data, int data_len, 
                                 const uint8_t *key, int key_len,
                                 const uint8_t *iv, int *out_len) {
    if (data_len % 16 != 0 || data_len == 0) {
        *out_len = 0;
        return NULL;
    }
    
    uint8_t *output = malloc(data_len);
    if (!output) {
        *out_len = 0;
        return NULL;
    }
    
    /* Expand key */
    int nr = (key_len == 16) ? 10 : (key_len == 24) ? 12 : 14;
    uint8_t round_keys[240];
    aes_key_expand(key, round_keys, key_len);
    
    /* Decrypt blocks */
    uint8_t prev_block[16];
    memcpy(prev_block, iv, 16);
    
    for (int i = 0; i < data_len; i += 16) {
        uint8_t decrypted[16];
        aes_decrypt_block(data + i, decrypted, round_keys, nr);
        
        for (int j = 0; j < 16; j++) {
            output[i + j] = decrypted[j] ^ prev_block[j];
        }
        memcpy(prev_block, data + i, 16);
    }
    
    /* Remove PKCS7 padding */
    int pad_len = output[data_len - 1];
    if (pad_len > 0 && pad_len <= 16) {
        *out_len = data_len - pad_len;
    } else {
        *out_len = data_len;
    }
    
    return output;
}

/* ============================================================================
 * PHASE 8: GZIP/ZLIB DECOMPRESSION
 * ============================================================================ */

/* Huffman code decoding table */
typedef struct {
    uint16_t sym;   /* Symbol or subtable offset */
    uint8_t bits;   /* Number of bits */
} HuffEntry;

/* Inflate state */
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

/* Read bits from input stream */
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

/* Output a byte */
static int inf_output(InflateState *s, uint8_t b) {
    if (s->out_pos >= s->out_cap) {
        int new_cap = s->out_cap ? s->out_cap * 2 : 4096;
        uint8_t *new_out = realloc(s->out, new_cap);
        if (!new_out) return 0;
        s->out = new_out;
        s->out_cap = new_cap;
    }
    s->out[s->out_pos++] = b;
    return 1;
}

/* Fixed Huffman code lengths */
static const uint8_t fixed_lit_lengths[288] = {
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8, 9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
    9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, 9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
    9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, 9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
    9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, 9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7, 7,7,7,7,7,7,7,7,8,8,8,8,8,8,8,8
};

/* Build Huffman decode table */
static int huff_build(const uint8_t *lengths, int count, HuffEntry *table, int *table_bits) {
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
            /* Reverse bits */
            int rev = 0;
            for (int j = 0; j < len; j++) {
                rev = (rev << 1) | (c & 1);
                c >>= 1;
            }
            
            if (len <= *table_bits) {
                /* Fill all entries for this code */
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

/* Decode a Huffman symbol */
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

/* Length and distance extra bits tables */
static const int len_base[] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
static const int len_extra[] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
static const int dist_base[] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
static const int dist_extra[] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};

/* Code length code order for dynamic Huffman */
static const int codelen_order[19] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

/* Inflate a deflate stream with given Huffman tables */
static int inflate_with_tables(InflateState *s, HuffEntry *lit_table, int lit_bits,
                                HuffEntry *dist_table, int dist_bits) {
    while (1) {
        int sym = huff_decode(s, lit_table, lit_bits);
        if (sym < 0) return 0;
        
        if (sym < 256) {
            /* Literal */
            if (!inf_output(s, sym)) return 0;
        } else if (sym == 256) {
            /* End of block */
            return 1;
        } else {
            /* Length/distance pair */
            sym -= 257;
            if (sym >= 29) return 0;
            int len = len_base[sym] + inf_read_bits(s, len_extra[sym]);
            
            int dist_sym = huff_decode(s, dist_table, dist_bits);
            if (dist_sym < 0 || dist_sym >= 30) return 0;
            int dist = dist_base[dist_sym] + inf_read_bits(s, dist_extra[dist_sym]);
            
            /* Copy from history */
            for (int i = 0; i < len; i++) {
                int src = s->out_pos - dist;
                if (src < 0) return 0;
                if (!inf_output(s, s->out[src])) return 0;
            }
        }
    }
}

/* Inflate a block with fixed Huffman codes */
static int inflate_block_fixed(InflateState *s) {
    HuffEntry lit_table[512];
    HuffEntry dist_table[32];
    int lit_bits, dist_bits;
    
    /* Build fixed tables */
    huff_build(fixed_lit_lengths, 288, lit_table, &lit_bits);
    
    uint8_t fixed_dist[32];
    for (int i = 0; i < 32; i++) fixed_dist[i] = 5;
    huff_build(fixed_dist, 32, dist_table, &dist_bits);
    
    return inflate_with_tables(s, lit_table, lit_bits, dist_table, dist_bits);
}

/* Inflate a block with dynamic Huffman codes */
static int inflate_block_dynamic(InflateState *s) {
    /* Read header */
    int hlit = inf_read_bits(s, 5) + 257;   /* Literal/length codes */
    int hdist = inf_read_bits(s, 5) + 1;    /* Distance codes */
    int hclen = inf_read_bits(s, 4) + 4;    /* Code length codes */
    
    if (hlit > 286 || hdist > 30) return 0;
    
    /* Read code length code lengths */
    uint8_t codelen_lengths[19] = {0};
    for (int i = 0; i < hclen; i++) {
        codelen_lengths[codelen_order[i]] = inf_read_bits(s, 3);
    }
    
    /* Build code length Huffman table */
    HuffEntry codelen_table[128];
    int codelen_bits;
    if (!huff_build(codelen_lengths, 19, codelen_table, &codelen_bits)) return 0;
    
    /* Read literal/length and distance code lengths */
    uint8_t lengths[286 + 30];
    int total_codes = hlit + hdist;
    int i = 0;
    
    while (i < total_codes) {
        int sym = huff_decode(s, codelen_table, codelen_bits);
        if (sym < 0) return 0;
        
        if (sym < 16) {
            /* Literal code length */
            lengths[i++] = sym;
        } else if (sym == 16) {
            /* Repeat previous length 3-6 times */
            if (i == 0) return 0;
            int repeat = 3 + inf_read_bits(s, 2);
            uint8_t prev = lengths[i - 1];
            while (repeat-- && i < total_codes) lengths[i++] = prev;
        } else if (sym == 17) {
            /* Repeat zero 3-10 times */
            int repeat = 3 + inf_read_bits(s, 3);
            while (repeat-- && i < total_codes) lengths[i++] = 0;
        } else if (sym == 18) {
            /* Repeat zero 11-138 times */
            int repeat = 11 + inf_read_bits(s, 7);
            while (repeat-- && i < total_codes) lengths[i++] = 0;
        } else {
            return 0;
        }
    }
    
    /* Build literal/length and distance tables */
    HuffEntry lit_table[32768];  /* Need larger table for dynamic codes */
    HuffEntry dist_table[32768];
    int lit_bits, dist_bits;
    
    if (!huff_build(lengths, hlit, lit_table, &lit_bits)) return 0;
    if (!huff_build(lengths + hlit, hdist, dist_table, &dist_bits)) return 0;
    
    return inflate_with_tables(s, lit_table, lit_bits, dist_table, dist_bits);
}

/* Decompress gzip data */
STATIC uint8_t *gzip_decompress(const uint8_t *data, int data_len, int *out_len) {
    if (data_len < 10) {
        *out_len = 0;
        return NULL;
    }
    
    /* Check gzip magic */
    if (data[0] != 0x1f || data[1] != 0x8b) {
        *out_len = 0;
        return NULL;
    }
    
    /* Check compression method (8 = deflate) */
    if (data[2] != 8) {
        *out_len = 0;
        return NULL;
    }
    
    int flags = data[3];
    int pos = 10;
    
    /* Skip extra field */
    if (flags & 0x04) {
        if (pos + 2 > data_len) { *out_len = 0; return NULL; }
        int xlen = data[pos] | (data[pos+1] << 8);
        pos += 2 + xlen;
    }
    
    /* Skip filename */
    if (flags & 0x08) {
        while (pos < data_len && data[pos]) pos++;
        pos++;
    }
    
    /* Skip comment */
    if (flags & 0x10) {
        while (pos < data_len && data[pos]) pos++;
        pos++;
    }
    
    /* Skip header CRC */
    if (flags & 0x02) pos += 2;
    
    if (pos >= data_len) {
        *out_len = 0;
        return NULL;
    }
    
    /* Initialize inflate state */
    InflateState s = {0};
    s.in = data + pos;
    s.in_len = data_len - pos - 8;  /* Exclude trailer */
    s.in_pos = 0;
    s.out = NULL;
    s.out_pos = 0;
    s.out_cap = 0;
    
    /* Process blocks */
    int bfinal;
    do {
        bfinal = inf_read_bits(&s, 1);
        int btype = inf_read_bits(&s, 2);
        
        if (btype == 0) {
            /* Stored block */
            s.bit_buf = 0;
            s.bit_cnt = 0;
            
            if (s.in_pos + 4 > s.in_len) break;
            int len = s.in[s.in_pos] | (s.in[s.in_pos+1] << 8);
            s.in_pos += 4;
            
            for (int i = 0; i < len && s.in_pos < s.in_len; i++) {
                if (!inf_output(&s, s.in[s.in_pos++])) break;
            }
        } else if (btype == 1) {
            /* Fixed Huffman */
            if (!inflate_block_fixed(&s)) break;
        } else if (btype == 2) {
            /* Dynamic Huffman */
            if (!inflate_block_dynamic(&s)) break;
        } else {
            break;  /* Invalid block type */
        }
    } while (!bfinal);
    
    *out_len = s.out_pos;
    return s.out;
}

/* ============================================================================
 * PHASE 9: SHA256 CHECKSUM
 * ============================================================================ */

/* SHA256 constants */
static const uint32_t sha256_k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

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
    
    /* Process full blocks */
    size_t i;
    for (i = 0; i + 64 <= len; i += 64) {
        sha256_transform(state, data + i);
    }
    
    /* Pad and process final block(s) */
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
    
    /* Append length in bits */
    uint64_t bit_len = len * 8;
    for (int j = 0; j < 8; j++) {
        block[63 - j] = bit_len & 0xff;
        bit_len >>= 8;
    }
    sha256_transform(state, block);
    
    /* Output hash */
    for (int j = 0; j < 8; j++) {
        hash[j*4] = (state[j] >> 24) & 0xff;
        hash[j*4+1] = (state[j] >> 16) & 0xff;
        hash[j*4+2] = (state[j] >> 8) & 0xff;
        hash[j*4+3] = state[j] & 0xff;
    }
}

static void print_sha256(const uint8_t *hash) {
    for (int i = 0; i < 32; i++) {
        printf("%02x", hash[i]);
    }
    printf("\n");
}

/* ============================================================================
 * SCAN IMAGE FOR QR CODES (Implementation)
 * ============================================================================ */

/* Check if three finder patterns could form a valid QR code */
static int valid_qr_triple(FinderPattern *p0, FinderPattern *p1, FinderPattern *p2) {
    /* Module sizes should be similar, but can vary significantly with distortion */
    float ms0 = p0->module_size;
    float ms1 = p1->module_size;
    float ms2 = p2->module_size;
    float avg_ms = (ms0 + ms1 + ms2) / 3.0f;
    
    /* Allow 25% variance on module sizes (tightened from 40%) */
    if (ms0 < avg_ms * 0.75f || ms0 > avg_ms * 1.25f) return 0;
    if (ms1 < avg_ms * 0.75f || ms1 > avg_ms * 1.25f) return 0;
    if (ms2 < avg_ms * 0.75f || ms2 > avg_ms * 1.25f) return 0;
    
    /* Check that distances are consistent with a QR code */
    int d01 = distance_sq(p0->x, p0->y, p1->x, p1->y);
    int d02 = distance_sq(p0->x, p0->y, p2->x, p2->y);
    int d12 = distance_sq(p1->x, p1->y, p2->x, p2->y);
    
    /* Sort distances to identify min, mid, max */
    int min_d, mid_d, max_d;
    if (d01 <= d02 && d01 <= d12) {
        min_d = d01;
        mid_d = (d02 < d12) ? d02 : d12;
        max_d = (d02 < d12) ? d12 : d02;
    } else if (d02 <= d01 && d02 <= d12) {
        min_d = d02;
        mid_d = (d01 < d12) ? d01 : d12;
        max_d = (d01 < d12) ? d12 : d01;
    } else {
        min_d = d12;
        mid_d = (d01 < d02) ? d01 : d02;
        max_d = (d01 < d02) ? d02 : d01;
    }
    
    /* Allow up to 2x difference between the two shorter sides (tightened from 5x)
     * This handles moderate aspect ratio distortion */
    if (mid_d > min_d * 2.0f) return 0;
    
    /* Hypotenuse check: for a right angle, max_d = min_d + mid_d (Pythagorean theorem) 
     * For distorted codes, this may not be exactly true. Allow 30% tolerance (up from 20%) */
    int expected_hyp = min_d + mid_d;
    float hyp_ratio = (float)max_d / expected_hyp;
    
    /* Scale tolerance with the size of the code - allow more distortion for larger codes
     * This is because even a small rotation can cause significant distortion at the edges
     * of large codes */
    float hyp_tolerance = 0.20f;  /* Base tolerance of 20% (tightened from 30%) */
    if (avg_ms > 3.0f) {
        /* Increase tolerance for larger modules, capped at 25% */
        hyp_tolerance += (avg_ms - 3.0f) * 0.01f;
        if (hyp_tolerance > 0.25f) hyp_tolerance = 0.25f;
    }
    
    if (hyp_ratio < 1.0f - hyp_tolerance || hyp_ratio > 1.0f + hyp_tolerance) return 0;
    
    /* Version consistency check with expanded tolerance */
    float side1 = fsqrt((float)min_d);
    float side2 = fsqrt((float)mid_d);
    float version1 = (side1 / avg_ms - 10.0f) / 4.0f;
    float version2 = (side2 / avg_ms - 10.0f) / 4.0f;
    
    /* Allow versions to differ by at most 3 (tightened from 10) - QR codes should have
     * consistent dimensions even with some distortion */
    float version_diff = (version1 > version2) ? version1 - version2 : version2 - version1;
    if (version_diff > 3.0f) return 0;
    
    if (debug_finder) {  // DEBUG
        fprintf(stderr, "  valid_triple: (%d,%d), (%d,%d), (%d,%d) ms=[%.1f,%.1f,%.1f] d=[%d,%d,%d] v=[%.1f,%.1f] hyp_ratio=%.2f -> PASS\n",  // DEBUG
                p0->x, p0->y, p1->x, p1->y, p2->x, p2->y,  // DEBUG
                ms0, ms1, ms2, min_d, mid_d, max_d, version1, version2, hyp_ratio);  // DEBUG
    }  // DEBUG
    
    /* For rotated codes, the module size from line scanning can be inflated by up to sqrt(2),
     * which throws off the version calculation. Rather than rejecting based on computed version,
     * rely on the geometric checks (right triangle, consistent distances) and let the actual
     * decode validate if it's a real QR code.
     * 
     * Only reject if the version estimates are wildly inconsistent (differ by more than 5)
     * or absurdly high (over 50), which indicates mismatched finder patterns. */
    if (version1 > 50 || version2 > 50) return 0;
    
    return 1;
}

static int scan_image_for_qr(Image *img, ChunkList *chunks) {
    FinderPattern patterns[500];
    int npatterns = find_finder_patterns(img, patterns, 500);
    
    /* Dump all finder patterns found in this image */
    if (debug_mode && npatterns > 0) {  // DEBUG
        debug_dump_finders(patterns, npatterns);  // DEBUG
    }
    
    if (npatterns < 3) {
        return 0;  /* Need at least 3 finder patterns for a QR code */
    }
    
    /* Track which patterns have been used */
    int *used = calloc(npatterns, sizeof(int));
    if (!used) return 0;
    
    int decoded_count = 0;
    
    /* Try all combinations of 3 patterns to find valid QR codes */
    for (int i = 0; i < npatterns - 2 && decoded_count < MAX_CHUNKS; i++) {
        if (used[i]) continue;
        
        for (int j = i + 1; j < npatterns - 1; j++) {
            if (used[j]) continue;
            
            for (int k = j + 1; k < npatterns; k++) {
                if (used[k]) continue;
                
                /* Check if these three patterns could form a valid QR code */
                if (!valid_qr_triple(&patterns[i], &patterns[j], &patterns[k])) {
                    continue;
                }
                
                if (debug_finder) {  // DEBUG
                    fprintf(stderr, "  Attempting decode with patterns %d,%d,%d\n", i, j, k);  // DEBUG
                }  // DEBUG
                
                /* Try to decode this QR code */
                FinderPattern triple[3] = {patterns[i], patterns[j], patterns[k]};
                char qr_content[4096];
                int len = decode_qr_from_finders(img, triple, 3, qr_content, sizeof(qr_content));
                
                if (len > 0) {
                    if (debug_finder) {  // DEBUG
                        fprintf(stderr, "  QR decoded (%d chars): '%s'\n", len, qr_content);  // DEBUG
                    }  // DEBUG
                    
                    /* Parse the chunk label */
                    char type;
                    int index, total;
                    const char *data_start;
                    
                    if (parse_chunk_label(qr_content, &type, &index, &total, &data_start)) {
                        /* Debug dump: decoded content */
                        debug_dump_decoded(qr_content, len, type, index, total);  // DEBUG
                        
                        /* Base64 decode the data portion */
                        uint8_t decoded[MAX_CHUNK_SIZE];
                        int decoded_len = base64_decode(data_start, strlen(data_start), 
                                                        decoded, sizeof(decoded));
                        
                        if (decoded_len > 0) {
                            if (debug_finder) {  // DEBUG
                                fprintf(stderr, "  -> chunk %c%02d/%02d, %d bytes, base64: '%.10s...%s'\n",  // DEBUG
                                        type, index, total, decoded_len, data_start, data_start + strlen(data_start) - 10);  // DEBUG
                            }  // DEBUG
                            chunk_list_add(chunks, type, index, total, decoded, decoded_len);
                            decoded_count++;
                            
                            /* Mark these patterns as used */
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

/* ============================================================================
 * MAIN PROGRAM
 * ============================================================================ */

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s [options] <image.pbm> [image2.pbm ...]\n", prog);
    fprintf(stderr, "\nRestores qr-backup paper backups from PBM images.\n");
    fprintf(stderr, "Supports GPG symmetric encryption (AES-128/192/256) and gzip compression.\n");
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "  -p <password>  Decrypt GPG-encrypted backup\n");
    fprintf(stderr, "  -o <file>      Output to file (default: stdout)\n");
    fprintf(stderr, "  -v             Verbose output (-vv for more detail)\n");
    fprintf(stderr, "  --debug        Write intermediate files (debug_*.txt/bin)\n");
    fprintf(stderr, "  -h             Show this help\n");
    fprintf(stderr, "\nExamples:\n");
    fprintf(stderr, "  %s backup.pbm                    # Basic restore\n", prog);
    fprintf(stderr, "  %s -p secret backup.pbm          # Decrypt and restore\n", prog);
    fprintf(stderr, "  %s -v page1.pbm page2.pbm        # Multi-page restore\n", prog);
    fprintf(stderr, "  %s --debug backup.pbm            # Debug output\n", prog);
}

#ifndef UNIT_TEST_BUILD
int main(int argc, char **argv) {
    const char *password = NULL;
    const char *output_file = NULL;
    int verbose = 0;
    int first_file = 1;
    
    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
                password = argv[++i];
            } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
                output_file = argv[++i];
            } else if (strcmp(argv[i], "-v") == 0) {
                verbose++;
            } else if (strcmp(argv[i], "-vv") == 0) {
                verbose = 2;
            } else if (strcmp(argv[i], "--debug") == 0) {
                debug_mode = 1;  // DEBUG
                verbose = 2;  /* Debug mode implies verbose */
            } else if (strcmp(argv[i], "-h") == 0) {
                print_usage(argv[0]);
                return 0;
            } else {
                fprintf(stderr, "Unknown option: %s\n", argv[i]);
                return 1;
            }
        } else {
            if (first_file) first_file = i;
        }
    }
    
    if (first_file == 0 || first_file >= argc) {
        print_usage(argv[0]);
        return 1;
    }
    
    /* Enable debug output for high verbosity */
    if (verbose >= 2) { // VERBOSE
        debug_finder = 1;  // DEBUG
    }  // VERBOSE
    
    /* Initialize chunk list */
    ChunkList chunks;
    chunk_list_init(&chunks);
    
    /* Process each input image */
    for (int i = first_file; i < argc; i++) {
        if (argv[i][0] == '-') continue;
        
        if (verbose) { // VERBOSE
            fprintf(stderr, "Processing: %s\n", argv[i]);  // VERBOSE
        }  // VERBOSE
        
        Image *img = image_read_pbm(argv[i]);
        if (!img) {
            fprintf(stderr, "Failed to read image: %s\n", argv[i]);
            continue;
        }
        
        int found = scan_image_for_qr(img, &chunks);
        if (verbose) { // VERBOSE
            fprintf(stderr, "  Found %d QR codes\n", found);  // VERBOSE
        }  // VERBOSE
        
        image_free(img);
    }
    
    if (chunks.count == 0) {
        fprintf(stderr, "No QR codes found in input images\n");
        chunk_list_free(&chunks);
        return 1;
    }
    
    if (verbose) { // VERBOSE
        fprintf(stderr, "Total chunks collected: %d\n", chunks.count);  // VERBOSE
    }  // VERBOSE
    
    /* Sort and deduplicate chunks */
    chunk_list_sort(&chunks);
    debug_dump_chunks(&chunks, "collected");  // DEBUG
    chunk_list_dedupe(&chunks);
    debug_dump_chunks(&chunks, "deduped");  // DEBUG
    
    if (verbose) { // VERBOSE
        fprintf(stderr, "After deduplication: %d chunks\n", chunks.count);  // VERBOSE
    }  // VERBOSE
    
    /* Count normal and parity chunks */
    int total_normal = 0, total_parity = 0;
    for (int i = 0; i < chunks.count; i++) {
        if (chunks.chunks[i].type == 'N') {
            if (chunks.chunks[i].total > total_normal) {
                total_normal = chunks.chunks[i].total;
            }
        } else if (chunks.chunks[i].type == 'P') {
            if (chunks.chunks[i].total > total_parity) {
                total_parity = chunks.chunks[i].total;
            }
        }
    }
    
    /* Attempt erasure recovery if needed */
    if (!erasure_recover(&chunks, total_normal, total_parity)) {
        fprintf(stderr, "Warning: Could not recover all missing chunks\n");
    }
    
    /* Assemble data */
    int data_len;
    uint8_t *data = assemble_data(&chunks, &data_len);
    chunk_list_free(&chunks);
    
    if (!data || data_len == 0) {
        fprintf(stderr, "Failed to assemble data\n");
        return 1;
    }
    
    if (verbose) { // VERBOSE
        fprintf(stderr, "Assembled data: %d bytes\n", data_len);  // VERBOSE
        fprintf(stderr, "First 16 bytes: ");  // VERBOSE
        for (int i = 0; i < 16 && i < data_len; i++) {  // VERBOSE
            fprintf(stderr, "%02x ", data[i]);  // VERBOSE
        }  // VERBOSE
        fprintf(stderr, "\n");  // VERBOSE
    }  // VERBOSE
    
    debug_dump_assembled(data, data_len);  // DEBUG
    
    /* Decrypt if GPG encrypted and password provided */
    if (is_gpg_encrypted(data, data_len)) {
        if (!password) {
            fprintf(stderr, "Error: Data is GPG encrypted but no password provided (-p)\n");
            free(data);
            return 1;
        }
        
        if (verbose) { // VERBOSE
            fprintf(stderr, "Decrypting GPG data...\n");  // VERBOSE
        }  // VERBOSE
        
        int decrypted_len;
        uint8_t *decrypted = gpg_decrypt(data, data_len, password, &decrypted_len);
        
        if (decrypted) {
            free(data);
            data = decrypted;
            data_len = decrypted_len;
            
            if (verbose) { // VERBOSE
                fprintf(stderr, "Decrypted %d bytes\n", data_len);  // VERBOSE
            }  // VERBOSE
        } else {
            fprintf(stderr, "GPG decryption failed\n");
            free(data);
            return 1;
        }
    }
    
    /* Mark legacy functions as used for API compatibility */
    (void)aes_cbc_decrypt;
    (void)xtime;
    (void)rs_check_syndromes;
    
    /* Decompress if gzip */
    uint8_t *final_data = data;
    int final_len = data_len;
    
    if (data_len >= 2 && data[0] == 0x1f && data[1] == 0x8b) {
        if (verbose) { // VERBOSE
            fprintf(stderr, "Decompressing gzip data...\n");  // VERBOSE
        }  // VERBOSE
        int decompressed_len;
        uint8_t *decompressed = gzip_decompress(data, data_len, &decompressed_len);
        if (decompressed) {
            free(data);
            final_data = decompressed;
            final_len = decompressed_len;
            debug_dump_decompressed(final_data, final_len);  // DEBUG
        } else {
            fprintf(stderr, "Warning: Decompression failed, outputting raw data\n");
        }
    } else {
        /* Not gzip compressed, still dump as "decompressed" */
        debug_dump_decompressed(final_data, final_len);  // DEBUG
    }
    
    /* Compute and display SHA256 */
    uint8_t hash[32];
    sha256(final_data, final_len, hash);
    fprintf(stderr, "SHA256: ");
    print_sha256(hash);
    
    /* Output data */
    FILE *out = stdout;
    if (output_file) {
        out = fopen(output_file, "wb");
        if (!out) {
            fprintf(stderr, "Failed to open output file: %s\n", output_file);
            free(final_data);
            return 1;
        }
    }
    
    fwrite(final_data, 1, final_len, out);
    
    if (output_file) {
        fclose(out);
    }
    
    free(final_data);
    
    if (verbose) { // VERBOSE
        fprintf(stderr, "Restored %d bytes\n", final_len);  // VERBOSE
    }  // VERBOSE
    
    return 0;
}
#endif /* UNIT_TEST_BUILD */
