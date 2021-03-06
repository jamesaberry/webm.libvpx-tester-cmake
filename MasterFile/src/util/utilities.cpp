#define _CRT_SECURE_NO_WARNINGS
#include "vpxt_test_definitions.h"
#include "vpxt_test_declarations.h"
#include "vpxt_utilities.h"
#include "vpxt_driver.h"
#include "yv12config.h"
#include "header.h"
#include "onyx.h"
#include "ivf.h"
#include "vp8.h"
#include "args.h"
#include "vp8cx.h"
#include "vp8dx.h"
#include "vpx_timer.h"
#include "vpx_encoder.h"
#include "vpx_decoder.h"
#include "vpx_version.h"
#include "vpx_config.h"
#include "tools_common.h"
#include "y4minput.h"
#include "EbmlWriter.h"
#include "EbmlIDs.h"
#include "nestegg.h"
#include "mem_ops.h"
extern "C" {
#include "vpx_rtcd.h"
}

#include <cmath>
#include <cassert>
#include <iostream>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <cstdlib>
#include <cctype>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

////////////////////////////////// MULIT ENC //////////////////////////////////
#include "basic_types.h"
#include "scale.h"
#include "cpu_id.h"
/////////////////////////////////////////////////////////////////////////////

typedef unsigned char BYTE;
typedef off_t EbmlLoc;

#define IVF_FRAME_HDR_SZ (sizeof(uint32_t) + sizeof(uint64_t))
#define RAW_FRAME_HDR_SZ (sizeof(uint32_t))
#define IVF_FILE_HDR_SZ (32)
#define VP8_FOURCC (0x00385056)
#define CONFIG_VP8_ENCODER 1

#if !defined(_MSC_VER)
#define LITERALU64(n) n##LLU
#endif

#if defined(_MSC_VER)
#define fseeko _fseeki64
#define ftello _ftelli64
#define LITERALU64(n) n
#elif defined(_WIN32)
#define fseeko fseeko64
#define ftello ftello64
#endif

/* These pointers are to the start of an element */
#if !CONFIG_OS_SUPPORT
typedef long off_t;
#define fseeko fseek
#define ftello ftell
#endif

#if CONFIG_OS_SUPPORT
#if defined(_WIN32)
#include <io.h>
#define snprintf _snprintf
#define isatty   _isatty
#define fileno   _fileno
#else
#include <unistd.h>
#endif
#endif
#ifndef PATH_MAX
#define PATH_MAX 256
#endif

#if CONFIG_VP8_DECODER
#include "vp8dx.h"
#endif
#if CONFIG_MD5
#include "md5_utils.h"
#endif

#if defined(_WIN32)
#define USE_POSIX_MMAP 0
#include "on2vpplugin.h"
#define snprintf _snprintf
#else
#define USE_POSIX_MMAP 1
#include "dirent.h"
typedef unsigned int  DWORD;
#endif

#if USE_POSIX_MMAP
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#ifdef _MSC_VER
#define USE_POSIX_MMAP 0
#else
#define USE_POSIX_MMAP 1
#endif

/////////////////////////////// Endian Conversions /////////////////////////////
#ifdef _PPC
# define make_endian_16(a)          \
    (((unsigned int)(a & 0xff)) << 8) | (((unsigned int)(a & 0xff00)) >> 8)
# define make_endian_32(a)          \
    (((unsigned int)(a & 0xff)) << 24)   | (((unsigned int)(a & 0xff00))<< 8) |\
    (((unsigned int)(a & 0xff0000)) >> 8)|(((unsigned int)(a & 0xff000000))>>24)
# define make_endian_64(a)          \
    ((a & 0xff) << 56           |   \
     ((a >>  8) & 0xff) << 48   |   \
     ((a >> 16) & 0xff) << 40   |   \
     ((a >> 24) & 0xff) << 32   |   \
     ((a >> 32) & 0xff) << 24   |   \
     ((a >> 40) & 0xff) << 16   |   \
     ((a >> 48) & 0xff) <<  8   |   \
     ((a >> 56) & 0xff))
# define MAKEFOURCC(ch0, ch1, ch2, ch3)                      \
    ((DWORD)(BYTE)(ch0) << 24 | ((DWORD)(BYTE)(ch1) << 16) | \
     ((DWORD)(BYTE)(ch2) << 8) | ((DWORD)(BYTE)(ch3)))
# define swap4(d)           \
    ((d&0x000000ff)<<24) |  \
    ((d&0x0000ff00)<<8)  |  \
    ((d&0x00ff0000)>>8)  |  \
    ((d&0xff000000)>>24)
#else
# define make_endian_16(a)  a
# define make_endian_32(a)  a
# define make_endian_64(a)  a
# define MAKEFOURCC(ch0, ch1, ch2, ch3)                         \
    ((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) |           \
     ((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24 ))
# define swap4(d) d
#endif

const int PSNR_MAX = 999.;
const int sizBuff = 512;

extern double VP8_CalcSSIM_Tester(YV12_BUFFER_CONFIG *source,
                                  YV12_BUFFER_CONFIG *dest,
                                  int lumamask,
                                  double *weight);
extern double vp8_calcpsnr_tester(YV12_BUFFER_CONFIG *source,
                                  YV12_BUFFER_CONFIG *dest,
                                  double *YPsnr,
                                  double *upsnr,
                                  double *vpsnr,
                                  double *sq_error,
                                  int print_out,
                                  int& possible_artifact);
extern double vp8_mse_2_psnr_tester(double Samples, double Peak, double Mse);

extern "C"
{
    extern int vp8_yv12_alloc_frame_buffer(YV12_BUFFER_CONFIG *ybf,
        int width,
        int height,
        int border);
    extern int vp8_yv12_de_alloc_frame_buffer(YV12_BUFFER_CONFIG *ybf);
    extern vpx_codec_iface_t vpx_codec_vp8_cx_algo;
    extern void vp8_yv12_scale_or_center(YV12_BUFFER_CONFIG *src_yuv_config,
        YV12_BUFFER_CONFIG *dst_yuv_config, int expanded_frame_width,
        int expanded_frame_height, int scaling_mode, int HScale, int HRatio,
        int VScale, int VRatio);
#ifndef vp8_yv12_copy_frame
    extern void vp8_yv12_copy_frame(YV12_BUFFER_CONFIG *src_ybc,
        YV12_BUFFER_CONFIG *dst_ybc);
    extern void vp8_scale_machine_specific_config(void);
#endif
}


static void initialize_scaler()
{
#ifndef vp8_yv12_copy_frame
    // Relying on the fact that there's no asm implementation of
    // vp8_yv12_copy_frame on x86, so it will be #defined to
    // vp8_yv12_copy_frame_c in commits where the scaler has been moved
    // to the RTCD system. This workaround can be removed after we're
    // past this transition.
    vp8_scale_machine_specific_config();
#else
    vpx_rtcd();
#endif
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// MULIT ENC //////////////////////////////////
#define NUM_ENCODERS 3
//////////////////////////////////SCALE PART//////////////////////////////////
static int mode_to_num_layers[9] = {2, 2, 3, 3, 3, 3, 5, 2, 3};
#define TEMP_SCALE_ENCODERS 6
///////////////////////// ENCODE-START /////////////////////////////////////////
enum video_file_type
{
    FILE_TYPE_RAW,
    FILE_TYPE_IVF,
    FILE_TYPE_Y4M
};

struct detect_buffer
{
    char buf[4];
    size_t buf_read;
    size_t position;
};

struct cue_entry
{
    unsigned int time;
    uint64_t     loc;
};
struct EbmlGlobal
{
    int debug;
    FILE    *stream;
    int64_t last_pts_ms;
    vpx_rational_t  framerate;
    off_t    position_reference;
    off_t    seek_info_pos;
    off_t    segment_info_pos;
    off_t    track_pos;
    off_t    cue_pos;
    off_t    cluster_pos;
    off_t    track_id_pos;
    EbmlLoc  startSegment;
    EbmlLoc  startCluster;
    uint32_t cluster_timecode;
    int      cluster_open;
    struct cue_entry *cue_list;
    unsigned int      cues;
};
static const char *exec_name;
int ctx_exit_on_error_tester(vpx_codec_ctx_t *ctx, const char *s)
{
    if (ctx->err)
    {
        const char *detail = vpx_codec_error_detail(ctx);

        tprintf(PRINT_STD, "%s: %s\n", s, vpx_codec_error(ctx));

        if (detail)
            tprintf(PRINT_STD, "    %s\n", detail);

        return -1;
    }

    return 0;
}
void *out_open(const char *out_fn, int do_md5)
{
    void *out = NULL;

    if (do_md5)
    {
#if CONFIG_MD5
        out = malloc(sizeof(MD5Context));
        MD5Context *md5_ctx = (MD5Context *) out;
        (void)out_fn;
        MD5Init(md5_ctx);
#endif
    }
    else
    {
        out = strcmp("-", out_fn) ? fopen(out_fn, "wb") :
            set_binary_mode(stdout);
        FILE *outfile = (FILE *)out;

        if (!outfile)
        {
            tprintf(PRINT_BTH, "Failed to open output file: %s", out_fn);
            exit(EXIT_FAILURE);
        }
    }

    return out;
}
void out_put(void *out, const uint8_t *buf, unsigned int len, int do_md5)
{
    if (do_md5)
    {
#if CONFIG_MD5
        MD5Update((MD5Context *) out, buf, len);
#endif
    }
    else
    {
        fwrite(buf, 1, len, (FILE *)out);
    }
}
void out_close(void *out, const char *out_fn, int do_md5)
{
    if (do_md5)
    {
#if CONFIG_MD5
        uint8_t md5[16];
        int i;

        MD5Final((unsigned char *) md5, (MD5Context *) out);
        free(out);

        tprintf(PRINT_STD, "\n");

        for (i = 0; i < 16; i++)
        {
            tprintf(PRINT_BTH, "%02x", md5[i]);
        }

        tprintf(PRINT_BTH, "\n");

        FILE *outfile;
        outfile = fopen(out_fn, "w");

        for (i = 0; i < 16; i++)
        {
            fprintf(outfile, "%02x", md5[i]);
        }

        fclose(outfile);
#endif
    }
    else
    {
        fclose((FILE *)out);
    }
}
void ivf_write_frame_header(FILE *outfile,
                            uint64_t timeStamp,
                            uint32_t frameSize)
{

    IVF_FRAME_HEADER ivf_fh;

    ivf_fh.timeStamp = make_endian_64(timeStamp);
    ivf_fh.frameSize = make_endian_32(frameSize);

    fwrite((char *)&ivf_fh, sizeof(ivf_fh), 1, outfile);

}
static const struct codec_item
{
    char const              *name;
    const vpx_codec_iface_t *iface;
    unsigned int             fourcc;
} codecs[] =
{
#if CONFIG_VP8_ENCODER
    {"vp8",  &vpx_codec_vp8_cx_algo, 0x30385056},
#endif
};

static void usage_exit_enc();

void die_enc(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    usage_exit_enc();
}
typedef struct
{
    vpx_fixed_buf_t buf;
    int             pass;
    FILE           *file;
    char           *buf_ptr;
    size_t          buf_alloc_sz;
} stats_io_t;
int stats_open_file(stats_io_t *stats, const char *fpf, int pass)
{
    int res;

    stats->pass = pass;

    if (pass == 0)
    {
        stats->file = fopen(fpf, "wb");
        stats->buf.sz = 0;
        stats->buf.buf = NULL,
                   res = (stats->file != NULL);
    }
    else
    {
#if 0
#elif USE_POSIX_MMAP
        struct stat stat_buf;
        int fd;

        fd = open(fpf, O_RDONLY);
        stats->file = fdopen(fd, "rb");
        fstat(fd, &stat_buf);
        stats->buf.sz = stat_buf.st_size;
        stats->buf.buf = mmap(NULL, stats->buf.sz, PROT_READ, MAP_PRIVATE,
                              fd, 0);
        res = (stats->buf.buf != NULL);
#else
        size_t nbytes;

        stats->file = fopen(fpf, "rb");

        if (fseek(stats->file, 0, SEEK_END))
        {
            fprintf(stderr, "First-pass stats file must be seekable!\n");
            exit(EXIT_FAILURE);
        }

        stats->buf.sz = stats->buf_alloc_sz = ftell(stats->file);
        rewind(stats->file);

        stats->buf.buf = malloc(stats->buf_alloc_sz);

        if (!stats->buf.buf)
        {
            fprintf(stderr, "Failed to allocate first-pass stats buffer"
                " (%d bytes)\n", stats->buf_alloc_sz);
            exit(EXIT_FAILURE);
        }

        nbytes = fread(stats->buf.buf, 1, stats->buf.sz, stats->file);
        res = (nbytes == stats->buf.sz);
#endif
    }

    return res;
}

int stats_open_mem(stats_io_t *stats, int pass)
{
    int res;
    stats->pass = pass;

    if (!pass)
    {
        stats->buf.sz = 0;
        stats->buf_alloc_sz = 64 * 1024;
        stats->buf.buf = malloc(stats->buf_alloc_sz);
    }

    stats->buf_ptr = (char *)stats->buf.buf;
    res = (stats->buf.buf != NULL);
    return res;
}


void stats_close(stats_io_t *stats, int last_pass)
{
    if (stats->file)
    {
        if (stats->pass == last_pass)
        {
#if 0
#elif USE_POSIX_MMAP
            munmap(stats->buf.buf, stats->buf.sz);
#else
            free(stats->buf.buf);
#endif
        }

        fclose(stats->file);
        stats->file = NULL;
    }
    else
    {
        if (stats->pass == last_pass)
            free(stats->buf.buf);
    }
}
void stats_write(stats_io_t *stats, const void *pkt, size_t len)
{
    if (stats->file)
    {
        if(fwrite(pkt, 1, len, stats->file));
    }
    else
    {
        if (stats->buf.sz + len > stats->buf_alloc_sz)
        {
            size_t  new_sz = stats->buf_alloc_sz + 64 * 1024;
            char   *new_ptr = (char *)realloc(stats->buf.buf, new_sz);

            if (new_ptr)
            {
                stats->buf_ptr = new_ptr + (stats->buf_ptr -
                    (char *)stats->buf.buf);
                stats->buf.buf = new_ptr;
                stats->buf_alloc_sz = new_sz;
            } /* else ... */
        }

        memcpy(stats->buf_ptr, pkt, len);
        stats->buf.sz += len;
        stats->buf_ptr += len;
    }
}

vpx_fixed_buf_t stats_get(stats_io_t *stats)
{
    return stats->buf;
}
/* Stereo 3D packed frame format */
typedef enum stereo_format
{
    STEREO_FORMAT_MONO       = 0,
    STEREO_FORMAT_LEFT_RIGHT = 1,
    STEREO_FORMAT_BOTTOM_TOP = 2,
    STEREO_FORMAT_TOP_BOTTOM = 3,
    STEREO_FORMAT_RIGHT_LEFT = 11
} stereo_format_t;
static int read_frame_enc(FILE *f,
                          vpx_image_t *img,
                          unsigned int file_type,
                          y4m_input *y4m,
                          struct detect_buffer *detect)
{
    int plane = 0;
    int shortread = 0;

    if (file_type == FILE_TYPE_Y4M)
    {
        if (y4m_input_fetch_frame(y4m, f, img) < 1)
            return 0;
    }
    else
    {
        if (file_type == FILE_TYPE_IVF)
        {
            char junk[IVF_FRAME_HDR_SZ];

            /* Skip the frame header. We know how big the frame should be. See
            * write_ivf_frame_header() for documentation on the frame header
            * layout.
            */
            if (fread(junk, 1, IVF_FRAME_HDR_SZ, f));
        }

        for (plane = 0; plane < 3; plane++)
        {
            unsigned char *ptr;
            int w = (plane ? (1 + img->d_w) / 2 : img->d_w);
            int h = (plane ? (1 + img->d_h) / 2 : img->d_h);
            int r;

            /* Determine the correct plane based on the image format. The
            * for-loop always counts in Y,U,V order, but this may not match the
            * order of the data on disk.
            */
            switch (plane)
            {
            case 1:
                ptr = img->planes[img->fmt==IMG_FMT_YV12? PLANE_V : PLANE_U];
                break;
            case 2:
                ptr = img->planes[img->fmt==IMG_FMT_YV12?PLANE_U : PLANE_V];
                break;
            default:
                ptr = img->planes[plane];
            }

            for (r = 0; r < h; r++)
            {
                size_t needed = w;
                size_t buf_position = 0;
                const size_t left = detect->buf_read - detect->position;

                if (left > 0)
                {
                    const size_t more = (left < needed) ? left : needed;
                    memcpy(ptr, detect->buf + detect->position, more);
                    buf_position = more;
                    needed -= more;
                    detect->position += more;
                }

                if (needed > 0)
                {
                    shortread |= (fread(ptr + buf_position, 1, needed, f) <
                        needed);
                }

                ptr += img->stride[plane];
            }
        }
    }

    return !shortread;
}
static int skim_frame_enc(FILE *f,
                          vpx_image_t *img,
                          unsigned int file_type,
                          y4m_input *y4m,
                          struct detect_buffer *detect)
{
    int plane = 0;
    int shortread = 0;

    if (file_type == FILE_TYPE_Y4M)
    {
        if (y4m_input_skim_frame(y4m, f, img) < 1)
            return 0;
    }

    if (file_type == FILE_TYPE_IVF)
    {
        IVF_FRAME_HEADER ivf_fhRaw;
        fread(&ivf_fhRaw.frameSize, 1, 4, f);
        fread(&ivf_fhRaw.timeStamp, 1, 8, f);
        vpxt_format_frame_header_read(ivf_fhRaw);
        fseek(f, ivf_fhRaw.frameSize, SEEK_CUR);
        return 0;
    }

    return 1;
}
unsigned int file_is_ivf(FILE *infile,
                         unsigned int *fourcc,
                         unsigned int *width,
                         unsigned int *height,
                         struct detect_buffer *detect,
                         unsigned int *scale,
                         unsigned int *rate)
{
    char raw_hdr[IVF_FILE_HDR_SZ];
    int is_ivf = 0;

    if (memcmp(detect->buf, "DKIF", 4) != 0)
        return 0;

    /* See write_ivf_file_header() for more documentation on the file header
    * layout.
    */
    if (fread(raw_hdr + 4, 1, IVF_FILE_HDR_SZ - 4, infile)
        == IVF_FILE_HDR_SZ - 4)
    {
        {
            is_ivf = 1;

            if (mem_get_le16(raw_hdr + 4) != 0)
                fprintf(stderr, "Error: Unrecognized IVF version! "
                "This file may not decode properly.");

            *fourcc = mem_get_le32(raw_hdr + 8);
        }
    }

    if (is_ivf)
    {
        *width = mem_get_le16(raw_hdr + 12);
        *height = mem_get_le16(raw_hdr + 14);
        *rate = mem_get_le32(raw_hdr + 16);
        *scale = mem_get_le32(raw_hdr + 20);
        detect->position = 4;
    }

    return is_ivf;
}

unsigned int file_is_y4m(FILE *infile,
                         y4m_input *y4m,
                         char detect[4])
{
    if (memcmp(detect, "YUV4", 4) == 0)
    {
        return 1;
    }

    return 0;
}
static void write_ivf_file_header(FILE *outfile,
                                  const vpx_codec_enc_cfg_t *cfg,
                                  unsigned int fourcc,
                                  int frame_cnt)
{
    char header[32];

    if (cfg->g_pass != VPX_RC_ONE_PASS && cfg->g_pass != VPX_RC_LAST_PASS)
        return;

    header[0] = 'D';
    header[1] = 'K';
    header[2] = 'I';
    header[3] = 'F';
    mem_put_le16(header + 4,  0);                 /* version */
    mem_put_le16(header + 6,  32);                /* headersize */
    mem_put_le32(header + 8,  fourcc);            /* headersize */
    mem_put_le16(header + 12, cfg->g_w);          /* width */
    mem_put_le16(header + 14, cfg->g_h);          /* height */
    mem_put_le32(header + 16, cfg->g_timebase.den); /* rate */
    mem_put_le32(header + 20, cfg->g_timebase.num); /* scale */
    mem_put_le32(header + 24, frame_cnt);         /* length */
    mem_put_le32(header + 28, 0);                 /* unused */

    if (fwrite(header, 1, 32, outfile));
}


static void write_ivf_frame_header(FILE *outfile,
                                   const vpx_codec_cx_pkt_t *pkt)
{
    char             header[12];
    vpx_codec_pts_t  pts;

    if (pkt->kind != VPX_CODEC_CX_FRAME_PKT)
        return;

    pts = pkt->data.frame.pts;
    mem_put_le32(header, pkt->data.frame.sz);
    mem_put_le32(header + 4, pts & 0xFFFFFFFF);
    mem_put_le32(header + 8, pts >> 32);

    if (fwrite(header, 1, 12, outfile));
}

void Ebml_Write(EbmlGlobal *glob, const void *buffer_in, unsigned long len)
{
    if (fwrite(buffer_in, 1, len, glob->stream));
}

#define WRITE_BUFFER(s) \
    for(i = len-1; i>=0; i--)\
    { \
        x = *(const s *)buffer_in >> (i * CHAR_BIT); \
        Ebml_Write(glob, &x, 1); \
    }
void Ebml_Serialize(EbmlGlobal *glob,
                    const void *buffer_in,
                    int buffer_size,
                    unsigned long len)
{
    char x;
    int i;

    /* buffer_size:
     * 1 - int8_t;
     * 2 - int16_t;
     * 3 - int32_t;
     * 4 - int64_t;
     */
    switch (buffer_size)
    {
    case 1:
        WRITE_BUFFER(int8_t)
        break;
    case 2:
        WRITE_BUFFER(int16_t)
        break;
    case 4:
        WRITE_BUFFER(int32_t)
        break;
    case 8:
        WRITE_BUFFER(int64_t)
        break;
    default:
        break;
    }
}
#undef WRITE_BUFFER

/* Need a fixed size serializer for the track ID. libmkv provides a 64 bit
 * one, but not a 32 bit one.
 */
static void Ebml_SerializeUnsigned32(EbmlGlobal *glob,
                                     unsigned long class_id,
                                     uint64_t ui)
{
    unsigned char sizeSerialized = 4 | 0x80;
    Ebml_WriteID(glob, class_id);
    Ebml_Serialize(glob, &sizeSerialized, sizeof(sizeSerialized), 1);
    Ebml_Serialize(glob, &ui, sizeof(ui), 4);
}


static void Ebml_StartSubElement(EbmlGlobal *glob,
                                 EbmlLoc *ebmlLoc,
                                 unsigned long class_id)
{
    // todo this is always taking 8 bytes, this may need later optimization
    // this is a key that says length unknown
    uint64_t unknownLen =  LITERALU64(0x01FFFFFFFFFFFFFF);

    Ebml_WriteID(glob, class_id);
    *ebmlLoc = ftello(glob->stream);
    Ebml_Serialize(glob, &unknownLen, sizeof(unknownLen), 8);
}

static void Ebml_EndSubElement(EbmlGlobal *glob, EbmlLoc *ebmlLoc)
{
    off_t pos;
    uint64_t size;

    /* Save the current stream pointer */
    pos = ftello(glob->stream);

    /* Calculate the size of this element */
    size = pos - *ebmlLoc - 8;
    size |=  LITERALU64(0x0100000000000000);

    /* Seek back to the beginning of the element and write the new size */
    fseeko(glob->stream, *ebmlLoc, SEEK_SET);
    Ebml_Serialize(glob, &size, sizeof(size), 8);

    /* Reset the stream pointer */
    fseeko(glob->stream, pos, SEEK_SET);
}


static void write_webm_seek_element(EbmlGlobal *ebml,
                                    unsigned long id,
                                    off_t pos)
{
    uint64_t offset = pos - ebml->position_reference;
    EbmlLoc start;
    Ebml_StartSubElement(ebml, &start, Seek);
    Ebml_SerializeBinary(ebml, SeekID, id);
    Ebml_SerializeUnsigned64(ebml, SeekPosition, offset);
    Ebml_EndSubElement(ebml, &start);
}
static void write_webm_seek_info(EbmlGlobal *ebml)
{
    off_t pos;
    pos = ftello(ebml->stream);

    if (ebml->seek_info_pos)
        fseeko(ebml->stream, ebml->seek_info_pos, SEEK_SET);
    else
        ebml->seek_info_pos = pos;

    {
        EbmlLoc start;
        Ebml_StartSubElement(ebml, &start, SeekHead);
        write_webm_seek_element(ebml, Tracks, ebml->track_pos);
        write_webm_seek_element(ebml, Cues,   ebml->cue_pos);
        write_webm_seek_element(ebml, Info,   ebml->segment_info_pos);
        Ebml_EndSubElement(ebml, &start);
    }
    {
        // segment info
        EbmlLoc startInfo;
        uint64_t frame_time;
        char version_string[64];

        /* Assemble version string */
        if(ebml->debug)
            strcpy(version_string, "vpxenc");
        else
        {
            strcpy(version_string, "vpxenc ");
            strncat(version_string,
                    vpx_codec_version_str(),
                    sizeof(version_string) - 1 - strlen(version_string));
        }

        frame_time = (uint64_t)1000 * ebml->framerate.den
                     / ebml->framerate.num;
        ebml->segment_info_pos = ftello(ebml->stream);
        Ebml_StartSubElement(ebml, &startInfo, Info);
        Ebml_SerializeUnsigned(ebml, TimecodeScale, 1000000);
        Ebml_SerializeFloat(ebml, Segment_Duration,
                            ebml->last_pts_ms + frame_time);
        Ebml_SerializeString(ebml, 0x4D80, version_string);
        Ebml_SerializeString(ebml, 0x5741, version_string);
        Ebml_EndSubElement(ebml, &startInfo);
    }
}
static void write_webm_file_header(EbmlGlobal *glob,
                                   const vpx_codec_enc_cfg_t *cfg,
                                   const struct vpx_rational *fps,
                                   stereo_format_t stereo_fmt)
{
    {
        EbmlLoc start;
        Ebml_StartSubElement(glob, &start, EBML);
        Ebml_SerializeUnsigned(glob, EBMLVersion, 1);
        Ebml_SerializeUnsigned(glob, EBMLReadVersion, 1); // EBML Read Version
        Ebml_SerializeUnsigned(glob, EBMLMaxIDLength, 4); // EBML Max ID Length
        Ebml_SerializeUnsigned(glob, EBMLMaxSizeLength, 8); // EBML Max Size Len
        Ebml_SerializeString(glob, DocType, "webm"); // Doc Type
        Ebml_SerializeUnsigned(glob, DocTypeVersion, 2); // Doc Type Version
        Ebml_SerializeUnsigned(glob, DocTypeReadVersion, 2); // Doc Type Read Ver
        Ebml_EndSubElement(glob, &start);
    }
    {
        Ebml_StartSubElement(glob, &glob->startSegment, Segment); // segment
        glob->position_reference = ftello(glob->stream);
        glob->framerate = *fps;
        write_webm_seek_info(glob);

        {
            EbmlLoc trackStart;
            glob->track_pos = ftello(glob->stream);
            Ebml_StartSubElement(glob, &trackStart, Tracks);
            {
                unsigned int trackNumber = 1;
                uint64_t     trackID = 0;

                EbmlLoc start;
                Ebml_StartSubElement(glob, &start, TrackEntry);
                Ebml_SerializeUnsigned(glob, TrackNumber, trackNumber);
                glob->track_id_pos = ftello(glob->stream);
                Ebml_SerializeUnsigned32(glob, TrackUID, trackID);
                Ebml_SerializeUnsigned(glob, TrackType, 1); // video is always 1
                Ebml_SerializeString(glob, CodecID, "V_VP8");
                {
                    unsigned int pixelWidth = cfg->g_w;
                    unsigned int pixelHeight = cfg->g_h;
                    float        frameRate   = (float)fps->num/(float)fps->den;

                    EbmlLoc videoStart;
                    Ebml_StartSubElement(glob, &videoStart, Video);
                    Ebml_SerializeUnsigned(glob, PixelWidth, pixelWidth);
                    Ebml_SerializeUnsigned(glob, PixelHeight, pixelHeight);
                    Ebml_SerializeUnsigned(glob, StereoMode, stereo_fmt);
                    Ebml_SerializeFloat(glob, FrameRate, frameRate);
                    Ebml_EndSubElement(glob, &videoStart); // Video
                }
                Ebml_EndSubElement(glob, &start); // Track Entry
            }
            Ebml_EndSubElement(glob, &trackStart);
        }
        // segment element is open
    }
}
static void write_webm_block(EbmlGlobal                *glob,
                 const vpx_codec_enc_cfg_t *cfg,
                 const vpx_codec_cx_pkt_t  *pkt)
{
    unsigned long  block_length;
    unsigned char  track_number;
    unsigned short block_timecode = 0;
    unsigned char  flags;
    int64_t        pts_ms;
    int            start_cluster = 0, is_keyframe;

    /* Calculate the PTS of this frame in milliseconds */
    pts_ms = pkt->data.frame.pts * 1000
             * (uint64_t)cfg->g_timebase.num / (uint64_t)cfg->g_timebase.den;
    if(pts_ms <= glob->last_pts_ms)
        pts_ms = glob->last_pts_ms + 1;
    glob->last_pts_ms = pts_ms;

    /* Calculate the relative time of this block */
    if(pts_ms - glob->cluster_timecode > SHRT_MAX)
        start_cluster = 1;
    else
        block_timecode = pts_ms - glob->cluster_timecode;

    is_keyframe = (pkt->data.frame.flags & VPX_FRAME_IS_KEY);
    if(start_cluster || is_keyframe)
    {
        if(glob->cluster_open)
            Ebml_EndSubElement(glob, &glob->startCluster);

        /* Open the new cluster */
        block_timecode = 0;
        glob->cluster_open = 1;
        glob->cluster_timecode = pts_ms;
        glob->cluster_pos = ftello(glob->stream);
        Ebml_StartSubElement(glob, &glob->startCluster, Cluster); // cluster
        Ebml_SerializeUnsigned(glob, Timecode, glob->cluster_timecode);

        /* Save a cue point if this is a keyframe. */
        if(is_keyframe)
        {
            struct cue_entry *cue, *new_cue_list;

            new_cue_list = (cue_entry*) realloc(glob->cue_list,
                                   (glob->cues+1) * sizeof(struct cue_entry));
            if(new_cue_list)
                glob->cue_list = new_cue_list;
            else
            {
                fprintf(stderr, "\nFailed to realloc cue list.\n");
                exit(EXIT_FAILURE);
            }

            cue = &glob->cue_list[glob->cues];
            cue->time = glob->cluster_timecode;
            cue->loc = glob->cluster_pos;
            glob->cues++;
        }
    }

    /* Write the Simple Block */
    Ebml_WriteID(glob, SimpleBlock);
    block_length = pkt->data.frame.sz + 4;
    block_length |= 0x10000000;
    Ebml_Serialize(glob, &block_length, sizeof(block_length), 4);

    track_number = 1;
    track_number |= 0x80;
    Ebml_Write(glob, &track_number, 1);

    Ebml_Serialize(glob, &block_timecode, sizeof(block_timecode), 2);

    flags = 0;
    if(is_keyframe)
        flags |= 0x80;
    if(pkt->data.frame.flags & VPX_FRAME_IS_INVISIBLE)
        flags |= 0x08;
    Ebml_Write(glob, &flags, 1);

    Ebml_Write(glob, pkt->data.frame.buf, pkt->data.frame.sz);
}


static void
write_webm_file_footer(EbmlGlobal *glob, long hash)
{
    if (glob->cluster_open)
        Ebml_EndSubElement(glob, &glob->startCluster);

    {
        EbmlLoc start;
        int i;
        glob->cue_pos = ftello(glob->stream);
        Ebml_StartSubElement(glob, &start, Cues);

        for (i = 0; i < glob->cues; i++)
        {
            struct cue_entry *cue = &glob->cue_list[i];
            EbmlLoc start;
            Ebml_StartSubElement(glob, &start, CuePoint);
            {
                EbmlLoc start;
                Ebml_SerializeUnsigned(glob, CueTime, cue->time);
                Ebml_StartSubElement(glob, &start, CueTrackPositions);
                Ebml_SerializeUnsigned(glob, CueTrack, 1);
                Ebml_SerializeUnsigned64(glob, CueClusterPosition,
                                         cue->loc - glob->position_reference);
                Ebml_EndSubElement(glob, &start);
            }
            Ebml_EndSubElement(glob, &start);
        }

        Ebml_EndSubElement(glob, &start);
    }
    Ebml_EndSubElement(glob, &glob->startSegment);
    write_webm_seek_info(glob);
    fseeko(glob->stream, glob->track_id_pos, SEEK_SET);
    Ebml_SerializeUnsigned32(glob, TrackUID, glob->debug ? 0xDEADBEEF : hash);
    fseeko(glob->stream, 0, SEEK_END);
}
static unsigned int murmur(const void *key, int len, unsigned int seed)
{
    const unsigned int m = 0x5bd1e995;
    const int r = 24;
    unsigned int h = seed ^ len;
    const unsigned char *data = (const unsigned char *)key;

    while (len >= 4)
    {
        unsigned int k;
        k  = data[0];
        k |= data[1] << 8;
        k |= data[2] << 16;
        k |= data[3] << 24;
        k *= m;
        k ^= k >> r;
        k *= m;
        h *= m;
        h ^= k;
        data += 4;
        len -= 4;
    }

    switch (len)
    {
    case 3:
        h ^= data[2] << 16;
    case 2:
        h ^= data[1] << 8;
    case 1:
        h ^= data[0];
        h *= m;
    };

    h ^= h >> 13;

    h *= m;

    h ^= h >> 15;

    return h;
}

static double vp8_mse2psnr(double Samples, double Peak, double Mse)
{
    double psnr;

    if ((double)Mse > 0.0)
        psnr = 10.0 * log10(Peak * Peak * Samples / Mse);
    else
        psnr = 60;      // Limit to prevent / 0

    if (psnr > 60)
        psnr = 60;

    return psnr;
}


static const arg_def_t debugmode = ARG_DEF("D", "debug", 0,
                                   "Debug mode (makes output deterministic)");
static const arg_def_t outputfile = ARG_DEF("o", "output", 1,
                                    "Output filename");
static const arg_def_t use_yv12 = ARG_DEF(NULL, "yv12", 0,
                                  "Input file is YV12 ");
static const arg_def_t use_i420 = ARG_DEF(NULL, "i420", 0,
                                  "Input file is I420 (default)");
static const arg_def_t codecarg = ARG_DEF(NULL, "codec", 1,
                                  "Codec to use");
static const arg_def_t passes           = ARG_DEF("p", "passes", 1,
        "Number of passes (1/2)");
static const arg_def_t pass_arg         = ARG_DEF(NULL, "pass", 1,
        "Pass to execute (1/2)");
static const arg_def_t fpf_name         = ARG_DEF(NULL, "fpf", 1,
        "First pass statistics file name");
static const arg_def_t limit = ARG_DEF(NULL, "limit", 1,
                                       "Stop encoding after n input frames");
static const arg_def_t deadline         = ARG_DEF("d", "deadline", 1,
        "Deadline per frame (usec)");
static const arg_def_t best_dl          = ARG_DEF(NULL, "best", 0,
        "Use Best Quality Deadline");
static const arg_def_t good_dl          = ARG_DEF(NULL, "good", 0,
        "Use Good Quality Deadline");
static const arg_def_t rt_dl            = ARG_DEF(NULL, "rt", 0,
        "Use Realtime Quality Deadline");
static const arg_def_t verbosearg       = ARG_DEF("v", "verbose", 0,
        "Show encoder parameters");
static const arg_def_t psnrarg          = ARG_DEF(NULL, "psnr", 0,
        "Show PSNR in status line");
static const arg_def_t framerate        = ARG_DEF(NULL, "fps", 1,
        "Stream frame rate (rate/scale)");
static const arg_def_t use_ivf          = ARG_DEF(NULL, "ivf", 0,
        "Output IVF (default is WebM)");
static const arg_def_t *main_args[] =
{
    &debugmode,
    &outputfile, &codecarg, &passes, &pass_arg, &fpf_name, &limit, &deadline,
    &best_dl, &good_dl, &rt_dl,
    &verbosearg, &psnrarg, &use_ivf, &framerate,
    NULL
};

static const arg_def_t usage            = ARG_DEF("u", "usage", 1,
        "Usage profile number to use");
static const arg_def_t threads          = ARG_DEF("t", "threads", 1,
        "Max number of threads to use");
static const arg_def_t profile          = ARG_DEF(NULL, "profile", 1,
        "Bitstream profile number to use");
static const arg_def_t width            = ARG_DEF("w", "width", 1,
        "Frame width");
static const arg_def_t height           = ARG_DEF("h", "height", 1,
        "Frame height");
static const struct arg_enum_list stereo_mode_enum[] = {
    {"mono"      , STEREO_FORMAT_MONO},
    {"left-right", STEREO_FORMAT_LEFT_RIGHT},
    {"bottom-top", STEREO_FORMAT_BOTTOM_TOP},
    {"top-bottom", STEREO_FORMAT_TOP_BOTTOM},
    {"right-left", STEREO_FORMAT_RIGHT_LEFT},
    {NULL, 0}
};
static const arg_def_t stereo_mode      = ARG_DEF_ENUM(NULL, "stereo-mode", 1,
        "Stereo 3D video format", stereo_mode_enum);
static const arg_def_t timebase         = ARG_DEF(NULL, "timebase", 1,
        "Output timestamp precision (fractional seconds)");
static const arg_def_t error_resilient  = ARG_DEF(NULL, "error-resilient", 1,
        "Enable error resiliency features");
static const arg_def_t lag_in_frames    = ARG_DEF(NULL, "lag-in-frames", 1,
        "Max number of frames to lag");

static const arg_def_t *global_args[] =
{
    &use_yv12, &use_i420, &usage, &threads, &profile,
    &width, &height, &stereo_mode, &timebase, &framerate, &error_resilient,
    &lag_in_frames, NULL
};

static const arg_def_t dropframe_thresh   = ARG_DEF(NULL, "drop-frame", 1,
        "Temporal resampling threshold (buf %)");
static const arg_def_t resize_allowed     = ARG_DEF(NULL, "resize-allowed", 1,
        "Spatial resampling enabled (bool)");
static const arg_def_t resize_up_thresh   = ARG_DEF(NULL, "resize-up", 1,
        "Upscale threshold (buf %)");
static const arg_def_t resize_down_thresh = ARG_DEF(NULL, "resize-down", 1,
        "Downscale threshold (buf %)");
static const struct arg_enum_list end_usage_enum[] = {
    {"vbr", VPX_VBR},
    {"cbr", VPX_CBR},
    {"cq",  VPX_CQ},
    {NULL, 0}
};
static const arg_def_t end_usage          = ARG_DEF_ENUM(NULL, "end-usage", 1,
        "Rate control mode", end_usage_enum);
static const arg_def_t target_bitrate     = ARG_DEF(NULL, "target-bitrate", 1,
        "Bitrate (kbps)");
static const arg_def_t min_quantizer      = ARG_DEF(NULL, "min-q", 1,
        "Minimum (best) quantizer");
static const arg_def_t max_quantizer      = ARG_DEF(NULL, "max-q", 1,
        "Maximum (worst) quantizer");
static const arg_def_t undershoot_pct     = ARG_DEF(NULL, "undershoot-pct", 1,
        "Datarate undershoot (min) target (%)");
static const arg_def_t overshoot_pct      = ARG_DEF(NULL, "overshoot-pct", 1,
        "Datarate overshoot (max) target (%)");
static const arg_def_t buf_sz             = ARG_DEF(NULL, "buf-sz", 1,
        "Client buffer size (ms)");
static const arg_def_t buf_initial_sz     = ARG_DEF(NULL, "buf-initial-sz", 1,
        "Client initial buffer size (ms)");
static const arg_def_t buf_optimal_sz     = ARG_DEF(NULL, "buf-optimal-sz", 1,
        "Client optimal buffer size (ms)");
static const arg_def_t *rc_args[] =
{
    &dropframe_thresh, &resize_allowed, &resize_up_thresh, &resize_down_thresh,
    &end_usage, &target_bitrate, &min_quantizer, &max_quantizer,
    &undershoot_pct, &overshoot_pct, &buf_sz, &buf_initial_sz, &buf_optimal_sz,
    NULL
};


static const arg_def_t bias_pct = ARG_DEF(NULL, "bias-pct", 1,
                                  "CBR/VBR bias (0=CBR, 100=VBR)");
static const arg_def_t minsection_pct = ARG_DEF(NULL, "minsection-pct", 1,
                                        "GOP min bitrate (% of target)");
static const arg_def_t maxsection_pct = ARG_DEF(NULL, "maxsection-pct", 1,
                                        "GOP max bitrate (% of target)");
static const arg_def_t *rc_twopass_args[] =
{
    &bias_pct, &minsection_pct, &maxsection_pct, NULL
};


static const arg_def_t kf_min_dist = ARG_DEF(NULL, "kf-min-dist", 1,
                                     "Minimum keyframe interval (frames)");
static const arg_def_t kf_max_dist = ARG_DEF(NULL, "kf-max-dist", 1,
                                     "Maximum keyframe interval (frames)");
static const arg_def_t kf_disabled = ARG_DEF(NULL, "disable-kf", 0,
                                     "Disable keyframe placement");
static const arg_def_t *kf_args[] =
{
    &kf_min_dist, &kf_max_dist, &kf_disabled, NULL
};


#if CONFIG_VP8_ENCODER
static const arg_def_t noise_sens = ARG_DEF(NULL, "noise-sensitivity", 1,
                                    "Noise sensitivity (frames to blur)");
static const arg_def_t sharpness = ARG_DEF(NULL, "sharpness", 1,
                                   "Filter sharpness (0-7)");
static const arg_def_t static_thresh = ARG_DEF(NULL, "static-thresh", 1,
                                       "Motion detection threshold");
#endif

#if CONFIG_VP8_ENCODER
static const arg_def_t cpu_used = ARG_DEF(NULL, "cpu-used", 1,
                                  "CPU Used (-16..16)");
#endif


#if CONFIG_VP8_ENCODER
static const arg_def_t token_parts = ARG_DEF(NULL, "token-parts", 1,
                                     "Number of token partitions to use, log2");
static const arg_def_t auto_altref = ARG_DEF(NULL, "auto-alt-ref", 1,
                                     "Enable automatic alt reference frames");
static const arg_def_t arnr_maxframes = ARG_DEF(NULL, "arnr-maxframes", 1,
                                        "AltRef Max Frames");
static const arg_def_t arnr_strength = ARG_DEF(NULL, "arnr-strength", 1,
                                       "AltRef Strength");
static const arg_def_t arnr_type = ARG_DEF(NULL, "arnr-type", 1,
                                   "AltRef Type");
static const struct arg_enum_list tuning_enum[] =
{
    {"psnr", VP8_TUNE_PSNR},
    {"ssim", VP8_TUNE_SSIM},
    {NULL, 0}
};
static const arg_def_t tune_ssim = ARG_DEF_ENUM(NULL, "tune", 1,
                                   "Material to favor", tuning_enum);
static const arg_def_t cq_level = ARG_DEF(NULL, "cq-level", 1,
                                   "Constrained Quality Level");
static const arg_def_t max_intra_rate_pct = ARG_DEF(NULL, "max-intra-rate", 1,
        "Max I-frame bitrate (pct)");

static const arg_def_t *vp8_args[] =
{
    &cpu_used, &auto_altref, &noise_sens, &sharpness, &static_thresh,
    &token_parts, &arnr_maxframes, &arnr_strength, &arnr_type,
    &tune_ssim, &cq_level, &max_intra_rate_pct, NULL
};
static const int vp8_arg_ctrl_map[] =
{
    VP8E_SET_CPUUSED, VP8E_SET_ENABLEAUTOALTREF,
    VP8E_SET_NOISE_SENSITIVITY, VP8E_SET_SHARPNESS, VP8E_SET_STATIC_THRESHOLD,
    VP8E_SET_TOKEN_PARTITIONS,
    VP8E_SET_ARNR_MAXFRAMES, VP8E_SET_ARNR_STRENGTH , VP8E_SET_ARNR_TYPE,
    VP8E_SET_TUNING, VP8E_SET_CQ_LEVEL, VP8E_SET_MAX_INTRA_BITRATE_PCT, 0
};
#endif

static const arg_def_t *no_args[] = { NULL };

static void usage_exit_enc()
{
    int i;

    fprintf(stderr, "Usage: %s <options> -o dst_filename src_filename \n",
            exec_name);

    fprintf(stderr, "\nOptions:\n");
    arg_show_usage(stdout, main_args);
    fprintf(stderr, "\nEncoder Global Options:\n");
    arg_show_usage(stdout, global_args);
    fprintf(stderr, "\nRate Control Options:\n");
    arg_show_usage(stdout, rc_args);
    fprintf(stderr, "\nTwopass Rate Control Options:\n");
    arg_show_usage(stdout, rc_twopass_args);
    fprintf(stderr, "\nKeyframe Placement Options:\n");
    arg_show_usage(stdout, kf_args);
#if CONFIG_VP8_ENCODER
    fprintf(stderr, "\nVP8 Specific Options:\n");
    arg_show_usage(stdout, vp8_args);
#endif
    fprintf(stderr, "\n"
            "Included encoders:\n"
            "\n");

    for (i = 0; i < sizeof(codecs) / sizeof(codecs[0]); i++)
        fprintf(stderr, "    %-6s - %s\n",
                codecs[i].name,
                vpx_codec_iface_name(codecs[i].iface));

    exit(EXIT_FAILURE);
}


////////////////////////// ENCODE-END //////////////////////////////////////////
////////////////////////// DECODE-START ////////////////////////////////////////
static const struct
{
    char const *name;
    const vpx_codec_iface_t *iface;
    unsigned int             fourcc;
    unsigned int             fourcc_mask;
} ifaces[] =
{
#if CONFIG_VP8_DECODER
    {"vp8",  &vpx_codec_vp8_dx_algo,   VP8_FOURCC, 0x00FFFFFF},
#endif
};

static const arg_def_t flipuvarg = ARG_DEF(NULL, "flipuv", 0,
                                   "Flip the chroma planes in the output");
static const arg_def_t noblitarg = ARG_DEF(NULL, "noblit", 0,
                                   "Don't process the decoded frames");
static const arg_def_t progressarg = ARG_DEF(NULL, "progress", 0,
                                     "Show progress after each frame decodes");
static const arg_def_t limitarg = ARG_DEF(NULL, "limit", 1,
                                  "Stop decoding after n frames");
static const arg_def_t postprocarg = ARG_DEF(NULL, "postproc", 0,
                                     "Postprocess decoded frames");
static const arg_def_t summaryarg = ARG_DEF(NULL, "summary", 0,
                                    "Show timing summary");
static const arg_def_t threadsarg = ARG_DEF("t", "threads", 1,
                                    "Max threads to use");

#if CONFIG_MD5
static const arg_def_t md5arg = ARG_DEF(NULL, "md5", 0,
                                    "Compute the MD5 sum of the decoded frame");
#endif
static const arg_def_t *all_args[] =
{
    &codecarg, &use_yv12, &use_i420, &flipuvarg, &noblitarg,
    &progressarg, &limitarg, &postprocarg, &summaryarg, &outputfile,
    &threadsarg, &verbosearg,
#if CONFIG_MD5
    &md5arg,
#endif
    NULL
};

#if CONFIG_VP8_DECODER
static const arg_def_t addnoise_level = ARG_DEF(NULL, "noise-level", 1,
                                        "Enable VP8 postproc add noise");
static const arg_def_t deblock = ARG_DEF(NULL, "deblock", 0,
                                 "Enable VP8 deblocking");
static const arg_def_t demacroblock_level = ARG_DEF(NULL, "demacroblock-level",
                                      1,"Enable VP8 demacroblocking, w/ level");
static const arg_def_t pp_debug_info = ARG_DEF(NULL, "pp-debug-info", 1,
                                       "Enable VP8 visible debug info");
static const arg_def_t pp_disp_ref_frame = ARG_DEF(NULL, "pp-dbg-ref-frame", 1,
        "Display only selected reference frame per macro block");
static const arg_def_t pp_disp_mb_modes = ARG_DEF(NULL, "pp-dbg-mb-modes", 1,
        "Display only selected macro block modes");
static const arg_def_t pp_disp_b_modes = ARG_DEF(NULL, "pp-dbg-b-modes", 1,
        "Display only selected block modes");
static const arg_def_t pp_disp_mvs = ARG_DEF(NULL, "pp-dbg-mvs", 1,
                                     "Draw only selected motion vectors");
static const arg_def_t mfqe = ARG_DEF(NULL, "mfqe", 0,
                                       "Enable multiframe quality enhancement");

static const arg_def_t *vp8_pp_args[] =
{
    &addnoise_level, &deblock, &demacroblock_level, &pp_debug_info,
    &pp_disp_ref_frame, &pp_disp_mb_modes, &pp_disp_b_modes, &pp_disp_mvs, &mfqe
    ,NULL
};
#endif
static void usage_exit_dec()
{
    int i;

    fprintf(stderr, "Usage: %s <options> filename\n\n"
            "Options:\n", exec_name);
    arg_show_usage(stderr, all_args);
#if CONFIG_VP8_DECODER
    fprintf(stderr, "\nVP8 Postprocessing Options:\n");
    arg_show_usage(stderr, vp8_pp_args);
#endif
    fprintf(stderr,
            "\nOutput File Patterns:\n\n"
            "  The -o argument specifies the name of the file(s) to "
            "write to. If the\n  argument does not include any escape "
            "characters, the output will be\n  written to a single file. "
            "Otherwise, the filename will be calculated by\n  expanding "
            "the following escape characters:\n"
            "\n\t%%w   - Frame width"
            "\n\t%%h   - Frame height"
            "\n\t%%<n> - Frame number, zero padded to <n> places (1..9)"
            "\n\n  Pattern arguments are only supported in conjunction "
            "with the --yv12 and\n  --i420 options. If the -o option is "
            "not specified, the output will be\n  directed to stdout.\n"
           );
    fprintf(stderr, "\nIncluded decoders:\n\n");

    for (i = 0; i < sizeof(ifaces) / sizeof(ifaces[0]); i++)
        fprintf(stderr, "    %-6s - %s\n",
                ifaces[i].name,
                vpx_codec_iface_name(ifaces[i].iface));

    exit(EXIT_FAILURE);
}

void die_dec(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    usage_exit_dec();
}

enum file_kind
{
    RAW_FILE,
    IVF_FILE,
    WEBM_FILE
};
struct nestegg_packet
{
    uint64_t track;
    uint64_t timecode;
    struct frame *frame;
};
struct input_ctx
{
    enum file_kind  kind;
    FILE           *infile;
    nestegg        *nestegg_ctx;
    nestegg_packet *pkt;
    unsigned int    chunk;
    unsigned int    chunks;
    unsigned int    video_track;
};
static int read_frame_dec(struct input_ctx *input,
                          uint8_t **buf,
                          size_t *buf_sz,
                          size_t *buf_alloc_sz,
                          uint64_t *timeStamp)
{
    char     raw_hdr[IVF_FRAME_HDR_SZ];
    size_t          new_buf_sz;
    FILE           *infile = input->infile;
    enum file_kind  kind = input->kind;

    if (kind == WEBM_FILE)
    {
        if (input->chunk >= input->chunks)
        {
            unsigned int track;

            do
            {
                if (input->pkt)
                    nestegg_free_packet(input->pkt);

                if (nestegg_read_packet(input->nestegg_ctx, &input->pkt) <= 0
                    || nestegg_packet_track(input->pkt, &track))
                    return 1;
            }
            while (track != input->video_track);

            *timeStamp = input->pkt->timecode;

            if (nestegg_packet_count(input->pkt, &input->chunks))
                return 1;

            input->chunk = 0;
        }

        if (nestegg_packet_data(input->pkt, input->chunk, buf, buf_sz))
            return 1;

        input->chunk++;
        return 0;
    }
    /* For both the raw and ivf formats, the frame size is the first 4 bytes
    * of the frame header. We just need to special case on the header
    * size.
    */
    else if (fread(raw_hdr, kind == IVF_FILE
                   ? IVF_FRAME_HDR_SZ : RAW_FRAME_HDR_SZ, 1, infile) != 1)
    {
        if (!feof(infile))
            fprintf(stderr, "Failed to read frame size\n");

        new_buf_sz = 0;
    }
    else
    {
        new_buf_sz = mem_get_le32(raw_hdr);
        *timeStamp = mem_get_le32(raw_hdr+4);

        if (new_buf_sz > 256 * 1024 * 1024)
        {
            fprintf(stderr, "Error: Read invalid frame size (%u)\n",
                    new_buf_sz);
            new_buf_sz = 0;
        }

        if (kind == RAW_FILE && new_buf_sz > 256 * 1024)
            fprintf(stderr, "Warning: Read invalid frame size (%u)"
                    " - not a raw file?\n", new_buf_sz);

        if (new_buf_sz > *buf_alloc_sz)
        {
            uint8_t *new_buf = (uint8_t *)realloc(*buf, 2 * new_buf_sz);

            if (new_buf)
            {
                *buf = new_buf;
                *buf_alloc_sz = 2 * new_buf_sz;
            }
            else
            {
                fprintf(stderr, "Failed to allocate compressed data buffer\n");
                new_buf_sz = 0;
            }
        }
    }

    *buf_sz = new_buf_sz;

    if (*buf_sz)
    {
        if (fread(*buf, 1, *buf_sz, infile) != *buf_sz)
        {
            fprintf(stderr, "Failed to read full frame\n");
            return 1;
        }

        return 0;
    }

    return 1;
}

static int skim_frame_dec(struct input_ctx *input,
                          uint8_t **buf,
                          size_t *buf_sz,
                          size_t *buf_alloc_sz,
                          uint64_t *timestamp)
{
    char     raw_hdr[IVF_FRAME_HDR_SZ];
    size_t          new_buf_sz;
    FILE           *infile = input->infile;
    enum file_kind  kind = input->kind;

    if (kind == WEBM_FILE)
    {
        if (input->chunk >= input->chunks)
        {
            unsigned int track;

            do
            {
                if (input->pkt)
                    nestegg_free_packet(input->pkt);

                if (nestegg_read_packet(input->nestegg_ctx, &input->pkt) <= 0
                    || nestegg_packet_track(input->pkt, &track))
                    return 1;
            }
            while (track != input->video_track);

            *timestamp = input->pkt->timecode;

            if (nestegg_packet_count(input->pkt, &input->chunks))
                return 1;

            input->chunk = 0;
        }

        if (nestegg_packet_data(input->pkt, input->chunk, buf, buf_sz))
            return 1;

        input->chunk++;
        return 0;
    }
    /* For both the raw and ivf formats, the frame size is the first 4 bytes
    * of the frame header. We just need to special case on the header
    * size.
    */
    else if (fread(raw_hdr, kind == IVF_FILE
                   ? IVF_FRAME_HDR_SZ : RAW_FRAME_HDR_SZ, 1, infile) != 1)
    {
        if (!feof(infile))
            fprintf(stderr, "Failed to read frame size\n");

        new_buf_sz = 0;
    }
    else
    {
        new_buf_sz = mem_get_le32(raw_hdr);
        *timestamp = mem_get_le32(raw_hdr+4);

        if (new_buf_sz > 256 * 1024 * 1024)
        {
            fprintf(stderr, "Error: Read invalid frame size (%u)\n",
                    new_buf_sz);
            new_buf_sz = 0;
        }

        if (kind == RAW_FILE && new_buf_sz > 256 * 1024)
            fprintf(stderr, "Warning: Read invalid frame size (%u)"
                    " - not a raw file?\n", (unsigned int)new_buf_sz);

        if (new_buf_sz > *buf_alloc_sz)
        {
            uint8_t *new_buf = (uint8_t *)realloc(*buf, 2 * new_buf_sz);

            if (new_buf)
            {
                *buf = new_buf;
                *buf_alloc_sz = 2 * new_buf_sz;
            }
            else
            {
                fprintf(stderr, "Failed to allocate compressed data buffer\n");
                new_buf_sz = 0;
            }
        }
    }

    *buf_sz = new_buf_sz;

    if (*buf_sz)
    {
        /*if (fread(*buf, 1, *buf_sz, infile) != *buf_sz)
        {
        fprintf(stderr, "Failed to read full frame\n");
        return 1;
        }*/
        long size = new_buf_sz;
        fseek(infile, size, SEEK_CUR);

        return 0;
    }

    return 1;
}

unsigned int file_is_ivf_dec(FILE *infile,
                             unsigned int *fourcc,
                             unsigned int *width,
                             unsigned int *height,
                             unsigned int *fps_den,
                             unsigned int *fps_num)
{
    char raw_hdr[32];
    int is_ivf = 0;

    if (fread(raw_hdr, 1, 32, infile) == 32)
    {
        if (raw_hdr[0] == 'D' && raw_hdr[1] == 'K'
            && raw_hdr[2] == 'I' && raw_hdr[3] == 'F')
        {
            is_ivf = 1;

            if (mem_get_le16(raw_hdr + 4) != 0)
                fprintf(stderr, "Error: Unrecognized IVF version! This file may"
                " not decode properly.");

            *fourcc = mem_get_le32(raw_hdr + 8);
            *width = mem_get_le16(raw_hdr + 12);
            *height = mem_get_le16(raw_hdr + 14);
            *fps_num = mem_get_le32(raw_hdr + 16);
            *fps_den = mem_get_le32(raw_hdr + 20);

            if (*fps_num < 1000)
            {
                if (*fps_num & 1)*fps_den <<= 1;
                else *fps_num >>= 1;
            }
            else
            {
                *fps_num = 30;
                *fps_den = 1;
            }
        }
    }

    if (!is_ivf)
        rewind(infile);

    return is_ivf;
}

unsigned int file_is_raw(FILE *infile,
                         unsigned int *fourcc,
                         unsigned int *width,
                         unsigned int *height,
                         unsigned int *fps_den,
                         unsigned int *fps_num)
{
    unsigned char buf[32];
    int is_raw = 0;
    vpx_codec_stream_info_t si;
    si.sz = sizeof(si);

    if (fread(buf, 1, 32, infile) == 32)
    {
        int i;

        if (mem_get_le32(buf) < 256 * 1024 * 1024)
            for (i = 0; i < sizeof(ifaces) / sizeof(ifaces[0]); i++)
                if (!vpx_codec_peek_stream_info(ifaces[i].iface,
                                                buf + 4, 32 - 4, &si))
                {
                    is_raw = 1;
                    *fourcc = ifaces[i].fourcc;
                    *width = si.w;
                    *height = si.h;
                    *fps_num = 30;
                    *fps_den = 1;
                    break;
                }
    }

    rewind(infile);
    return is_raw;
}
static int nestegg_read_cb(void *buffer, size_t length, void *userdata)
{
    FILE *f = (FILE *)userdata;

    if (fread(buffer, 1, length, f) < length)
    {
        if (ferror(f))
            return -1;

        if (feof(f))
            return 0;
    }

    return 1;
}
static int nestegg_seek_cb(int64_t offset, int whence, void *userdata)
{
    switch (whence)
    {
    case NESTEGG_SEEK_SET:
        whence = SEEK_SET;
        break;
    case NESTEGG_SEEK_CUR:
        whence = SEEK_CUR;
        break;
    case NESTEGG_SEEK_END:
        whence = SEEK_END;
        break;
    };

    return fseek((FILE *)userdata, offset, whence) ? -1 : 0;
}
static int64_t nestegg_tell_cb(void *userdata)
{
    return ftell((FILE *)userdata);
}
static void nestegg_log_cb(nestegg *context,
                           unsigned int severity,
                           char const *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}
static int webm_guess_framerate(struct input_ctx *input,
                                unsigned int *fps_den,
                                unsigned int *fps_num)
{
    unsigned int i;
    uint64_t     tstamp = 0;
    int visible_frames = 0;

    unsigned char       *buf = NULL;
    size_t               buf_sz = 0;
    int                  nestegg_pkt_read = 1;

    for (i = 0; tstamp < 1000000000 && i < 50;)
    {
        nestegg_packet * pkt;

        if (input->chunk >= input->chunks)
        {
            unsigned int track;

            do
            {
                nestegg_pkt_read = nestegg_read_packet(input->nestegg_ctx,&pkt);
                if (nestegg_pkt_read <= 0)
                    break;

                if (nestegg_packet_track(pkt, &track))
                    return 1;

            }
            while (track != input->video_track);

            if (nestegg_pkt_read <= 0)
            break;

            nestegg_packet_tstamp(pkt, &tstamp);

            if (nestegg_packet_count(pkt, &input->chunks))
                return 1;

            input->chunk = 0;
        }

        if (nestegg_packet_data(pkt, input->chunk, &buf, &buf_sz))
            return 1;

        if ((buf[0] >> 4) & 0x1)
            visible_frames++;

        input->chunk++;
        nestegg_free_packet(pkt);
        i++;
    }

    if (nestegg_track_seek(input->nestegg_ctx, input->video_track, 0))
        goto fail;

    *fps_num = (visible_frames - 1) * 1000000;
    *fps_den = tstamp / 1000;
    return 0;
fail:
    nestegg_destroy(input->nestegg_ctx);
    input->nestegg_ctx = NULL;
    rewind(input->infile);
    return 1;
}
static int file_is_webm(struct input_ctx *input,
                        unsigned int *fourcc, unsigned int *width,
                        unsigned int *height,
                        unsigned int *fps_den,
                        unsigned int *fps_num)
{
    unsigned int i, n;
    int          track_type = -1;
    uint64_t     tstamp = 0;
    nestegg_io io = {nestegg_read_cb, nestegg_seek_cb, nestegg_tell_cb,
                     input->infile
                    };
    nestegg_video_params params;
    nestegg_packet *pkt;

    if (nestegg_init(&input->nestegg_ctx, io, NULL))
        goto fail;

    if (nestegg_track_count(input->nestegg_ctx, &n))
        goto fail;

    for (i = 0; i < n; i++)
    {
        track_type = nestegg_track_type(input->nestegg_ctx, i);

        if (track_type == NESTEGG_TRACK_VIDEO)
            break;
        else if (track_type < 0)
            goto fail;
    }

    if (nestegg_track_codec_id(input->nestegg_ctx, i) != NESTEGG_CODEC_VP8)
    {
        fprintf(stderr, "Not VP8 video, quitting.\n");
        exit(1);
    }

    input->video_track = i;

    if (nestegg_track_video_params(input->nestegg_ctx, i, &params))
        goto fail;

    *fps_den = 0;
    *fps_num = 0;
    *fourcc = VP8_FOURCC;
    *width = params.width;
    *height = params.height;
    return 1;
fail:
    input->nestegg_ctx = NULL;
    rewind(input->infile);
    return 0;
}
void show_progress(int frame_in, int frame_out, unsigned long dx_time)
{
    fprintf(stderr, "%d decoded frames/%d showed frames in %lu us (%.2f fps)\r",
            frame_in, frame_out, dx_time,
            (float)frame_out * 1000000.0 / (float)dx_time);
}
void generate_filename(const char *pattern,
                       char *out, size_t q_len,
                       unsigned int d_w,
                       unsigned int d_h,
                       unsigned int frame_in)
{
    const char *p = pattern;
    char *q = out;

    do
    {
        char *next_pat = strchr((char *)p, '%');

        if (p == next_pat)
        {
            size_t pat_len;
            q[q_len - 1] = '\0';

            switch (p[1])
            {
            case 'w':
                snprintf(q, q_len - 1, "%d", d_w);
                break;
            case 'h':
                snprintf(q, q_len - 1, "%d", d_h);
                break;
            case '1':
                snprintf(q, q_len - 1, "%d", frame_in);
                break;
            case '2':
                snprintf(q, q_len - 1, "%02d", frame_in);
                break;
            case '3':
                snprintf(q, q_len - 1, "%03d", frame_in);
                break;
            case '4':
                snprintf(q, q_len - 1, "%04d", frame_in);
                break;
            case '5':
                snprintf(q, q_len - 1, "%05d", frame_in);
                break;
            case '6':
                snprintf(q, q_len - 1, "%06d", frame_in);
                break;
            case '7':
                snprintf(q, q_len - 1, "%07d", frame_in);
                break;
            case '8':
                snprintf(q, q_len - 1, "%08d", frame_in);
                break;
            case '9':
                snprintf(q, q_len - 1, "%09d", frame_in);
                break;
            default:
                die_dec("Unrecognized pattern %%%c\n", p[1]);
            }

            pat_len = strlen(q);

            if (pat_len >= q_len - 1)
                die_dec("Output filename too long.\n");

            q += pat_len;
            p += 2;
            q_len -= pat_len;
        }
        else
        {
            size_t copy_len;

            if (!next_pat)
                copy_len = strlen(p);
            else
                copy_len = next_pat - p;

            if (copy_len >= q_len - 1)
                die_dec("Output filename too long.\n");

            memcpy(q, p, copy_len);
            q[copy_len] = '\0';
            q += copy_len;
            p += copy_len;
            q_len -= copy_len;
        }
    }
    while (*p);
}
struct parsed_header
{
    char key_frame;
    int version;
    char show_frame;
    int first_part_size;
};
int next_packet(struct parsed_header *hdr, int pos, int length, int mtu)
{
    int size = 0;
    int remaining = length - pos;
    /* Uncompressed part is 3 bytes for P frames and 10 bytes for I frames */
    int uncomp_part_size = (hdr->key_frame ? 10 : 3);
    /* number of bytes yet to send from header and the first partition */
    int remainFirst = uncomp_part_size + hdr->first_part_size - pos;

    if (remainFirst > 0)
    {
        if (remainFirst <= mtu)
        {
            size = remainFirst;
        }
        else
        {
            size = mtu;
        }

        return size;
    }

    /* second partition; just slot it up according to MTU */
    if (remaining <= mtu)
    {
        size = remaining;
        return size;
    }

    return mtu;
}
void throw_packets(unsigned char *frame,
                   int *size,
                   int loss_rate,
                   int *thrown,
                   int *kept,
                   int printVar)
{
    unsigned char loss_frame[256*1024];
    int pkg_size = 1;
    int pos = 0;
    int loss_pos = 0;
    struct parsed_header hdr;
    unsigned int tmp;
    int mtu = 1500;

    if (*size < 3)
    {
        return;
    }

    // putc('|', stdout);
    tprintf(printVar, "|");
    /* parse uncompressed 3 bytes */
    tmp = (frame[2] << 16) | (frame[1] << 8) | frame[0];
    hdr.key_frame = !(tmp & 0x1); /* inverse logic */
    hdr.version = (tmp >> 1) & 0x7;
    hdr.show_frame = (tmp >> 4) & 0x1;
    hdr.first_part_size = (tmp >> 5) & 0x7FFFF;

    /* don't drop key frames */
    if (hdr.key_frame)
    {
        int i;
        *kept = *size / mtu + ((*size % mtu > 0) ? 1 : 0); /* approximate */

        for (i = 0; i < *kept; i++)
            tprintf(printVar, ".");

        // putc('.', stdout);
        return;
    }

    while ((pkg_size = next_packet(&hdr, pos, *size, mtu)) > 0)
    {
        int loss_event = ((rand() + 1.0) /(RAND_MAX + 1.0) < loss_rate / 100.0);

        if (*thrown == 0 && !loss_event)
        {
            memcpy(loss_frame + loss_pos, frame + pos, pkg_size);
            loss_pos += pkg_size;
            (*kept)++;
            tprintf(printVar, ".");
            // putc('.', stdout);
        }
        else
        {
            (*thrown)++;
            tprintf(printVar, "X");
            // putc('X', stdout);
        }

        pos += pkg_size;
    }

    memcpy(frame, loss_frame, loss_pos);
    memset(frame + loss_pos, 0, *size - loss_pos);
    *size = loss_pos;
}
/////////////////////////// DECODE-END /////////////////////////////////////////
// --------------------------------VP8 Settings---------------------------------
void vpxt_default_parameters(VP8_CONFIG &opt)
{
    opt.allow_lag = 1;
    opt.alt_freq = 16;
    opt.alt_q = 20;
    opt.cpu_used = 0;
    opt.encode_breakout = 0;
    opt.gold_q = 28;
    opt.key_q = 12;
    opt.play_alternate = 1;
    opt.worst_allowed_q = 56;
    opt.lag_in_frames = 0;
    opt.allow_df = 0;
    opt.allow_spatial_resampling = 0;
    opt.auto_key = 1;
    opt.best_allowed_q = 4;
    opt.drop_frames_water_mark = 70;
    opt.end_usage = 1;
    opt.fixed_q = -1;
    opt.key_freq = 999999;

    opt.maximum_buffer_size = 6000;
    opt.Mode = 2;
    opt.noise_sensitivity = 0;
    opt.optimal_buffer_level = 5000;
    opt.resample_down_water_mark = 30;
    opt.resample_up_water_mark = 60;
    opt.Sharpness = 0;
    opt.starting_buffer_level = 4000;
    opt.target_bandwidth = 40;
    opt.two_pass_vbrbias = 50;
    opt.two_pass_vbrmax_section = 400;
    opt.two_pass_vbrmin_section = 0;
    opt.over_shoot_pct = 100;
    opt.under_shoot_pct = 100;
    opt.Version = 0;

    // not included in default settings file
    opt.Height = 0;
    opt.multi_threaded = 0;
    opt.Width = 0;

    opt.token_partitions = 0;
    opt.error_resilient_mode = 0;

    opt.cq_level = 10;
    opt.arnr_max_frames = 0;
    opt.arnr_strength = 3;
    opt.arnr_type = 3;

    opt.rc_max_intra_bitrate_pct = 0;
    opt.timebase.num = 30;
    opt.timebase.den = 1;
}
void vpxt_determinate_parameters(VP8_CONFIG &opt)
{
    opt.noise_sensitivity = 0; // Noise sensitivity not currently det.2011-07-20
    opt.multi_threaded = 0;    // Multithread not currently det.      2011-07-27
    // Make sure valid cpu_used settings are used                     2011-07-27
    // (negative cpu_used should be deterministic for realtime)
    if (opt.Mode == kRealTime)
        if (opt.cpu_used > 0)
            opt.cpu_used = opt.cpu_used * -1;
        else if (opt.cpu_used == 0)
            opt.cpu_used = -1;
}
int vpxt_core_config_to_api_config(VP8_CONFIG coreCfg, vpx_codec_enc_cfg_t *cfg)
{
    // Converts a core configuration to api configuration

    cfg->g_threads = coreCfg.multi_threaded;
    cfg->g_profile = coreCfg.Version;
    cfg->g_error_resilient = coreCfg.error_resilient_mode;
    cfg->rc_resize_allowed = coreCfg.allow_spatial_resampling;
    cfg->rc_resize_up_thresh = coreCfg.resample_up_water_mark;
    cfg->rc_resize_down_thresh = coreCfg.resample_down_water_mark;
    cfg->rc_target_bitrate = coreCfg.target_bandwidth;
    cfg->rc_min_quantizer = coreCfg.best_allowed_q;
    cfg->rc_max_quantizer = coreCfg.worst_allowed_q;
    cfg->rc_overshoot_pct = coreCfg.over_shoot_pct;
    cfg->rc_undershoot_pct = coreCfg.under_shoot_pct;
    cfg->rc_buf_sz = coreCfg.maximum_buffer_size;
    cfg->rc_buf_initial_sz  = coreCfg.starting_buffer_level;
    cfg->rc_buf_optimal_sz  = coreCfg.optimal_buffer_level;
    cfg->rc_2pass_vbr_bias_pct      = coreCfg.two_pass_vbrbias;
    cfg->rc_2pass_vbr_minsection_pct    = coreCfg.two_pass_vbrmin_section;
    cfg->rc_2pass_vbr_maxsection_pct  = coreCfg.two_pass_vbrmax_section;

    if (coreCfg.auto_key == 0)
    {
        cfg->kf_mode                = VPX_KF_FIXED;
    }

    if (coreCfg.auto_key == 1)
    {
        cfg->kf_mode                = VPX_KF_AUTO;
    }

    cfg->kf_max_dist                = coreCfg.key_freq;

    if (coreCfg.fixed_q != -1)
    {
        if (coreCfg.fixed_q > 63)
        {
            coreCfg.fixed_q = 63;
        }

        if (coreCfg.fixed_q < 0)
        {
            coreCfg.fixed_q = 0;
        }

        cfg->rc_min_quantizer = coreCfg.fixed_q;
        cfg->rc_max_quantizer = coreCfg.fixed_q;
    }

    if (coreCfg.allow_lag == 0)
    {
        cfg->g_lag_in_frames = 0;
    }
    else
    {
        cfg->g_lag_in_frames = coreCfg.lag_in_frames;
    }

    if (coreCfg.allow_df == 0)
    {
        cfg->rc_dropframe_thresh = 0;
    }
    else
    {
        cfg->rc_dropframe_thresh = coreCfg.drop_frames_water_mark;
    }

    if (coreCfg.end_usage == USAGE_LOCAL_FILE_PLAYBACK)
    {
        cfg->rc_end_usage = VPX_VBR;
    }

    if (coreCfg.end_usage == USAGE_STREAM_FROM_SERVER)
    {
        cfg->rc_end_usage = VPX_CBR;
    }

    if (coreCfg.end_usage == USAGE_CONSTRAINED_QUALITY)
    {
        cfg->rc_end_usage = VPX_CQ;
    }

#if ENABLE_LAYERS
    cfg->ts_number_layers = 1;
#endif

    return 0;
}
int vpxt_api_config_to_core_config(vpx_codec_enc_cfg_t cfg, VP8_CONFIG &coreCfg)
{
    // Converts a api configuration to core configuration
    coreCfg.multi_threaded = cfg.g_threads;
    coreCfg.Version = cfg.g_profile;
    coreCfg.error_resilient_mode = cfg.g_error_resilient;
    coreCfg.allow_spatial_resampling = cfg.rc_resize_allowed;
    coreCfg.resample_up_water_mark = cfg.rc_resize_up_thresh;
    coreCfg.resample_down_water_mark = cfg.rc_resize_down_thresh;
    coreCfg.target_bandwidth = cfg.rc_target_bitrate;
    coreCfg.best_allowed_q = cfg.rc_min_quantizer;
    coreCfg.worst_allowed_q = cfg.rc_max_quantizer;
    coreCfg.over_shoot_pct = cfg.rc_overshoot_pct;
    coreCfg.under_shoot_pct = cfg.rc_undershoot_pct;
    coreCfg.maximum_buffer_size = cfg.rc_buf_sz;
    coreCfg.starting_buffer_level  = cfg.rc_buf_initial_sz;
    coreCfg.optimal_buffer_level  = cfg.rc_buf_optimal_sz;
    coreCfg.two_pass_vbrbias      = cfg.rc_2pass_vbr_bias_pct;
    coreCfg.two_pass_vbrmin_section    = cfg.rc_2pass_vbr_minsection_pct;
    coreCfg.two_pass_vbrmax_section  = cfg.rc_2pass_vbr_maxsection_pct;

    if (cfg.kf_mode == VPX_KF_FIXED)
    {
        coreCfg.auto_key = 0;
    }

    if (cfg.kf_mode == VPX_KF_AUTO)
    {
        coreCfg.auto_key = 1;
    }

    coreCfg.key_freq = cfg.kf_max_dist;

    if(cfg.rc_min_quantizer == cfg.rc_max_quantizer)
    {
        coreCfg.fixed_q = cfg.rc_min_quantizer;
    }

    if (cfg.g_lag_in_frames == 0)
    {
        coreCfg.allow_lag = 0;
    }
    else
    {
        coreCfg.allow_lag = cfg.g_lag_in_frames;
    }

    if (cfg.rc_dropframe_thresh == 0)
    {
        coreCfg.allow_df = 0;
    }
    else
    {
        coreCfg.drop_frames_water_mark = cfg.rc_dropframe_thresh;
    }

    if (cfg.rc_end_usage == VPX_VBR)
    {
        coreCfg.end_usage = USAGE_LOCAL_FILE_PLAYBACK;
    }

    if (cfg.rc_end_usage == VPX_CBR)
    {
        coreCfg.end_usage = USAGE_STREAM_FROM_SERVER;
    }

    if (cfg.rc_end_usage == VPX_CQ)
    {
        coreCfg.end_usage = USAGE_CONSTRAINED_QUALITY;
    }

    return 0;
}
VP8_CONFIG vpxt_random_parameters(VP8_CONFIG &opt,
                                  const char *inputfile,
                                  int display)
{
    // Ranges can be found in validate_config in vp8_cx_iface.c

    ////////////////////////////////// Randomly Generated \\\\\\\\\\\\\\\\\\\\\

    srand(vpxt_get_high_res_timer_tick());
    int w = 0;
    int h = 0;
    int fr  = 0;
    int length = 1;


    FILE *GetWHinfile = fopen(inputfile, "rb");

    if (GetWHinfile == NULL)
    {
        w = rand() % 1920;
        h = rand() % 1080;
        length = rand() % 100 + rand() % 5;
    }
    else
    {
        IVF_HEADER ivf_h_raw;
        InitIVFHeader(&ivf_h_raw);
        fread(&ivf_h_raw, 1, sizeof(ivf_h_raw), GetWHinfile);
        vpxt_format_ivf_header_read(&ivf_h_raw);

        w       = ivf_h_raw.width;
        h       = ivf_h_raw.height;
        fr      = (ivf_h_raw.rate / ivf_h_raw.scale);
        length  = ivf_h_raw.length;

        if (length == 0)
            length = 1;

        if (fr == 0)
            fr = 1;

        if (h == 0)
            h = 1;

        if (w == 0)
            w = 1;

        fclose(GetWHinfile);
    }

    opt.Width = w;
    opt.Height = h;

    opt.Mode = rand() % 5;                   // valid Range: (0 to 4)

    if (opt.Mode == kRealTime)
        opt.noise_sensitivity = 0;           // valid Range:
    else                                     // if Real time Mode 0 to 0
#if CONFIG_TEMPORAL_DENOISING                // if Not Real time and Temp De 0-1
        opt.noise_sensitivity = rand() % 2;  // if Not Real time not Temp De 0-6
#else
        opt.noise_sensitivity = rand() % 7;
#endif

    if (opt.Mode == kRealTime)
        opt.lag_in_frames = 0;                // valid Range:
    else                                      // if Not Real time Mode 0 to 25
        opt.lag_in_frames = rand() % 26;      // if Real time Mode 0 to 0

    if (opt.lag_in_frames > 0)
        opt.allow_lag = 1;                    // valid Range:
    else                                      // if Not Real time Mode 0 to 1
        opt.allow_lag = 0;                    // if Real time Mode 0

    if (opt.Mode == kRealTime)
    {
        opt.cpu_used = rand() % 13 + 4;       // valid Range:

        if (rand() % 2)                       // if Not Real time Mode -16 to 16
            opt.cpu_used = opt.cpu_used * -1; // if Real time Mode -16 to -4 or
                                              // 4 to 16
    }
    else
    {
        opt.cpu_used = rand() % 17;

        if (rand() % 2)
            opt.cpu_used = opt.cpu_used * -1;
    }

    if (rand() % 21 == 20) // 1 out of 20 chance of a fixed q
    {
        opt.fixed_q = rand() % 64;// valid Range: 0 to 63 or -1(-1 = fixedQ off)
        opt.best_allowed_q = opt.fixed_q;     // valid Range: 0 to 63
        opt.worst_allowed_q = opt.fixed_q;    // valid Range: 0 to 63
        opt.cq_level = opt.worst_allowed_q;   // valid Range: 0 to 63
    }
    else
    {
        opt.fixed_q = -1; // valid Range: 0 to 63 or -1 (-1 = fixedQ off)
        opt.worst_allowed_q   = rand() % 64;  // valid Range: 0 to 63
        opt.best_allowed_q = rand() % 64;     // valid Range:0 to 63

        while (opt.best_allowed_q > opt.worst_allowed_q)
        {
            opt.best_allowed_q = rand() % 64;
            opt.worst_allowed_q   = rand() % 64;
        }

        // valid Range: opt.best_allowed_q to opt.worst_allowed_q
        opt.cq_level = 0;
        while(opt.cq_level < opt.best_allowed_q
              || opt.cq_level > opt.worst_allowed_q)
            opt.cq_level   = rand() % 64;
    }

    opt.auto_key = rand() % 2;                // valid Range: 0 to 1
    opt.multi_threaded =  rand() % 65;        // valid Range: 0 to 64
    opt.over_shoot_pct = rand() % 101;        // valid Range: 0 to 100
    opt.under_shoot_pct = rand() % 101;       // valid Range: 0 to 100
    opt.allow_df = rand() % 2;                // valid Range: 0 to 1

    if (opt.allow_df > 0)
        opt.drop_frames_water_mark = rand() % 101; // valid Range: 0 to 100
    else
        opt.drop_frames_water_mark = 0;

    opt.allow_spatial_resampling = rand() % 2;// valid Range: 0 to 1
    opt.resample_up_water_mark = rand() % 101;// valid Range: 0 to 100
    opt.resample_down_water_mark = rand() % 101;// valid Range:  0 to 100
    opt.two_pass_vbrbias = rand() % 101;      // valid Range: 0 to 100
    opt.encode_breakout = rand() % 101;       // valid Range:
    opt.end_usage = rand() % 3;               // valid Range: 0 to 2
    opt.Version = rand() % 4;                 // valid Range: 0 to 3
    opt.Sharpness = rand() % 8;               // valid Range: 0 to 7
    opt.play_alternate = rand() % 2;          // valid Range: 0 to 1
    opt.token_partitions = rand() % 4;        // valid Range: 0 to 3
    opt.error_resilient_mode = rand() % 101;  // valid Range: 0 to 100

    int TBUpperBound = ((w * h) / (320 * 240) * 2048);

    if (TBUpperBound == 0)
        TBUpperBound = 1;

    // valid Range: No Max so based on resolution
    opt.target_bandwidth = rand() %  TBUpperBound + 1;
    // valid Range: No Max so based on number of frames
    opt.key_freq = rand() % length + 2;

    opt.arnr_max_frames = rand() % 16;        // valid Range: 0 to 15
    opt.arnr_strength = rand() % 7;           // valid Range: 6
    opt.arnr_type = rand() % 3 + 1;           // valid Range: 1 to 3

    opt.maximum_buffer_size = 6000;
    opt.starting_buffer_level = 4000;
    opt.optimal_buffer_level = 5000;
    opt.two_pass_vbrmin_section = 400;
    opt.two_pass_vbrmax_section = 0;

    opt.alt_freq = 16;
    opt.alt_q = 20;
    opt.gold_q = 28;
    opt.key_q = 12;

    // valid Range: 0 to 1500 (soft range limit most reasonable value between
    // 100 and 1500)
    opt.rc_max_intra_bitrate_pct = rand() % 1501;

    if (display != 2)
    {
        tprintf(PRINT_STD, "\nRandom Parameters Generated:");
        std::cout << "\n\n";
        std::cout << "TargetBandwidth " << opt.target_bandwidth << "\n";
        std::cout << "NoiseSensitivity " << opt.noise_sensitivity << "\n";
        std::cout << "Sharpness " << opt.Sharpness << "\n";
        std::cout << "CpuUsed " << opt.cpu_used << "\n";
        std::cout << "Mode " << opt.Mode << "\n";
        std::cout << "AutoKey " << opt.auto_key << "\n";
        std::cout << "KeyFreq " << opt.key_freq << "\n";
        std::cout << "EndUsage " << opt.end_usage << "\n";
        std::cout << "OverShootPct " << opt.over_shoot_pct << "\n";
        std::cout << "UnderShootPct " << opt.under_shoot_pct << "\n";
        std::cout << "StartingBufferLevel " << opt.starting_buffer_level
            << "\n";
        std::cout << "OptimalBufferLevel " << opt.optimal_buffer_level << "\n";
        std::cout << "MaximumBufferSize " << opt.maximum_buffer_size << "\n";
        std::cout << "FixedQ " << opt.fixed_q << "\n";
        std::cout << "WorstAllowedQ " << opt.worst_allowed_q  << "\n";
        std::cout << "BestAllowedQ " << opt.best_allowed_q << "\n";
        std::cout << "AllowSpatialResampling " << opt.allow_spatial_resampling
            << "\n";
        std::cout << "ResampleDownWaterMark " << opt.resample_down_water_mark
            << "\n";
        std::cout << "ResampleUpWaterMark " << opt.resample_up_water_mark
            << "\n";
        std::cout << "AllowDF " << opt.allow_df << "\n";
        std::cout << "DropFramesWaterMark " << opt.drop_frames_water_mark
            << "\n";
        std::cout << "AllowLag " << opt.allow_lag << "\n";
        std::cout << "PlayAlternate " << opt.play_alternate << "\n";
        std::cout << "AltQ " << opt.alt_q << "\n";
        std::cout << "AltFreq " << opt.alt_freq << "\n";
        std::cout << "GoldQ " << opt.gold_q << "\n";
        std::cout << "KeyQ " << opt.key_q << "\n";
        std::cout << "Version " << opt.Version << "\n";
        std::cout << "LagInFrames " << opt.lag_in_frames << "\n";
        std::cout << "TwoPassVBRBias " << opt.two_pass_vbrbias << "\n";
        std::cout << "TwoPassVBRMinSection " << opt.two_pass_vbrmin_section
            << "\n";
        std::cout << "TwoPassVBRMaxSection " << opt.two_pass_vbrmax_section
            << "\n";
        std::cout << "EncodeBreakout " << opt.encode_breakout << "\n";
        std::cout << "TokenPartitions " << opt.token_partitions << "\n";
        std::cout << "MultiThreaded " << opt.multi_threaded << "\n";
        std::cout << "ErrorResilientMode " << opt.error_resilient_mode << "\n";
        std::cout << "rc_max_intra_bitrate_pct " << opt.rc_max_intra_bitrate_pct
            << "\n";
    }

    if (display == 1)
    {
        fprintf(stderr, "\nRandom Parameters Generated:");
        std::cerr << "\n\n";
        std::cerr << "TargetBandwidth " << opt.target_bandwidth << "\n";
        std::cerr << "NoiseSensitivity " << opt.noise_sensitivity << "\n";
        std::cerr << "Sharpness " << opt.Sharpness << "\n";
        std::cerr << "CpuUsed " << opt.cpu_used << "\n";
        std::cerr << "Mode " << opt.Mode << "\n";
        std::cerr << "AutoKey " << opt.auto_key << "\n";
        std::cerr << "KeyFreq " << opt.key_freq << "\n";
        std::cerr << "EndUsage " << opt.end_usage << "\n";
        std::cout << "OverShootPct " << opt.over_shoot_pct << "\n";
        std::cerr << "UnderShootPct " << opt.under_shoot_pct << "\n";
        std::cerr << "StartingBufferLevel " << opt.starting_buffer_level
            << "\n";
        std::cerr << "OptimalBufferLevel " << opt.optimal_buffer_level << "\n";
        std::cerr << "MaximumBufferSize " << opt.maximum_buffer_size << "\n";
        std::cerr << "FixedQ " << opt.fixed_q << "\n";
        std::cerr << "WorstAllowedQ " << opt.worst_allowed_q  << "\n";
        std::cerr << "BestAllowedQ " << opt.best_allowed_q << "\n";
        std::cerr << "AllowSpatialResampling " << opt.allow_spatial_resampling
            << "\n";
        std::cerr << "ResampleDownWaterMark " << opt.resample_down_water_mark
            << "\n";
        std::cerr << "ResampleUpWaterMark " << opt.resample_up_water_mark
            << "\n";
        std::cerr << "AllowDF " << opt.allow_df << "\n";
        std::cerr << "DropFramesWaterMark " << opt.drop_frames_water_mark
            << "\n";
        std::cerr << "AllowLag " << opt.allow_lag << "\n";
        std::cerr << "PlayAlternate " << opt.play_alternate << "\n";
        std::cerr << "AltQ " << opt.alt_q << "\n";
        std::cerr << "AltFreq " << opt.alt_freq << "\n";
        std::cerr << "GoldQ " << opt.gold_q << "\n";
        std::cerr << "KeyQ " << opt.key_q << "\n";
        std::cerr << "Version " << opt.Version << "\n";
        std::cerr << "LagInFrames " << opt.lag_in_frames << "\n";
        std::cerr << "TwoPassVBRBias " << opt.two_pass_vbrbias << "\n";
        std::cerr << "TwoPassVBRMinSection " << opt.two_pass_vbrmin_section
            << "\n";
        std::cerr << "TwoPassVBRMaxSection " << opt.two_pass_vbrmax_section
            << "\n";
        std::cerr << "EncodeBreakout " << opt.encode_breakout << "\n";
        std::cerr << "TokenPartitions " << opt.token_partitions << "\n";
        std::cerr << "MultiThreaded " << opt.multi_threaded << "\n";
        std::cerr << "ErrorResilientMode " << opt.error_resilient_mode << "\n";
        std::cerr << "rc_max_intra_bitrate_pct " << opt.rc_max_intra_bitrate_pct
            << "\n";
    }

    return opt;
}
VP8_CONFIG vpxt_input_settings(const char *input_file)
{
    // Reads an input file and sets VP8_CONFIG accordingly
    std::ifstream infile2(input_file);

    std::string Garbage;

    VP8_CONFIG opt;
    vpxt_default_parameters(opt);

    infile2 >> opt.target_bandwidth;
    infile2 >> Garbage;
    infile2 >> opt.noise_sensitivity;
    infile2 >> Garbage;
    infile2 >> opt.Sharpness;
    infile2 >> Garbage;
    infile2 >> opt.cpu_used;
    infile2 >> Garbage;
    infile2 >> opt.Mode;
    infile2 >> Garbage;
    infile2 >> opt.auto_key;
    infile2 >> Garbage;
    infile2 >> opt.key_freq;
    infile2 >> Garbage;
    infile2 >> opt.end_usage;
    infile2 >> Garbage;
    infile2 >> opt.over_shoot_pct;
    infile2 >> Garbage;
    infile2 >> opt.under_shoot_pct;
    infile2 >> Garbage;
    infile2 >> opt.starting_buffer_level;
    infile2 >> Garbage;
    infile2 >> opt.optimal_buffer_level;
    infile2 >> Garbage;
    infile2 >> opt.maximum_buffer_size;
    infile2 >> Garbage;
    infile2 >> opt.fixed_q;
    infile2 >> Garbage;
    infile2 >> opt.worst_allowed_q;
    infile2 >> Garbage;
    infile2 >> opt.best_allowed_q;
    infile2 >> Garbage;
    infile2 >> opt.allow_spatial_resampling;
    infile2 >> Garbage;
    infile2 >> opt.resample_down_water_mark;
    infile2 >> Garbage;
    infile2 >> opt.resample_up_water_mark;
    infile2 >> Garbage;
    infile2 >> opt.allow_df;
    infile2 >> Garbage;
    infile2 >> opt.drop_frames_water_mark;
    infile2 >> Garbage;
    infile2 >> opt.allow_lag;
    infile2 >> Garbage;
    infile2 >> opt.play_alternate;
    infile2 >> Garbage;
    infile2 >> opt.alt_q;
    infile2 >> Garbage;
    infile2 >> opt.alt_freq;
    infile2 >> Garbage;
    infile2 >> opt.gold_q;
    infile2 >> Garbage;
    infile2 >> opt.key_q;
    infile2 >> Garbage;
    infile2 >> opt.Version;
    infile2 >> Garbage;
    infile2 >> opt.lag_in_frames;
    infile2 >> Garbage;
    infile2 >> opt.two_pass_vbrbias;
    infile2 >> Garbage;
    infile2 >> opt.two_pass_vbrmin_section;
    infile2 >> Garbage;
    infile2 >> opt.two_pass_vbrmax_section;
    infile2 >> Garbage;
    infile2 >> opt.encode_breakout;
    infile2 >> Garbage;
    infile2 >> opt.token_partitions ;
    infile2 >> Garbage;
    infile2 >> opt.multi_threaded;
    infile2 >> Garbage;
    infile2 >> opt.error_resilient_mode;
    infile2 >> Garbage;

    infile2 >> opt.Height;
    infile2 >> Garbage;
    infile2 >> opt.Width;
    infile2 >> Garbage;

    infile2 >> opt.cq_level;
    infile2 >> Garbage;
    infile2 >> opt.arnr_max_frames;
    infile2 >> Garbage;
    infile2 >> opt.arnr_strength;
    infile2 >> Garbage;
    infile2 >> opt.arnr_type;
    infile2 >> Garbage;
    infile2 >> opt.rc_max_intra_bitrate_pct;
    infile2 >> Garbage;

    infile2.close();

    return opt;
}
int vpxt_output_settings(const char *output_file, VP8_CONFIG opt)
{
    // Saves all VP8_CONFIG settings to a settings file readable by InputSettings

    std::ofstream outfile(output_file);

    outfile <<  opt.target_bandwidth << " TargetBandwidth\n";
    outfile <<  opt.noise_sensitivity << " NoiseSensitivity\n";
    outfile <<  opt.Sharpness << " Sharpness\n";
    outfile <<  opt.cpu_used << " CpuUsed\n";
    outfile <<  opt.Mode << " Mode\n";
    outfile <<  opt.auto_key << " AutoKey\n";
    outfile <<  opt.key_freq << " KeyFreq\n";
    outfile <<  opt.end_usage << " EndUsage\n";
    outfile <<  opt.over_shoot_pct << " OverShootPct\n";
    outfile <<  opt.under_shoot_pct << " UnderShootPct\n";
    outfile <<  opt.starting_buffer_level << " StartingBufferLevel\n";
    outfile <<  opt.optimal_buffer_level << " OptimalBufferLevel\n";
    outfile <<  opt.maximum_buffer_size << " MaximumBufferSize\n";
    outfile <<  opt.fixed_q << " FixedQ\n";
    outfile <<  opt.worst_allowed_q << " WorstAllowedQ\n";
    outfile <<  opt.best_allowed_q << " BestAllowedQ\n";
    outfile <<  opt.allow_spatial_resampling << " AllowSpatialResampling\n";
    outfile <<  opt.resample_down_water_mark << " ResampleDownWaterMark\n";
    outfile <<  opt.resample_up_water_mark << " ResampleUpWaterMark\n";
    outfile <<  opt.allow_df << " AllowDF\n";
    outfile <<  opt.drop_frames_water_mark << " DropFramesWaterMark\n";
    outfile <<  opt.allow_lag << " AllowLag\n";
    outfile <<  opt.play_alternate << " PlayAlternate\n";
    outfile <<  opt.alt_q << " AltQ\n";
    outfile <<  opt.alt_freq << " AltFreq\n";
    outfile <<  opt.gold_q << " GoldQ\n";
    outfile <<  opt.key_q << " KeyQ\n";
    outfile <<  opt.Version << " Version\n";
    outfile <<  opt.lag_in_frames << " LagInFrames\n";
    outfile <<  opt.two_pass_vbrbias << " TwoPassVBRBias\n";
    outfile <<  opt.two_pass_vbrmin_section << " TwoPassVBRMinSection\n";
    outfile <<  opt.two_pass_vbrmax_section << " TwoPassVBRMaxSection\n";
    outfile <<  opt.encode_breakout << " EncodeBreakout\n";
    outfile <<  opt.token_partitions  << " TokenPartitions\n";
    outfile <<  opt.multi_threaded << " MultiThreaded\n";
    outfile <<  opt.error_resilient_mode << " ErrorResilientMode\n";

    // not included in default settings file
    outfile << opt.Height << " Height\n";
    outfile << opt.Width << " Width\n";

    outfile << opt.cq_level << " CQLevel\n";
    outfile << opt.arnr_max_frames << " ArnrMaxFrames\n";
    outfile << opt.arnr_strength << " ArnrStr\n";
    outfile << opt.arnr_type << " ArnrType\n";
    outfile << opt.rc_max_intra_bitrate_pct << " rc_max_intra_bitrate_pct\n";

    outfile.close();
    return 0;
}
int vpxt_output_compatable_settings(const char *output_file,
                                    VP8_CONFIG opt,
                                    int ParVersionNum)
{
    // Saves all VP8_CONFIG settings to a settings file readable by
    // InputSettings Tester Uses prebuilt executables from prior builds

    std::ofstream outfile(output_file);

    outfile <<  opt.target_bandwidth << " TargetBandwidth\n";
    outfile <<  opt.noise_sensitivity << " NoiseSensitivity\n";
    outfile <<  opt.Sharpness << " Sharpness\n";
    outfile <<  opt.cpu_used << " CpuUsed\n";
    outfile <<  opt.Mode << " Mode\n";

    if (ParVersionNum == 1)
    {
        tprintf(PRINT_BTH, "\nRunning in 1.0.4 and earlier Parameter "
            "Compatability Mode\n");
        outfile <<  1 << " DeleteFirstPassFile\n";
    }

    outfile <<  opt.auto_key << " AutoKey\n";
    outfile <<  opt.key_freq << " KeyFreq\n";
    outfile <<  opt.end_usage << " EndUsage\n";
    outfile <<  opt.over_shoot_pct << " OverShootPct\n";
    outfile <<  opt.under_shoot_pct << " UnderShootPct\n";
    outfile <<  opt.starting_buffer_level << " StartingBufferLevel\n";
    outfile <<  opt.optimal_buffer_level << " OptimalBufferLevel\n";
    outfile <<  opt.maximum_buffer_size << " MaximumBufferSize\n";
    outfile <<  opt.fixed_q << " FixedQ\n";
    outfile <<  opt.worst_allowed_q << " WorstAllowedQ\n";
    outfile <<  opt.best_allowed_q << " BestAllowedQ\n";
    outfile <<  opt.allow_spatial_resampling << " AllowSpatialResampling\n";
    outfile <<  opt.resample_down_water_mark << " ResampleDownWaterMark\n";
    outfile <<  opt.resample_up_water_mark << " ResampleUpWaterMark\n";
    outfile <<  opt.allow_df << " AllowDF\n";
    outfile <<  opt.drop_frames_water_mark << " DropFramesWaterMark\n";
    outfile <<  opt.allow_lag << " AllowLag\n";
    outfile <<  opt.play_alternate << " PlayAlternate\n";
    outfile <<  opt.alt_q << " AltQ\n";
    outfile <<  opt.alt_freq << " AltFreq\n";
    outfile <<  opt.gold_q << " GoldQ\n";
    outfile <<  opt.key_q << " KeyQ\n";
    outfile <<  opt.Version << " Version\n";
    outfile <<  opt.lag_in_frames << " LagInFrames\n";
    outfile <<  opt.two_pass_vbrbias << " TwoPassVBRBias\n";
    outfile <<  opt.two_pass_vbrmin_section << " TwoPassVBRMinSection\n";
    outfile <<  opt.two_pass_vbrmax_section << " TwoPassVBRMaxSection\n";
    outfile <<  opt.encode_breakout << " EncodeBreakout\n";
    outfile <<  opt.token_partitions  << " TokenPartitions\n";
    outfile <<  opt.multi_threaded << " MultiThreaded\n";
    outfile <<  opt.error_resilient_mode << " ErrorResilientMode\n";

    // not included in default settings file
    if (ParVersionNum == 1)
    {
        std::string DummyFPFFile = output_file;
        DummyFPFFile += ".fpf";
        outfile << "Test" << " FirstPassFile\n";
    }

    outfile << opt.Height << " Height\n";
    outfile << opt.Width << " Width\n";
    outfile << opt.cq_level << " CQLevel\n";
    outfile << opt.arnr_max_frames << " ArnrMaxFrames\n";
    outfile << opt.arnr_strength << " ArnrStr\n";
    outfile << opt.arnr_type << " ArnrType\n";
    outfile << opt.rc_max_intra_bitrate_pct << " rc_max_intra_bitrate_pct\n";

    outfile.close();
    return 0;
}
int vpxt_output_settings_api(const char *output_file, vpx_codec_enc_cfg_t cfg)
{
    // Saves all vpx_codec_enc_cfg_t settings to a settings file

    std::ofstream outfile(output_file);

    outfile << cfg.g_usage << "\tg_usage\n";
    outfile << cfg.g_threads << "\tg_threads\n";
    outfile << cfg.g_profile << "\tg_profile\n";
    outfile << cfg.g_w << "\tg_w\n";
    outfile << cfg.g_h << "\tg_h\n";
    outfile << cfg.g_timebase.num << "\tcfg.g_timebase.num\n";
    outfile << cfg.g_timebase.den << "\tcfg.g_timebase.den\n";
    outfile << cfg.g_error_resilient << "\tg_error_resilient\n";
    outfile << cfg.g_pass << "\tg_pass\n";
    outfile << cfg.g_lag_in_frames << "\tg_lag_in_frames\n";
    outfile << cfg.rc_dropframe_thresh << "\trc_dropframe_thresh\n";
    outfile << cfg.rc_resize_allowed  << "\trc_resize_allowed\n";
    outfile << cfg.rc_resize_up_thresh << "\trc_resize_up_thresh\n";
    outfile << cfg.rc_resize_down_thresh << "\trc_resize_down_thresh\n";
    outfile << cfg.rc_end_usage << "\trc_end_usage\n";
    outfile << cfg.rc_target_bitrate << "\trc_target_bitrate\n";
    outfile << cfg.rc_min_quantizer << "\trc_min_quantizer\n";
    outfile << cfg.rc_max_quantizer << "\trc_max_quantizer\n";
    outfile << cfg.rc_undershoot_pct  << "\trc_undershoot_pct\n";
    outfile << cfg.rc_overshoot_pct  << "\trc_overshoot_pct\n";
    outfile << cfg.rc_buf_sz << "\trc_buf_sz\n";
    outfile << cfg.rc_buf_initial_sz  << "\trc_buf_initial_sz \n";
    outfile << cfg.rc_buf_optimal_sz << "\trc_buf_optimal_sz\n";
    outfile << cfg.rc_2pass_vbr_bias_pct << "\trc_2pass_vbr_bias_pct\n";
    outfile << cfg.rc_2pass_vbr_minsection_pct
        << "\trc_2pass_vbr_minsection_pct\n";
    outfile << cfg.rc_2pass_vbr_maxsection_pct
        << "\trc_2pass_vbr_maxsection_pct\n";
    outfile << cfg.kf_mode << "\tkf_mode\n";
    outfile << cfg.kf_min_dist << "\tkf_min_dist\n";
    outfile << cfg.kf_max_dist << "\tkf_max_dist\n";

    outfile.close();
    return 0;
}

int vpxt_input_settings_api(const char *input_file, vpx_codec_enc_cfg_t &cfg)
{
    // Saves all vpx_codec_enc_cfg_t settings to a settings file

    std::ifstream infile(input_file);
    std::string Garbage;
    int IntValue;

    infile >> cfg.g_usage;
    infile >> Garbage;
    infile >> cfg.g_threads;
    infile >> Garbage;
    infile >> cfg.g_profile;
    infile >> Garbage;
    infile >> cfg.g_w;
    infile >> Garbage;
    infile >> cfg.g_h;
    infile >> Garbage;
    infile >> cfg.g_timebase.num;
    infile >> Garbage;
    infile >> cfg.g_timebase.den;
    infile >> Garbage;
    infile >> cfg.g_error_resilient;
    infile >> Garbage;
    infile >> IntValue;

    if (IntValue == 0)
        cfg.g_pass = VPX_RC_ONE_PASS;

    if (IntValue == 1)
        cfg.g_pass = VPX_RC_FIRST_PASS;

    if (IntValue == 2)
        cfg.g_pass = VPX_RC_LAST_PASS;

    infile >> Garbage;
    infile >> cfg.g_lag_in_frames;
    infile >> Garbage;
    infile >> cfg.rc_dropframe_thresh;
    infile >> Garbage;
    infile >> cfg.rc_resize_allowed;
    infile >> Garbage;
    infile >> cfg.rc_resize_up_thresh;
    infile >> Garbage;
    infile >> cfg.rc_resize_down_thresh;
    infile >> Garbage;
    infile >> IntValue;

    if (IntValue == 0)
        cfg.rc_end_usage = VPX_VBR;

    if (IntValue == 1)
        cfg.rc_end_usage = VPX_CBR;

    infile >> Garbage;
    infile >> cfg.rc_target_bitrate;
    infile >> Garbage;
    infile >> cfg.rc_min_quantizer;
    infile >> Garbage;
    infile >> cfg.rc_max_quantizer;
    infile >> Garbage;
    infile >> cfg.rc_undershoot_pct;
    infile >> Garbage;
    infile >> cfg.rc_overshoot_pct;
    infile >> Garbage;
    infile >> cfg.rc_buf_sz;
    infile >> Garbage;
    infile >> cfg.rc_buf_initial_sz;
    infile >> Garbage;
    infile >> cfg.rc_buf_optimal_sz;
    infile >> Garbage;
    infile >> cfg.rc_2pass_vbr_bias_pct;
    infile >> Garbage;
    infile >> cfg.rc_2pass_vbr_minsection_pct;
    infile >> Garbage;
    infile >> cfg.rc_2pass_vbr_maxsection_pct;
    infile >> Garbage;
    infile >> IntValue;

    if (IntValue == 0)
        cfg.kf_mode = VPX_KF_FIXED;

    if (IntValue == 1)
        cfg.kf_mode = VPX_KF_AUTO;

    infile >> Garbage;
    infile >> cfg.kf_min_dist;
    infile >> Garbage;
    infile >> cfg.kf_max_dist;
    infile >> Garbage;

    infile.close();
    return 0;
}
int vpxt_output_settings_ivfenc(const char *output_file, VP8_CONFIG opt)
{
    // Saves all VP8_CONFIG settings to a settings file readable by InputSettings

    std::ofstream outfile(output_file);

    outfile << "TargetBandwidth " << opt.target_bandwidth << "\n";
    outfile << "NoiseSensitivity " << opt.noise_sensitivity <<  "\n";
    outfile << "Sharpness " << opt.Sharpness <<  "\n";
    outfile << "CpuUsed " << opt.cpu_used <<  "\n";
    outfile << "Mode " << opt.Mode <<  "\n";
    outfile << "AutoKey " << opt.auto_key <<  "\n";
    outfile << "KeyFreq " << opt.key_freq <<  "\n";
    outfile << "EndUsage " << opt.end_usage <<  "\n";
    outfile << "UnderShootPct " << opt.under_shoot_pct <<  "\n";
    outfile << "StartingBufferLevel " << opt.starting_buffer_level <<  "\n";
    outfile << "OptimalBufferLevel " << opt.optimal_buffer_level <<  "\n";
    outfile << "MaximumBufferSize " << opt.maximum_buffer_size <<  "\n";
    outfile << "FixedQ " << opt.fixed_q <<  "\n";
    outfile << "WorstAllowedQ " << opt.worst_allowed_q <<  "\n";
    outfile << "BestAllowedQ " << opt.best_allowed_q <<  "\n";
    outfile << " llowSpatialResampling " << opt.allow_spatial_resampling
        <<  "\n";
    outfile << "ResampleDownWaterMark " << opt.resample_down_water_mark
        <<  "\n";
    outfile << "ResampleUpWaterMark " << opt.resample_up_water_mark <<  "\n";
    outfile << "AllowDF " << opt.allow_df << "\n";
    outfile << "DropFramesWaterMark " << opt.drop_frames_water_mark <<  "\n";
    outfile << "TwoPassVBRBias " << opt.two_pass_vbrbias <<  "\n";
    outfile << "TwoPassVBRMinSection " << opt.two_pass_vbrmin_section <<  "\n";
    outfile << "TwoPassVBRMaxSection " << opt.two_pass_vbrmax_section <<  "\n";
    outfile << "AllowLag " << opt.allow_lag <<  "\n";
    outfile << "LagInFrames " << opt.lag_in_frames <<  "\n";
    outfile << "AltFreq " << opt.alt_freq <<  "\n";
    outfile << "KeyQ " << opt.key_q <<  "\n";
    outfile << "AltQ " << opt.alt_q <<  "\n";
    outfile << "GoldQ " << opt.gold_q <<  "\n";
    outfile << "PlayAlternate " << opt.play_alternate <<  "\n";
    outfile << "Version " << opt.Version <<  "\n";
    outfile << "EncodeBreakout " << opt.encode_breakout <<  "\n";
    outfile << "rc_max_intra_bitrate_pct " << opt.rc_max_intra_bitrate_pct
        << "\n";

    outfile.close();
    return 0;
}
int vpxt_convert_par_file_to_ivfenc(const char *input, const char *output)
{

    VP8_CONFIG opt;
    vpxt_default_parameters(opt);

    opt = vpxt_input_settings(input);
    vpxt_output_settings_ivfenc(output, opt);

    return 0;
}
int vpxt_convert_par_file_to_vpxenc(const char *input_core,
                                    const char *input_api,
                                    char *vpxenc_parameters,
                                    int vpxenc_parameters_sz)
{
    VP8_CONFIG opt;
    opt = vpxt_input_settings(input_core);

    vpx_codec_enc_cfg_t    cfg;
    const struct codec_item  *codec = codecs;
    vpx_codec_enc_config_default(codec->iface, &cfg, 0);
    vpxt_input_settings_api(input_api, cfg);

    int endofstr = 0;

    //--debug                         // Debug mode (makes output deterministic)
    //--output=<arg>                  // Output filename
    //--codec=<arg>                   // Codec to use
    if (opt.Mode > 2)
        endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--passes=2 ");               // Number of passes (1/2)

    //--pass=<arg>                    // Pass to execute (1/2)
    //--fpf=<arg>                     // First pass statistics file name
    //--limit=<arg>                   // Stop encoding after n input frames
    //--deadline=<arg>                // Deadline per frame (usec)
    if (opt.Mode == kOnePassBestQuality || opt.Mode == kTwoPassBestQuality)
        endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--best ");                   // Use Best Quality Deadline

    if (opt.Mode == kOnePassGoodQuality || opt.Mode == kTwoPassGoodQuality)
        endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--good ");                   // Use Good Quality Deadline

    if (opt.Mode == kRealTime)
        endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--rt ");                     // Use Realtime Quality Deadline

    endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--verbose ");                // Show encoder parameters
    //--psnr                          // Show PSNR in status line

    // Encoder Global Options:
    //--yv12                          // Input file is YV12
    //--i420                          // Input file is I420 (default)
    endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--usage=%i ", cfg.g_usage);  // Usage profile number to use
    endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--threads=%i ", opt.multi_threaded); // Max number of threads to use
    endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--profile=%i ", opt.Version); // Bitstream profile number to use
    endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--width=%i ", opt.Width);     // Frame width
    endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--height=%i ", opt.Height);   // Frame height
    endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--timebase=%i/%i ", cfg.g_timebase.num, cfg.g_timebase.den);
                                       // Stream timebase (frame duration)
    endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--fps=%i/%i ", cfg.g_timebase.den, cfg.g_timebase.num);
                                       // Stream frame rate (rate/scale)
    endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--error-resilient=%i ", opt.error_resilient_mode);
                                       // Enable error resiliency features

    if (opt.allow_lag == 0)
        endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--lag-in-frames=%i ", 0);     // Max number of frames to lag
    else
        endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--lag-in-frames=%i ", opt.lag_in_frames); // Max num frames to lag

    // Rate Control Options:
    if (opt.allow_df == 0)
        endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--drop-frame=%i ", 0);        // Temporal resampling threshold (buf %)
    else
        endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--drop-frame=%i ", opt.drop_frames_water_mark);
                                       // Temporal resampling threshold (buf %)

    endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--resize-allowed=%i ", opt.allow_spatial_resampling);
                                      // Spatial resampling enabled (bool)
    endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--resize-up=%i ", opt.resample_up_water_mark);
                                      // Upscale threshold (buf %)
    endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--resize-down=%i ", opt.resample_down_water_mark);
                                      // Downscale threshold (buf %)

    if (opt.end_usage == USAGE_LOCAL_FILE_PLAYBACK)
        endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--end-usage=%i ", VPX_VBR);  // VBR=0 | CBR=1

    if (opt.end_usage == USAGE_STREAM_FROM_SERVER)
        endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--end-usage=%i ", VPX_CBR);  // VBR=0 | CBR=1

    if (opt.end_usage == USAGE_CONSTRAINED_QUALITY)
        endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--end-usage=%i ", VPX_CQ);   // VBR=0 | CBR=1

    endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--target-bitrate=%i ", opt.target_bandwidth); // Bitrate (kbps)
    endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--min-q=%i ", opt.best_allowed_q);           // Min(best) quantizer
    endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--max-q=%i ", opt.worst_allowed_q);         // Max(worst) quantizer

    if (opt.fixed_q != -1)
    {
        endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
            "--min-q=%i ", opt.fixed_q); // Minimum (best) quantizer
        endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
            "--max-q=%i ", opt.fixed_q); // Maximum (worst) quantizer
    }

    endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--undershoot-pct=%i ", opt.under_shoot_pct);
                                      // Datarate undershoot (min) target (%)
    endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--overshoot-pct=%i ", cfg.rc_overshoot_pct);
                                      // Datarate overshoot (max) target (%)
    endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--buf-sz=%i ", opt.maximum_buffer_size); //Client buffer size (ms)
    endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--buf-initial-sz=%i ", opt.starting_buffer_level);
                                      // Client initial buffer size (ms)
    endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--buf-optimal-sz=%i ", opt.optimal_buffer_level);
                                      // Client optimal buffer size (ms)

    // Twopass Rate Control Options:
    endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--bias-pct=%i ", opt.two_pass_vbrbias); // CBR/VBR bias(0=CBR, 100=VBR)
    endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--minsection-pct=%i ", opt.two_pass_vbrmin_section);
                                      // GOP min bitrate (% of target)
    endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--maxsection-pct=%i ", opt.two_pass_vbrmax_section);
                                      //GOP max bitrate (% of target)

    //Keyframe Placement Options:
    endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--kf-min-dist=%i ", cfg.kf_min_dist);
                                      // Minimum keyframe interval (frames)
    endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--kf-max-dist=%i ", opt.key_freq); // Maximum keyframe interval(frames)

    if (opt.auto_key == 0)
        endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--disable-kf ");             // Disable keyframe placement

    // VP8 Specific Options:
    endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--cpu-used=%i ", opt.cpu_used); // CPU Used (-16..16)
    endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--auto-alt-ref=%i ", opt.play_alternate);
                                      // Enable automatic alt reference frames
    endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--noise-sensitivity=%i ", opt.noise_sensitivity);
                                      // Noise sensitivity (frames to blur)
    endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--sharpness=%i ", opt.Sharpness); // Filter sharpness (0-7)
    endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--static-thresh=%i ", opt.encode_breakout);
                                      // Motion detection threshold
    endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--token-parts=%i ", opt.token_partitions);
                                      // Number of token partitions to use, log2
    endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--arnr-maxframes=%i ", opt.arnr_max_frames);  // AltRef Max Frames
    endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--arnr-strength=%i ", opt.arnr_strength);     // AltRef Strength
    endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--arnr-type=%i ", opt.arnr_type);             // AltRef Type
    //--tune=<arg>                    // Material to favor - psnr, ssim
    endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--cq-level=%i ", opt.cq_level);
    endofstr += snprintf(vpxenc_parameters + endofstr, vpxenc_parameters_sz,
        "--max-intra-rate=%i ", opt.rc_max_intra_bitrate_pct);

    return 0;
}
int vpxt_file_is_yv12(const char *in_fn)
{
    FILE* infile = strcmp(in_fn, "-") ? fopen(in_fn, "rb") :
        set_binary_mode(stdin);

    if (!infile)
    {
        tprintf(PRINT_BTH, "Failed to open input file: %s", in_fn);
        return -1;
    }

    struct detect_buffer detect;
    detect.buf_read = fread(detect.buf, 1, 4, infile);
    detect.position = 0;
    unsigned int             file_type, fourcc;
    y4m_input                y4m;

    unsigned int rate;
    unsigned int scale;
    unsigned int width = 0;
    unsigned int height = 0;

    if (detect.buf_read == 4 && file_is_y4m(infile, &y4m, detect.buf))
    {
        if (y4m_input_open(&y4m, infile, detect.buf, 4) >= 0)
        {
            file_type = FILE_TYPE_Y4M;
        }
        else
        {
            fprintf(stderr, "Unsupported Y4M stream.\n");
            return EXIT_FAILURE;
        }
    }
    else if (detect.buf_read == 4 && file_is_ivf(infile, &fourcc, &width,
        &height, &detect, &scale, &rate))
    {
        file_type = FILE_TYPE_IVF;
    }
    else
    {
        file_type = FILE_TYPE_RAW;
    }

    fclose(infile);

    switch (fourcc)
    {
    case 0x32315659:
        return 1;
    case 0x30323449:
        return 0;
    default:
        fprintf(stderr, "Unsupported fourcc (%08x)\n", fourcc);
    }

    return 0;
}
// ---------------------------IVF Header Data-----------------------------------
int vpxt_print_ivf_file_header(IVF_HEADER ivf)
{
    tprintf(PRINT_STD, "IVF FIle Header Data\n\n"
            "FILE HEADER \n\n"
            "File Header            - %c%c%c%c \n"
            "File Format Version    - %i \n"
            "File Header Size       - %i \n"
            "Video Data FourCC      - %i \n"
            "Video Image Width      - %i \n"
            "Video Image Height     - %i \n"
            "Frame Rate Rate        - %i \n"
            "Frame Rate Scale       - %i \n"
            "Video Length in Frames - %i \n"
            "Unused                 - %c \n"
            "\n\n"
            , ivf.signature[0], ivf.signature[1], ivf.signature[2],
            ivf.signature[3], ivf.version, ivf.headersize, ivf.four_cc,
            ivf.width, ivf.height, ivf.rate, ivf.scale, ivf.length, ivf.unused);
    return 0;
}
int vpxt_format_ivf_header_read(IVF_HEADER *ivf)
{
#ifdef _PPC
    // std::cout << "\n\n\n\n\nPPC DEFINED\n\n\n\n\n";
    // For big endian systems need to swap bytes on height and width
    ivf->width  = ((ivf->width & 0xff) << 8)  | ((ivf->width >> 8) & 0xff);
    ivf->height = ((ivf->height & 0xff) << 8) | ((ivf->height >> 8) & 0xff);
    ivf->rate = (((ivf->rate & 0xff)) << 24)    | (((ivf->rate & 0xff00)) << 8) |   \
                (((ivf->rate & 0xff0000)) >> 8) | (((ivf->rate & 0xff000000)) >> 24);
    ivf->scale = (((ivf->scale & 0xff)) << 24)    | (((ivf->scale & 0xff00)) << 8) |    \
                 (((ivf->scale & 0xff0000)) >> 8) | (((ivf->scale & 0xff000000)) >> 24);
    ivf->length = (((ivf->length & 0xff)) << 24)    | (((ivf->length & 0xff00)) << 8) | \
                  (((ivf->length & 0xff0000)) >> 8) | (((ivf->length & 0xff000000)) >> 24);
#endif

    return 0;
}
int vpxt_format_ivf_header_write(IVF_HEADER &ivf)
{
    ivf.version = 0;
    ivf.headersize = make_endian_16(32);

    ivf.width      = make_endian_16(ivf.width);
    ivf.height     = make_endian_16(ivf.height);
    ivf.scale      = make_endian_32(ivf.scale);
    ivf.rate       = make_endian_32(ivf.rate);
    ivf.length     = make_endian_32(ivf.length);

    if (ivf.four_cc != 842094169 && ivf.four_cc != 808596553)
    {
        // tprintf(  PRINT_STD, "\nUtil_FourCC: %i",ivf.four_cc);
        // tprintf(  PRINT_STD, "\nini vp8 fourcc\n");
        ivf.four_cc     = MAKEFOURCC('V', 'P', '8', '0');
    }

    return 0;
}
int vpxt_format_frame_header_read(IVF_FRAME_HEADER &ivf_fh)
{
#ifdef _PPC
    ivf_fh.frameSize = ((ivf_fh.frameSize & 0xff) << 24) |
                       ((ivf_fh.frameSize & 0xff00) << 8) |
                       ((ivf_fh.frameSize & 0xff0000) >> 8) |
                       ((ivf_fh.frameSize & 0xff000000) >> 24);
    ivf_fh.timeStamp = ((ivf_fh.timeStamp & 0xff) << 56         |   \
                        ((ivf_fh.timeStamp >>  8) & 0xff) << 48 |   \
                        ((ivf_fh.timeStamp >> 16) & 0xff) << 40 |   \
                        ((ivf_fh.timeStamp >> 24) & 0xff) << 32 |   \
                        ((ivf_fh.timeStamp >> 32) & 0xff) << 24 |   \
                        ((ivf_fh.timeStamp >> 40) & 0xff) << 16 |   \
                        ((ivf_fh.timeStamp >> 48) & 0xff) <<  8 |   \
                        ((ivf_fh.timeStamp >> 56) & 0xff));

    // std::cout << "POWERPC-Read\n";
#endif

    return 0;
}
int vpxt_format_frame_header_write(IVF_FRAME_HEADER &ivf_fh)
{
#ifdef _PPC
    ivf_fh.timeStamp = make_endian_64(ivf_fh.timeStamp);
    ivf_fh.frameSize = make_endian_32(ivf_fh.frameSize);
#endif

    return 0;
}
// -----------------------File Management---------------------------------------
long vpxt_file_size(const char *inFile, int printbool)
{
    // finds and returns the size of a file with output.
    char FileNameinFile[256];
    vpxt_file_name(inFile, FileNameinFile, 0);

    if (printbool)
        tprintf(PRINT_BTH, "Size of %s: ", FileNameinFile);

    long pos;
    long end;

    FILE *f;
    f = fopen(inFile , "r");

    pos = ftell(f);
    fseek(f, 0, SEEK_END);
    end = ftell(f);
    fseek(f, pos, SEEK_SET);

    if (printbool)
        tprintf(PRINT_BTH, "%i bytes", end - pos);

    fclose(f);

    return end - pos;
}
void vpxt_file_name(const char *input, char *FileName, int removeExt)
{
    // Extracts only the files name from its full path.

    int parser = 0;
    int slashcount = 0;
    int slashcount2 = 0;

    while (input[parser] != '\0')
    {
        if (input[parser] == slashChar())
        {
            slashcount++;
        }

        parser++;
    }

    parser = 0;
    int parser2 = 0;

    while (input[parser] != '\0')
    {

        if (slashcount2 > slashcount - 1)
        {
            FileName[parser2] = input[parser];
            parser2++;
        }

        if (input[parser] == slashChar())
        {
            slashcount2++;
        }

        parser++;
    }

    if (removeExt == 0)FileName[parser2] = '\0';

    if (removeExt == 1)FileName[parser2-4] = '\0';

    return;
}


void vpxt_folder_name(const char *input, std::string *output_str)
{
    // Gets the full name of the folder a file is in and returns it.

    output_str->clear();
    int parser = 0;
    int slashcount = 0;
    int slashcount2 = 0;
    const char *Dir = input;

    while (Dir[parser] != '\0')
    {
        if (Dir[parser] == slashChar())
        {
            slashcount++;
        }

        parser++;
    }

    parser = 0;

    while (slashcount2 < slashcount)
    {
        if (Dir[parser] == slashChar())
        {
            slashcount2++;
        }

        *output_str += Dir[parser];
        parser++;
    }

    return;
}

int  vpxt_get_number_of_frames(const char *input_file)
{
    // Get frame count from y4m ivf and or webm return -1 on error

    int length = 0;
    int use_y4m = 1;
    uint8_t               *buf = NULL;
    size_t               buf_sz = 0, buf_alloc_sz = 0;
    FILE                  *infile;

    unsigned int           fourcc;
    unsigned int            width;
    unsigned int            height;
    unsigned int            fps_den;
    unsigned int            fps_num;

    struct input_ctx        input;
    struct vpx_rational      arg_framerate = {30, 1};
    int                      arg_use_i420 = 1;

    input.chunk = 0;
    input.chunks = 0;
    input.infile = NULL;
    input.kind = RAW_FILE;
    input.nestegg_ctx = 0;
    input.pkt = 0;
    input.video_track = 0;

    int CharCount = 0;
    int                      arg_have_framerate = 0;

    /* Open file */
    infile = strcmp(input_file, "-") ? fopen(input_file, "rb") :
        set_binary_mode(stdin);

    if (!infile)
    {
        tprintf(PRINT_BTH, "Failed to open input file: %s", input_file);
        return -1;
    }

    input.infile = infile;

    if (file_is_webm(&input, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = WEBM_FILE;

    if (input.kind == WEBM_FILE)
    {
        if (webm_guess_framerate(&input, &fps_den, &fps_num))
        {
            fprintf(stderr, "Failed to guess framerate -- error parsing "
                    "webm file?\n");
            return EXIT_FAILURE;
        }

        uint64_t timestamp = 0;
        while (!skim_frame_dec(&input, &buf, &buf_sz, &buf_alloc_sz,
            &timestamp))
            length++;

        // printf("\nWebm Length: %i\n",length);
        fclose(infile);

        if (input.nestegg_ctx)
            nestegg_destroy(input.nestegg_ctx);

        if (input.kind != WEBM_FILE)
            free(buf);

        return length;
    }

    if (input.nestegg_ctx)
        nestegg_destroy(input.nestegg_ctx);

    if (input.kind != WEBM_FILE)
        free(buf);

    y4m_input y4m;
    unsigned int file_type;
    unsigned long nbytes = 0;
    struct detect_buffer detect;

    unsigned int rate;
    unsigned int scale;

    detect.buf_read = fread(detect.buf, 1, 4, infile);
    detect.position = 0;

    if (detect.buf_read == 4 && file_is_y4m(infile, &y4m, detect.buf))
    {
        if (y4m_input_open(&y4m, infile, detect.buf, 4) >= 0)
        {
            file_type = FILE_TYPE_Y4M;
            width = y4m.pic_w;
            height = y4m.pic_h;

            /* Use the frame rate from the file only if none was specified
            * on the command-line.
            */
            if (!arg_have_framerate)
            {
                arg_framerate.num = y4m.fps_n;
                arg_framerate.den = y4m.fps_d;
            }

            arg_use_i420 = 0;
        }
        else
        {
            fprintf(stderr, "Unsupported Y4M stream.\n");
            return EXIT_FAILURE;
        }
    }
    else if (detect.buf_read == 4 && file_is_ivf(infile, &fourcc, &width,
        &height, &detect, &scale, &rate))
    {
        file_type = FILE_TYPE_IVF;
    }

    if (file_type == FILE_TYPE_IVF)
    {
        rewind(infile);
        IVF_HEADER ivf_h_raw;
        InitIVFHeader(&ivf_h_raw);
        fread(&ivf_h_raw, 1, sizeof(ivf_h_raw), infile);

        // printf("\nIVF Length: %i\n",ivf_h_raw.length);

        fclose(infile);
        return ivf_h_raw.length;
    }

    vpx_image_t raw;
    vpx_img_alloc(&raw, VPX_IMG_FMT_I420, width, height, 1);

    if (file_type == FILE_TYPE_Y4M)
    {

        int frame_avail = 1;

        while (frame_avail)
        {
            frame_avail = skim_frame_enc(infile, &raw, file_type, &y4m,
                &detect);

            if (!frame_avail)
                break;

            length++;
        }

        // printf("\nY4M Length: %i\n",length);
        fclose(infile);

        vpx_img_free(&raw);
        return length;
    }

    fclose(infile);

    if (input.nestegg_ctx)
        nestegg_destroy(input.nestegg_ctx);

    if (input.kind != WEBM_FILE)
        free(buf);

    vpx_img_free(&raw);

    return -1;
}
int  vpxt_get_multi_res_width_height(const char *input_file,
                                     int FileNumber,
                                     unsigned int &width,
                                     unsigned int &height)
{
    unsigned char  signature[4];    // ='DKIF';
    unsigned short version = 0;     //  -
    unsigned short headersize = 0;  //  -
    unsigned int fourcc = 0;        // good
    unsigned int rate = 0;          // good
    unsigned int scale = 0;         // good
    unsigned int length = 0;        // other measure

    signature[0] = ' ';
    signature[1] = ' ';
    signature[2] = ' ';
    signature[3] = ' ';

    int frames_in = 0, frames_out = 0;
    unsigned long nbytes = 0;
    struct detect_buffer detect;
    y4m_input                y4m;
    unsigned int             file_type;
    vpx_codec_enc_cfg_t    cfg;
    int                      arg_have_framerate = 0;
    int                      arg_use_i420 = 1;
    struct vpx_rational      arg_framerate = {30, 1};

    FILE *infile = strcmp(input_file, "-") ? fopen(input_file, "rb") :
        set_binary_mode(stdin);

    if (!infile)
    {
        tprintf(PRINT_BTH, "Failed to open input file: %s", input_file);
        return -1;
    }

    detect.buf_read = fread(detect.buf, 1, 4, infile);
    detect.position = 0;

    if (detect.buf_read == 4 && file_is_y4m(infile, &y4m, detect.buf))
    {
        if (y4m_input_open(&y4m, infile, detect.buf, 4) >= 0)
        {
            file_type = FILE_TYPE_Y4M;
            cfg.g_w = y4m.pic_w;
            cfg.g_h = y4m.pic_h;

            /* Use the frame rate from the file only if none was specified
            * on the command-line.
            */
            if (!arg_have_framerate)
            {
                arg_framerate.num = y4m.fps_n;
                arg_framerate.den = y4m.fps_d;
            }

            arg_use_i420 = 0;
        }
        else
        {
            fprintf(stderr, "Unsupported Y4M stream.\n");
            return EXIT_FAILURE;
        }
    }
    else if (detect.buf_read == 4 && file_is_ivf(infile, &fourcc, &cfg.g_w,
        &cfg.g_h, &detect, &scale, &rate))
    {
        file_type = FILE_TYPE_IVF;

        switch (fourcc)
        {
        case 0x32315659:
            arg_use_i420 = 0;
            break;
        case 0x30323449:
            arg_use_i420 = 1;
            break;
        default:
            fprintf(stderr, "Unsupported fourcc (%08x) in IVF\n", fourcc);
            return EXIT_FAILURE;
        }
    }
    else
    {
        file_type = FILE_TYPE_RAW;
    }

    if (!cfg.g_w || !cfg.g_h)
    {
        fprintf(stderr, "Specify stream dimensions with --width (-w) "
            " and --height (-h).\n");
        return EXIT_FAILURE;
    }

    int i;
    int starting_width[NUM_ENCODERS];
    int starting_height[NUM_ENCODERS];

    width = cfg.g_w;
    height = cfg.g_h;

    for (i=0; i < NUM_ENCODERS; i++)
    {
        starting_width[i] = width;
        starting_height[i] = height;
    }

    vpx_rational_t dsf[NUM_ENCODERS] = {{2, 1}, {2, 1}, {1, 1}};

    for (i=1; i< NUM_ENCODERS; i++)
    {
        /* Note: Width & height of other-resolution encoders are calculated
        * from the highest-resolution encoder's size and the corresponding
        * down_sampling_factor.
        */
        {
            unsigned int iw = starting_width[i-1]*dsf[i-1].den + dsf[i-1].num
                - 1;
            unsigned int ih = starting_height[i-1]*dsf[i-1].den + dsf[i-1].num
                - 1;
            starting_width[i] = iw/dsf[i-1].num;
            starting_height[i] = ih/dsf[i-1].num;
        }

        if((starting_width[i])%2)starting_width[i]++;
        if((starting_height[i])%2)starting_height[i]++;
    }

    for (i=0; i< FileNumber; i++)
    {
        // printf("width[%i]: %i\n", i, starting_width[i]);
        // printf("height[%i]: %i\n", i, starting_height[i]);

        width = starting_width[i];
        height = starting_height[i];
    }

    // printf("width[%i]: %i\n", FileNumber, width);
    // printf("height[%i]: %i\n", FileNumber, height);

    fclose(infile);

    if(file_type == FILE_TYPE_Y4M)
        y4m_input_close(&y4m);

    return 0;
}
int  vpxt_raw_file_size(const char *input_file)
{
    // Get frame count from y4m ivf and or webm return -1 on error

    int size = 0;
    int use_y4m = 1;
    uint8_t               *buf = NULL;
    size_t               buf_sz = 0, buf_alloc_sz = 0;
    FILE                  *infile;

    unsigned int           fourcc;
    unsigned int            width;
    unsigned int            height;
    unsigned int            fps_den;
    unsigned int            fps_num;

    struct input_ctx        input;

    input.chunk = 0;
    input.chunks = 0;
    input.infile = NULL;
    input.kind = RAW_FILE;
    input.nestegg_ctx = 0;
    input.pkt = 0;
    input.video_track = 0;

    int CharCount = 0;

    /* Open file */
    infile = strcmp(input_file, "-") ? fopen(input_file, "rb") :
        set_binary_mode(stdin);

    if (!infile)
    {
        tprintf(PRINT_BTH, "Failed to open input file: %s", input_file);
        return -1;
    }

    input.infile = infile;

    if (file_is_webm(&input, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = WEBM_FILE;

    if (input.kind == WEBM_FILE)
    {
        if (webm_guess_framerate(&input, &fps_den, &fps_num))
        {
            fprintf(stderr, "Failed to guess framerate -- error parsing "
                    "webm file?\n");
            return EXIT_FAILURE;
        }

        uint64_t timestamp = 0;
        while (!skim_frame_dec(&input, &buf, &buf_sz, &buf_alloc_sz,
            &timestamp))
            size = size + buf_sz;

        // printf("\nWebm size: %i\n",size);
        fclose(infile);
        return size;
    }

    y4m_input y4m;
    unsigned int file_type;
    unsigned long nbytes = 0;
    struct detect_buffer detect;

    unsigned int rate;
    unsigned int scale;

    detect.buf_read = fread(detect.buf, 1, 4, infile);
    detect.position = 0;

    if (detect.buf_read == 4 && file_is_y4m(infile, &y4m, detect.buf))
    {
        if (y4m_input_open(&y4m, infile, detect.buf, 4) >= 0)
        {
            file_type = FILE_TYPE_Y4M;
            width = y4m.pic_w;
            height = y4m.pic_h;

            /* Use the frame rate from the file only if none was specified
            * on the command-line.
            */
            if (1)
            {
                rate = y4m.fps_n;
                scale = y4m.fps_d;
            }

            // arg_use_i420 = 0;
        }
        else
        {
            fprintf(stderr, "Unsupported Y4M stream.\n");
            return EXIT_FAILURE;
        }
    }
    else if (detect.buf_read == 4 && file_is_ivf(infile, &fourcc, &width,
        &height, &detect, &scale, &rate))
    {
        file_type = FILE_TYPE_IVF;
    }

    if (file_type == FILE_TYPE_IVF)
    {
        rewind(infile);
        IVF_HEADER ivf_h_raw;
        InitIVFHeader(&ivf_h_raw);
        fread(&ivf_h_raw, 1, sizeof(ivf_h_raw), infile);

        // printf("\nIVF Length: %i\n",ivf_h_raw.length);

        while (1)
        {
            IVF_FRAME_HEADER ivf_fhRaw;
            fread(&ivf_fhRaw.frameSize, 1, 4, infile);

            if (feof(infile))
                break;

            fread(&ivf_fhRaw.timeStamp, 1, 8, infile);
            vpxt_format_frame_header_read(ivf_fhRaw);
            fseek(infile, ivf_fhRaw.frameSize, SEEK_CUR);
            size = size + ivf_fhRaw.frameSize;
        }

        // printf("\nIVF Size: %i\n",size);

        fclose(infile);
        return size;
    }

    vpx_image_t raw;
    vpx_img_alloc(&raw, VPX_IMG_FMT_I420, width, height, 1);

    if (file_type == FILE_TYPE_Y4M)
    {
        int frame_avail = 1;

        while (frame_avail)
        {
            frame_avail = skim_frame_enc(infile, &raw, file_type, &y4m,
                &detect);

            if (!frame_avail)
                break;

            size = size + (width * height * 3 / 2);
        }

        // printf("\nY4M Size: %i\n",size);
        fclose(infile);
        vpx_img_free(&raw);
        y4m_input_close(&y4m);

        return size;
    }

    fclose(infile);
    vpx_img_free(&raw);

    if (file_type == FILE_TYPE_Y4M)
        y4m_input_close(&y4m);

    return -1;
}
int vpxt_remove_file_extension(const char *In, std::string &Out)
{
    // Takes in the full name of a file and writes the directory and file name
    // (without its extention) to the second input.
    // return extension length on success
    int parser = 0;
    int lastDot = 0;

    while (In[parser] != '\0')
    {
        if (In[parser] == '.')
            lastDot = parser;

        parser++;
    }

    Out = In;

    if (lastDot > 0)
        Out.erase(lastDot, parser - lastDot);

    Out += "_";

    // printf("\nOutput: %s\n",Out.c_str());

    return parser - lastDot;
}

int vpxt_get_file_extension(const char *In, std::string &Out)
{
    // Takes in the full name of a file and write the extention
    // to out return value is size of ext
    int parser = 0;
    int lastDot = 0;

    while (In[parser] != '\0')
    {
        if (In[parser] == '.')
            lastDot = parser;

        parser++;
    }

    Out = In;

    if (lastDot > 0)
        Out.erase(0, lastDot);

    return Out.length();
}

int vpxt_enc_format_append(std::string &InputString, std::string EncFormat)
{
    vpxt_lower_case_string(EncFormat);

    if (EncFormat.compare("ivf") == 0)
        InputString += ".ivf";
    else
        InputString += ".webm";

    return 0;
}
int vpxt_dec_format_append(std::string &InputString, std::string DecFormat)
{
    vpxt_lower_case_string(DecFormat);

    if (DecFormat.compare("ivf") == 0)
        InputString += ".ivf";
    else
        InputString += ".y4m";

    return 0;
}
std::string vpxt_extract_date_time(const std::string InputStr)
{
    // Extracts only the files name from its full path.

#if defined(_WIN32)
    enum { len = 255 };
    char input[len];
    const errno_t e = strcpy_s(input, len, InputStr.c_str());
    assert(e == 0);
#elif defined(linux)
    char input[255];
    snprintf(input, 255, "%s", InputStr.c_str());
#elif defined(__APPLE__)
    char input[255];
    snprintf(input, 255, "%s", InputStr.c_str());
#elif defined(_PPC)
    char input[255];
    snprintf(input, 255, "%s", InputStr.c_str());
#endif

    int parser = 0;
    int slashcount = 0;
    int slashcount2 = 0;
    char  FileName[255];

    while (input[parser] != '\0')
    {
        if (input[parser] == slashChar())
        {
            slashcount++;
        }

        parser++;
    }

    parser = 0;
    int parser2 = 0;

    while (input[parser] != '\0')
    {

        if (slashcount2 > slashcount - 1)
        {
            FileName[parser2] = input[parser];
            parser2++;
        }

        if (input[parser] == slashChar())
        {
            slashcount2++;
        }

        parser++;
    }

    FileName[parser2] = '\0';
    std::string FileNameStr = FileName;
    return FileNameStr;
}
int vpxt_timestamp_compare(const std::string TimeStampNow,
                           const std::string prev_time_stamp)
{
    int i = 0;

    while (i < 24)
    {
        if (TimeStampNow[i] != prev_time_stamp[i])
            return 0;

        i++;
    }

    return 1;
}
int get_test_name(int TestNumber, std::string &TestName)
{
    if (TestNumber == kTestMultiRun) TestName = "run_multiple_tests";

    if (TestNumber == kTestAllowDropFrames) TestName = "test_allow_drop_frames";

    if (TestNumber == kTestAllowLag) TestName = "test_allow_lag";

    if (TestNumber == kTestAllowSpatialResampling) TestName = "test_allow_spatial_resampling";

    if (TestNumber == kTestArnr) TestName = "test_arnr";

    if (TestNumber == kTestAutoKeyFrame) TestName = "test_auto_key_frame";

    if (TestNumber == kTestBufferLevel) TestName = "test_buffer_level";

    if (TestNumber == kTestChangeCpuDec) TestName = "test_change_cpu_dec";

    if (TestNumber == kTestChangeCpuEnc) TestName = "test_change_cpu_enc";

    if (TestNumber == kTestConstrainedQuality) TestName = "test_constrained_quality";

    if (TestNumber == kTestCopySetReference) TestName = "test_copy_set_reference";

    if (TestNumber == kTestDataRate) TestName = "test_data_rate";

    if (TestNumber == kTestDebugMatchesRelease) TestName = "test_debug_matches_release";

    if (TestNumber == kTestDropFrameWaterMark) TestName = "test_drop_frame_watermark";

    if (TestNumber == kTestEncoderBreakout) TestName = "test_encoder_break_out";

    if (TestNumber == kTestErrorConcealment) TestName = "test_error_concealment";

    if (TestNumber == kTestErrorResolution) TestName = "test_error_resolution";

    if (TestNumber == kTestExtraFile) TestName = "test_extra_file";

    if (TestNumber == kTestFixedQuantizer) TestName = "test_fixed_quantizer";

    if (TestNumber == kTestForcedKeyFrame) TestName = "test_force_key_frame";

    if (TestNumber == kTestFrameSize) TestName = "test_frame_size";

    if (TestNumber == kTestGoodVsBest) TestName = "test_good_vs_best";

    if (TestNumber == kTestLagInFrames) TestName = "test_lag_in_frames";

    if (TestNumber == kTestMaxQuantizer) TestName = "test_max_quantizer";

    if (TestNumber == kTestMemLeak) TestName = "test_mem_leak";

    if (TestNumber == kTestMemLeak2) TestName = "test_mem_leak2";

    if (TestNumber == kTestMinQuantizer) TestName = "test_min_quantizer";

    if (TestNumber == kTestMultiResolutionEncode) TestName = "test_multiple_resolution_encode";

    if (TestNumber == kTestMultiThreadedDec) TestName = "test_multithreaded_dec";

    if (TestNumber == kTestMultiThreadedEnc) TestName = "test_multithreaded_enc";

    if (TestNumber == kTestNewVsOldEncCpuTick) TestName = "test_new_vs_old_enc_cpu_tick";

    if (TestNumber == kTestNewVsOldPsnr) TestName = "test_new_vs_old_psnr";

    if (TestNumber == kTestNewVsOldTempScale) TestName = "test_new_vs_old_temp_scale";

    if (TestNumber == kTestNoiseSensitivity) TestName = "test_noise_sensitivity";

    if (TestNumber == kTestOnePassVsTwoPass) TestName = "test_one_pass_vs_two_pass";

    if (TestNumber == kTestPlayAlternate) TestName = "test_play_alternate";

    if (TestNumber == kTestPostProcessor) TestName = "test_post_processor";

    if (TestNumber == kTestPostProcessorMfqe) TestName = "test_post_processor_mfqe";

    if (TestNumber == kTestReconstructBuffer) TestName = "test_reconstruct_buffer";

    if (TestNumber == kTestResampleDownWatermark) TestName = "test_resample_down_watermark";

    if (TestNumber == kTestSpeed) TestName = "test_speed";

    if (TestNumber == kTestTemporalScalability) TestName = "test_temporal_scalability";

    if (TestNumber == kTestTestVector) TestName = "test_test_vector";

    if (TestNumber == kTestThirtytwoVsSixtyfour) TestName = "test_thirtytwo_vs_sixtyfour";

    if (TestNumber == kTestTwoPassVsTwoPassBest) TestName = "test_two_pass_vs_two_pass_best";

    if (TestNumber == kTestUndershoot) TestName = "test_undershoot";

    if (TestNumber == kTestVersion) TestName = "test_version";

    if (TestNumber == kTestVpxMatchesInt) TestName = "test_vpx_matches_int";

    if (TestNumber == kTestWinLinMacMatch) TestName = "test_win_lin_mac_match";

    return 0;
}
int vpxt_identify_test(const char *test_char)
{
    // parse through possible options
    // if input is number return number
    // if input is string check for number string coresponds to return that
    // on error/cant find number string coresponds to return -1

    if (atoi(test_char) != 0)
        return atoi(test_char);
    else
    {
        char test_char_no_space[255];
        // make sure no white spaces
        vpxt_remove_char_spaces(test_char, test_char_no_space, 255);
        std::string id_test_str = test_char_no_space;
        vpxt_lower_case_string(id_test_str);

        if (id_test_str.substr(0, 1).compare("+") == 0)
            id_test_str.erase(0, 1);

        if (id_test_str.compare("run_multiple_tests") == 0)
            return kTestMultiRun;

        if (id_test_str.compare("test_allow_drop_frames") == 0)
            return kTestAllowDropFrames;

        if (id_test_str.compare("test_allow_lag") == 0)
            return kTestAllowLag;

        if (id_test_str.compare("test_allow_spatial_resampling") == 0)
            return kTestAllowSpatialResampling;

        if (id_test_str.compare("test_arnr") == 0)
            return kTestArnr;

        if (id_test_str.compare("test_auto_key_frame") == 0)
            return kTestAutoKeyFrame;

        if (id_test_str.compare("test_buffer_level") == 0)
            return kTestBufferLevel;

        if (id_test_str.compare("test_change_cpu_dec") == 0)
            return kTestChangeCpuDec;

        if (id_test_str.compare("test_change_cpu_enc") == 0)
            return kTestChangeCpuEnc;

        if (id_test_str.compare("test_constrained_quality") == 0)
            return kTestConstrainedQuality;

        if (id_test_str.compare("test_copy_set_reference") == 0)
            return kTestCopySetReference;

        if (id_test_str.compare("test_data_rate") == 0)
            return kTestDataRate;

        if (id_test_str.compare("test_debug_matches_release") == 0)
            return kTestDebugMatchesRelease;

        if (id_test_str.compare("test_drop_frame_watermark") == 0)
            return kTestDropFrameWaterMark;

        if (id_test_str.compare("test_encoder_break_out") == 0)
            return kTestEncoderBreakout;

        if (id_test_str.compare("test_error_concealment") == 0)
            return kTestErrorConcealment;

        if (id_test_str.compare("test_error_resolution") == 0)
            return kTestErrorResolution;

        if (id_test_str.compare("test_extra_file") == 0)
            return kTestExtraFile;

        if (id_test_str.compare("test_fixed_quantizer") == 0)
            return kTestFixedQuantizer;

        if (id_test_str.compare("test_force_key_frame") == 0)
            return kTestForcedKeyFrame;

        if (id_test_str.compare("test_frame_size") == 0)
            return kTestFrameSize;

        if (id_test_str.compare("test_good_vs_best") == 0)
            return kTestGoodVsBest;

        if (id_test_str.compare("test_lag_in_frames") == 0)
            return kTestLagInFrames;

        if (id_test_str.compare("test_max_quantizer") == 0)
            return kTestMaxQuantizer;

        if (id_test_str.compare("test_mem_leak") == 0)
            return kTestMemLeak;

        if (id_test_str.compare("test_mem_leak2") == 0)
            return kTestMemLeak2;

        if (id_test_str.compare("test_min_quantizer") == 0)
            return kTestMinQuantizer;

        if (id_test_str.compare("test_multiple_resolution_encode") == 0)
            return kTestMultiResolutionEncode;

        if (id_test_str.compare("test_multithreaded_dec") == 0)
            return kTestMultiThreadedDec;

        if (id_test_str.compare("test_multithreaded_enc") == 0)
            return kTestMultiThreadedEnc;

        if (id_test_str.compare("test_new_vs_old_psnr") == 0)
            return kTestNewVsOldPsnr;

        if (id_test_str.compare("test_new_vs_old_temp_scale") == 0)
            return kTestNewVsOldTempScale;

        if (id_test_str.compare("test_new_vs_old_enc_cpu_tick") == 0)
            return kTestNewVsOldEncCpuTick;

        if (id_test_str.compare("test_noise_sensitivity") == 0)
            return kTestNoiseSensitivity;

        if (id_test_str.compare("test_one_pass_vs_two_pass") == 0)
            return kTestOnePassVsTwoPass;

        if (id_test_str.compare("test_play_alternate") == 0)
            return kTestPlayAlternate;

        if (id_test_str.compare("test_post_processor") == 0)
            return kTestPostProcessor;

        if (id_test_str.compare("test_post_processor_mfqe") == 0)
            return kTestPostProcessorMfqe;

        if (id_test_str.compare("test_reconstruct_buffer") == 0)
            return kTestReconstructBuffer;

        if (id_test_str.compare("test_resample_down_watermark") == 0)
            return kTestResampleDownWatermark;

        if (id_test_str.compare("test_speed") == 0)
            return kTestSpeed;

        if (id_test_str.compare("test_temporal_scalability") == 0)
            return kTestTemporalScalability;

        if (id_test_str.compare("test_test_vector") == 0)
            return kTestTestVector;

        if (id_test_str.compare("test_thirtytwo_vs_sixtyfour") == 0)
            return kTestThirtytwoVsSixtyfour;

        if (id_test_str.compare("test_two_pass_vs_two_pass_best") == 0)
            return kTestTwoPassVsTwoPassBest;

        if (id_test_str.compare("test_undershoot") == 0)
            return kTestUndershoot;

        if (id_test_str.compare("test_version") == 0)
            return kTestVersion;

        if (id_test_str.compare("test_vpx_matches_int") == 0)
            return kTestVpxMatchesInt;

        if (id_test_str.compare("test_win_lin_mac_match") == 0)
            return kTestWinLinMacMatch;

        if (id_test_str.compare("0") == 0)
            return 0;
    }

    return -1;
}
int vpxt_run_multiple_tests_input_check(const char *input, int MoreInfo)
{
    // function returns number of tests found if input is correct -1 if not
    // correct and -3 if there is an error

    ///////////////////// Read Number of Tests /////////////////////////////////
    std::fstream infile2;
    infile2.open(input);

    if (!infile2.good())
    {
        tprintf(PRINT_STD, "\nInput File does not exist\n");
        infile2.close();
        return 0;
    }

    int numberoftests = 0;

    while (!infile2.eof())
    {
        char buffer3[1024];
        infile2.getline(buffer3, 1024);

        int Buffer0CharAscii = buffer3[0];

        // skips over any line starting with a % in the input file to allow for
        // comenting
        if (Buffer0CharAscii == 37 || Buffer0CharAscii == '\0' ||
            Buffer0CharAscii == '\r')
        {
            // lines_skipped_cnt++;
        }
        else
        {
            numberoftests++;
        }
    }

    infile2.clear();
    infile2.close();
    ////////////////////////////////////////////////////////////////////////////

    if (numberoftests > 990)
    {
        tprintf(PRINT_BTH, "\nNumber of test exceeds current capacity please "
            "limit external tests to 990\n");
        return -1;
    }

    int lines_skipped_cnt = 0;

    char buffer[1024];
    char buffer2[1024];

    int Buf1Var = 0;
    int DummyArgvVar = 1;
    int CommentBool = 0;

    int PassFail[999];                // = new int[numberoftests+2];
    int PassFailInt = 0;
    int TestsRun = 0;

    std::string StringAr[999];        // = new string[numberoftests+2];
    std::string SelectorAr[999];      // = new string[numberoftests+2];
    std::string SelectorAr2[999];     // = new string[numberoftests+2];


    int SelectorArInt = 0;
    int y;
    int PassFailReturn = 1;

    std::fstream infile;
    infile.open(input);

    if (!infile.good())
    {
        tprintf(PRINT_STD, "\nInput File does not exist\n");
        infile.close();
        return 0;
    }

    int number_of_tests_run = 1;
    ///////////////////////////////////////
    int trackthis1 = 0;
    int trackthis2 = 0;

    ////////////////////////////////////////
    while (!infile.eof())
    {
        const char *DummyArgv[999];
        DummyArgv[0] = "";
        DummyArgvVar = 1;
        infile.getline(buffer, 1024);

        std::cout << buffer << "\n";

        trackthis1++;

        int Buffer0CharAscii = buffer[0];

        // skips over any line starting with a % in the input file to allow for
        // comenting
        if (Buffer0CharAscii == 37 || Buffer0CharAscii == '\0' ||
            Buffer0CharAscii == '\r')
        {
            lines_skipped_cnt++;
        }
        else
        {
            Buf1Var = 0;

            // parses through gotline and seperates commands out
            while (buffer[Buf1Var] != '\0' && buffer[Buf1Var] != '\r')
            {
                int Buf2Var = 0;

                while (buffer[Buf1Var] != 64 && buffer[Buf1Var] != '\0' &&
                    buffer[Buf1Var] != '\r')
                {
                    buffer2[Buf2Var] = buffer[Buf1Var];
                    Buf1Var++;
                    Buf2Var++;
                }

                buffer2[Buf2Var] = '\0';

                if (buffer[Buf1Var] != '\0' && buffer[Buf1Var] != '\r')
                {
                    Buf1Var++;
                }

                if (CommentBool == 0)
                {
                    StringAr[DummyArgvVar] = buffer2;
                    DummyArgvVar++;
                }
            }

            y = 1;

            while (y < DummyArgvVar)
            {
                DummyArgv[y] = StringAr[y].c_str();
                y++;
            }

            // this is for Mode 3 test only mode in order to find the right
            // TimeStamp
            ////////////////////////////////////////////////////////////////////
            if (CommentBool == 0)
            {
                int selector =  vpxt_identify_test(DummyArgv[1]);

                if (selector >= 0 && selector < MAXTENUM)
                {
                    number_of_tests_run++;
                }

                if (selector == 0)
                {

                }

                if (selector == kTestAllowDropFrames)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "AllowDropFrames";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestAllowLag)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "AllowLagTest";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestAllowSpatialResampling)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "AllowSpatialResampling";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestArnr)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "Arnr";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestAutoKeyFrame)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "AutoKeyFramingWorks";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestBufferLevel)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "BufferLevelWorks";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestChangeCpuDec)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "CPUDecOnlyWorks";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestChangeCpuEnc)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "ChangeCPUWorks";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestConstrainedQuality)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "ConstrainQuality";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestCopySetReference)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "CopySetReference";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestDropFrameWaterMark)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "DropFramesWaterMarkWorks";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestDataRate)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "DataRateTest";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestDebugMatchesRelease)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "DebugMatchesRelease";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestEncoderBreakout)
                {

                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "EncoderBreakOut";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestErrorConcealment)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "ErrorConcealment";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestErrorResolution)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "ErrorResilientModeWorks";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestExtraFile)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "ExtraFileCheck";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestFixedQuantizer)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "FixedQ";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestForcedKeyFrame)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "ForceKeyFameWorks";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestFrameSize)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "FrameSizeTest";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestGoodVsBest)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "GoodQualityVsBestQuality";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {
                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestLagInFrames)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "LagInFramesTest";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestMaxQuantizer)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "MaxQuantizerTest";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestMemLeak)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "MemLeakCheck";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestMemLeak2)
                {

                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "MemLeakCheck2";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestMinQuantizer)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "MinQuantizerTest";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestMultiResolutionEncode)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "MultipleResolutionEncode";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestMultiThreadedDec)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "MultiThreadedTestDec";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestMultiThreadedEnc)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "MultiThreadedTestEnc";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestNewVsOldPsnr)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "NewVsOldPSNR";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestNewVsOldTempScale)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "NewVsOldTempScale";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestNewVsOldEncCpuTick)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "NewVsOldRealTimeSpeed";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestNoiseSensitivity)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "NoiseSensititivityWorks";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestOnePassVsTwoPass)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "OnePassVsTwoPass";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestPlayAlternate)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "PlayAlternate";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestPostProcessor)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "PostProcessorWorks";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestPostProcessorMfqe)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "PostProcessorWorksMFQE";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestReconstructBuffer)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "RECBFNUMfer";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestResampleDownWatermark)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "ResampleDownWaterMark";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestSpeed)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "SpeedTest";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {
                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestTemporalScalability)
                {

                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "TemporalScalability";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestTestVector)
                {

                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "TestVectorCheck";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestThirtytwoVsSixtyfour)
                {

                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "32BitVs64Bit";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestTwoPassVsTwoPassBest)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "TwoPassVsTwoPassBest";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestUndershoot)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "UnderShoot";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestVersion)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "Version";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestVpxMatchesInt)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "VpxencMatchesIntComp";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                if (selector == kTestWinLinMacMatch)
                {
                    if (!vpxt_check_arg_input(DummyArgv[1], DummyArgvVar))
                    {
                        SelectorAr[SelectorArInt] += buffer;
                        SelectorAr2[SelectorArInt] = "WinLinMacMatch";
                        PassFail[PassFailInt] = trackthis1;
                    }
                    else
                    {

                        PassFail[PassFailInt] = -1;
                    }
                }

                // Make sure that all tests input are vaild tests by checking
                // the list (make sure to add new tests here!)
                if (selector != kTestMultiRun && selector != kTestAllowDropFrames &&
                    selector != kTestAllowLag && selector != kTestFrameSize &&
                    selector != kTestArnr && selector != kTestAutoKeyFrame &&
                    selector != kTestBufferLevel && selector != kTestChangeCpuDec &&
                    selector != kTestChangeCpuEnc && selector != kTestConstrainedQuality &&
                    selector != kTestCopySetReference && selector != kTestDropFrameWaterMark &&
                    selector != kTestDataRate && selector != kTestDebugMatchesRelease &&
                    selector != kTestEncoderBreakout && selector != kTestErrorConcealment &&
                    selector != kTestErrorResolution && selector != kTestExtraFile &&
                    selector != kTestFixedQuantizer && selector != kTestForcedKeyFrame &&
                    selector != kTestGoodVsBest && selector != kTestLagInFrames &&
                    selector != kTestMaxQuantizer && selector != kTestMemLeak &&
                    selector != kTestMemLeak2 && selector != kTestMinQuantizer &&
                    selector != kTestMultiThreadedEnc && selector != kTestMultiThreadedDec &&
                    selector != kTestNewVsOldPsnr && selector != kTestNewVsOldTempScale &&
                    selector != kTestNewVsOldEncCpuTick && selector != kTestNoiseSensitivity &&
                    selector != kTestOnePassVsTwoPass && selector != kTestPlayAlternate &&
                    selector != kTestPostProcessor && selector != kTestPostProcessorMfqe &&
                    selector != kTestResampleDownWatermark && selector != kTestSpeed &&
                    selector != kTestTemporalScalability && selector != kTestTestVector &&
                    selector != kTestThirtytwoVsSixtyfour && selector != kTestReconstructBuffer &&
                    selector != kTestTwoPassVsTwoPassBest && selector != kTestUndershoot &&
                    selector != kTestVersion && selector != kTestWinLinMacMatch &&
                    selector != kTestAllowSpatialResampling && selector != kTestVpxMatchesInt &&
                    selector != kTestMultiResolutionEncode)
                {
                    SelectorAr[SelectorArInt] += buffer;
                    SelectorAr2[SelectorArInt] = "Test Not Found";
                    PassFail[PassFailInt] = trackthis1;
                }

                PassFailInt++;
                SelectorArInt++;
            }
        }
    }

    y = 0;
    tprintf(PRINT_STD, "\n");

    while (y < SelectorArInt)
    {
        if (PassFail[y] != -1)
        {
            PassFailReturn = 0;
            tprintf(PRINT_STD, "Line: %4i Test: %-25s - Not properly Formatted"
                "\n", PassFail[y], SelectorAr2[y].c_str());
            tprintf(PRINT_STD, "%s\n\n", SelectorAr[y].c_str());
        }

        y++;
    }

    // more info if == 1 then will display text file statistics if 0 then will
    // not 0 ment for File Check prior to running External Test runner to ensure
    // input is correct
    if (MoreInfo == 0)
    {
        if (PassFailReturn == 0)
        {

            infile.close();
            // delete [] PassFail;
            // delete [] StringAr;
            // delete [] SelectorAr;
            // delete [] SelectorAr2;
            // return 0;
            return -1;

        }
        else
        {
            if (PassFailInt == 0)
            {
                tprintf(PRINT_STD, "Test File Specified is empty.");
                // delete [] PassFail;
                // delete [] StringAr;
                // delete [] SelectorAr;
                // delete [] SelectorAr2;
                return 0;
            }
            else
            {

                tprintf(PRINT_STD, "\nAll %i Tests in text file: %s - "
                    "are properly Formatted\n\n", y, input);

                // return 1;
                // delete [] PassFail;
                // delete [] StringAr;   //will cause error
                // delete [] SelectorAr; //will cause error
                // delete [] SelectorAr2;
                infile.close();
                return SelectorArInt;
            }
        }
    }
    else
    {

        if (PassFailReturn == 0)
        {
            tprintf(PRINT_STD, "\nFile Contains %i Tests\n", SelectorArInt);
            tprintf(PRINT_STD, "\nFile Contains %i Lines\n", trackthis1);
            infile.close();
            // delete [] PassFail;
            // delete [] StringAr;
            // delete [] SelectorAr;
            // delete [] SelectorAr2;
            // return 0;
            return -1;
        }
        else
        {
            tprintf(PRINT_STD, "\nAll %i Tests in text file: %s - are properly "
                "Formatted\n\n", y, input);
            tprintf(PRINT_STD, "\nFile Contains %i Tests\n", SelectorArInt);
            tprintf(PRINT_STD, "\nFile Contains %i Lines\n", trackthis1);
            infile.close();
            return SelectorArInt;
        }
    }

    infile.close();
    return -3;
}
int vpxt_file_exists_check(const std::string input)
{

    std::ifstream infile(input.c_str());

    if (infile)
    {
        infile.close();
        return 1;
    }

    infile.close();
    return 0;
}
int vpxt_folder_exist_check(const std::string FolderName)
{
#if defined(_WIN32)

    if (GetFileAttributes(FolderName.c_str()) == INVALID_FILE_ATTRIBUTES)
        return 0;
    else
        return 1;

#else
    if (opendir(FolderName.c_str()) == NULL)
        return 0;
    else
        return 1;
#endif
    return 0;
}
void vpxt_subfolder_name(const char *input, char *FileName)
{
    // extracts name of last two folders a target file is in and returns it
    int parser = 0;
    int slashcount = 0;
    int slashcount2 = 0;

    while (input[parser] != '\0')
    {
        if (input[parser] == slashChar())
        {
            slashcount++;
        }

        parser++;
    }

    parser = 0;
    int parser2 = 0;

    while (input[parser] != '\0')
    {

        if (slashcount2 > slashcount - 2)
        {
            FileName[parser2] = input[parser];
            parser2++;
        }

        if (input[parser] == slashChar())
        {
            slashcount2++;
        }

        parser++;
    }

    FileName[parser2] = '\0';

    return;
}
void vpxt_test_name(char *input, char *FileName)
{
    // Gets the directory test name from an input sting
    int parser = 0;
    int slashcount = 0;
    int slashcount2 = 0;
    char OpSlashChar;

    // find out what the default shash char is then define the oposite slashchar
    if (slashChar() == '\\')
    {
        OpSlashChar = '/';
    }

    if (slashChar() == '/')
    {
        OpSlashChar = '\\';
    }

    // Parse through stirng replaceing any instance of an oposite slash char
    // with a correct slash char in order to run cross plat.
    while (input[parser] != '\0')
    {
        if (input[parser] == OpSlashChar)
        {
            input[parser] = slashChar();
        }

        parser++;
    }

    // continue with the function after proper initialization.
    parser = 0;

    while (input[parser] != '\0' && input[parser] != slashChar())
    {
        FileName[parser] = input[parser];
        parser++;
    }

    FileName[parser] = '\0';

    return;
}
int  vpxt_init_new_vs_old_log(const char *input, std::string TestIDStr)
{
    // returns the number of instances TestIDStr was found in the input file
    // if no instances are found one will be created and the number 1 will be
    // retunred as output.
    std::fstream input_file;
    input_file.open(input, std::fstream::in);

    char inputFileLine[256];
    input_file.getline(inputFileLine, 256);

    int numberOfUniqueIDs = 0;

    // loop through log file for TestIDStr to make sure that log entry exists
    while (!input_file.eof())
    {
        if (TestIDStr.compare(inputFileLine) == 0)
            numberOfUniqueIDs = numberOfUniqueIDs + 1;

        input_file.getline(inputFileLine, 256);
    }

    input_file.close();

    if (numberOfUniqueIDs != 0)
        return numberOfUniqueIDs;

    // if log entry does not exist create it and return that only 1 entry exists
    std::fstream output_file;
    output_file.open(input, std::fstream::out | std::fstream::app);
    output_file << TestIDStr.c_str() << "\n";
    output_file.close();

    return 1;
}
int  vpxt_sync_new_vs_old_log(const char *testlog,
                              const char *gitlog,
                              const char *newtestlog,
                              const char *updateinfo,
                              std::string TestIDStr,
                              std::string testName)
{
    if (vpxt_file_size(gitlog, 0) == 0)
        return -1;

    std::fstream testlogFile;
    testlogFile.open(testlog, std::fstream::in);

    std::fstream gitlogFile;
    gitlogFile.open(gitlog, std::fstream::in);

    std::fstream newtestlogFile;
    newtestlogFile.open(newtestlog, std::fstream::out);

    char testlogFileLine[256];
    char gitlogFileLine[256];

    testlogFile.getline(testlogFileLine, 256);
    gitlogFile.getline(gitlogFileLine, 256);

    std::string versionStr = vpx_codec_iface_name(&vpx_codec_vp8_cx_algo);
    std::string versionStrSub = versionStr.substr(versionStr.length() - 7, 7);

    int firstheader = 1;
    int correctTest = 0;
    int correctCommit = 0;
    int writenext = 0;

    while (!gitlogFile.eof() || !testlogFile.eof())
    {
        // if we are at the correct test log (ident by input vars)
        if (TestIDStr.compare(testlogFileLine) == 0)
            correctTest = 1;
        // if we are at the correct commit id (ident by partial version str)
        if (correctTest == 1)
            if (strncmp(testlogFileLine, versionStrSub.c_str(), 7) == 0)
                correctCommit = 1;
        // if we are at the correct commit id (ident by partial version str)
        if (correctTest == 1)
            if (strncmp(gitlogFileLine, versionStrSub.c_str(), 7) == 0)
                correctCommit = 1;

        char gitlogFileLineCommit[41] = "";
        char testlogFileLineCommit[41] = "";

        // formatted lines to print
        strncpy(gitlogFileLineCommit, gitlogFileLine, 40);
        strncpy(testlogFileLineCommit, testlogFileLine, 40);

        // if we come to a test identification line finish writing git history
        // if applicable and reset git log to start the next run through.
        if (strncmp(testlogFileLine, testName.c_str(), testName.length()) == 0)
        {
            if (firstheader != 1) // igore first instance
            {
                if (writenext == 0)
                {
                    if (correctTest == 1)
                        writenext = 1;

                    while (!gitlogFile.eof())
                    {
                        strncpy(gitlogFileLineCommit, gitlogFileLine, 40);
                        newtestlogFile << gitlogFileLineCommit << "\n";
                        gitlogFile.getline(gitlogFileLine, 256);

                        if (!testlogFile.eof())
                            gitlogFile.getline(gitlogFileLine, 256);
                    }
                }
                else
                {
                    while (!gitlogFile.eof())
                    {
                        strncpy(gitlogFileLineCommit, gitlogFileLine, 40);

                        if (correctCommit == 1)
                        {
                            newtestlogFile << gitlogFileLineCommit << " "
                                << updateinfo << "\n";
                            correctCommit = 0;
                            correctTest = 0;
                        }
                        else
                            newtestlogFile << gitlogFileLineCommit << "\n";

                        gitlogFile.getline(gitlogFileLine, 256);
                    }

                    writenext = 0;
                }
            }

            firstheader = 0;

            gitlogFile.clear();
            gitlogFile.seekg(std::ios::beg);
            gitlogFile.getline(gitlogFileLine, 256);

            newtestlogFile << testlogFileLine << "\n";

            if (!testlogFile.eof())
                testlogFile.getline(testlogFileLine, 256);

        }
        else
        {
            // if git and new-vs-old log commits are the same print the log info
            // and advance both
            if (strncmp(testlogFileLine, gitlogFileLine, 40) == 0)
            {
                if (correctCommit == 1 && correctTest == 1)
                {
                    newtestlogFile << testlogFileLineCommit << " "
                        << updateinfo << "\n";
                    correctCommit = 0;
                    correctTest = 0;
                }
                else
                    newtestlogFile << testlogFileLine << "\n";

                if (!testlogFile.eof())
                    testlogFile.getline(testlogFileLine, 256);

                if (!gitlogFile.eof())
                    gitlogFile.getline(gitlogFileLine, 256);
            }
            else
            {
                // if git and new-vs-old are not the same print the git info and
                // advance git

                if (correctCommit == 1 && correctTest == 1)
                {
                    newtestlogFile << gitlogFileLineCommit << " " << updateinfo
                        << "\n";
                    correctCommit = 0;
                    correctTest = 0;
                }
                else
                    newtestlogFile << gitlogFileLine << "\n";

                if (!gitlogFile.eof())
                    gitlogFile.getline(gitlogFileLine, 256);
            }
        }
    }

    testlogFile.close();
    gitlogFile.close();
    newtestlogFile.close();

    return 0;
}
double vpxt_get_new_vs_old_val(std::string fileline,
                               std::vector<double> &ValueList,
                               int &values_per_line)
{
    unsigned int last_num_pos = 0;
    int last_space_pos = 0;
    int is_number = 1;
    double number = 0.0;
    int new_values_per_line = 0;

    // loop through line stop at end
    while (41 + last_num_pos < fileline.length())
    {
        // look for spaces
        if (fileline.substr(41 + last_num_pos, 1).compare(" ") == 0){
            // alternate between number and its description if expecting number
            // use last_space_pos and last_num_pos to get it.
            if(is_number){
                is_number = 0;
                number = strtod(fileline.substr(41 + last_space_pos, 41 +
                    last_num_pos - 1).c_str(), NULL);

                // if valid number add it to vector and add one to values per
                // line
                if(number > 0.0){
                    ValueList.push_back(number);
                    ++new_values_per_line;
                }
            }
            else{
                is_number = 1;
                last_space_pos = last_num_pos;
            }
        }

        ++last_num_pos;
    }

    if(new_values_per_line > values_per_line)
        values_per_line = new_values_per_line;

    // if no input found after commit description return 0 else 1
    if (last_num_pos == 0)
        return 0.0;
    else
        return 1.0;
}
int  vpxt_eval_new_vs_old_log(const char *logfile,
                              std::string TestIDStr,
                              int printvar,
                              std::vector<double> &ValueList,
                              std::string testName)
{
    std::string versionStr = vpx_codec_iface_name(&vpx_codec_vp8_cx_algo);
    std::string versionStrSub = versionStr.substr(versionStr.length() - 7, 7);

    std::fstream logFile;
    logFile.open(logfile, std::fstream::in);

    char logFileLine[256];
    logFile.getline(logFileLine, 256);

    int correctTest = 0;
    int correctCommit = 0;
    int contextLines = 0;
    int totalLines = 0;
    int FirstLineReached = 0;
    int values_per_line = 1;

    while (!logFile.eof())
    {
        if (TestIDStr.compare(logFileLine) == 0)
            correctTest = 1;

        if (correctTest == 1)
            if (strncmp(logFileLine, versionStrSub.c_str(), 7) == 0)
                correctCommit = 1;

        if (correctCommit == 1)
        {
            while (!logFile.eof() && (ValueList.size()/values_per_line < 2 ||
                contextLines < 5) && (ValueList.size()/values_per_line < 2 ||
                totalLines < 30))
            {
                totalLines = totalLines + 1;

                if(vpxt_get_new_vs_old_val(logFileLine, ValueList,
                    values_per_line))
                {
                    if (strncmp(logFileLine, versionStrSub.c_str(), 7) == 0 ||
                        FirstLineReached == 1)
                    {
                        if (FirstLineReached == 0)
                        {
                            std::string log_file_str = logFileLine;
                            tprintf(PRINT_BTH, "%s", log_file_str.substr(0,40).c_str());

                            std::string::iterator str_it;
                            int spaces = -1;
                            for(str_it = log_file_str.begin() + 40 ; str_it <
                                log_file_str.end(); str_it++)
                            {
                                if(spaces == 4){
                                    tprintf(PRINT_BTH, "\n");

                                    for(int i=0; i<41; i++)
                                        tprintf(PRINT_BTH, " ");

                                    spaces = 0;
                                }
                                if(*str_it == ' ')
                                    ++spaces;

                                tprintf(PRINT_BTH, "%c", *str_it);
                            }

                            tprintf(PRINT_BTH, " <--\n");
                        }
                        else
                        {
                            std::string log_file_str = logFileLine;
                            tprintf(PRINT_BTH, "%s", log_file_str.substr(0,40).c_str());

                            std::string::iterator str_it;
                            int spaces = -1;
                            for(str_it = log_file_str.begin() + 40 ; str_it <
                                log_file_str.end(); str_it++)
                            {
                                if(spaces == 4){
                                    tprintf(PRINT_BTH, "\n");

                                    for(int i=0; i<41; i++)
                                        tprintf(PRINT_BTH, " ");

                                    spaces = 0;
                                }
                                if(*str_it == ' ')
                                    ++spaces;

                                tprintf(PRINT_BTH, "%c", *str_it);
                            }

                            tprintf(PRINT_BTH, "\n");
                        }

                        FirstLineReached = 1;
                        contextLines = contextLines + 1;
                    }
                }
                else
                    tprintf(PRINT_BTH, "%s\n", logFileLine);

                if (!logFile.eof())
                    logFile.getline(logFileLine, 256);

                // if reach next test break out.
                if (strncmp(logFileLine, testName.c_str(), testName.length())
                    == 0)
                {
                    correctCommit = 0;
                    break;
                }
            }

            correctCommit = 0;
            correctTest = 0;
        }

        if (!logFile.eof())
            logFile.getline(logFileLine, 256);
    }

    logFile.close();
    return 0;
}
int  vpxt_check_arg_input(const char *testName, int argNum)
{
    // return 1 if correct number of inputs 2 if par file input and - 1 if fail
    int selector = vpxt_identify_test(testName);

    // test_allow_drop_frames
    if (selector == kTestAllowDropFrames)
    {
        if (argNum == 7)
            return 1;

        if (argNum == 8)
            return 2;
    }

    // test_allow_lag
    if (selector == kTestAllowLag)
    {
        if (argNum == 7)
            return 1;

        if (argNum == 8)
            return 2;
    }

    // test_allow_spatial_resampling
    if (selector == kTestAllowSpatialResampling)
    {
        if (argNum == 7)
            return 1;

        if (argNum == 8)
            return 2;
    }

    // test_arnr
    if (selector == kTestArnr)
    {
        if (argNum == 7)
            return 1;

        if (argNum == 8)
            return 2;
    }

    // test_auto_key_frame
    if (selector == kTestAutoKeyFrame)
    {
        if (argNum == 8)
            return 1;

        if (argNum == 9)
            return 2;
    }

    // test_buffer_level
    if (selector == kTestBufferLevel)
    {
        if (argNum == 7)
            return 1;

        if (argNum == 8)
            return 2;
    }

    // test_change_cpu_dec
    if (selector == kTestChangeCpuDec)
    {
        if (argNum == 8)
            return 1;

        if (argNum == 9)
            return 2;
    }

    // test_change_cpu_enc
    if (selector == kTestChangeCpuEnc)
    {
        if (argNum == 8)
            return 1;

        if (argNum == 9)
            return 2;
    }

    // test_constrained_quality
    if (selector == kTestConstrainedQuality)
    {
        if (argNum == 8)
            return 1;

        if (argNum == 9)
            return 2;
    }

    // test_copy_set_reference
    if (selector == kTestCopySetReference)
    {
        if (argNum == 8)
            return 1;

        if (argNum == 9)
            return 2;
    }

    // test_data_rate
    if (selector == kTestDropFrameWaterMark)
    {
        if (argNum == 7)
            return 1;

        if (argNum == 8)
            return 2;
    }

    // test_debug_matches_release
    if (selector == kTestDataRate)
    {
        if (argNum == 7)
            return 1;

        if (argNum == 8)
            return 2;
    }

    // test_drop_frame_watermark
    if (selector == kTestDebugMatchesRelease)
    {
        if (argNum == 9)
            return 1;

        if (argNum == 10)
            return 2;
    }

    // test_encoder_break_out
    if (selector == kTestEncoderBreakout)
    {
        if (argNum == 7)
            return 1;

        if (argNum == 8)
            return 2;
    }

    // test_error_concealment
    if (selector == kTestErrorConcealment)
    {
        if (argNum == 7)
            return 1;

        if (argNum == 8)
            return 2;
    }

    // test_error_resolution
    if (selector == kTestErrorResolution)
    {
        if (argNum == 7)
            return 1;

        if (argNum == 8)
            return 2;
    }

    // test_extra_file
    if (selector == kTestExtraFile)
    {
        if (argNum == 5)
            return 1;

        if (argNum == 6)
            return 2;
    }

    // test_fixed_quantizer
    if (selector == kTestFixedQuantizer)
    {
        if (argNum == 9)
            return 1;

        if (argNum == 10)
            return 2;
    }

    // test_force_key_frame
    if (selector == kTestForcedKeyFrame)
    {
        if (argNum == 8)
            return 1;

        if (argNum == 9)
            return 2;
    }

    // test_frame_size
    if (selector == kTestFrameSize)
    {
        if (argNum == 9)
            return 1;

        if (argNum == 10)
            return 2;
    }

    // test_good_vs_best
    if (selector == kTestGoodVsBest)
    {
        if (argNum == 6)
            return 1;

        if (argNum == 7)
            return 2;
    }

    // test_lag_in_frames
    if (selector == kTestLagInFrames)
    {
        if (argNum == 9)
            return 1;

        if (argNum == 10)
            return 2;
    }

    // test_max_quantizer
    if (selector == kTestMaxQuantizer)
    {
        if (argNum == 7)
            return 1;

        if (argNum == 8)
            return 2;
    }

    // test_mem_leak
    if (selector == kTestMemLeak)
    {
        if (argNum == 8)
            return 1;

        if (argNum == 9)
            return 2;
    }

    // test_mem_leak2
    if (selector == kTestMemLeak2)
    {
        if (argNum == 3)
            return 1;

        if (argNum == 4)
            return 2;
    }

    // test_min_quantizer
    if (selector == kTestMinQuantizer)
    {
        if (argNum == 7)
            return 1;

        if (argNum == 8)
            return 2;
    }

    // test_multiple_resolution_encode
    if (selector == kTestMultiResolutionEncode)
    {
        if (argNum == 6)
            return 1;

        if (argNum == 7)
            return 2;
    }

    // test_multithreaded_dec
    if (selector == kTestMultiThreadedDec)
    {
        if (argNum == 8)
            return 1;

        if (argNum == 9)
            return 2;
    }

    // test_multithreaded_enc
    if (selector == kTestMultiThreadedEnc)
    {
        if (argNum == 8)
            return 1;

        if (argNum == 9)
            return 2;
    }

    // test_new_vs_old_enc_cpu_tick
    if (selector == kTestNewVsOldPsnr)
    {
        if (argNum == 9)
            return 1;

        if (argNum == 10)
            return 2;
    }

    // test_new_vs_old_enc_cpu_tick
    if (selector == kTestNewVsOldTempScale)
    {
        if (argNum == 11)
            return 1;

        if (argNum == 12)
            return 2;
    }

    // test_new_vs_old_psnr
    if (selector == kTestNewVsOldEncCpuTick)
    {
        if (argNum == 9)
            return 1;

        if (argNum == 10)
            return 2;
    }

    // test_noise_sensitivity
    if (selector == kTestNoiseSensitivity)
    {
        if (argNum == 7)
            return 1;

        if (argNum == 8)
            return 2;
    }

    // test_one_pass_vs_two_pass
    if (selector == kTestOnePassVsTwoPass)
    {
        if (argNum == 6)
            return 1;

        if (argNum == 7)
            return 2;
    }

    // test_play_alternate
    if (selector == kTestPlayAlternate)
    {
        if (argNum == 7)
            return 1;

        if (argNum == 8)
            return 2;
    }

    // test_post_processor
    if (selector == kTestPostProcessor)
    {
        if (argNum == 7)
            return 1;

        if (argNum == 8)
            return 2;
    }

    // test_post_processor_mfqe
    if (selector == kTestPostProcessorMfqe)
    {
        if (argNum == 9)
            return 1;

        if (argNum == 10)
            return 2;
    }

    // test_reconstruct_buffer
    if (selector == kTestReconstructBuffer)
    {
        if (argNum == 7)
            return 1;

        if (argNum == 8)
            return 2;
    }

    // test_resample_down_watermark
    if (selector == kTestResampleDownWatermark)
    {
        if (argNum == 7)
            return 1;

        if (argNum == 8)
            return 2;
    }

    // test_speed
    if (selector == kTestSpeed)
    {
        if (argNum == 8)
            return 1;

        if (argNum == 9)
            return 2;
    }

    // test_temporal_scalability
    if (selector == kTestTemporalScalability)
    {
        if (argNum == 10)
            return 1;

        if (argNum == 11)
            return 2;
    }

    // test_test_vector
    if (selector == kTestTestVector)
        if ((argNum == 4))
            return 1;

    // test_test_vector
    if (selector == kTestThirtytwoVsSixtyfour)
    {
        if (argNum == 9)
            return 1;

        if (argNum == 10)
            return 2;
    }

    // test_two_pass_vs_two_pass_best
    if (selector == kTestTwoPassVsTwoPassBest)
    {
        if (argNum == 6)
            return 1;

        if (argNum == 7)
            return 2;
    }

    // test_undershoot
    if (selector == kTestUndershoot)
    {
        if (argNum == 7)
            return 1;

        if (argNum == 8)
            return 2;
    }

    // test_version
    if (selector == kTestVersion)
    {
        if (argNum == 7)
            return 1;

        if (argNum == 8)
            return 2;
    }

    // test_win_lin_mac_match
    if (selector == kTestVpxMatchesInt)
    {
        if (argNum == 9)
            return 1;

        if (argNum == 10)
            return 2;
    }

    // test_win_lin_mac_match
    if (selector == kTestWinLinMacMatch)
    {
        if (argNum == 9)
            return 1;

        if (argNum == 10)
            return 2;
    }

    return -1;
}
int vpxt_remove_char_spaces(const char *input, char *output, int maxsize)
{
    int pos = 0;
    int offset = 0;

    while (input[pos] != '\0' && pos < maxsize)
    {
        if (input[pos] != ' ')
            output[pos-offset] = input[pos];
        else
            offset++;

        pos++;
    }

    output[pos-offset] = '\0';

    return 0;
}
void replace_substring(const std::string& old_str, const std::string& new_str,
                         std::string* str) {
    if (!str) return;
    // search for |old_str| in |str|.
    size_t pos = 0;
    while((pos = str->find(old_str, pos)) != std::string::npos)
    {
       str->replace(pos, old_str.length(), new_str);
       pos += new_str.length();
    }
  }
// ------------------Math-------------------------------------------------------
int vpxt_decimal_places(int InputNumber)
{
    int y = 0;
    int totaltensplaces = 0;

    // find out how many decimal places
    while (y >= 0)
    {
        y = InputNumber - (int)pow(10.0, totaltensplaces);
        totaltensplaces++;
    }

    totaltensplaces = totaltensplaces - 1;

    return totaltensplaces;
}
int vpxt_gcd(int a, int b)
{
    while (1)
    {
        a = a % b;

        if (a == 0)
            return b;

        b = b % a;

        if (b == 0)
            return a;
    }
}
int vpxt_abs_int(int input)
{
    if (input < 0)
    {
        input = input * -1;
    }

    return input;
}
float vpxt_abs_float(float input)
{
    if (input < 0.0)
        input = input * -1.0;

    return input;
}
double vpxt_abs_double(double input)
{
    if (input < 0)
        input = input * -1.0;

    return input;
}
int vpxt_solve_quadratic(double X1, double X2, double X3, double Y1, double Y2,
                         double Y3, double &A, double &B, double &C)
{
    A = ((Y2 - Y1) * (X1 - X3) + (Y3 - Y1) * (X2 - X1)) / ((X1 - X3) *
        ((X2 * X2) - (X1 * X1)) + (X2 - X1) * ((X3 * X3) - (X1 * X1)));
    B = ((Y2 - Y1) - A * ((X2 * X2) - (X1 * X1))) / (X2 - X1);
    C = Y1 - A * (X1 * X1) - B * X1;

    return 0;
}
double vpxt_area_under_quadratic(double A, double B, double C, double X1,
                                 double X2)
{
    double Area1 = ((A * X1 * X1 * X1) / 3) + ((B * X1 * X1) / 2) + C * X1;
    double Area2 = ((A * X2 * X2 * X2) / 3) + ((B * X2 * X2) / 2) + C * X2;
    double TotalArea = Area2 - Area1;
    return TotalArea;
}
char *vpxt_itoa_custom(int value, char *result, int base)
{
    int x = 0;

    if (base < 2 || base > 16)
    {
        *result = 0;
        return result;
    }

    char *out = result;
    int quotient = value;

    do
    {
        *out = "0123456789abcdef"[ abs(quotient % base)];
        ++out;
        quotient /= base;
    }
    while (quotient);

    // Only apply negative sign for base 10
    if (value < 0 && base == 10) *out++ = '-';

    std::reverse(result, out);
    *out = 0;

    return result;
}
// ---------------------------Cross Plat----------------------------------------
void tprintf(int PrintSelection, const char *fmt, ...)
{
    // Output for python
    // FILE * STDOUT_File;
    // STDOUT_File = fopen ("PathToPythonTxtFile","a");

    char buffer[2048];
    buffer[0] = NULL;
    va_list ap;
    va_start(ap, fmt);

    int charswritten = vsnprintf(buffer, sizeof(buffer) - 1, fmt, ap);

    if (charswritten < 2048)
    {
        std::string bufferStr = buffer;
        std::string bufferStr2 = buffer;
        int curPos = 0;
        int lastNewLine = 0;

        if (bufferStr.size() > 79)
        {
            while ((unsigned int)curPos < bufferStr.size())
            {
                if (bufferStr.substr(curPos, 1).compare("\n") == 0)
                {
                    lastNewLine = curPos;
                }

                if (curPos - lastNewLine > 79)
                {
                    bufferStr2.insert(curPos, "\n");
                    lastNewLine = curPos;
                    curPos++;
                }

                curPos++;
            }
        }

        if (PrintSelection == PRINT_BTH)
        {
            // fputs (bufferStr2.c_str(),STDOUT_File);
            printf("%s", bufferStr.c_str());
            fprintf(stderr, "%s", bufferStr2.c_str());
        }

        if (PrintSelection == PRINT_ERR)
        {
            fprintf(stderr, "%s", bufferStr2.c_str());
        }

        if (PrintSelection == PRINT_STD)
        {
            // fputs (bufferStr2.c_str(),STDOUT_File);
            printf("%s", bufferStr.c_str());
        }
    }
    else
    {
        if (PrintSelection == PRINT_BTH)
        {
            vprintf(fmt, ap);
            vfprintf(stderr, fmt, ap);
        }

        if (PrintSelection == PRINT_ERR)
        {
            vfprintf(stderr, fmt, ap);
        }

        if (PrintSelection == PRINT_STD)
        {
            vprintf(fmt, ap);
        }
    }

    // fclose(STDOUT_File);
    va_end(ap);
}
std::string slashCharStr()
{

#if defined(_WIN32)
    return "\\";
#elif defined(linux)
    return "/";
#elif defined(__APPLE__)
    return "/";
#elif defined(_PPC)
    return "/";
#endif

    return "\\";
}
char slashChar()
{

#if defined(_WIN32)
    return '\\';
#elif defined(linux)
    return '/';
#elif defined(__APPLE__)
    return '/';
#elif defined(_PPC)
    return'/';
#endif

    return '\\';
}
void vpxt_delete_files(int arg_count, ...)
{
    va_list vl;
    va_start(vl, arg_count);
    tprintf(PRINT_BTH, "\n");
    int i;

    for (i = 0; i < arg_count; ++i)
    {
        std::string file_to_delete = va_arg(vl, const char *);

        if (remove(file_to_delete.c_str()) == 0)
            tprintf(PRINT_BTH, "* %s - Successfully Deleted\n\n",
            file_to_delete.c_str());
        else
            tprintf(PRINT_BTH, "- Error: %s - Not Deleted\n\n",
            file_to_delete.c_str());
    }

    va_end(vl);
}
void vpxt_delete_files_quiet(int arg_count, ...)
{
    va_list vl;
    va_start(vl, arg_count);
    tprintf(PRINT_BTH, "\n");
    int i;

    for (i = 0; i < arg_count; ++i)
    {
        std::string file_to_delete = va_arg(vl, const char *);
        remove(file_to_delete.c_str());
    }

    va_end(vl);
}
void vpxt_copy_file(const char *input, const char *output)
{
    std::ifstream input_file(input, std::ios::binary);
    std::ofstream output_file(output, std::ios::binary);
    output_file << input_file.rdbuf();

    input_file.close();
    output_file.close();

    return;
}
unsigned int vpxt_get_high_res_timer_tick()
{
#if defined(_WIN32)
    LARGE_INTEGER pf;                       // Performance Counter Frequency
    LARGE_INTEGER current_time;
    unsigned int current_tick;

    if (QueryPerformanceFrequency(&pf))
    {
        // read the counter and TSC at start
        QueryPerformanceCounter(&current_time);
        current_tick  = current_time.LowPart;
    }
    else
    {
        current_tick = timeGetTime();
    }

    return current_tick;
#else
    struct timeval tv;
    gettimeofday(&tv, 0);
    return (0xffffffff & (tv.tv_sec * 1000000 + tv.tv_usec)) ;
#endif
}
unsigned int vpxt_get_time_in_micro_sec(unsigned int start_tick,
                                        unsigned int stop_tick)
{
#if defined(_WIN32)
    LARGE_INTEGER pf;
    uint64_t duration = stop_tick - start_tick;

    if (QueryPerformanceFrequency(&pf))
        return (unsigned int)(duration * 1000000 /  pf.LowPart);
    else
        return (unsigned int)(duration * 1000);

#else
    long long duration = stop_tick - start_tick;
    return duration;
#endif
}
unsigned int vpxt_get_cpu_tick()
{
#if defined(_WIN32)

    FILETIME creation_time;
    FILETIME exit_time;
    FILETIME kernel_time;
    FILETIME user_time;

    GetProcessTimes(GetCurrentProcess(), &creation_time, &exit_time,
        &kernel_time, &user_time);

    // user_time is meaured in groupings of 100 nano seconds, so 10^-9 * 100
    // = 10^-7 so to get to 10^-6 res divide by 10
    return user_time.dwLowDateTime / 10;

#else
    return clock();
#endif
}
unsigned int vpxt_get_time()
{
    unsigned int time = 0;
    time = vpxt_get_high_res_timer_tick();
    return time;
}
int vpxt_get_cur_dir(std::string &CurrentDir)
{
#if defined(_WIN32)
    TCHAR current_dir_char[MAX_PATH] = "";

    if (!::GetCurrentDirectory(sizeof(current_dir_char) - 1, current_dir_char))
    {
        tprintf(PRINT_BTH, "Could not get current directory");
        return 0;
    }

    CurrentDir = current_dir_char;
#else
    char current_dir_char[256];
    getcwd(current_dir_char, 256);
    CurrentDir = current_dir_char;
#endif
    return 0;
}
int vpxt_make_dir(std::string create_dir)
{
#if defined(_WIN32)
    /////////////////////////////////////
    create_dir.insert(0, "mkdir \"");
    create_dir += "\"";
    system(create_dir.c_str());
    /////////////////////////////////////
#elif defined(linux)
    create_dir.insert(0, "mkdir -p \"");
    create_dir += "\"";
    system(create_dir.c_str());
#elif defined(__APPLE__)
    create_dir.insert(0, "mkdir -p \"");
    create_dir += "\"";
    system(create_dir.c_str());
#elif defined(_PPC)
    create_dir.insert(0, "mkdir -p \"");
    create_dir += "\"";
    system(create_dir.c_str());
#endif
    return 0;
}
int vpxt_make_dir_vpx(std::string create_dir_2)
{
#if defined(_WIN32)
    /////////////////////////////////////
    create_dir_2.erase(0, 4);
    create_dir_2.insert(0, "mkdir \"");
    create_dir_2 += "\"";
    system(create_dir_2.c_str());
    /////////////////////////////////////
#elif defined(linux)
    create_dir_2.erase(0, 4);
    create_dir_2.insert(0, "mkdir -p \"");
    create_dir_2 += "\"";
    system(create_dir_2.c_str());
#elif defined(__APPLE__)
    create_dir_2.erase(0, 4);
    create_dir_2.insert(0, "mkdir -p \"");
    create_dir_2 += "\"";
    system(create_dir_2.c_str());
#elif defined(_PPC)
    create_dir_2.erase(0, 4);
    create_dir_2.insert(0, "mkdir -p \"");
    create_dir_2 += "\"";
    system(create_dir_2.c_str());
#endif

    return 0;
}

void vpxt_run_exe(std::string run_exe)
{
    tprintf(PRINT_STD, "\nAtempting to run: %s\n\n", run_exe.c_str());
#if defined(_WIN32)
    system(run_exe.c_str());
#elif defined(linux)
    system(run_exe.c_str());
#elif defined(__APPLE__)
    system(run_exe.c_str());
#elif defined(_PPC)
    system(run_exe.c_str());
#endif

    return;
}
int vpxt_list_files_in_dir(std::vector<std::string> &file_name_vector,
                           std::string directory)
{
#if defined(_WIN32)
    std::string RawDirectory = directory + "\\";
    directory += "\\*";
    WIN32_FIND_DATA FileData;
    HANDLE hFind;
    std::string FileName;
    hFind = FindFirstFile(directory.c_str(), &FileData);

    while (FileName.compare(FileData.cFileName) != 0)
    {
        FileName = FileData.cFileName;
        std::string FullFileName = RawDirectory + FileName;
        file_name_vector.push_back(FullFileName);
        FindNextFile(hFind, &FileData);
    }

#else
    DIR *FileData;
    struct dirent *hFind;
    std::string FileName;
    FileData = opendir(directory.c_str());
    hFind = readdir(FileData);

    while (FileName.compare(hFind->d_name) != 0)
    {
        FileName = hFind->d_name;
        file_name_vector.push_back(FileName);
        hFind = readdir(FileData);

        if (hFind == NULL)
        {
            break;
        }
    }

    closedir(FileData);
#endif
    return 0;
}
int vpxt_add_dir_files_to_ignore(std::vector<std::string> &IgnoredFiles,
                                 std::string directory)
{

#if defined(_WIN32)
    WIN32_FIND_DATA FileData;
    HANDLE hFind;
    std::string FileName;

    hFind = FindFirstFile(directory.c_str(), &FileData);

    while (FileName.compare(FileData.cFileName) != 0)
    {
        FileName = FileData.cFileName;
        IgnoredFiles.push_back(FileName);
        FindNextFile(hFind, &FileData);
    }

#else
    // Record all files in the current directory.
    DIR *FileData;
    struct dirent *hFind;
    std::string FileName;

    FileData = opendir(directory.c_str());
    hFind = readdir(FileData);

    while (FileName.compare(hFind->d_name) != 0)
    {
        FileName = hFind->d_name;
        IgnoredFiles.push_back(FileName);

        hFind = readdir(FileData);

        if (hFind == NULL)
        {
            break;
        }
    }

    closedir(FileData);
#endif

    return 0;
}
int vpxt_find_non_ignored_files_in_dir(std::vector<std::string> IgnoredFiles,
                                       std::vector<std::string> &FilesFound,
                                       std::string directory)
{
    // Function returns the number of non ignored files found on success
    // -1 on error

#if defined(_WIN32)
    int Fail = 0;
    std::string FileName;
    WIN32_FIND_DATA FileData;
    HANDLE hFind;

    hFind = FindFirstFile(directory.c_str(), &FileData);

    while (FileName.compare(FileData.cFileName) != 0)
    {
        FileName = FileData.cFileName;

        unsigned int CurVecPos = 0;
        int IgnoreFile = 0;

        while (CurVecPos < IgnoredFiles.size())
        {
            if (IgnoredFiles[CurVecPos].compare(FileName.c_str()) == 0)
            {
                IgnoreFile = 1;
            }

            CurVecPos++;
        }

        if (IgnoreFile == 0)
        {
            FilesFound.push_back(FileName);
        }

        FindNextFile(hFind, &FileData);
    }

#else

    DIR *FileData;
    struct dirent *hFind;

    FileData = opendir(directory.c_str());
    hFind = readdir(FileData);

    std::string FileName;
    std::vector<std::string> DestFileExtraVector;

    while (FileName.compare(hFind->d_name) != 0)
    {
        FileName = hFind->d_name;

        int CurVecPos = 0;
        int IgnoreFile = 0;

        while (CurVecPos < IgnoredFiles.size())
        {
            if (IgnoredFiles[CurVecPos].compare(FileName.c_str()) == 0)
            {
                IgnoreFile = 1;
            }

            CurVecPos++;
        }

        if (IgnoreFile == 0)
        {
            FilesFound.push_back(FileName);
        }

        hFind = readdir(FileData);

        if (hFind == NULL)
        {
            break;
        }
    }

    closedir(FileData);

#endif

    return FilesFound.size();
}
// -------------------Evaluation------------------------------------------------
int vpxt_yv12_alloc_frame_buffer(YV12_BUFFER_CONFIG *ybf,
                                 int width,
                                 int height,
                                 int border)
{
/*NOTE:*/

    if (ybf)
    {
        int y_stride = ((width + 2 * border) + 31) & ~31;
        int yplane_size = (height + 2 * border) * y_stride;
        int uv_width = width >> 1;
        int uv_height = height >> 1;
        /** There is currently a bunch of code which assumes
          *  uv_stride == y_stride/2, so enforce this here. */
        int uv_stride = y_stride >> 1;
        int uvplane_size = (uv_height + border) * uv_stride;

        vp8_yv12_de_alloc_frame_buffer(ybf);

        /** Only support allocating buffers that have a height and width that
          *  are multiples of 16, and a border that's a multiple of 32.
          * The border restriction is required to get 16-byte alignment of the
          *  start of the chroma rows without intoducing an arbitrary gap
          *  between planes, which would break the semantics of things like
          *  vpx_img_set_rect(). */
        /*if ((width & 0xf) | (height & 0xf) | (border & 0x1f))
            return -3;*/

        ybf->y_width  = width;
        ybf->y_height = height;
        ybf->y_stride = y_stride;

        ybf->uv_width = uv_width;
        ybf->uv_height = uv_height;
        ybf->uv_stride = uv_stride;

        ybf->border = border;
        ybf->frame_size = yplane_size + 2 * uvplane_size;

        ybf->buffer_alloc = (unsigned char *) vpx_memalign(32, ybf->frame_size);

        if (ybf->buffer_alloc == NULL)
            return -1;

        ybf->y_buffer = ybf->buffer_alloc + (border * y_stride) + border;
        ybf->u_buffer = ybf->buffer_alloc + yplane_size + (border / 2  *
            uv_stride) + border / 2;
        ybf->v_buffer = ybf->buffer_alloc + yplane_size + uvplane_size +
            (border / 2  * uv_stride) + border / 2;

        ybf->corrupted = 0; /* assume not currupted by errors */
    }
    else
    {
        return -2;
    }

    return 0;
}

int image2yuvconfig(const vpx_image_t   *img, YV12_BUFFER_CONFIG  *yv12)
{
    yv12->buffer_alloc = img->planes[PLANE_Y];
    yv12->y_buffer = img->planes[PLANE_Y];
    yv12->u_buffer = img->planes[PLANE_U];
    yv12->v_buffer = img->planes[PLANE_V];

    yv12->y_width  = img->d_w;
    yv12->y_height = img->d_h;
    yv12->uv_width = (1 + yv12->y_width) / 2;
    yv12->uv_height = (1 + yv12->y_height) / 2;

    yv12->y_stride = img->stride[PLANE_Y];
    yv12->uv_stride = img->stride[PLANE_U];

    yv12->border  = (img->stride[PLANE_Y] - img->w) / 2;

    return 0;
}
int yuvconfig2image(vpx_image_t *img,
                    const YV12_BUFFER_CONFIG  *yv12,
                    void *user_priv)
{
    /** vpx_img_wrap() doesn't allow specifying independent strides for
      * the Y, U, and V planes, nor other alignment adjustments that
      * might be representable by a YV12_BUFFER_CONFIG, so we just
      * initialize all the fields.*/
    img->fmt = VPX_IMG_FMT_I420;
    img->w = yv12->y_stride;
    img->h = (yv12->y_height + 2 * VP8BORDERINPIXELS + 15) & ~15;
    img->d_w = yv12->y_width;
    img->d_h = yv12->y_height;
    img->x_chroma_shift = 1;
    img->y_chroma_shift = 1;
    img->planes[VPX_PLANE_Y] = yv12->y_buffer;
    img->planes[VPX_PLANE_U] = yv12->u_buffer;
    img->planes[VPX_PLANE_V] = yv12->v_buffer;
    img->planes[VPX_PLANE_ALPHA] = NULL;
    img->stride[VPX_PLANE_Y] = yv12->y_stride;
    img->stride[VPX_PLANE_U] = yv12->uv_stride;
    img->stride[VPX_PLANE_V] = yv12->uv_stride;
    img->stride[VPX_PLANE_ALPHA] = yv12->y_stride;
    img->bps = 12;
    img->user_priv = user_priv;
    img->img_data = yv12->buffer_alloc;
    img->img_data_owner = 0;
    img->self_allocd = 0;

    return 0;
}
double vpxt_psnr(const char *input_file1,
                 const char *input_file2,
                 int force_uvswap,
                 int print_out,
                 int print_embl,
                 int deblock_level,
                 int noise_level,
                 int flags,
                 double *ssim_out,
                 int& potential_artifact)
{
    double summed_quality = 0;
    double summed_weights = 0;
    double summed_psnr = 0;
    double summed_ypsnr = 0;
    double summed_upsnr = 0;
    double summed_vpsnr = 0;
    double sum_sq_error = 0;
    double sum_bytes = 0;
    double sum_bytes2 = 0;

    int run_potential_artifact = potential_artifact;

    unsigned int             decoded_frames = 0;
    int                      raw_offset = 0;
    int                      comp_offset = 0;
    unsigned int             maximumFrameCount = 0;
    YV12_BUFFER_CONFIG       raw_yv12;
    YV12_BUFFER_CONFIG       comp_yv12;
    vpx_image_t              raw_img;
    unsigned int             frameCount = 0;
    unsigned int             file_type = FILE_TYPE_RAW;
    unsigned int             fourcc    = 808596553;
    struct                   detect_buffer detect;
    unsigned int             raw_width = 0;
    unsigned int             raw_height = 0;
    unsigned int             raw_rate = 0;
    unsigned int             raw_scale = 0;
    y4m_input                y4m;
    int                      arg_have_framerate = 0;
    struct vpx_rational      arg_framerate = {30, 1};
    int                      arg_use_i420 = 1;

    force_uvswap = 0;
    //////////////////////// Initilize Raw File ////////////////////////
    FILE *raw_file = strcmp(input_file1, "-") ?
        fopen(input_file1, "rb") : set_binary_mode(stdin);

    if (!raw_file)
    {
        tprintf(print_out, "Failed to open input file: %s", input_file1);
        return -1;
    }

    detect.buf_read = fread(detect.buf, 1, 4, raw_file);
    detect.position = 0;

    if (detect.buf_read == 4 && file_is_y4m(raw_file, &y4m, detect.buf))
    {
        if (y4m_input_open(&y4m, raw_file, detect.buf, 4) >= 0)
        {
            file_type = FILE_TYPE_Y4M;
            raw_width = y4m.pic_w;
            raw_height = y4m.pic_h;
            raw_rate = y4m.fps_n;
            raw_scale = y4m.fps_d;

            /* Use the frame rate from the file only if none was specified
            * on the command-line.
            */
            if (!arg_have_framerate)
            {
                arg_framerate.num = y4m.fps_n;
                arg_framerate.den = y4m.fps_d;
            }

            arg_use_i420 = 0;
        }
        else
        {
            tprintf(print_out, "Unsupported Y4M stream.\n");
            return EXIT_FAILURE;
        }
    }
    else if (detect.buf_read == 4 &&
        file_is_ivf(raw_file, &fourcc, &raw_width, &raw_height, &detect,
            &raw_scale, &raw_rate))
    {
        switch (fourcc)
        {
        case 0x32315659:
            arg_use_i420 = 0;
            break;
        case 0x30323449:
            arg_use_i420 = 1;
            break;
        default:
            tprintf(print_out, "Unsupported fourcc (%08x) in IVF\n", fourcc);
            return EXIT_FAILURE;
        }

        file_type = FILE_TYPE_IVF;
    }
    else
    {
        file_type = FILE_TYPE_RAW;
    }

    if (!raw_width || !raw_height)
    {
        tprintf(print_out, "Specify stream dimensions with --width (-w) "
            " and --height (-h).\n");
        return EXIT_FAILURE;
    }

    if(file_type != FILE_TYPE_Y4M)
        vpx_img_alloc(&raw_img, IMG_FMT_I420, raw_width, raw_height, 1);

    // Burn Frames untill Raw frame offset reached - currently disabled by
    // override of raw_offset
    if (raw_offset > 0)
    {
        for (int i = 0; i < raw_offset; i++)
        {
        }
    }

    // I420 hex-0x30323449 dec-808596553
    // YV12 hex-0x32315659 dec-842094169
    // if YV12 Do not swap Frames
    if (fourcc == 842094169)
        force_uvswap = 1;

    //////////////////////// Initilize Compressed File ////////////////////////
    /* Open file */
    FILE *comp_file = strcmp(input_file2, "-") ?
        fopen(input_file2, "rb") : set_binary_mode(stdin);

    if (!comp_file)
    {
        tprintf(print_out, "Failed to open input file: %s", input_file2);
        return -1;
    }

    unsigned int            comp_fourcc;
    unsigned int            comp_width;
    unsigned int            comp_height;
    unsigned int            comp_scale;
    unsigned int            comp_rate;
    struct input_ctx        input;

    input.chunk = 0;
    input.chunks = 0;
    input.infile = NULL;
    input.kind = RAW_FILE;
    input.nestegg_ctx = 0;
    input.pkt = 0;
    input.video_track = 0;

    input.infile = comp_file;

    if (file_is_ivf_dec(comp_file, &comp_fourcc, &comp_width, &comp_height,
        &comp_scale, &comp_rate))
        input.kind = IVF_FILE;
    else if (file_is_webm(&input, &comp_fourcc, &comp_width, &comp_height,
        &comp_scale, &comp_rate))
        input.kind = WEBM_FILE;
    else if (file_is_raw(comp_file, &comp_fourcc, &comp_width, &comp_height,
        &comp_scale, &comp_rate))
        input.kind = RAW_FILE;
    else
    {
        tprintf(print_out, "Unrecognized input file type.\n");
        return EXIT_FAILURE;
    }

    if (input.kind == WEBM_FILE)
        if (webm_guess_framerate(&input, &comp_scale, &comp_rate))
        {
            tprintf(print_out, "Failed to guess framerate -- error parsing "
                "webm file?\n");
            return EXIT_FAILURE;
        }

        // Burn Frames untill Compressed frame offset reached - currently
        // disabled by override of comp_offset
        if (comp_offset > 0)
        {
        }

        ////////////////////////////////////////////////////////////////////////
        //////// Printing ////////
        if (print_embl)
            if(run_potential_artifact)
                tprintf(print_out, "\n\n                        "
                "---------Computing PSNR---------\n"
                "                             With Artifact Detection");
            else
                tprintf(print_out, "\n\n                        "
                "---------Computing PSNR---------");
        else
            if(run_potential_artifact)
                tprintf(print_out, "\nArtifact Detection On");


        tprintf(print_out, "\n\nComparing %s to %s:\n                        \n"
            , input_file1, input_file2);
        ////////////////////////

        vpx_codec_ctx_t       decoder;
        vpx_codec_iface_t       *iface = NULL;
        const struct codec_item  *codec = codecs;
        vpx_codec_iface_t  *ivf_iface = ifaces[0].iface;
        vpx_codec_dec_cfg_t     cfg = {0};
        iface = ivf_iface;

        vp8_postproc_cfg_t ppcfg = {0};

        ppcfg.deblocking_level = deblock_level;
        ppcfg.noise_level = noise_level;
        ppcfg.post_proc_flag = flags;

        if(!deblock_level && !noise_level && !flags)
        {
            if (vpx_codec_dec_init(&decoder, ifaces[0].iface, &cfg, 0))
            {
                tprintf(print_out, "Failed to initialize decoder: %s\n",
                    vpx_codec_error(&decoder));

                fclose(raw_file);
                fclose(comp_file);
                vpx_img_free(&raw_img);
                return EXIT_FAILURE;
            }
        }
        else
        {
            if (vpx_codec_dec_init(&decoder, ifaces[0].iface, &cfg,
                VPX_CODEC_USE_POSTPROC))
            {
                tprintf(print_out, "Failed to initialize decoder: %s\n",
                    vpx_codec_error(&decoder));
                fclose(raw_file);
                fclose(comp_file);
                vpx_img_free(&raw_img);
                return EXIT_FAILURE;
            }

            if (vpx_codec_control(&decoder, VP8_SET_POSTPROC, &ppcfg) != 0)
            {
                tprintf(print_out, "Failed to update decoder post processor "
                    "settings\n");
            }
        }

        /////////// Setup Temp YV12 Buffer to Resize if nessicary //////////////
        initialize_scaler();

        YV12_BUFFER_CONFIG temp_yv12;
        YV12_BUFFER_CONFIG temp_yv12b;

        memset(&temp_yv12, 0, sizeof(temp_yv12));
        memset(&temp_yv12b, 0, sizeof(temp_yv12b));
        ////////////////////////////////////////////////////////////////////////

        vpx_codec_control(&decoder, VP8_SET_POSTPROC, &ppcfg);

        uint8_t *comp_buff = NULL;
        size_t buf_sz = 0, buf_alloc_sz = 0;
        int current_raw_frame = 0;
        int comp_frame_available = 1;

        uint64_t raw_timestamp = 0;
        uint64_t comp_timestamp = 0;

        while (comp_frame_available)
        {
            buf_sz = 0;

            if(read_frame_dec(&input, &comp_buff, &buf_sz, &buf_alloc_sz,
                &comp_timestamp))
                comp_frame_available = 0;

            unsigned long lpdwFlags = 0;
            unsigned long lpckid = 0;
            long bytes1;
            long bytes2;
            int resized_frame = 0;
            int dropped_frame = 0;
            int non_visible_frame = 0;

            if (input.kind == WEBM_FILE){
                raw_timestamp = current_raw_frame * 1000 * raw_scale / raw_rate;
                raw_timestamp = raw_timestamp * 1000000;
            }
            else
                raw_timestamp = current_raw_frame;

            bytes2 = buf_sz;
            sum_bytes2 += bytes2;

            vpx_codec_iter_t  iter = NULL;
            vpx_image_t    *img;

            // make sure the timestamps sync otherwise process
            // last shown compressed frame vs current raw frame
            // for psnr calculations, turn off artifact detection
            while(raw_timestamp <= comp_timestamp || !comp_frame_available)
            {
                if(comp_timestamp == raw_timestamp)
                {
                    dropped_frame = 0;

                    if (vpx_codec_decode(&decoder, (uint8_t *) comp_buff,
                        buf_sz, NULL, 0))
                    {
                        const char *detail = vpx_codec_error_detail(&decoder);
                        tprintf(print_out, "Failed to decode frame: %s\n",
                            vpx_codec_error(&decoder));
                    }

                    if (img = vpx_codec_get_frame(&decoder, &iter))
                    {
                        ++decoded_frames;

                        image2yuvconfig(img, &comp_yv12);

                        int TempBuffState = vpxt_yv12_alloc_frame_buffer(
                            &temp_yv12, comp_yv12.y_width, comp_yv12.y_height,
                            VP8BORDERINPIXELS);

                        vp8_yv12_copy_frame_c(&comp_yv12, &temp_yv12);

                        int resize_frame_width = 0;
                        int resize_frame_height = 0;

                        // if frame not correct size resize it for psnr
                        if (img->d_w != raw_width || img->d_h != raw_height)
                        {
                            if (TempBuffState < 0)
                            {
                                tprintf(print_out, "Could not allocate yv12 "
                                    "buffer for %i x %i\n", raw_width,
                                    raw_height);

                                fclose(raw_file);
                                fclose(comp_file);
                                vpx_img_free(&raw_img);

                                if (input.nestegg_ctx)
                                    nestegg_destroy(input.nestegg_ctx);

                                if (input.kind != WEBM_FILE)
                                    free(comp_buff);

                                return 0;
                            }

                            int gcd_width = vpxt_gcd(img->d_w, raw_width);
                            int gcd_height = vpxt_gcd(img->d_h, raw_height);

                            // Possible Scales
                            /* 4-5 Scale in Width direction */
                            /* 3-4 Scale in Width direction */
                            /* 2-3 Scale in Width direction */
                            /* 3-5 Scale in Width direction */
                            /* 1-2 Scale in Width direction */
                            /* no scale in Width direction */

                            int resize_width = 0;
                            int resize_height = 0;
                            int width_scale = 0;
                            int height_scale = 0;
                            int width_ratio = 0;
                            int height_ratio = 0;

                            if((img->d_w / gcd_width) == 1){
                                if((raw_width / gcd_width) % 2 == 0){
                                    width_scale = 2;
                                    width_ratio = 1;
                                    resize_width = (raw_width / gcd_width) / 2;
                                }
                            }
                            else if((img->d_w / gcd_width) == 2){
                                if((raw_width / gcd_width) % 3 == 0){
                                    width_scale = 3;
                                    width_ratio = 2;
                                    resize_width = (raw_width / gcd_width) / 3;
                                }
                            }
                            else if((img->d_w / gcd_width) == 3){
                                if((raw_width / gcd_width) % 4 == 0){
                                    width_scale = 4;
                                    width_ratio = 3;
                                    resize_width = (raw_width / gcd_width) / 4;
                                }
                                else if((raw_width / gcd_width) % 5 == 0){
                                    width_scale = 5;
                                    width_ratio = 3;
                                    resize_width = (raw_width / gcd_width) / 5;
                                }
                            }
                            else if((img->d_w / gcd_width) == 4){
                                if((raw_width / gcd_width) % 5 == 0){
                                    width_scale = 5;
                                    width_ratio = 4;
                                    resize_width = (raw_width / gcd_width) / 5;
                                }
                            }
                            else{
                                tprintf(print_out, "No scale match found for"
                                    " width \n");
                                return 0;
                            }

                            if((img->d_h / gcd_height) == 1){
                                if((raw_height / gcd_height) % 2 == 0){
                                    height_scale = 2;
                                    height_ratio = 1;
                                    resize_height = (raw_height / gcd_height)/2;
                                }
                            }
                            else if((img->d_h / gcd_height) == 2){
                                if((raw_height / gcd_height) % 3 == 0){
                                    height_scale = 3;
                                    height_ratio = 2;
                                    resize_height = (raw_height / gcd_height)/3;
                                }
                            }
                            else if((img->d_h / gcd_height) == 3){
                                if((raw_height / gcd_height) % 4 == 0){
                                    height_scale = 4;
                                    height_ratio = 3;
                                    resize_height = (raw_height / gcd_height)/4;
                                }
                                else if((raw_height / gcd_height) % 5 == 0){
                                    height_scale = 5;
                                    height_ratio = 3;
                                    resize_height = (raw_height / gcd_height)/5;
                                }
                            }
                            else if((img->d_h / gcd_height) == 4){
                                if((raw_height / gcd_height) % 5 == 0){
                                    height_scale = 5;
                                    height_ratio = 4;
                                    resize_height = (raw_height / gcd_height)/5;
                                }
                            }
                            else{
                                tprintf(print_out, "No scale match found for "
                                    "height \n");
                                return 0;
                            }

                            // resize YV12 untill it is scaled properly.
                            while(resize_width || resize_height){

                                resized_frame = 1;

                                if(resize_width)
                                    resize_frame_width = raw_width - (raw_width/
                                    (width_scale/width_ratio))*(resize_width-1);
                                else
                                    resize_frame_width = img->d_w;

                                if(resize_height)
                                    resize_frame_height = raw_height -
                                    (raw_height / (height_scale/height_ratio))
                                    *(resize_height-1);
                                else
                                    resize_frame_height = img->d_h;

                                vpxt_yv12_alloc_frame_buffer(&temp_yv12b,
                                    resize_frame_width, resize_frame_height,
                                    VP8BORDERINPIXELS);

                                // if resize width or height is done but still
                                // need to resize the other set finished to 1:1
                                if(!resize_width){
                                    width_scale = 1;
                                    width_ratio = 1;
                                }
                                if(!resize_height){
                                    height_scale = 1;
                                    height_ratio = 1;
                                }

                                vp8_yv12_scale_or_center(&temp_yv12,
                                    &temp_yv12b, resize_frame_width,
                                    resize_frame_height, 0, width_scale,
                                    width_ratio, height_scale, height_ratio);

                                if(resize_width)
                                    resize_width--;
                                if(resize_height)
                                    resize_height--;

                                vpxt_yv12_alloc_frame_buffer(&temp_yv12,
                                    resize_frame_width, resize_frame_height,
                                    VP8BORDERINPIXELS);
                                vp8_yv12_copy_frame_c(&temp_yv12b, &temp_yv12);
                            }

                            comp_yv12 = temp_yv12;
                        }
                    }
                }
                else
                    dropped_frame = 1;

                // run psnr if img returned(skip non visibile frames), dropped
                // frame detected or if end of comp input file detected.
                if(img || dropped_frame || !comp_frame_available)
                {
                    //////////////// Get YV12 Data For Raw File ////////////////
                    // if end of uncompressed file break out
                    if(!read_frame_enc(raw_file, &raw_img, file_type, &y4m,
                        &detect))
                        break;

                    image2yuvconfig(&raw_img, &raw_yv12);
                    if (force_uvswap == 1){
                        unsigned char *temp = raw_yv12.u_buffer;
                        raw_yv12.u_buffer = raw_yv12.v_buffer;
                        raw_yv12.v_buffer = temp;
                    }
                    bytes1 = (raw_width * raw_height * 3) / 2;
                    sum_bytes += bytes1;
                    current_raw_frame = current_raw_frame + 1;
                    if (input.kind == WEBM_FILE){
                        raw_timestamp = current_raw_frame * 1000 * raw_scale /
                            raw_rate;
                        raw_timestamp = raw_timestamp * 1000000;
                    }
                    else
                        raw_timestamp = current_raw_frame;

                    ///////////////////////// Preform PSNR Calc ////////////////
                    if (ssim_out)
                    {
                        double weight;
                        double this_ssim = VP8_CalcSSIM_Tester(&raw_yv12,
                            &comp_yv12, 1, &weight);
                        summed_quality += this_ssim * weight ;
                        summed_weights += weight;
                    }

                    int pa;
                    double ypsnr = 0.0;
                    double upsnr = 0.0;
                    double vpsnr = 0.0;
                    double sq_error = 0.0;

                    // if no drop frame and no resized frame and not dont run
                    if((!dropped_frame && !resized_frame) && potential_artifact
                        != kDontRunArtifactDetection)
                        pa = kRunArtifactDetection;
                    else
                        pa = kDontRunArtifactDetection;

                    double this_psnr = vp8_calcpsnr_tester(&raw_yv12,
                        &comp_yv12, &ypsnr, &upsnr, &vpsnr, &sq_error,
                        PRINT_NONE, pa);

                    summed_ypsnr += ypsnr;
                    summed_upsnr += upsnr;
                    summed_vpsnr += vpsnr;
                    summed_psnr += this_psnr;
                    sum_sq_error += sq_error;
                    ////////////////////////////////////////////////////////////

                    //////// Printing ////////
                    tprintf(print_out, "F:%5d, 1:%6.0f 2:%6.0f, Avg :%5.2f, "
                        "Y:%5.2f, U:%5.2f, V:%5.2f",
                        current_raw_frame,
                        bytes1 * 8.0,
                        bytes2 * 8.0,
                        this_psnr, 1.0 * ypsnr ,
                        1.0 * upsnr ,
                        1.0 * vpsnr);

                    if(dropped_frame)
                        tprintf(print_out, " D");

                    if(resized_frame)
                        tprintf(print_out, " R");

                    if(pa == kPossibleArtifactFound &&
                        !dropped_frame && !resized_frame){
                        tprintf(print_out, " PA");
                        potential_artifact = kPossibleArtifactFound;
                    }

                    tprintf(print_out, "\n");
                    ////////////////////////
                }
                else
                {
                    // if time stamps do not match find out why if droped
                    // frame process psnr on last frame keep trying
                    // subtract one unit to move calculation along.
                    // if invisible frame get next frame and keep trying.
                    VP8_HEADER oz;
                    memcpy(&oz, comp_buff, 3);

                    if (oz.show_frame){
                        current_raw_frame = current_raw_frame - 1;

                        if (input.kind == WEBM_FILE){
                            raw_timestamp = current_raw_frame * 1000 * raw_scale /
                                raw_rate;
                            raw_timestamp = raw_timestamp * 1000000;
                        }
                        else
                            raw_timestamp = current_raw_frame;
                    }
                    else{
                        if(read_frame_dec(&input, &comp_buff, &buf_sz,
                            &buf_alloc_sz, &comp_timestamp))
                                comp_frame_available = 0;

                        raw_timestamp = comp_timestamp;
                        non_visible_frame = 1;
                    }
                }
            }
        }

        // Over All PSNR Calc
        double samples = 3.0 / 2 * current_raw_frame * raw_yv12.y_width *
            raw_yv12.y_height;
        double avg_psnr = summed_psnr / current_raw_frame;
        double total_psnr = vp8_mse_2_psnr_tester(samples, 255.0, sum_sq_error);

        if (summed_weights < 1.0)
            summed_weights = 1.0;

        double total_ssim = 100 * pow(summed_quality / summed_weights, 8.0);

        //////// Printing ////////
        tprintf(print_out, "\nDr1:%8.2f Dr2:%8.2f, Avg: %5.2f, Avg Y: %5.2f, "
            "Avg U: %5.2f, Avg V: %5.2f, Ov PSNR: %8.2f, ",
            sum_bytes * 8.0 / current_raw_frame*(raw_rate) / raw_scale / 1000,
            sum_bytes2 * 8.0 /current_raw_frame*(comp_rate) / comp_scale / 1000,
            avg_psnr,
            1.0 * summed_ypsnr / current_raw_frame,
            1.0 * summed_upsnr / current_raw_frame,
            1.0 * summed_vpsnr / current_raw_frame,
            total_psnr);

        tprintf(print_out, ssim_out ? "SSIM: %8.2f\n" : "SSIM: Not run.",
            total_ssim);

        if (print_embl)
            tprintf(print_out, "\n                        "
                "--------------------------------\n");
        ////////////////////////
        if (ssim_out)
            *ssim_out = total_ssim;

        fclose(raw_file);
        fclose(comp_file);
        vp8_yv12_de_alloc_frame_buffer(&temp_yv12);
        vp8_yv12_de_alloc_frame_buffer(&temp_yv12b);

        if(file_type != FILE_TYPE_Y4M)
            vpx_img_free(&raw_img);

        if(file_type == FILE_TYPE_Y4M)
            y4m_input_close(&y4m);

        vpx_codec_destroy(&decoder);

        if (input.nestegg_ctx)
            nestegg_destroy(input.nestegg_ctx);

        if (input.kind != WEBM_FILE)
            free(comp_buff);

        return total_psnr;
}
double vpxt_psnr_dec(const char *inputFile1,
                     const char *inputFile2,
                     int forceUVswap,
                     int frameStats,
                     int printvar,
                     double *SsimOut,
                     int width,
                     int height)
{
    if (frameStats != 3)
        frameStats = 1; // Overide to print individual frames to screen

    double summedQuality = 0;
    double summedWeights = 0;
    double summedPsnr = 0;
    double summedYPsnr = 0;
    double summedUPsnr = 0;
    double summedVPsnr = 0;
    double sumSqError = 0;
    double sumBytes = 0;
    double sumBytes2 = 0;

    unsigned int currentVideo1Frame = 0;
    int RawFrameOffset = 0;
    int CompressedFrameOffset = 0;
    unsigned int maximumFrameCount = 0;

    int original_forceUVswap = forceUVswap;
    forceUVswap = 0;

    //////////////////////// Initilize Raw File ////////////////////////
    unsigned int frameCount = 0; // ivf_h_raw.length;

    unsigned int             file_type, fourcc;
    struct detect_buffer detect;
    unsigned int RawWidth = 0;
    unsigned int RawHeight = 0;
    unsigned int    RawRate = 0;
    unsigned int    RawScale = 0;
    y4m_input                y4m;
    int                      arg_have_framerate = 0;
    struct vpx_rational      arg_framerate = {30, 1};
    int                      arg_use_i420 = 1;

    FILE *RawFile = strcmp(inputFile1, "-") ? fopen(inputFile1, "rb") :
        set_binary_mode(stdin);

    if (!RawFile)
    {
        tprintf(PRINT_BTH, "Failed to open input file: %s", inputFile1);
        return -1;
    }

    detect.buf_read = fread(detect.buf, 1, 4, RawFile);
    detect.position = 0;

    if (detect.buf_read == 4 && file_is_y4m(RawFile, &y4m, detect.buf))
    {
        if (y4m_input_open(&y4m, RawFile, detect.buf, 4) >= 0)
        {
            file_type = FILE_TYPE_Y4M;
            RawWidth = y4m.pic_w;
            RawHeight = y4m.pic_h;
            RawRate = y4m.fps_n;
            RawScale = y4m.fps_d;

            /* Use the frame rate from the file only if none was specified
            * on the command-line.
            */
            if (!arg_have_framerate)
            {
                arg_framerate.num = y4m.fps_n;
                arg_framerate.den = y4m.fps_d;
            }

            arg_use_i420 = 0;
        }
        else
        {
            fprintf(stderr, "Unsupported Y4M stream.\n");
            return EXIT_FAILURE;
        }
    }
    else if (detect.buf_read == 4 &&
             file_is_ivf(RawFile, &fourcc, &RawWidth, &RawHeight, &detect,
             &RawScale, &RawRate))
    {
        file_type = FILE_TYPE_IVF;

        switch (fourcc)
        {
        case 0x32315659:
            arg_use_i420 = 0;
            break;
        case 0x30323449:
            arg_use_i420 = 1;
            break;
        default:
            fprintf(stderr, "Unsupported fourcc (%08x) in IVF\n", fourcc);
            return EXIT_FAILURE;
        }
    }
    else
    {
        file_type = FILE_TYPE_RAW;
        RawWidth = width;
        RawHeight = height;
    }

    if (!RawWidth || !RawHeight)
    {
        fprintf(stderr, "\n     No width or height specified\n");
        return EXIT_FAILURE;
    }

    vpx_image_t    raw_img;
    vpx_img_alloc(&raw_img, IMG_FMT_I420, RawWidth, RawHeight, 1);

    YV12_BUFFER_CONFIG Raw_YV12;
    Raw_YV12.y_width   = raw_img.d_w;
    Raw_YV12.y_height  = raw_img.d_h;
    Raw_YV12.y_stride  = raw_img.stride[PLANE_Y];
    Raw_YV12.uv_width  = (1 + Raw_YV12.y_width) / 2;
    Raw_YV12.uv_height = (1 + Raw_YV12.y_height) / 2;
    Raw_YV12.uv_stride = raw_img.stride[PLANE_U];
    Raw_YV12.buffer_alloc        = raw_img.img_data;
    Raw_YV12.y_buffer           = raw_img.img_data;
    Raw_YV12.u_buffer = raw_img.planes[PLANE_U];
    Raw_YV12.v_buffer = raw_img.planes[PLANE_V];

    // Burn Frames untill Raw frame offset reached - currently disabled by
    // override of RawFrameOffset
    if (RawFrameOffset > 0)
    {
        for (int i = 0; i < RawFrameOffset; i++)
        {
        }
    }

    // I420 hex-0x30323449 dec-808596553
    // YV12 hex-0x32315659 dec-842094169

    if (fourcc == 842094169)
        forceUVswap = 1;   // if YV12 Do not swap Frames

    if (forceUVswap == 1)
    {
        unsigned char *temp = Raw_YV12.u_buffer;
        Raw_YV12.u_buffer = Raw_YV12.v_buffer;
        Raw_YV12.v_buffer = temp;
    }

    //////////////////////// Initilize CompRaw File ////////////////////////
    // unsigned int frameCount = 0; // ivf_h_raw.length;

    unsigned int             compraw_file_type, compraw_fourcc;
    struct detect_buffer compraw_detect;
    unsigned int compraw_width = 0;
    unsigned int compraw_height = 0;
    unsigned int    compraw_rate = 0;
    unsigned int    compraw_scale = 0;
    y4m_input                compraw_y4m;
    int                      compraw_arg_have_framerate = 0;
    struct vpx_rational      compraw_arg_framerate = {30, 1};
    int                      compraw_arg_use_i420 = 1;

    FILE *compraw_file = strcmp(inputFile2, "-") ? fopen(inputFile2, "rb") :
        set_binary_mode(stdin);

    if (!compraw_file)
    {
        tprintf(PRINT_BTH, "Failed to open input file: %s", inputFile1);
        return -1;
    }

    compraw_detect.buf_read = fread(compraw_detect.buf, 1, 4, compraw_file);
    compraw_detect.position = 0;

    if (compraw_detect.buf_read == 4 && file_is_y4m(compraw_file, &compraw_y4m,
        compraw_detect.buf))
    {
        if (y4m_input_open(&compraw_y4m, compraw_file, compraw_detect.buf, 4)
            >= 0)
        {
            compraw_file_type = FILE_TYPE_Y4M;
            compraw_width = compraw_y4m.pic_w;
            compraw_height = compraw_y4m.pic_h;
            compraw_rate = compraw_y4m.fps_n;
            compraw_scale = compraw_y4m.fps_d;

            /* Use the frame rate from the file only if none was specified
            * on the command-line.
            */
            if (!compraw_arg_have_framerate)
            {
                compraw_arg_framerate.num = compraw_y4m.fps_n;
                compraw_arg_framerate.den = compraw_y4m.fps_d;
            }

            compraw_arg_use_i420 = 0;
        }
        else
        {
            fprintf(stderr, "Unsupported Y4M stream.\n");
            return EXIT_FAILURE;
        }
    }
    else if (compraw_detect.buf_read == 4 &&
             file_is_ivf(compraw_file, &compraw_fourcc, &compraw_width,
             &compraw_height, &compraw_detect, &RawScale, &compraw_rate))
    {
        compraw_file_type = FILE_TYPE_IVF;

        switch (compraw_fourcc)
        {
        case 0x32315659:
            compraw_arg_use_i420 = 0;
            break;
        case 0x30323449:
            compraw_arg_use_i420 = 1;
            break;
        default:
            fprintf(stderr, "Unsupported fourcc (%08x) in IVF\n", fourcc);
            return EXIT_FAILURE;
        }
    }
    else
    {
        compraw_file_type = FILE_TYPE_RAW;
        compraw_width = width;
        compraw_height = height;
        forceUVswap = original_forceUVswap;
    }

    if (!compraw_width || !compraw_height)
    {
        fprintf(stderr, "\n     No width or height specified\n");
        return EXIT_FAILURE;
    }

    vpx_image_t    compraw_img;
    vpx_img_alloc(&compraw_img, IMG_FMT_I420, compraw_width, compraw_height, 1);

    YV12_BUFFER_CONFIG compraw_YV12;
    compraw_YV12.y_width   = compraw_img.d_w;
    compraw_YV12.y_height  = compraw_img.d_h;
    compraw_YV12.y_stride  = compraw_img.stride[PLANE_Y];
    compraw_YV12.uv_width  = (1 + compraw_YV12.y_width) / 2;
    compraw_YV12.uv_height = (1 + compraw_YV12.y_height) / 2;
    compraw_YV12.uv_stride = compraw_img.stride[PLANE_U];
    compraw_YV12.buffer_alloc        = compraw_img.img_data;
    compraw_YV12.y_buffer           = compraw_img.img_data;
    compraw_YV12.u_buffer = compraw_img.planes[PLANE_U];
    compraw_YV12.v_buffer = compraw_img.planes[PLANE_V];

    // Burn Frames untill Compressed frame offset reached - currently disabled
    // by override of CompressedFrameOffset
    if (CompressedFrameOffset > 0)
    {
    }

    if (compraw_fourcc == 842094169)
        forceUVswap = 1;   // if YV12 Do not swap Frames

    if (forceUVswap == 1)
    {
        unsigned char *temp = compraw_YV12.u_buffer;
        compraw_YV12.u_buffer = compraw_YV12.v_buffer;
        compraw_YV12.v_buffer = temp;
    }

    /////////////////////////////////////////////////////////////////////////
    //////// Printing ////////
    if (printvar != 0)
    {
        tprintf(PRINT_BTH, "\n\n                        "
            "---------Computing PSNR---------");
    }

    if (printvar == 0)
    {
        tprintf(PRINT_STD, "\n\nComparing %s to %s:\n                        \n"
            , inputFile1, inputFile2);
    }

    if (printvar == 1)
    {
        tprintf(PRINT_BTH, "\n\nComparing %s to %s:\n                        \n"
            , inputFile1, inputFile2);
    }

    if (printvar == 5)
    {
        tprintf(PRINT_BTH, "\n\nComparing %s to %s:                        \n\n"
            , inputFile1, inputFile2);
    }

    if (printvar == 1 || printvar == 5 || printvar == 0)
    {
        if (frameStats == 3)
        {
            tprintf(PRINT_STD, "File Has: %d total frames. \n Frame Offset 1 is"
                " 0\n Frame Offset 2 is 0\n Force UV Swap is %d\n Frame "
                "Statistics is %d:\n \n", frameCount, forceUVswap, frameStats);
        }
        else
        {
            tprintf(PRINT_BTH, "File Has: %d total frames. \n Frame Offset 1 is"
                " 0\n Frame Offset 2 is 0\n Force UV Swap is %d\n Frame "
                "Statistics is %d:\n \n", frameCount, forceUVswap, frameStats);
        }
    }

    ////////////////////////

    uint64_t *timeStamp2 = new uint64_t;
    uint64_t *timeEndStamp2 = new uint64_t;
    int deblock_level2 = 0;
    int noise_level2 = 0;
    int flags2 = 0;
    int currentFrame2 = 0;

    while (read_frame_enc(RawFile, &raw_img, file_type, &y4m, &detect))
    {
        if (file_type == 2)
        {
            raw_img.img_data = y4m.dst_buf;
            Raw_YV12.buffer_alloc        = raw_img.img_data;
            Raw_YV12.y_buffer           = raw_img.img_data;
            Raw_YV12.u_buffer = raw_img.planes[PLANE_U];
            Raw_YV12.v_buffer = raw_img.planes[PLANE_V];
        }

        unsigned long lpdwFlags = 0;
        unsigned long lpckid = 0;
        long bytes1;
        long bytes2;

        if (read_frame_enc(compraw_file, &compraw_img, compraw_file_type,
            &compraw_y4m, &compraw_detect))
        {
            if (compraw_file_type == 2)
            {
                compraw_img.img_data      = compraw_y4m.dst_buf;
                compraw_YV12.buffer_alloc = compraw_img.img_data;
                compraw_YV12.y_buffer     = compraw_img.img_data;
                compraw_YV12.u_buffer     = compraw_img.planes[PLANE_U];
                compraw_YV12.v_buffer     = compraw_img.planes[PLANE_V];
            }

            ++currentVideo1Frame;

            //////////////////// Get YV12 Data For Raw File ////////////////////
            bytes1 = (RawWidth * RawHeight * 3) / 2; // ivf_fhRaw.frameSize;
            bytes2 = (RawWidth * RawHeight * 3) / 2;
            sumBytes += bytes1;
            sumBytes2 += bytes2;

            ///////////////////////// Preform PSNR Calc ////////////////////////
            if (SsimOut)
            {
                double weight;
                double thisSsim = VP8_CalcSSIM_Tester(&Raw_YV12, &compraw_YV12,
                    1, &weight);
                summedQuality += thisSsim * weight ;
                summedWeights += weight;
            }

            double YPsnr;
            double upsnr;
            double vpsnr;
            double sq_error;
            int pa = kDontRunArtifactDetection;

            double thisPsnr = vp8_calcpsnr_tester(&Raw_YV12, &compraw_YV12,
                &YPsnr, &upsnr, &vpsnr, &sq_error, PRINT_NONE, pa);

            summedYPsnr += YPsnr;
            summedUPsnr += upsnr;
            summedVPsnr += vpsnr;
            summedPsnr += thisPsnr;
            sumSqError += sq_error;
            ////////////////////////////////////////////////////////////////////

            //////// Printing ////////
            if (printvar == 1 || printvar == 5 || printvar == 0)
            {
                if (frameStats == 0)
                {
                    tprintf(PRINT_STD, "%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%"
                        "c%7d of %7d  ", 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
                        8, 8, 8, 8, 8, 8, 8, currentVideo1Frame, frameCount);
                }

                if (frameStats == 1)
                {
                    tprintf(PRINT_BTH, "F:%5d, 1:%6.0f 2:%6.0f, Avg :%5.2f, "
                        "Y:%5.2f, U:%5.2f, V:%5.2f\n", currentVideo1Frame,
                        bytes1 * 8.0, bytes2 * 8.0, thisPsnr, 1.0 * YPsnr,
                        1.0 * upsnr, 1.0 * vpsnr);
                }

                if (frameStats == 2)
                {
                    tprintf(PRINT_STD, "%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%"
                        "c%7d of %7d  ", 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
                        8, 8, 8, 8, 8, 8, 8, currentVideo1Frame, frameCount);

                    fprintf(stderr, "F:%5d, 1:%6.0f 2:%6.0f, Avg :%5.2f, "
                        "Y:%5.2f, U:%5.2f, V:%5.2f\n",currentVideo1Frame,
                        bytes1 * 8.0,bytes2 * 8.0, thisPsnr, 1.0 * YPsnr,
                            1.0 * upsnr, 1.0 * vpsnr);
                }

                if (frameStats == 3)
                {
                    tprintf(PRINT_STD, "F:%5d, 1:%6.0f 2:%6.0f, Avg :%5.2f, "
                        "Y:%5.2f, U:%5.2f, V:%5.2f\n", currentVideo1Frame,
                        bytes1 * 8.0, bytes2 * 8.0, thisPsnr, 1.0 * YPsnr,
                        1.0 * upsnr, 1.0 * vpsnr);
                }
            }
            ////////////////////////
        }
        else
        {
            // delete [] CompBuff;
        }
    }

    // Over All PSNR Calc
    double samples = 3.0 / 2 * currentVideo1Frame * Raw_YV12.y_width *
        Raw_YV12.y_height;
    double avgPsnr = summedPsnr / currentVideo1Frame;
    double totalPsnr = vp8_mse_2_psnr_tester(samples, 255.0, sumSqError);

    if (summedWeights < 1.0)
        summedWeights = 1.0;

    double totalSSim = 100 * pow(summedQuality / summedWeights, 8.0);

    //////// Printing ////////
    if (printvar == 1 || printvar == 5 || printvar == 0)
    {
        if (frameStats == 3)
        {
            tprintf(PRINT_STD, "\nDr1:%8.2f Dr2:%8.2f, Avg: %5.2f, Avg Y: %5.2f"
                ", Avg U: %5.2f, Avg V: %5.2f, Ov PSNR: %8.2f, ",
                sumBytes * 8.0 / currentVideo1Frame*(RawRate / 2) / RawScale /
                1000, sumBytes2 * 8.0 / currentVideo1Frame*(compraw_rate / 2) /
                compraw_scale / 1000, avgPsnr, 1.0 * summedYPsnr /
                currentVideo1Frame, 1.0 * summedUPsnr / currentVideo1Frame,
                1.0 * summedVPsnr / currentVideo1Frame, totalPsnr);
            tprintf(PRINT_STD, SsimOut ? "SSIM: %8.2f\n" : "SSIM: Not run.",
                totalSSim);

        }
        else
        {
            tprintf(PRINT_BTH, "\nDr1:%8.2f Dr2:%8.2f, Avg: %5.2f, Avg Y: "
                "%5.2f, Avg U: %5.2f, Avg V: %5.2f, Ov PSNR: %8.2f, ",
                sumBytes * 8.0 / currentVideo1Frame*(RawRate / 2) / RawScale /
                1000, sumBytes2 * 8.0 / currentVideo1Frame*(compraw_rate / 2) /
                compraw_scale / 1000, avgPsnr, 1.0 * summedYPsnr /
                currentVideo1Frame, 1.0 * summedUPsnr / currentVideo1Frame,
                1.0 * summedVPsnr / currentVideo1Frame, totalPsnr);
            tprintf(PRINT_BTH, SsimOut ? "SSIM: %8.2f\n" : "SSIM: Not run.",
                totalSSim);
        }
    }

    if (printvar != 0)
    {
        tprintf(PRINT_BTH, "\n                        -------------------------"
            "-------\n");
    }

    if (SsimOut)
        *SsimOut = totalSSim;

    ////////////////////////
    fclose(RawFile);
    fclose(compraw_file);
    delete timeStamp2;
    delete timeEndStamp2;

    vpx_img_free(&raw_img);
    vpx_img_free(&compraw_img);

    return totalPsnr;
}
double vpxt_data_rate(const char *input_file, int DROuputSel)
{
    if (DROuputSel != 2)
    {
        tprintf(PRINT_BTH, "\n--------Data Rate-------\n");

        char FileNameOnly[256];

        vpxt_file_name(input_file, FileNameOnly, 0);
        tprintf(PRINT_BTH, "Data Rate for: %s", FileNameOnly);
    }

    int currentVideoFrame = 0;
    int frameCount = 0;
    int byteRec = 0;

    long PosSize = vpxt_file_size(input_file, 0);
    ///////////////////////////////////
    unsigned int            width;
    unsigned int            height;
    unsigned int            fps_den;
    unsigned int            fps_num;
    unsigned int           fourcc;

    uint8_t               *buf = NULL;
    size_t               buf_sz = 0, buf_alloc_sz = 0;

    struct input_ctx        input;

    input.chunk = 0;
    input.chunks = 0;
    input.infile = NULL;
    input.kind = RAW_FILE;
    input.nestegg_ctx = 0;
    input.pkt = 0;
    input.video_track = 0;

    FILE *infile = strcmp(input_file, "-") ? fopen(input_file, "rb") :
        set_binary_mode(stdin);

    if (!infile)
    {
        tprintf(PRINT_BTH, "Failed to open input file: %s", input_file);
        return -1;
    }

    input.infile = infile;

    if (file_is_ivf_dec(infile, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = IVF_FILE;
    else if (file_is_webm(&input, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = WEBM_FILE;
    else if (file_is_raw(infile, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = RAW_FILE;
    else
    {
        fprintf(stderr, "Unrecognized input file type.\n");
        return EXIT_FAILURE;
    }

    if (input.kind == WEBM_FILE)
        if (webm_guess_framerate(&input, &fps_den, &fps_num))
        {
            fprintf(stderr, "Failed to guess framerate -- error parsing "
                    "webm file?\n");
            return EXIT_FAILURE;
        }

    ///////////////////////////////////

    tprintf(PRINT_STD, "\n");

    size_t nBytes = 0;
    size_t nBytesMin = 999999;
    size_t nBytesMax = 0;

    uint64_t timestamp = 0;
    while (!skim_frame_dec(&input, &buf, &buf_sz, &buf_alloc_sz, &timestamp))
    {
        nBytes = nBytes + buf_sz;

        if (buf_sz < nBytesMin)
        {
            nBytesMin = buf_sz;
        }

        if (buf_sz > nBytesMax)
        {
            nBytesMax = buf_sz;
        }

        currentVideoFrame ++;
    }

    long nSamples = currentVideoFrame;
    long lRateNum = fps_num;
    long lRateDenom = fps_den;

    long nSamplesPerBlock = 1;

    double dRateFactor = static_cast<double>(lRateNum) /
        static_cast<double>(lRateDenom) * static_cast<double>(8) /
        static_cast<double>(1000);

    double Avg = (double)nBytes * (double)dRateFactor / (double)nSamples;
    double Min = (double)nBytesMin * (double)dRateFactor /
        (double)nSamplesPerBlock;
    double Max = (double)nBytesMax * (double)dRateFactor /
        (double)nSamplesPerBlock;
    double File = (double)PosSize * (double)dRateFactor /
        (double)nSamples;

    tprintf(PRINT_STD, "\nData rate for frames 0..%i\n", currentVideoFrame - 1);
    tprintf(PRINT_STD, "Average %*.2f kb/s\n", 10, Avg);
    tprintf(PRINT_STD, "Min     %*.2f kb/s\n", 10, Min);
    tprintf(PRINT_STD, "Max     %*.2f kb/s\n", 10, Max);
    tprintf(PRINT_STD, "File    %*.2f kb/s\n", 10, File);

    if (DROuputSel == 1)
    {
        fprintf(stderr, "\nData rate for frames 0..%i\n",currentVideoFrame - 1);
        fprintf(stderr, "Average %*.2f kb/s\n", 10, Avg);
        fprintf(stderr, "Min     %*.2f kb/s\n", 10, Min);
        fprintf(stderr, "Max     %*.2f kb/s\n", 10, Max);
        fprintf(stderr, "File    %*.2f kb/s\n", 10, File);
    }

    // fclose(in);

    if (DROuputSel != 2)
    {
        tprintf(PRINT_BTH, "\n------------------------\n");
    }

    //   delete [] outputVideoBuffer;
    if (input.nestegg_ctx)
        nestegg_destroy(input.nestegg_ctx);

    if (input.kind != WEBM_FILE)
        free(buf);

    fclose(infile);

    return Avg;
}

int vpxt_check_pbm(const char *input_file,
                   int bitRate,
                   int64_t maxBuffer,
                   int64_t preBuffer)
{
    // bitRate    bitrate in kbps
    // maxBuffer  maxbuffer in ms
    // preBuffer  prebuffer in ms

    unsigned int            width;
    unsigned int            height;
    unsigned int            fps_den;
    unsigned int            fps_num;
    unsigned int            fourcc;

    uint8_t               *buf = NULL;
    size_t               buf_sz = 0, buf_alloc_sz = 0;

    struct input_ctx        input;

    input.chunk = 0;
    input.chunks = 0;
    input.infile = NULL;
    input.kind = RAW_FILE;
    input.nestegg_ctx = 0;
    input.pkt = 0;
    input.video_track = 0;

    FILE *infile = strcmp(input_file, "-") ? fopen(input_file, "rb") :
        set_binary_mode(stdin);

    if (!infile)
    {
        tprintf(PRINT_BTH, "Failed to open input file: %s", input_file);
        return -1;
    }

    input.infile = infile;

    if (file_is_ivf_dec(infile, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = IVF_FILE;
    else if (file_is_webm(&input, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = WEBM_FILE;
    else if (file_is_raw(infile, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = RAW_FILE;
    else
    {
        fprintf(stderr, "Unrecognized input file type.\n");
        return EXIT_FAILURE;
    }

    if (input.kind == WEBM_FILE)
        if (webm_guess_framerate(&input, &fps_den, &fps_num))
        {
            fprintf(stderr, "Failed to guess framerate -- error parsing "
                    "webm file?\n");
            return EXIT_FAILURE;
        }

    int currentVideoFrame = 0;
    int frameCount = 0;
    int byteRec = 0;

    // frameCount = ivf_h_raw.length;
    int nFrameFail = 0;

    bool checkOverrun = false;
    double secondsperframe = ((double)fps_den / (double)fps_num);
    // -.5 to cancel out rounding
    int bitsAddedPerFrame = ((bitRate * 1000 * secondsperframe)) - .5;
    // scale factors cancel (ms * kbps = bits)
    int bitsInBuffer = preBuffer * bitRate;
    // scale factors cancel (ms * kbps = bits)
    int maxBitsInBuffer = maxBuffer * bitRate;

    uint64_t timestamp = 0;
    while (!skim_frame_dec(&input, &buf, &buf_sz, &buf_alloc_sz, &timestamp))
    {
        bitsInBuffer += bitsAddedPerFrame;
        bitsInBuffer -= buf_sz * 8; // buf_sz in kB

        if (bitsInBuffer < 0.)
        {
            if (input.nestegg_ctx)
                nestegg_destroy(input.nestegg_ctx);

            if (input.kind != WEBM_FILE)
                free(buf);

            fclose(infile);
            return currentVideoFrame;
        }

        if (bitsInBuffer > maxBitsInBuffer)
        {
            if (checkOverrun)
            {
                if (input.nestegg_ctx)
                    nestegg_destroy(input.nestegg_ctx);

                if (input.kind != WEBM_FILE)
                    free(buf);

                fclose(infile);
                return currentVideoFrame;
            }
            else
            {
                bitsInBuffer = maxBitsInBuffer;
            }
        }

        currentVideoFrame ++;
    }

    if (input.nestegg_ctx)
        nestegg_destroy(input.nestegg_ctx);

    if (input.kind != WEBM_FILE)
        free(buf);

    fclose(infile);
    return -11;
}
int vpxt_check_pbm_threshold(const char *input_file,
                             double bitRate,
                             int64_t maxBuffer,
                             int64_t preBuffer,
                             int64_t optimalbuffer,
                             int Threshold)
{
    std::string ResizeInStr;
    vpxt_remove_file_extension(input_file, ResizeInStr);
    ResizeInStr += "CheckPBMThresh.txt";
    char output_file[255] = "";
    snprintf(output_file, 255, "%s", ResizeInStr.c_str());

    FILE *out;

    int PrintSwitch = 1;

    if (PrintSwitch == 1)
    {
        out = fopen(output_file, "w");
    }

    unsigned int            width;
    unsigned int            height;
    unsigned int            fps_den;
    unsigned int            fps_num;
    unsigned int            fourcc;

    uint8_t               *buf = NULL;
    size_t               buf_sz = 0, buf_alloc_sz = 0;

    struct input_ctx        input;

    input.chunk = 0;
    input.chunks = 0;
    input.infile = NULL;
    input.kind = RAW_FILE;
    input.nestegg_ctx = 0;
    input.pkt = 0;
    input.video_track = 0;

    FILE *infile = strcmp(input_file, "-") ? fopen(input_file, "rb") :
        set_binary_mode(stdin);

    if (!infile)
    {
        tprintf(PRINT_BTH, "Failed to open input file: %s", input_file);
        return -1;
    }

    input.infile = infile;

    if (file_is_ivf_dec(infile, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = IVF_FILE;
    else if (file_is_webm(&input, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = WEBM_FILE;
    else if (file_is_raw(infile, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = RAW_FILE;
    else
    {
        fprintf(stderr, "Unrecognized input file type.\n");
        return EXIT_FAILURE;
    }

    if (input.kind == WEBM_FILE)
        if (webm_guess_framerate(&input, &fps_den, &fps_num))
        {
            fprintf(stderr, "Failed to guess framerate -- error parsing "
                    "webm file?\n");
            return EXIT_FAILURE;
        }

    int currentVideoFrame = 0;
    int frameCount = 0;
    int byteRec = 0;

    int nFrameFail = 0;

    bool checkOverrun = false;
    double secondsperframe = ((double)fps_den / (double)fps_num);
     // -.5 to cancel out rounding
    int bitsAddedPerFrame = ((bitRate * 1000 * secondsperframe)) - .5;
    int bitsInBuffer = preBuffer * bitRate;
    int maxBitsInBuffer = maxBuffer * bitRate;

    uint64_t timestamp = 0;
    while (!skim_frame_dec(&input, &buf, &buf_sz, &buf_alloc_sz, &timestamp))
    {
        bitsInBuffer += bitsAddedPerFrame;
        bitsInBuffer -= buf_sz * 8;

        int optimalbufferFloat = optimalbuffer;
        double MaxPercentBuffer = (((Threshold * optimalbufferFloat) / 100) *
            bitRate);

        if (bitsInBuffer < MaxPercentBuffer)
        {
            fprintf(out, "%i %i\n", currentVideoFrame, 1);
        }
        else
        {
            fprintf(out, "%i %i\n", currentVideoFrame, 0);
        }

        if (bitsInBuffer > maxBitsInBuffer)
        {
            if (checkOverrun)
            {
            }
            else
            {
                bitsInBuffer = maxBitsInBuffer;
            }
        }

        currentVideoFrame ++;
    }

    fclose(infile);

    if (PrintSwitch == 1)
    {
        fclose(out);
    }

    if (input.nestegg_ctx)
        nestegg_destroy(input.nestegg_ctx);

    if (input.kind != WEBM_FILE)
        free(buf);

    return -11;
}

int vpxt_faux_compress()
{
#ifdef API
    vpx_codec_ctx_t        encoder;
    vpx_codec_iface_t     *iface = &vpx_codec_vp8_cx_algo;
    vpx_codec_enc_cfg_t    cfg;
    vpx_codec_enc_config_default(iface, &cfg, 0);
    vpx_codec_enc_init(&encoder, iface, &cfg, 0);
    vpx_codec_destroy(&encoder);
#else
    VP8_CONFIG opt;
    vpxt_default_parameters(opt);
    VP8_Initialize();
    VP8_PTR optr = VP8_CreateCompressor(&opt);
    VP8_RemoveCompressor(&optr);
    VP8_Shutdown();
#endif

    return 1;
}
int vpxt_faux_decompress(const char *inputChar)
{
#ifdef API

    vpx_codec_ctx_t        decoder;
    vpx_codec_iface_t     *iface = &vpx_codec_vp8_dx_algo;
    vpx_codec_dec_cfg_t     cfg;
    uint8_t               *buf = NULL;
    size_t               buf_sz = 0, buf_alloc_sz = 0;
    FILE                  *infile;
    unsigned int           fourcc;
    const char            *fn = inputChar;

    unsigned int            width;
    unsigned int            height;
    unsigned int            fps_den;
    unsigned int            fps_num;

    struct input_ctx        input;
    input.chunk = 0;
    input.chunks = 0;
    input.infile = NULL;
    input.kind = RAW_FILE;
    input.nestegg_ctx = 0;
    input.pkt = 0;
    input.video_track = 0;

    int CharCount = 0;

    /* Open file */
    infile = strcmp(fn, "-") ? fopen(fn, "rb") : set_binary_mode(stdin);

    if (!infile)
    {
        tprintf(PRINT_BTH, "Failed to open input file: %s", fn);
        return -1;
    }

    input.infile = infile;

    if (file_is_ivf_dec(infile, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = IVF_FILE;
    else if (file_is_webm(&input, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = WEBM_FILE;
    else if (file_is_raw(infile, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = RAW_FILE;
    else
    {
        fprintf(stderr, "Unrecognized input file type.\n");
        return EXIT_FAILURE;
    }

    uint64_t timestamp = 0;

    vpx_codec_dec_init(&decoder, iface, &cfg, 0);
    read_frame_dec(&input, &buf, (size_t *)&buf_sz, (size_t *)&buf_alloc_sz, &timestamp);
    vpx_codec_decode(&decoder, buf, buf_sz, NULL, 0);
    vpx_codec_destroy(&decoder);
    free(buf);
    fclose(infile);

#else
    vp8dx_initialize();
    VP8D_CONFIG oxcf;
    VP8D_PTR optr = vp8dx_create_decompressor(&oxcf);
    vp8dx_remove_decompressor(optr);
    // vp8dx_Shutdown();
#endif

    return 1;
}
// -------------------------Test Functions--------------------------------------
int initialize_test_directory(int argc,
                              const char** argv,
                              int test_type,
                              const std::string &working_dir,
                              const char *test_dir,
                              std::string &cur_test_dir_str,
                              std::string &file_index_str,
                              char main_test_dir_char[255],
                              char file_index_output_char[255],
                              const std::string sub_folder_str)
{
    // Initilizes cur_test_dir_str, file_index_str, main_test_dir_char, and
    // file_index_output_char to proper values.

    std::string PrefTestOnlyTestMatch;
    char CurTestDirChar[255] = "";

    if (test_type == kCompOnly || test_type == kFullTest)
    {
        snprintf(CurTestDirChar, 255, "%s", working_dir.c_str());

        int v = 0;

        while (CurTestDirChar[v] != '\"')
        {
            main_test_dir_char[v] = CurTestDirChar[v];
            v++;
        }

        main_test_dir_char[v] = slashChar();
        main_test_dir_char[v+1] = '\0';
        ////////////////////////////////////////////////////////////////////////
        file_index_str = main_test_dir_char;
        file_index_str += "FileIndex.txt";
        ////////////////////////////////////////////////////////////////////////
        cur_test_dir_str = main_test_dir_char;
        cur_test_dir_str += test_dir + slashCharStr() + sub_folder_str;
        cur_test_dir_str.erase(cur_test_dir_str.length() - 1, 1);

        std::string create_dir_2 = cur_test_dir_str;
        create_dir_2.insert(0, "md \"");
        vpxt_make_dir_vpx(create_dir_2.c_str());

        ///////////////// Records FileLocations for MultiPlat Test /////////////
        if (test_type == kCompOnly)
        {
            char CurTestDirStr2[255];
            snprintf(CurTestDirStr2, 255, "%s", cur_test_dir_str.c_str());
            vpxt_subfolder_name(CurTestDirStr2, file_index_output_char);
        }

        ////////////////////////////////////////////////////////////////////////
    }
    else
    {
        // Use working_dir to get the main folder
        // Use Index File to get the rest of the string
        // Put it all together Setting cur_test_dir_str to the location of the
        // files we want to examine.
        char buffer[255];

        std::string CurTestDirChar = working_dir + slashCharStr();
        file_index_str = CurTestDirChar + "FileIndex.txt";

        std::fstream FileStream;
        FileStream.open(file_index_str.c_str(), std::fstream::in |
            std::fstream::out | std::fstream::app);

        int n = 0;

        while (n < atoi(argv[argc]))
        {
            FileStream.getline(buffer, 255);
            n++;
        }

        FileStream.close();

        char PrefTestOnlyTestMatchChar[255];
        vpxt_test_name(buffer, PrefTestOnlyTestMatchChar);
        PrefTestOnlyTestMatch = PrefTestOnlyTestMatchChar;

        if (PrefTestOnlyTestMatch.compare(test_dir) != 0)
        {
            tprintf(PRINT_STD, "Error: File mismatch ");
            tprintf(PRINT_STD, "PrefTestOnlyTestMatch: %s test_dir: %s",
                PrefTestOnlyTestMatch.c_str(), test_dir);
            return 11;
        }

        CurTestDirChar += buffer;
        cur_test_dir_str = CurTestDirChar;
    }

    return 0;
}
void record_test_complete(const std::string MainDirString,
                          const char *file_index_output_char,
                          int test_type)
{
    if (test_type == kCompOnly)
    {
        std::fstream FileStream;
        FileStream.open(MainDirString.c_str(), std::fstream::out |
            std::fstream::app);

        if (!FileStream.good())
        {
            tprintf(PRINT_STD, "\nERROR WRITING FILE: FILEINDEX: %s\n",
                MainDirString.c_str());
            return;
        }

        FileStream << file_index_output_char << "\n";
        FileStream.close();
    }

    return;
}
int print_version()
{
    std::string arch = "Unknown";
    std::string Platform = "Unknown";

#if ARCH_X86
    arch = "32 bit";
#elif ARCH_X86_64
    arch = "64 bit";
#elif HAVE_ARMV6
    arch = "Arm 6";
#elif HAVE_ARMV7
    arch = "Arm 7";
#endif

#if defined(_WIN32)
    Platform = "Windows";
#elif defined(linux)
    Platform = "Linux";
#elif defined(__APPLE__)
    Platform = "Intel Mac";
#elif defined(_PPC)
    Platform = "PowerPC";
#endif

    tprintf(PRINT_STD, "\n %s - %s %s\n",
        vpx_codec_iface_name(&vpx_codec_vp8_cx_algo),
        arch.c_str(), Platform.c_str());
    return 0;
}
void print_header_info()
{
    std::string TestMachineInfo;
    std::string arch = "Unknown";
    std::string comp = "Unknown";
    std::string plat = "Unknown";

#if ARCH_X86
    arch = "32 bit";
#else if ARCH_X86_64
    arch = "64 bit";
#endif
#if defined(COMP_GCC)
    comp = "GCC";
#elif defined(COMP_ICC)
    comp = "ICC";
#elif defined(COMP_VS)
    comp = "VS";
#endif

#if defined(_WIN32)
    plat = " Windows ";
#endif
#if defined(linux)
    plat = " Linux ";
#endif
#if defined(__APPLE__)
#if defined(_PPC)
    plat = " PowerPC";
#else
    plat = " Intel Mac ";
#endif
#endif

    std::string CodecNameStr = vpx_codec_iface_name(&vpx_codec_vp8_cx_algo);
    unsigned int x = 0;

    while (x < 40 - (CodecNameStr.length() / 2))
    {
        TestMachineInfo += " ";
        x++;
    }

    TestMachineInfo += CodecNameStr + "\n";
    std::string Platform = "Test Machine is Running: " + arch + plat + comp;

    x = 0;

    while (x < 40 - (Platform.length() / 2))
    {
        TestMachineInfo += " ";
        x++;
    }

    TestMachineInfo += Platform + "\n\n";

    tprintf(PRINT_BTH, "%s", TestMachineInfo.c_str());
}
void print_header_info_to_file(const char *FileName)
{
    FILE *outfile;
    outfile = fopen(FileName, "a");

 std::string TestMachineInfo;
    std::string arch = "Unknown";
    std::string comp = "Unknown";
    std::string plat = "Unknown";

#if ARCH_X86
    arch = "32 bit";
#else if ARCH_X86_64
    arch = "64 bit";
#endif
#if defined(COMP_GCC)
    comp = "GCC";
#elif defined(COMP_ICC)
    comp = "ICC";
#elif defined(COMP_VS)
    comp = "VS";
#endif

#if defined(_WIN32)
    plat = " Windows ";
#endif
#if defined(linux)
    plat = " Linux ";
#endif
#if defined(__APPLE__)
#if defined(_PPC)
    plat = " PowerPC";
#else
    plat = " Intel Mac ";
#endif
#endif

    std::string CodecNameStr = vpx_codec_iface_name(&vpx_codec_vp8_cx_algo);
    unsigned int x = 0;

    while (x < 40 - (CodecNameStr.length() / 2))
    {
        TestMachineInfo += " ";
        x++;
    }

    TestMachineInfo += CodecNameStr + "\n";
    std::string Platform = "Test Machine is Running: " + arch + plat + comp;

    x = 0;

    while (x < 40 - (Platform.length() / 2))
    {
        TestMachineInfo += " ";
        x++;
    }

    TestMachineInfo += Platform + "\n\n";

    fprintf(outfile, "%s", TestMachineInfo.c_str());
    fclose(outfile);
}
void print_header_full_test(int argc,
                            const char** argv,
                            std::string working_dir_3)
{
    // Full Test Header Output
    // Formats workingDir3 string to fit in text box
    // records settings from argv to be written to text file
    std::string PrintWorkingDir3 = working_dir_3;
    std::string PrintInput = "Input:";
    PrintWorkingDir3.insert(0, "Output: ");

    unsigned int y = 0;

    while (y < argc)
    {
        PrintInput += " ";
        PrintInput += argv[y];
        y++;
    }

    y = 75;

    while (y < PrintInput.length())
    {
        PrintInput.insert(y, "\n");
        y = y + 75;
    }

    y = 75;

    while (y < PrintWorkingDir3.length())
    {
        PrintWorkingDir3.insert(y, "\n");
        y = y + 75;
    }

    print_header_info();

    tprintf(PRINT_BTH, "\n/////////////////////////////////Full Test///////////"
        "//////////////////////\n%s\n\n%s", PrintInput.c_str(),
        PrintWorkingDir3.c_str());
    tprintf(PRINT_BTH, "\n/////////////////////////////////////////////////////"
        "//////////////////////\n\n");
}
void print_header_compression_only(int argc,
                                   const char** argv,
                                   std::string working_dir_3)
{
    // Compression Header
    // Formats workingDir3 string to fit in text box
    // records settings from argv to be written to text file

    std::string PrintWorkingDir3 = working_dir_3;
    std::string PrintInput = "Input:";
    PrintWorkingDir3.insert(0, "Output: ");

    int y = 0;

    while (y < argc)
    {
        PrintInput += " ";
        PrintInput += argv[y];
        y++;
    }

    y = 75;

    while (y < PrintInput.length())
    {
        PrintInput.insert(y, "\n");
        y = y + 75;
    }

    y = 75;

    while (y < PrintWorkingDir3.length())
    {
        PrintWorkingDir3.insert(y, "\n");
        y = y + 75;
    }

    print_header_info();

    tprintf(PRINT_BTH, "\n///////////////////////////////Compress Only/////////"
        "//////////////////////\n%s\n\n%s", PrintInput.c_str(),
        PrintWorkingDir3.c_str());
    tprintf(PRINT_BTH, "\n/////////////////////////////////////////////////////"
        "//////////////////////\n\n");
}
void print_header_test_only(int argc,
                            const char** argv,
                            std::string working_dir_3)
{
    // Test Only Header
    // Formats workingDir3 string to fit in text box records input
    // location and output location both are the same

    std::string PrintWorkingDir3 = working_dir_3;
    std::string PrintWorkingDir4 = working_dir_3;
    std::string PrintInput = "Input:";
    PrintWorkingDir3.insert(0, "Output: ");
    PrintWorkingDir4.insert(0, "Input: ");

    int y = 0;

    while (y < argc)
    {
        PrintInput += " ";
        PrintInput += argv[y];
        y++;
    }

    y = 75;

    while (y < PrintWorkingDir4.length())
    {
        PrintWorkingDir4.insert(y, "\n");
        y = y + 75;
    }

    y = 75;

    while (y < PrintWorkingDir3.length())
    {
        PrintWorkingDir3.insert(y, "\n");
        y = y + 75;
    }

    print_header_info();

    tprintf(PRINT_BTH, "\n/////////////////////////Existing Compression Test///"
        "//////////////////////\n%s\n\n%s", PrintWorkingDir4.c_str(),
        PrintWorkingDir3.c_str());
    tprintf(PRINT_BTH, "\n/////////////////////////////////////////////////////"
        "//////////////////////\n\n");
}
void vpxt_print_header(int argc, const char** argv, char* main_test_dir_char,
                       std::string cur_test_dir_str, char *test_dir,
                       int test_type)
{
    if (test_type == kFullTest)
        print_header_full_test(argc, argv, main_test_dir_char);

    if (test_type == kCompOnly)
        print_header_compression_only(argc, argv, main_test_dir_char);

    if (test_type == kTestOnly)
        print_header_test_only(argc, argv, cur_test_dir_str);

    vpxt_cap_string_print(PRINT_BTH, "%s", test_dir);

    return;
}
void vpxt_open_output_file(int test_type, std::string &text_file_str,
                           FILE*& fp)
{
    if (test_type == kCompOnly || test_type == kFullTest)
        text_file_str += ".txt";
    else
        text_file_str += "_TestOnly.txt";

    if ((fp = freopen(text_file_str.c_str(), "w", stderr)) == NULL)
    {
        tprintf(PRINT_STD, "Cannot open out put file: %s\n",
            text_file_str.c_str());
        exit(1);
    }

    return;
}
int vpxt_use_custom_settings(const char** argv, int argc, int input_ver,
                             FILE*& fp, std::string file_index_str,
                             char* file_index_output_char, int test_type,
                             VP8_CONFIG& opt, int& bitrate)
{
    if (input_ver == 2)
    {
        if (!vpxt_file_exists_check(argv[argc-1]))
        {
            tprintf(PRINT_BTH, "\nInput Settings file %s does not exist\n",
                argv[argc-1]);

            fclose(fp);
            record_test_complete(file_index_str, file_index_output_char,
                test_type);
            return kTestIndeterminate;
        }

        opt = vpxt_input_settings(argv[argc-1]);
        bitrate = opt.target_bandwidth;
    }

    return 0;
}
void check_time_stamp(int SelectorArInt,
                      std::string *SelectorAr,
                      std::string *SelectorAr2,
                      std::string prev_time_stamp,
                      int &identical_file_cnt,
                      std::string *TimeStampAr2)
{
    char identicalFileBuffer[3] = "";

    if (SelectorArInt != 0 &&
        SelectorAr[SelectorArInt].compare(SelectorAr[SelectorArInt-1]) == 0)
    {
        if (vpxt_timestamp_compare(TimeStampAr2[0], prev_time_stamp))
        {
            identical_file_cnt++;
            vpxt_itoa_custom(identical_file_cnt, identicalFileBuffer, 10);
            TimeStampAr2[0].erase(TimeStampAr2[0].end() - 1);
            TimeStampAr2[0] += "_";
            TimeStampAr2[0] += identicalFileBuffer;
            TimeStampAr2[0] += "\"";
        }
        else
        {
            identical_file_cnt = 1;
        }
    }
    else
    {
        identical_file_cnt = 1;
    }
}
void vpxt_formated_print(int selector, const char *fmt, ...)
{
    char buffer[2048];
    va_list ap;
    va_start(ap, fmt);

    int charswritten = vsnprintf(buffer, sizeof(buffer) - 1, fmt, ap);
    std::string SummaryStr = buffer;

    // selector == HLPPRT -> Summary
    // selector == TOLPRT -> Help
    // selector == FUNPRT -> Function
    // selector == OTRPRT -> Other non formatted output
    // selector == RESPRT -> Individual Pass Fail output

    std::string SummaryStrOutput;
    int EndOfLineLength = 0;

    // add padding for formating
    if (selector == HLPPRT || selector == TOLPRT || selector == FUNPRT)
        SummaryStrOutput += "         ";

    if (selector == RESPRT) // add padding for formating
        SummaryStrOutput += " * ";

    // determine cut off to keep words whole
    int Cutoff;

    if (selector == HLPPRT || selector == TOLPRT || selector == FUNPRT)
        Cutoff = 66;

    if (selector == OTRPRT)
        Cutoff = 79;

    if (selector == RESPRT)
        Cutoff = 70;

    unsigned int x = 0;

    while (x + Cutoff < SummaryStr.length())
    {
        if (SummaryStr.substr(x + Cutoff + 1, 1).compare(" ") == 0 ||
            SummaryStr.substr(x + Cutoff, 1).compare(" ") == 0)
            Cutoff++;
        else
        {
            while (SummaryStr.substr(x + Cutoff, 1).compare(" ") != 0)
            {
                Cutoff--;
            }

            Cutoff++;
        }

        // add the properly formated string to the output string
        SummaryStrOutput += SummaryStr.substr(x, Cutoff);

        // add padding for formating
        if (selector == HLPPRT || selector == TOLPRT || selector == FUNPRT)
            SummaryStrOutput += "\n         ";

        if (selector == RESPRT) // add padding for formating
            SummaryStrOutput += "\n   ";

        x = x + Cutoff;

        while (SummaryStr.substr(x, 1).compare(" ") == 0)
            x++;

        if (selector == HLPPRT || selector == TOLPRT || selector == FUNPRT)
            Cutoff = 66;

        if (selector == OTRPRT)
            Cutoff = 79;

        if (selector == RESPRT)
            Cutoff = 70;
    }

    SummaryStrOutput += SummaryStr.substr(x, SummaryStr.length() - x);

    if (selector == HLPPRT)
    {
        tprintf(PRINT_STD, "\n\nSummary:\n");
        tprintf(PRINT_STD, "%s\n\n", SummaryStrOutput.c_str());
    }

    if (selector == TOLPRT)
    {
        tprintf(PRINT_STD, "\n\n  Help:\n");
        tprintf(PRINT_STD, "%s\n\n", SummaryStrOutput.c_str());
    }

    if (selector == FUNPRT)
    {
        tprintf(PRINT_STD, "\n\nFunction:\n");
        tprintf(PRINT_STD, "%s\n\n", SummaryStrOutput.c_str());
    }

    if (selector == OTRPRT)
    {
        fprintf(stderr, "%s", SummaryStrOutput.c_str());
    }

    if (selector == RESPRT)
    {
        tprintf(PRINT_BTH, "%s", SummaryStrOutput.c_str());
    }

    return;
}
void vpxt_cap_string_print(int selector, const char *fmt, ...)
{
    // This function will capitalize the first letter of any word
    // seperated with either an '_' or a ' ' and print it.

    char buffer[2048];
    char buffer_cap[2048];
    va_list ap;
    va_start(ap, fmt);

    int charswritten = vsnprintf(buffer, sizeof(buffer) - 1, fmt, ap);

    int parse_int = 0;
    int cap_next = 0;

    while (buffer[parse_int] != '\0' && parse_int < sizeof(buffer))
    {
        char add_to_buffer_cap;

        if (cap_next == 1)
        {
            // Capitalize current letter
            add_to_buffer_cap = toupper(buffer[parse_int]);
            cap_next = 0;
        }
        else
        {
            add_to_buffer_cap =  buffer[parse_int];
        }

        if (parse_int == 0)
        {
            // Capitalize current letter
            add_to_buffer_cap = toupper(buffer[parse_int]);
        }

        if (buffer[parse_int] == '_' || buffer[parse_int] == ' ')
        {
            cap_next = 1;
        }

        buffer_cap[parse_int] = add_to_buffer_cap;
        parse_int++;
    }

    buffer_cap[parse_int] = '\0';

    if (selector == PRINT_STD)
        tprintf(PRINT_STD, "%s", buffer_cap);

    if (selector == PRINT_ERR)
        fprintf(stderr, "%s", buffer_cap);

    if (selector == PRINT_BTH)
        tprintf(PRINT_BTH, "%s", buffer_cap);

    return;
}
int  vpxt_lower_case_string(std::string &input)
{
    unsigned int pos = 0;

    while (pos < input.length())
    {
        input[pos] = tolower(input[pos]);
        pos = pos + 1;
    }

    return 0;
}
// --------------------------------Enc/Dec--------------------------------------
#ifdef API
int vpxt_compress(const char *input_file,
                  const char *outputFile2,
                  int speed, int bitrate,
                  VP8_CONFIG &oxcf,
                  const char *comp_out_str,
                  int compress_int,
                  int RunQCheck,
                  std::string EncFormat,
                  int set_config)
{
    int write_webm = 1;
    vpxt_lower_case_string(EncFormat);

    if (EncFormat.compare("ivf") == 0)
        write_webm = 0;

    // RunQCheck - Signifies if the quantizers should be check to make sure
    // theyre working properly during an encode
    // RunQCheck = 0 = Do not save q values
    // RunQCheck = 1 = Save q values
    // RunQCheck = 2 = display q values only
    std::ofstream quant_out_file;

    if (RunQCheck == 1)
    {
        std::string QuantOutStr;
        vpxt_remove_file_extension(outputFile2, QuantOutStr);
        QuantOutStr += "quantizers.txt";
        quant_out_file.open(QuantOutStr.c_str());
    }

    /////////////////////////// DELETE ME TEMP MEASURE /////////////////////////
    if (oxcf.Mode == 3) // Real time Mode
    {
        if (RunQCheck == 1)
            quant_out_file.close();

        return 0;
    }
    ////////////////////////////////////////////////////////////////////////////

    unsigned int             file_type, fourcc;
    y4m_input                y4m;
    int                      arg_have_framerate = 0;
    EbmlGlobal               ebml = {0};
    uint32_t                 hash = 0;
    vpx_codec_ctx_t          encoder;
    const char              *in_fn = input_file, *out_fn = outputFile2;
    const char              *stats_fn = NULL;
    FILE                    *infile, *outfile;
    vpx_codec_enc_cfg_t      cfg;
    vpx_codec_err_t          res;
    int                      pass, one_pass_only = 0;
    stats_io_t               stats;
    vpx_image_t              raw;
    const struct codec_item *codec = codecs;
    int                      frame_avail, got_data;
    int                      arg_usage = 0, arg_passes = 1, arg_deadline = 0;
    int                      arg_limit = 0;
    static const arg_def_t **ctrl_args = no_args;
    int                      verbose = 0;
    int                      arg_use_i420 = 1;
    unsigned long            cx_time = 0;
    struct vpx_rational      arg_framerate = {30, 1};
    stereo_format_t          stereo_fmt = STEREO_FORMAT_MONO;

    ebml.last_pts_ms = -1;
    /* Populate encoder configuration */
    res = vpx_codec_enc_config_default(codec->iface, &cfg, arg_usage);

    if (res){
        tprintf(PRINT_STD, "Failed to get config: %s\n",
            vpx_codec_err_to_string(res));

        if (RunQCheck == 1)
            quant_out_file.close();

        return -1;
    }

    ////////////////////// Update CFG Settings Here ////////////////////
    // if mode == kTwoPassGoodQuality or 5 arg_passes = 2
    vpxt_core_config_to_api_config(oxcf, &cfg);

    // Set Mode Pass and bitrate Here
    cfg.rc_target_bitrate = bitrate;

    if (oxcf.Mode == kRealTime) // Real time Mode
        arg_deadline = 1;

    if (oxcf.Mode == kOnePassGoodQuality) // One Pass Good
        arg_deadline = 1000000;

    if (oxcf.Mode == kOnePassBestQuality) // One Pass Best
        arg_deadline = 0;

    if (oxcf.Mode == kTwoPassGoodQuality) // Two Pass Good
    {
        arg_passes = 2;
        arg_deadline = 1000000;
    }

    if (oxcf.Mode == kTwoPassBestQuality) // Two Pass Best
    {
        arg_passes = 2;
        arg_deadline = 0;
    }

    ///////////////////////////////////////////////////////////////////

    /* Handle codec specific options */
    if (codec->iface == &vpx_codec_vp8_cx_algo)
    {
        ctrl_args = vp8_args;
    }

    // cfg.g_timebase.den *= 2;
    memset(&stats, 0, sizeof(stats));

    for (pass = 0; pass < arg_passes; pass++)
    {
        tprintf(PRINT_BTH, "\n\n Target Bit Rate: %d \n Max Quantizer: %d \n"
            " Min Quantizer %d \n %s: %d \n", oxcf.target_bandwidth,
            oxcf.worst_allowed_q, oxcf.best_allowed_q, comp_out_str,
            compress_int);

        if(set_config == kSetConfigOn)
            tprintf(PRINT_BTH, " Set Config On\n");

        tprintf(PRINT_BTH, "\n");

        int CharCount = 0;

        if (pass == 0 && arg_passes == 2)
            tprintf(PRINT_BTH, "\nFirst Pass - ");

        if (pass == 1 && arg_passes == 2)
            tprintf(PRINT_BTH, "\nSecond Pass - ");

        if (oxcf.Mode == kRealTime) // Real time Mode
            tprintf(PRINT_BTH, " RealTime\n\n");

        if (oxcf.Mode == kOnePassGoodQuality ||
            oxcf.Mode == kTwoPassGoodQuality) // One Pass Good
            tprintf(PRINT_BTH, " GoodQuality\n\n");

        if (oxcf.Mode == kOnePassBestQuality ||
            oxcf.Mode == kTwoPassBestQuality) // One Pass Best
            tprintf(PRINT_BTH, " BestQuality\n\n");

        int frames_in = 0, frames_out = 0;
        unsigned long nbytes = 0;
        struct detect_buffer detect;

        infile = strcmp(in_fn, "-") ? fopen(in_fn, "rb") :
            set_binary_mode(stdin);

        if (!infile)
        {
            tprintf(PRINT_BTH, "Failed to open input file: %s", in_fn);

            if (RunQCheck == 1)
                quant_out_file.close();

            return -1;
        }

        detect.buf_read = fread(detect.buf, 1, 4, infile);
        detect.position = 0;

        unsigned int rate;
        unsigned int scale;

        if (detect.buf_read == 4 && file_is_y4m(infile, &y4m, detect.buf))
        {
            if (y4m_input_open(&y4m, infile, detect.buf, 4) >= 0)
            {
                file_type = FILE_TYPE_Y4M;
                cfg.g_w = y4m.pic_w;
                cfg.g_h = y4m.pic_h;

                /* Use the frame rate from the file only if none was specified
                * on the command-line.
                */
                if (!arg_have_framerate)
                {
                    rate = y4m.fps_n;
                    scale = y4m.fps_d;
                    arg_framerate.num = y4m.fps_n;
                    arg_framerate.den = y4m.fps_d;
                }

                arg_use_i420 = 0;
            }
            else
            {
                fprintf(stderr, "Unsupported Y4M stream.\n");
                return EXIT_FAILURE;
            }
        }
        else if (detect.buf_read == 4 && file_is_ivf(infile, &fourcc, &cfg.g_w,
            &cfg.g_h, &detect, &scale, &rate))
        {
            file_type = FILE_TYPE_IVF;

            switch (fourcc)
            {
            case 0x32315659:
                arg_use_i420 = 0;
                break;
            case 0x30323449:
                arg_use_i420 = 1;
                break;
            default:
                fprintf(stderr, "Unsupported fourcc (%08x) in IVF\n", fourcc);
                return EXIT_FAILURE;
            }
        }
        else
        {
            file_type = FILE_TYPE_RAW;
        }

        if (!cfg.g_w || !cfg.g_h)
        {
            fprintf(stderr, "Specify stream dimensions with --width (-w) "
                    " and --height (-h).\n");
            return EXIT_FAILURE;
        }

        std::string inputformat;
        std::string outputformat;

        if (write_webm == 1)
            outputformat = "Webm";

        if (write_webm == 0)
            outputformat = "IVF";

        if (file_type == FILE_TYPE_RAW)
            inputformat = "Raw";

        if (file_type == FILE_TYPE_Y4M)
            inputformat = "Y4M";

        if (file_type == FILE_TYPE_IVF)
            inputformat = "IVF";

        tprintf(PRINT_BTH, "API - Compressing Raw %s File to VP8 %s File: \n",
            inputformat.c_str(), outputformat.c_str());

        if (pass == (one_pass_only ? one_pass_only - 1 : 0))
        {
            if (file_type == FILE_TYPE_Y4M)
                /*The Y4M reader does its own allocation.
                Just initialize this here to avoid problems if we never read any
                frames.*/
                memset(&raw, 0, sizeof(raw));
            else
                vpx_img_alloc(&raw, arg_use_i420 ? VPX_IMG_FMT_I420 :
                VPX_IMG_FMT_YV12, cfg.g_w, cfg.g_h, 1);
        }

        outfile = strcmp(out_fn, "-") ? fopen(out_fn, "wb") :
            set_binary_mode(stdout);

        if (!outfile)
        {
            tprintf(PRINT_BTH, "Failed to open output file: %s", out_fn);
            fclose(infile);

            if (RunQCheck == 1)
                quant_out_file.close();

            return -1;
        }

        if (write_webm && fseek(outfile, 0, SEEK_CUR))
        {
            fprintf(stderr, "WebM output to pipes not supported.\n");
            fclose(infile);
            fclose(outfile);

            if (RunQCheck == 1)
                quant_out_file.close();

            return -1;
        }

        if (stats_fn)
        {
            if (!stats_open_file(&stats, stats_fn, pass))
            {
                tprintf(PRINT_STD, "Failed to open statistics store\n");
                fclose(infile);
                fclose(outfile);

                if (RunQCheck == 1)
                    quant_out_file.close();

                return -1;
            }
        }
        else
        {
            if (!stats_open_mem(&stats, pass))
            {
                tprintf(PRINT_STD, "Failed to open statistics store\n");
                fclose(infile);
                fclose(outfile);

                if (RunQCheck == 1)
                    quant_out_file.close();

                return -1;
            }
        }

        cfg.g_pass = arg_passes == 2
                     ? pass ? VPX_RC_LAST_PASS : VPX_RC_FIRST_PASS
                 : VPX_RC_ONE_PASS;
#if VPX_ENCODER_ABI_VERSION > (1 + VPX_CODEC_ABI_VERSION)

        if (pass)
        {
            cfg.rc_twopass_stats_in = stats_get(&stats);
        }

#endif

        if (write_webm)
        {
            ebml.stream = outfile;
            write_webm_file_header(&ebml, &cfg, &arg_framerate, stereo_fmt);
        }
        else
            write_ivf_file_header(outfile, &cfg, codec->fourcc, 0);

        /* Construct Encoder Context */
        if (cfg.kf_min_dist == cfg.kf_max_dist)
            cfg.kf_mode = VPX_KF_FIXED;

        /*set timebase*/
        cfg.g_timebase.den = rate;
        cfg.g_timebase.num = scale;
        arg_framerate.num = rate;
        arg_framerate.den = scale;

        /*check timebase*/
        if(arg_framerate.num > cfg.g_timebase.den)
        {
            tprintf(PRINT_BTH,"Invalid timebase: %i/%i - changing to default:"
                                " %i/%i\n",cfg.g_timebase.num,cfg.g_timebase.den
                                , 1, arg_framerate.num);
            cfg.g_timebase.den = arg_framerate.num;
            cfg.g_timebase.num = 1;
        }

        /*check cq*/
        if(oxcf.cq_level < oxcf.best_allowed_q)
        {
            int new_cq_level = oxcf.best_allowed_q +
                0.2*(oxcf.worst_allowed_q-oxcf.best_allowed_q);

            tprintf(PRINT_BTH, "cq_level: %i less than min q: %i - changing to:"
                               " %i\n", oxcf.cq_level, oxcf.best_allowed_q,
                               new_cq_level);

            oxcf.cq_level = new_cq_level;
        }
        if(oxcf.cq_level > oxcf.worst_allowed_q)
        {
            int new_cq_level = oxcf.best_allowed_q +
                0.2*(oxcf.worst_allowed_q-oxcf.best_allowed_q);

            tprintf(PRINT_BTH, "cq_level: %i greater than max q: %i - changing"
                               " to: %i\n", oxcf.cq_level, oxcf.worst_allowed_q,
                                new_cq_level);

            oxcf.cq_level = new_cq_level;
        }

        vpx_codec_enc_init(&encoder, codec->iface, &cfg, 0);
        /////////// Set Encoder Custom Settings /////////////////
        vpx_codec_control(&encoder, VP8E_SET_CPUUSED, oxcf.cpu_used);
        vpx_codec_control(&encoder, VP8E_SET_STATIC_THRESHOLD,
            oxcf.encode_breakout);
        vpx_codec_control(&encoder, VP8E_SET_ENABLEAUTOALTREF,
            oxcf.play_alternate);
        vpx_codec_control(&encoder, VP8E_SET_NOISE_SENSITIVITY,
            oxcf.noise_sensitivity);
        vpx_codec_control(&encoder, VP8E_SET_SHARPNESS, oxcf.Sharpness);
        vpx_codec_control(&encoder, VP8E_SET_TOKEN_PARTITIONS,
            (vp8e_token_partitions) oxcf.token_partitions);
        vpx_codec_control(&encoder, VP8E_SET_ARNR_MAXFRAMES,
            oxcf.arnr_max_frames);
        vpx_codec_control(&encoder, VP8E_SET_ARNR_STRENGTH, oxcf.arnr_strength);
        vpx_codec_control(&encoder, VP8E_SET_ARNR_TYPE, oxcf.arnr_type);
        vpx_codec_control(&encoder, VP8E_SET_CQ_LEVEL, oxcf.cq_level);
        vpx_codec_control(&encoder, VP8E_SET_MAX_INTRA_BITRATE_PCT,
            oxcf.rc_max_intra_bitrate_pct);

        ///////////////////////////////////////////////////////
        if (ctx_exit_on_error_tester(&encoder, "Failed to initialize encoder")
            == -1)
        {
            fclose(infile);
            fclose(outfile);

            if (RunQCheck == 1)
                quant_out_file.close();

            return -1;
        }

        frame_avail = 1;
        got_data = 0;

        if ((arg_passes == 2 && pass == 1) || arg_passes == 1)
        {
            //////////////////////// OUTPUT PARAMATERS /////////////////////////
            std::string OutputsettingsFile;
            int extLength = vpxt_remove_file_extension(outputFile2,
                OutputsettingsFile);
            OutputsettingsFile += "parameters_core.txt";
            vpxt_output_settings(OutputsettingsFile.c_str(),  oxcf);
            //////////////////////// OUTPUT PARAMATERS API /////////////////////
            OutputsettingsFile.erase(OutputsettingsFile.length() -
                (15 + extLength), 15 + extLength);
            OutputsettingsFile += "_parameters_vpx.txt";
            vpxt_output_settings_api(OutputsettingsFile.c_str(),  cfg);
            ////////////////////////////////////////////////////////////////////
        }

        while (frame_avail || got_data)
        {
            vpx_codec_iter_t iter = NULL;
            const vpx_codec_cx_pkt_t *pkt;
            struct vpx_usec_timer timer;
            int64_t frame_start, next_frame_start;

            // if set_config reset cfg using vpx_codec_enc_config_set
            // tests to make sure vpx_codec_enc_config_set functions correctly.
            if(set_config == kSetConfigOn && frames_in > 1)
                vpx_codec_enc_config_set(&encoder, &cfg);

            if (!arg_limit || frames_in < arg_limit)
            {
                frame_avail = read_frame_enc(infile, &raw, file_type, &y4m,
                    &detect);

                if (frame_avail)
                    frames_in++;

                if (CharCount == 79)
                {
                    tprintf(PRINT_BTH, "\n");
                    CharCount = 0;
                }

                CharCount++;
                tprintf(PRINT_BTH, ".");
            }
            else
                frame_avail = 0;

            frame_start = (cfg.g_timebase.den * (int64_t)(frames_in - 1)
                           * arg_framerate.den) / cfg.g_timebase.num /
                           arg_framerate.num;
            next_frame_start = (cfg.g_timebase.den * (int64_t)(frames_in)
                                * arg_framerate.den)
                               / cfg.g_timebase.num / arg_framerate.num;

            vpx_usec_timer_start(&timer);
            vpx_codec_encode(&encoder, frame_avail ? &raw : NULL, frame_start,
                next_frame_start - frame_start, 0, arg_deadline);
            vpx_usec_timer_mark(&timer);
            cx_time += vpx_usec_timer_elapsed(&timer);
            ctx_exit_on_error_tester(&encoder, "Failed to encode frame");
            got_data = 0;

            if (RunQCheck == 1)
            {
                if ((arg_passes == 2 && pass == 1) || arg_passes == 1)
                {
                    int lastQuantizerValue = 0;
                    vpx_codec_control(&encoder, VP8E_GET_LAST_QUANTIZER_64,
                        &lastQuantizerValue);
                    quant_out_file << frames_out << " " << lastQuantizerValue
                        << "\n";
                }
            }

            if (RunQCheck == 2)
            {
                // Print Quantizers
                int lastQuantizerValue = 0;
                vpx_codec_control(&encoder, VP8E_GET_LAST_QUANTIZER_64,
                    &lastQuantizerValue);
                tprintf(PRINT_STD, "frame %i Quantizer: %i\n", frames_out,
                    lastQuantizerValue);
            }

            while ((pkt = vpx_codec_get_cx_data(&encoder, &iter)))
            {
                got_data = 1;

                switch (pkt->kind)
                {
                case VPX_CODEC_CX_FRAME_PKT:
                    frames_out++;

                    if (write_webm)
                    {
                        if (!ebml.debug)
                            hash = murmur(pkt->data.frame.buf,
                                          pkt->data.frame.sz, hash);

                        write_webm_block(&ebml, &cfg, pkt);
                    }
                    else
                    {
                        write_ivf_frame_header(outfile, pkt);

                        if (fwrite(pkt->data.frame.buf, 1,
                                   pkt->data.frame.sz, outfile));
                    }

                    nbytes += pkt->data.raw.sz;
                    break;
                case VPX_CODEC_STATS_PKT:
                    frames_out++;
                    stats_write(&stats,
                                pkt->data.twopass_stats.buf,
                                pkt->data.twopass_stats.sz);
                    nbytes += pkt->data.raw.sz;
                    break;
                case VPX_CODEC_PSNR_PKT:

                    if (0)
                    {
                        int i;

                        for (i = 0; i < 4; i++)
                            fprintf(stderr, "%.3lf ", pkt->data.psnr.psnr[i]);
                    }

                    break;
                default:
                    break;
                }
            }

            fflush(stdout);
        }

        /* this bitrate calc is simplified and relies on the fact that this
        * application uses 1/timebase for framerate.
        */
        {
            uint64_t temp = (uint64_t)frames_in * 1000000;

        }
        vpx_codec_destroy(&encoder);

        fclose(infile);
        if (file_type == FILE_TYPE_Y4M)
            y4m_input_close(&y4m);

        if (write_webm)
        {
            write_webm_file_footer(&ebml, hash);
        }
        else
        {
            if (!fseek(outfile, 0, SEEK_SET))
                write_ivf_file_header(outfile, &cfg, codec->fourcc, frames_out);
        }

        fclose(outfile);
        stats_close(&stats, arg_passes - 1);
        tprintf(PRINT_BTH, "\n");
    }

    vpx_img_free(&raw);
    free(ebml.cue_list);

    if (RunQCheck == 1)
        quant_out_file.close();

    return 0;
}
int vpxt_compress_no_error_output(const char *input_file,
                                  const char *outputFile2,
                                  int speed, int bitrate,
                                  VP8_CONFIG &oxcf,
                                  const char *comp_out_str,
                                  int compress_int,
                                  int RunQCheck,
                                  std::string EncFormat)
{
    int write_webm = 1;
    vpxt_lower_case_string(EncFormat);

    if (EncFormat.compare("ivf") == 0)
        write_webm = 0;

    // RunQCheck - Signifies if the quantizers should be check to make sure
    // theyre working properly during an encode
    // RunQCheck = 0 = Do not save q values
    // RunQCheck = 1 = Save q values
    // RunQCheck = 2 = display q values only
    std::ofstream quant_out_file;

    if (RunQCheck == 1)
    {
        std::string QuantOutStr;
        vpxt_remove_file_extension(outputFile2, QuantOutStr);
        QuantOutStr += "quantizers.txt";
        quant_out_file.open(QuantOutStr.c_str());
    }

    ///////////////// DELETE ME TEMP MEASURE ///////////////////////////////////
    if (oxcf.Mode == 3) // Real time Mode
    {
        if (RunQCheck == 1)
            quant_out_file.close();

        return 0;
    }
    ////////////////////////////////////////////////////////////////////////////

    unsigned int             file_type, fourcc;
    y4m_input                y4m;
    int                      arg_have_framerate = 0;
    EbmlGlobal               ebml = {0};
    uint32_t                 hash = 0;
    vpx_codec_ctx_t          encoder;
    const char              *in_fn = input_file, *out_fn = outputFile2;
    const char              *stats_fn = NULL;
    FILE                    *infile, *outfile;
    vpx_codec_enc_cfg_t      cfg;
    vpx_codec_err_t          res;
    int                      pass, one_pass_only = 0;
    stats_io_t               stats;
    vpx_image_t              raw;
    const struct codec_item *codec = codecs;
    int                      frame_avail, got_data;
    int                      arg_usage = 0, arg_passes = 1, arg_deadline = 0;
    int                      arg_limit = 0;
    static const arg_def_t **ctrl_args = no_args;
    int                      verbose = 0;
    int                      arg_use_i420 = 1;
    double                   total_cpu_time_used = 0;
    unsigned long            cx_time = 0;
    struct vpx_rational      arg_framerate = {30, 1};
    stereo_format_t          stereo_fmt = STEREO_FORMAT_MONO;

    ebml.last_pts_ms = -1;
    /* Populate encoder configuration */
    res = vpx_codec_enc_config_default(codec->iface, &cfg, arg_usage);
    if (res){
        tprintf(PRINT_STD, "Failed to get config: %s\n",
            vpx_codec_err_to_string(res));

        if (RunQCheck == 1)
            quant_out_file.close();

        return -1;
    }

    vpxt_core_config_to_api_config(oxcf, &cfg);

    // Set Mode Pass and bitrate Here
    cfg.rc_target_bitrate = bitrate;

    if (oxcf.Mode == kRealTime) // Real time Mode
        arg_deadline = 1;

    if (oxcf.Mode == kOnePassGoodQuality) // One Pass Good
        arg_deadline = 1000000;

    if (oxcf.Mode == kOnePassBestQuality) // One Pass Best
        arg_deadline = 0;

    if (oxcf.Mode == kTwoPassGoodQuality) // Two Pass Good
    {
        arg_passes = 2;
        arg_deadline = 1000000;
    }

    if (oxcf.Mode == kTwoPassBestQuality) // Two Pass Best
    {
        arg_passes = 2;
        arg_deadline = 0;
    }

    ///////////////////////////////////////////////////////////////////

    /* Handle codec specific options */
    if (codec->iface == &vpx_codec_vp8_cx_algo)
    {
        ctrl_args = vp8_args;
    }

    /*vpx_img_alloc(&raw, arg_use_i420 ? IMG_FMT_I420 : IMG_FMT_YV12,
    cfg.g_w, cfg.g_h, 1);*/

    // cfg.g_timebase.den *= 2;
    memset(&stats, 0, sizeof(stats));

    for (pass = 0; pass < arg_passes; pass++)
    {
        tprintf(PRINT_STD, "\n\n Target Bit Rate: %d \n Max Quantizer: %d \n"
            " Min Quantizer %d \n %s: %d \n \n", oxcf.target_bandwidth,
            oxcf.worst_allowed_q, oxcf.best_allowed_q, comp_out_str,
            compress_int);

        int CharCount = 0;

        if (pass == 0 && arg_passes == 2)
            tprintf(PRINT_STD, "\nFirst Pass - ");

        if (pass == 1 && arg_passes == 2)
            tprintf(PRINT_STD, "\nSecond Pass - ");

        if (oxcf.Mode == kRealTime) // Real time Mode
            tprintf(PRINT_STD, " RealTime\n\n");

        if (oxcf.Mode == kOnePassGoodQuality ||
            oxcf.Mode == kTwoPassGoodQuality) // One Pass Good
            tprintf(PRINT_STD, " GoodQuality\n\n");

        if (oxcf.Mode == kOnePassBestQuality ||
            oxcf.Mode == kTwoPassBestQuality) // One Pass Best
            tprintf(PRINT_STD, " BestQuality\n\n");

        int frames_in = 0, frames_out = 0;
        unsigned long nbytes = 0;
        struct detect_buffer detect;

        infile = strcmp(in_fn, "-") ? fopen(in_fn, "rb") :
            set_binary_mode(stdin);

        if (!infile)
        {
            tprintf(PRINT_BTH, "Failed to open input file: %s", in_fn);

            if (RunQCheck == 1)
                quant_out_file.close();

            return -1;
        }

        detect.buf_read = fread(detect.buf, 1, 4, infile);
        detect.position = 0;
        unsigned int rate;
        unsigned int scale;

        if (detect.buf_read == 4 && file_is_y4m(infile, &y4m, detect.buf))
        {
            if (y4m_input_open(&y4m, infile, detect.buf, 4) >= 0)
            {
                file_type = FILE_TYPE_Y4M;
                cfg.g_w = y4m.pic_w;
                cfg.g_h = y4m.pic_h;

                /* Use the frame rate from the file only if none was specified
                * on the command-line.
                */
                if (!arg_have_framerate)
                {
                    rate = y4m.fps_n;
                    scale = y4m.fps_d;
                    arg_framerate.num = y4m.fps_n;
                    arg_framerate.den = y4m.fps_d;
                }

                arg_use_i420 = 0;
            }
            else
            {
                fprintf(stderr, "Unsupported Y4M stream.\n");
                return EXIT_FAILURE;
            }
        }
        else if (detect.buf_read == 4 &&
                 file_is_ivf(infile, &fourcc, &cfg.g_w, &cfg.g_h, &detect,
                 &scale, &rate))
        {
            file_type = FILE_TYPE_IVF;

            switch (fourcc)
            {
            case 0x32315659:
                arg_use_i420 = 0;
                break;
            case 0x30323449:
                arg_use_i420 = 1;
                break;
            default:
                fprintf(stderr, "Unsupported fourcc (%08x) in IVF\n", fourcc);
                return EXIT_FAILURE;
            }
        }
        else
        {
            file_type = FILE_TYPE_RAW;
        }

        if (!cfg.g_w || !cfg.g_h)
        {
            fprintf(stderr, "Specify stream dimensions with --width (-w) "
                    " and --height (-h).\n");
            return EXIT_FAILURE;
        }

        std::string inputformat;
        std::string outputformat;

        if (write_webm == 1)
            outputformat = "Webm";

        if (write_webm == 0)
            outputformat = "IVF";

        if (file_type == FILE_TYPE_RAW)
            inputformat = "Raw";

        if (file_type == FILE_TYPE_Y4M)
            inputformat = "Y4M";

        if (file_type == FILE_TYPE_IVF)
            inputformat = "IVF";

        tprintf(PRINT_STD, "API - Compressing Raw %s File to VP8 %s File: \n",
            inputformat.c_str(), outputformat.c_str());

        if (pass == (one_pass_only ? one_pass_only - 1 : 0))
        {
            if (file_type == FILE_TYPE_Y4M)
                /*The Y4M reader does its own allocation.
                Just initialize this here to avoid problems if we never read any
                frames.*/
                memset(&raw, 0, sizeof(raw));
            else
                vpx_img_alloc(&raw, arg_use_i420 ? VPX_IMG_FMT_I420 :
                VPX_IMG_FMT_YV12,
                              cfg.g_w, cfg.g_h, 1);
        }

        outfile = strcmp(out_fn, "-") ? fopen(out_fn, "wb") :
            set_binary_mode(stdout);

        if (!outfile)
        {
            tprintf(PRINT_BTH, "Failed to open output file: %s", out_fn);
            fclose(infile);

            if (RunQCheck == 1)
                quant_out_file.close();

            return -1;
        }

        if (write_webm && fseek(outfile, 0, SEEK_CUR))
        {
            fprintf(stderr, "WebM output to pipes not supported.\n");
            fclose(infile);
            fclose(outfile);

            if (RunQCheck == 1)
                quant_out_file.close();

            return -1;
        }

        if (stats_fn)
        {
            if (!stats_open_file(&stats, stats_fn, pass))
            {
                tprintf(PRINT_STD, "Failed to open statistics store\n");
                fclose(infile);
                fclose(outfile);

                if (RunQCheck == 1)
                    quant_out_file.close();

                return -1;
            }
        }
        else
        {
            if (!stats_open_mem(&stats, pass))
            {
                tprintf(PRINT_STD, "Failed to open statistics store\n");
                fclose(infile);
                fclose(outfile);

                if (RunQCheck == 1)
                    quant_out_file.close();

                return -1;
            }
        }

        cfg.g_pass = arg_passes == 2
                     ? pass ? VPX_RC_LAST_PASS : VPX_RC_FIRST_PASS
                 : VPX_RC_ONE_PASS;
#if VPX_ENCODER_ABI_VERSION > (1 + VPX_CODEC_ABI_VERSION)

        if (pass)
        {
            cfg.rc_twopass_stats_in = stats_get(&stats);
        }

#endif

        if (write_webm)
        {
            ebml.stream = outfile;
            write_webm_file_header(&ebml, &cfg, &arg_framerate, stereo_fmt);
        }
        else
            write_ivf_file_header(outfile, &cfg, codec->fourcc, 0);

        /* Construct Encoder Context */
        if (cfg.kf_min_dist == cfg.kf_max_dist)
            cfg.kf_mode = VPX_KF_FIXED;

        /*set timebase*/
        cfg.g_timebase.den = rate;
        cfg.g_timebase.num = scale;
        arg_framerate.num = rate;
        arg_framerate.den = scale;

        /*check timebase*/
        if(arg_framerate.num > cfg.g_timebase.den)
        {
            tprintf(PRINT_BTH,"Invalid timebase: %i/%i - changing to default:"
                                " %i/%i\n",cfg.g_timebase.num,cfg.g_timebase.den
                                , 1, arg_framerate.num);
            cfg.g_timebase.den = arg_framerate.num;
            cfg.g_timebase.num = 1;
        }

        /*check cq*/
        if(oxcf.cq_level < oxcf.best_allowed_q)
        {
            int new_cq_level = oxcf.best_allowed_q +
                0.2*(oxcf.worst_allowed_q-oxcf.best_allowed_q);

            tprintf(PRINT_BTH, "cq_level: %i less than min q: %i - changing to:"
                               " %i\n", oxcf.cq_level, oxcf.best_allowed_q,
                               new_cq_level);

            oxcf.cq_level = new_cq_level;
        }
        if(oxcf.cq_level > oxcf.worst_allowed_q)
        {
            int new_cq_level = oxcf.best_allowed_q +
                0.2*(oxcf.worst_allowed_q-oxcf.best_allowed_q);

            tprintf(PRINT_BTH, "cq_level: %i greater than max q: %i - changing"
                               " to: %i\n", oxcf.cq_level, oxcf.worst_allowed_q,
                                new_cq_level);

            oxcf.cq_level = new_cq_level;
        }

        vpx_codec_enc_init(&encoder, codec->iface, &cfg, 0);
        /////////// Set Encoder Custom Settings /////////////////
        vpx_codec_control(&encoder, VP8E_SET_CPUUSED, oxcf.cpu_used);
        vpx_codec_control(&encoder, VP8E_SET_STATIC_THRESHOLD,
            oxcf.encode_breakout);
        vpx_codec_control(&encoder, VP8E_SET_ENABLEAUTOALTREF,
            oxcf.play_alternate);
        vpx_codec_control(&encoder, VP8E_SET_NOISE_SENSITIVITY,
            oxcf.noise_sensitivity);
        vpx_codec_control(&encoder, VP8E_SET_SHARPNESS, oxcf.Sharpness);
        vpx_codec_control(&encoder, VP8E_SET_TOKEN_PARTITIONS,
            (vp8e_token_partitions) oxcf.token_partitions);
        vpx_codec_control(&encoder, VP8E_SET_ARNR_MAXFRAMES,
            oxcf.arnr_max_frames);
        vpx_codec_control(&encoder, VP8E_SET_ARNR_STRENGTH, oxcf.arnr_strength);
        vpx_codec_control(&encoder, VP8E_SET_ARNR_TYPE, oxcf.arnr_type);
        vpx_codec_control(&encoder, VP8E_SET_CQ_LEVEL, oxcf.cq_level);
        vpx_codec_control(&encoder, VP8E_SET_MAX_INTRA_BITRATE_PCT,
            oxcf.rc_max_intra_bitrate_pct);

        ///////////////////////////////////////////////////////
        if (ctx_exit_on_error_tester(&encoder, "Failed to initialize encoder")
            == -1)
        {
            fclose(infile);
            fclose(outfile);

            if (RunQCheck == 1)
                quant_out_file.close();

            return -1;
        }

        frame_avail = 1;
        got_data = 0;

        if ((arg_passes == 2 && pass == 1) || arg_passes == 1)
        {
            //////////////////////// OUTPUT PARAMATERS /////////////////////////
            std::string OutputsettingsFile;
            int extLength = vpxt_remove_file_extension(outputFile2,
                OutputsettingsFile);
            OutputsettingsFile += "parameters_core.txt";
            vpxt_output_settings(OutputsettingsFile.c_str(),  oxcf);
            ////////////////////// OUTPUT PARAMATERS API ///////////////////////
            OutputsettingsFile.erase(OutputsettingsFile.length() -
                (15 + extLength), 15 + extLength);
            OutputsettingsFile += "_parameters_vpx.txt";
            vpxt_output_settings_api(OutputsettingsFile.c_str(),  cfg);
            ////////////////////////////////////////////////////////////////////
        }

        while (frame_avail || got_data)
        {
            vpx_codec_iter_t iter = NULL;
            const vpx_codec_cx_pkt_t *pkt;
            clock_t start = 0, end = 0;
            struct vpx_usec_timer timer;
            int64_t frame_start, next_frame_start;

            if (!arg_limit || frames_in < arg_limit)
            {
                frame_avail = read_frame_enc(infile, &raw, file_type, &y4m,
                                             &detect);
                if (frame_avail)
                    frames_in++;

                if (CharCount == 79)
                {
                    tprintf(PRINT_STD, "\n");
                    CharCount = 0;
                }

                CharCount++;
                tprintf(PRINT_STD, ".");
            }
            else
                frame_avail = 0;

            frame_start = (cfg.g_timebase.den * (int64_t)(frames_in - 1)
                           * arg_framerate.den) / cfg.g_timebase.num /
                           arg_framerate.num;
            next_frame_start = (cfg.g_timebase.den * (int64_t)(frames_in)
                                * arg_framerate.den)
                               / cfg.g_timebase.num / arg_framerate.num;

            vpx_usec_timer_start(&timer);
            vpx_codec_encode(&encoder, frame_avail ? &raw : NULL, frame_start,
                next_frame_start - frame_start, 0, arg_deadline);
            vpx_usec_timer_mark(&timer);
            cx_time += vpx_usec_timer_elapsed(&timer);

            total_cpu_time_used = total_cpu_time_used + (end - start);

            ctx_exit_on_error_tester(&encoder, "Failed to encode frame");
            got_data = 0;

            if (RunQCheck == 1)
            {
                if ((arg_passes == 2 && pass == 1) || arg_passes == 1)
                {
                    int lastQuantizerValue = 0;
                    vpx_codec_control(&encoder, VP8E_GET_LAST_QUANTIZER_64,
                        &lastQuantizerValue);
                    quant_out_file << frames_out << " " << lastQuantizerValue
                        << "\n";
                }
            }

            if (RunQCheck == 2)
            {
                // Print Quantizers
                int lastQuantizerValue = 0;
                vpx_codec_control(&encoder, VP8E_GET_LAST_QUANTIZER_64,
                    &lastQuantizerValue);
                tprintf(PRINT_STD, "frame %i Quantizer: %i\n", frames_out,
                    lastQuantizerValue);
            }

            while ((pkt = vpx_codec_get_cx_data(&encoder, &iter)))
            {
                got_data = 1;

                switch (pkt->kind)
                {
                case VPX_CODEC_CX_FRAME_PKT:
                    frames_out++;

                    if (write_webm)
                    {
                        if (!ebml.debug)
                            hash = murmur(pkt->data.frame.buf,
                                          pkt->data.frame.sz, hash);

                        write_webm_block(&ebml, &cfg, pkt);
                    }
                    else
                    {
                        write_ivf_frame_header(outfile, pkt);

                        if (fwrite(pkt->data.frame.buf, 1,
                                   pkt->data.frame.sz, outfile));
                    }

                    nbytes += pkt->data.raw.sz;
                    break;
                case VPX_CODEC_STATS_PKT:
                    frames_out++;
                    stats_write(&stats,
                                pkt->data.twopass_stats.buf,
                                pkt->data.twopass_stats.sz);
                    nbytes += pkt->data.raw.sz;
                    break;
                case VPX_CODEC_PSNR_PKT:

                    if (0)
                    {
                        int i;

                        for (i = 0; i < 4; i++)
                            fprintf(stderr, "%.3lf ", pkt->data.psnr.psnr[i]);
                    }

                    break;
                default:
                    break;
                }
            }

            fflush(stdout);
        }

        /* this bitrate calc is simplified and relies on the fact that this
        * application uses 1/timebase for framerate.
        */
        {
            // uint64_t temp= (uint64_t)frames_in * 1000000;
        }
        vpx_codec_destroy(&encoder);

        fclose(infile);
        if (file_type == FILE_TYPE_Y4M)
            y4m_input_close(&y4m);

        if (write_webm)
        {
            write_webm_file_footer(&ebml, hash);
        }
        else
        {
            if (!fseek(outfile, 0, SEEK_SET))
                write_ivf_file_header(outfile, &cfg, codec->fourcc, frames_out);
        }

        fclose(outfile);
        stats_close(&stats, arg_passes - 1);
        tprintf(PRINT_STD, "\n");
    }

    vpx_img_free(&raw);
    free(ebml.cue_list);

    if (RunQCheck == 1)
        quant_out_file.close();

    return 0;
}
unsigned int vpxt_time_compress(const char *input_file,
                                const char *outputFile2,
                                int speed, int bitrate,
                                VP8_CONFIG &oxcf,
                                const char *comp_out_str,
                                int compress_int,
                                int RunQCheck,
                                unsigned int &CPUTick,
                                std::string EncFormat)
{
    int                      write_webm = 1;
    vpxt_lower_case_string(EncFormat);

    if (EncFormat.compare("ivf") == 0)
        write_webm = 0;

    // RunQCheck - Signifies if the quantizers should be check to make sure
    // theyre working properly during an encode
    // RunQCheck = 0 = Do not save q values
    // RunQCheck = 1 = Save q values
    // RunQCheck = 2 = display q values only
    std::ofstream quant_out_file;

    if (RunQCheck == 1)
    {
        std::string QuantOutStr = outputFile2;
        QuantOutStr.erase(QuantOutStr.length() - 4, 4);
        QuantOutStr += "_quantizers.txt";
        quant_out_file.open(QuantOutStr.c_str());
    }

    ///////////////////// DELETE ME TEMP MEASURE ///////////////////////////////
    if (oxcf.Mode == 3) // Real time Mode
    {
        if (RunQCheck == 1)
            quant_out_file.close();

        return 0;
    }
    ////////////////////////////////////////////////////////////////////////////
    unsigned int             file_type, fourcc;
    y4m_input                y4m;
    int                      arg_have_framerate = 0;
    EbmlGlobal               ebml = {0};
    uint32_t                 hash = 0;
    vpx_codec_ctx_t          encoder;
    const char              *in_fn = input_file, *out_fn = outputFile2, *stats_fn = NULL;
    FILE                    *infile, *outfile;
    vpx_codec_enc_cfg_t      cfg;
    vpx_codec_err_t          res;
    int                      pass, one_pass_only = 0;
    stats_io_t               stats;
    vpx_image_t              raw;
    const struct codec_item *codec = codecs;
    int                      frame_avail, got_data;
    int                      arg_usage = 0, arg_passes = 1, arg_deadline = 0;
    int                      arg_limit = 0;
    static const arg_def_t **ctrl_args = no_args;
    int                      verbose = 0;
    int                      arg_use_i420 = 1;
    unsigned long            cx_time = 0;
    unsigned int             total_cpu_time_used = 0;
    int                      framesoutrec = 0;
    struct vpx_rational      arg_framerate = {30, 1};
    stereo_format_t          stereo_fmt = STEREO_FORMAT_MONO;

    ebml.last_pts_ms = -1;
    /* Populate encoder configuration */
    res = vpx_codec_enc_config_default(codec->iface, &cfg, arg_usage);
    if (res){
        tprintf(PRINT_STD, "Failed to get config: %s\n",
            vpx_codec_err_to_string(res));

        if (RunQCheck == 1)
            quant_out_file.close();

        return -1;
    }

    vpxt_core_config_to_api_config(oxcf, &cfg);

    // Set Mode Pass and bitrate Here
    cfg.rc_target_bitrate = bitrate;

    if (oxcf.Mode == kRealTime) // Real time Mode
        arg_deadline = 1;

    if (oxcf.Mode == kOnePassGoodQuality) // One Pass Good
        arg_deadline = 1000000;

    if (oxcf.Mode == kOnePassBestQuality) // One Pass Best
        arg_deadline = 0;

    if (oxcf.Mode == kTwoPassGoodQuality) // Two Pass Good
    {
        arg_passes = 2;
        arg_deadline = 1000000;
    }

    if (oxcf.Mode == kTwoPassBestQuality) // Two Pass Best
    {
        arg_passes = 2;
        arg_deadline = 0;
    }

    ///////////////////////////////////////////////////////////////////

    /* Handle codec specific options */
    if (codec->iface == &vpx_codec_vp8_cx_algo)
    {
        ctrl_args = vp8_args;
    }

    cfg.g_timebase.den = 1000;
    memset(&stats, 0, sizeof(stats));

    for (pass = 0; pass < arg_passes; pass++)
    {
        tprintf(PRINT_BTH, "\n\n Target Bit Rate: %d \n Max Quantizer: %d \n"
            " Min Quantizer %d \n %s: %d \n \n", oxcf.target_bandwidth,
            oxcf.worst_allowed_q, oxcf.best_allowed_q, comp_out_str,
            compress_int);

        int CharCount = 0;

        if (pass == 0 && arg_passes == 2)
            tprintf(PRINT_BTH, "\nFirst Pass - ");

        if (pass == 1 && arg_passes == 2)
            tprintf(PRINT_BTH, "\nSecond Pass - ");

        if (oxcf.Mode == kRealTime) // Real time Mode
            tprintf(PRINT_BTH, " RealTime\n\n");

        if (oxcf.Mode == kOnePassGoodQuality ||
            oxcf.Mode == kTwoPassGoodQuality) // One Pass Good
            tprintf(PRINT_BTH, " GoodQuality\n\n");

        if (oxcf.Mode == kOnePassBestQuality ||
            oxcf.Mode == kTwoPassBestQuality) // One Pass Best
            tprintf(PRINT_BTH, " BestQuality\n\n");

        int frames_in = 0, frames_out = 0;
        unsigned long nbytes = 0;
        struct detect_buffer detect;

        infile = strcmp(in_fn, "-") ? fopen(in_fn, "rb") :
            set_binary_mode(stdin);

        if (!infile)
        {
            tprintf(PRINT_STD, "Failed to open input file");

            if (RunQCheck == 1)
                quant_out_file.close();

            return -1;
        }

        detect.buf_read = fread(detect.buf, 1, 4, infile);
        detect.position = 0;

        unsigned int rate;
        unsigned int scale;

        if (detect.buf_read == 4 && file_is_y4m(infile, &y4m, detect.buf))
        {
            if (y4m_input_open(&y4m, infile, detect.buf, 4) >= 0)
            {
                file_type = FILE_TYPE_Y4M;
                cfg.g_w = y4m.pic_w;
                cfg.g_h = y4m.pic_h;

                /* Use the frame rate from the file only if none was specified
                * on the command-line.
                */
                if (!arg_have_framerate)
                {
                    rate = y4m.fps_n;
                    scale = y4m.fps_d;
                    arg_framerate.num = y4m.fps_n;
                    arg_framerate.den = y4m.fps_d;
                }

                arg_use_i420 = 0;
            }
            else
            {
                fprintf(stderr, "Unsupported Y4M stream.\n");
                return EXIT_FAILURE;
            }
        }
        else if (detect.buf_read == 4 &&
                 file_is_ivf(infile, &fourcc, &cfg.g_w, &cfg.g_h, &detect,
                 &scale, &rate))
        {
            file_type = FILE_TYPE_IVF;

            switch (fourcc)
            {
            case 0x32315659:
                arg_use_i420 = 0;
                break;
            case 0x30323449:
                arg_use_i420 = 1;
                break;
            default:
                fprintf(stderr, "Unsupported fourcc (%08x) in IVF\n", fourcc);
                return EXIT_FAILURE;
            }
        }
        else
        {
            file_type = FILE_TYPE_RAW;
        }

        if (!cfg.g_w || !cfg.g_h)
        {
            fprintf(stderr, "Specify stream dimensions with --width (-w) "
                    " and --height (-h).\n");
            return EXIT_FAILURE;
        }

        std::string inputformat;
        std::string outputformat;

        if (write_webm == 1)
            outputformat = "Webm";

        if (write_webm == 0)
            outputformat = "IVF";

        if (file_type == FILE_TYPE_RAW)
            inputformat = "Raw";

        if (file_type == FILE_TYPE_Y4M)
            inputformat = "Y4M";

        if (file_type == FILE_TYPE_IVF)
            inputformat = "IVF";

        tprintf(PRINT_BTH, "API - Compressing Raw %s File to VP8 %s File: \n",
            inputformat.c_str(), outputformat.c_str());

        if (pass == (one_pass_only ? one_pass_only - 1 : 0))
        {
            if (file_type == FILE_TYPE_Y4M)
                /*The Y4M reader does its own allocation.
                Just initialize this here to avoid problems if we never read any
                frames.*/
                memset(&raw, 0, sizeof(raw));
            else
                vpx_img_alloc(&raw, arg_use_i420 ? VPX_IMG_FMT_I420 :
                VPX_IMG_FMT_YV12, cfg.g_w, cfg.g_h, 1);
        }

        outfile = strcmp(out_fn, "-") ? fopen(out_fn, "wb") :
            set_binary_mode(stdout);

        if (!outfile)
        {
            tprintf(PRINT_STD, "Failed to open output file");
            fclose(infile);

            if (RunQCheck == 1)
                quant_out_file.close();

            return -1;
        }

        if (write_webm && fseek(outfile, 0, SEEK_CUR))
        {
            fprintf(stderr, "WebM output to pipes not supported.\n");
            fclose(infile);
            fclose(outfile);

            if (RunQCheck == 1)
                quant_out_file.close();

            return -1;

        }

        if (stats_fn)
        {
            if (!stats_open_file(&stats, stats_fn, pass))
            {
                tprintf(PRINT_STD, "Failed to open statistics store\n");
                fclose(infile);
                fclose(outfile);

                if (RunQCheck == 1)
                    quant_out_file.close();

                return -1;
            }
        }
        else
        {
            if (!stats_open_mem(&stats, pass))
            {
                tprintf(PRINT_STD, "Failed to open statistics store\n");
                fclose(infile);
                fclose(outfile);

                if (RunQCheck == 1)
                    quant_out_file.close();

                return -1;
            }
        }

        cfg.g_pass = arg_passes == 2
                     ? pass ? VPX_RC_LAST_PASS : VPX_RC_FIRST_PASS
                 : VPX_RC_ONE_PASS;
#if VPX_ENCODER_ABI_VERSION > (1 + VPX_CODEC_ABI_VERSION)

        if (pass)
        {
            cfg.rc_twopass_stats_in = stats_get(&stats);
        }

#endif

        if (write_webm)
        {
            ebml.stream = outfile;
            write_webm_file_header(&ebml, &cfg, &arg_framerate, stereo_fmt);
        }
        else
            write_ivf_file_header(outfile, &cfg, codec->fourcc, 0);

        /* Construct Encoder Context */
        if (cfg.kf_min_dist == cfg.kf_max_dist)
            cfg.kf_mode = VPX_KF_FIXED;

        /*set timebase*/
        cfg.g_timebase.den = rate;
        cfg.g_timebase.num = scale;
        arg_framerate.num = rate;
        arg_framerate.den = scale;

        /*check timebase*/
        if(arg_framerate.num > cfg.g_timebase.den)
        {
            tprintf(PRINT_BTH,"Invalid timebase: %i/%i - changing to default:"
                                " %i/%i\n",cfg.g_timebase.num,cfg.g_timebase.den
                                , 1, arg_framerate.num);
            cfg.g_timebase.den = arg_framerate.num;
            cfg.g_timebase.num = 1;
        }

        /*check cq*/
        if(oxcf.cq_level < oxcf.best_allowed_q)
        {
            int new_cq_level = oxcf.best_allowed_q +
                0.2*(oxcf.worst_allowed_q-oxcf.best_allowed_q);

            tprintf(PRINT_BTH, "cq_level: %i less than min q: %i - changing to:"
                               " %i\n", oxcf.cq_level, oxcf.best_allowed_q,
                               new_cq_level);

            oxcf.cq_level = new_cq_level;
        }
        if(oxcf.cq_level > oxcf.worst_allowed_q)
        {
            int new_cq_level = oxcf.best_allowed_q +
                0.2*(oxcf.worst_allowed_q-oxcf.best_allowed_q);

            tprintf(PRINT_BTH, "cq_level: %i greater than max q: %i - changing"
                               " to: %i\n", oxcf.cq_level, oxcf.worst_allowed_q,
                                new_cq_level);

            oxcf.cq_level = new_cq_level;
        }

        vpx_codec_enc_init(&encoder, codec->iface, &cfg, 0);
        /////////// Set Encoder Custom Settings /////////////////
        vpx_codec_control(&encoder, VP8E_SET_CPUUSED, oxcf.cpu_used);
        vpx_codec_control(&encoder, VP8E_SET_STATIC_THRESHOLD,
            oxcf.encode_breakout);
        vpx_codec_control(&encoder, VP8E_SET_ENABLEAUTOALTREF,
            oxcf.play_alternate);
        vpx_codec_control(&encoder, VP8E_SET_NOISE_SENSITIVITY,
            oxcf.noise_sensitivity);
        vpx_codec_control(&encoder, VP8E_SET_SHARPNESS, oxcf.Sharpness);
        vpx_codec_control(&encoder, VP8E_SET_TOKEN_PARTITIONS,
            (vp8e_token_partitions) oxcf.token_partitions);
        vpx_codec_control(&encoder, VP8E_SET_ARNR_MAXFRAMES,
            oxcf.arnr_max_frames);
        vpx_codec_control(&encoder, VP8E_SET_ARNR_STRENGTH, oxcf.arnr_strength);
        vpx_codec_control(&encoder, VP8E_SET_ARNR_TYPE, oxcf.arnr_type);
        vpx_codec_control(&encoder, VP8E_SET_CQ_LEVEL, oxcf.cq_level);
        vpx_codec_control(&encoder, VP8E_SET_MAX_INTRA_BITRATE_PCT,
            oxcf.rc_max_intra_bitrate_pct);

        ///////////////////////////////////////////////////////
        if (ctx_exit_on_error_tester(&encoder, "Failed to initialize encoder")
            == -1)
        {
            fclose(infile);
            fclose(outfile);

            if (RunQCheck == 1)
                quant_out_file.close();

            return -1;
        }

        frame_avail = 1;
        got_data = 0;

        if ((arg_passes == 2 && pass == 1) || arg_passes == 1)
        {
            ////////////////////// OUTPUT PARAMATERS ///////////////////////////
            std::string OutputsettingsFile;
            int extLength = vpxt_remove_file_extension(outputFile2,
                OutputsettingsFile);
            OutputsettingsFile += "parameters_core.txt";
            vpxt_output_settings(OutputsettingsFile.c_str(),  oxcf);
            ////////////////// OUTPUT PARAMATERS API ///////////////////////////
            OutputsettingsFile.erase(OutputsettingsFile.length() -
                (15 + extLength), 15 + extLength);
            OutputsettingsFile += "_parameters_vpx.txt";
            vpxt_output_settings_api(OutputsettingsFile.c_str(),  cfg);
            ////////////////////////////////////////////////////////////////////
        }

        while (frame_avail || got_data)
        {
            vpx_codec_iter_t iter = NULL;
            const vpx_codec_cx_pkt_t *pkt;
            unsigned int start, end;
            struct vpx_usec_timer timer;
            int64_t frame_start, next_frame_start;

            if (!arg_limit || frames_in < arg_limit)
            {

                frame_avail = read_frame_enc(infile, &raw, file_type, &y4m,
                                             &detect);

                if (frame_avail)
                    frames_in++;

                if (CharCount == 79)
                {
                    tprintf(PRINT_BTH, "\n");
                    CharCount = 0;
                }

                CharCount++;
                tprintf(PRINT_BTH, ".");
            }
            else
                frame_avail = 0;

            frame_start = (cfg.g_timebase.den * (int64_t)(frames_in - 1)
                           * arg_framerate.den) / cfg.g_timebase.num /
                           arg_framerate.num;
            next_frame_start = (cfg.g_timebase.den * (int64_t)(frames_in)
                                * arg_framerate.den)
                               / cfg.g_timebase.num / arg_framerate.num;

            start = vpxt_get_cpu_tick();
            vpx_usec_timer_start(&timer);
            vpx_codec_encode(&encoder, frame_avail ? &raw : NULL, frame_start,
                next_frame_start - frame_start, 0, arg_deadline);
            vpx_usec_timer_mark(&timer);
            cx_time += vpx_usec_timer_elapsed(&timer);
            end = vpxt_get_cpu_tick();

            ctx_exit_on_error_tester(&encoder, "Failed to encode frame");
            got_data = 0;

            if (RunQCheck == 1)
            {
                if ((arg_passes == 2 && pass == 1) || arg_passes == 1)
                {
                    int lastQuantizerValue = 0;
                    vpx_codec_control(&encoder, VP8E_GET_LAST_QUANTIZER_64,
                        &lastQuantizerValue);
                    quant_out_file << frames_out << " " << lastQuantizerValue
                        << "\n";
                }
            }

            if (RunQCheck == 2)
            {
                // Print Quantizers
                int lastQuantizerValue = 0;
                vpx_codec_control(&encoder, VP8E_GET_LAST_QUANTIZER_64,
                    &lastQuantizerValue);
                tprintf(PRINT_STD, "frame %i Quantizer: %i\n", frames_out,
                    lastQuantizerValue);
            }

            total_cpu_time_used = total_cpu_time_used + (end - start);

            while ((pkt = vpx_codec_get_cx_data(&encoder, &iter)))
            {
                got_data = 1;

                switch (pkt->kind)
                {
                case VPX_CODEC_CX_FRAME_PKT:
                    frames_out++;

                    if (write_webm)
                    {
                        if (!ebml.debug)
                            hash = murmur(pkt->data.frame.buf,
                                          pkt->data.frame.sz, hash);

                        write_webm_block(&ebml, &cfg, pkt);
                    }
                    else
                    {
                        write_ivf_frame_header(outfile, pkt);

                        if (fwrite(pkt->data.frame.buf, 1,
                                   pkt->data.frame.sz, outfile));
                    }

                    nbytes += pkt->data.raw.sz;
                    break;
                case VPX_CODEC_STATS_PKT:
                    frames_out++;
                    stats_write(&stats,
                                pkt->data.twopass_stats.buf,
                                pkt->data.twopass_stats.sz);
                    nbytes += pkt->data.raw.sz;
                    break;
                case VPX_CODEC_PSNR_PKT:

                    if (0)
                    {
                        int i;

                        for (i = 0; i < 4; i++)
                            fprintf(stderr, "%.3lf ", pkt->data.psnr.psnr[i]);
                    }

                    break;
                default:
                    break;
                }
            }

            fflush(stdout);
        }

        /* this bitrate calc is simplified and relies on the fact that this
        * application uses 1/timebase for framerate.
        */
        {
            uint64_t temp = (uint64_t)frames_in * 1000000;
        }
        vpx_codec_destroy(&encoder);

        fclose(infile);
        if (file_type == FILE_TYPE_Y4M)
            y4m_input_close(&y4m);

        if (write_webm)
        {
            write_webm_file_footer(&ebml, hash);
        }
        else
        {
            if (!fseek(outfile, 0, SEEK_SET))
                write_ivf_file_header(outfile, &cfg, codec->fourcc, frames_out);
        }

        fclose(outfile);
        stats_close(&stats, arg_passes - 1);
        tprintf(PRINT_STD, "\n");
    }

    vpx_img_free(&raw);
    free(ebml.cue_list);

    tprintf(PRINT_BTH, "\n File completed: time in Microseconds: %u, Fps: %d \n",
            cx_time, 1000 * framesoutrec / (cx_time / 1000));
    tprintf(PRINT_BTH, " Total CPU Ticks: %u\n", total_cpu_time_used);

    std::string FullNameMs;
    std::string FullNameCpu;

    vpxt_remove_file_extension(outputFile2, FullNameMs);
    vpxt_remove_file_extension(outputFile2, FullNameCpu);

    FullNameMs += "compression_time.txt";

    std::ofstream FullNameMsFile(FullNameMs.c_str());
    FullNameMsFile << cx_time;
    FullNameMsFile.close();

    FullNameCpu += "compression_cpu_tick.txt";

    std::ofstream FullNameCpuFile(FullNameCpu.c_str());
    FullNameCpuFile << total_cpu_time_used;
    FullNameCpuFile.close();

    CPUTick = total_cpu_time_used;

    if (RunQCheck == 1)
        quant_out_file.close();

    return cx_time;
}
int vpxt_compress_force_key_frame(const char *input_file,
                                  const char *outputFile2,
                                  int speed, int bitrate,
                                  VP8_CONFIG &oxcf,
                                  const char *comp_out_str,
                                  int compress_int,
                                  int RunQCheck,
                                  int forceKeyFrame,
                                  std::string EncFormat)
{
    int write_webm = 1;
    vpxt_lower_case_string(EncFormat);

    if (EncFormat.compare("ivf") == 0)
        write_webm = 0;

    // RunQCheck - Signifies if the quantizers should be check to make sure
    // theyre working properly during an encode
    // RunQCheck = 0 = Do not save q values
    // RunQCheck = 1 = Save q values
    // RunQCheck = 2 = display q values only
    std::ofstream quant_out_file;

    if (RunQCheck == 1)
    {
        std::string QuantOutStr = outputFile2;
        QuantOutStr.erase(QuantOutStr.length() - 4, 4);
        QuantOutStr += "_quantizers.txt";
        quant_out_file.open(QuantOutStr.c_str());
    }

    ///////////////////// DELETE ME TEMP MEASURE ///////////////////////////////
    if (oxcf.Mode == 3) // Real time Mode
    {
        if (RunQCheck == 1)
            quant_out_file.close();

        return 0;
    }
    ////////////////////////////////////////////////////////////////////////////
    unsigned int             file_type, fourcc;
    y4m_input                y4m;
    int                      arg_have_framerate = 0;
    EbmlGlobal               ebml = {0};
    uint32_t                 hash = 0;
    vpx_codec_ctx_t          encoder;
    const char              *in_fn = input_file, *out_fn = outputFile2;
    const char              *stats_fn = NULL;
    FILE                    *infile, *outfile;
    vpx_codec_enc_cfg_t      cfg;
    vpx_codec_err_t          res;
    int                      pass, one_pass_only = 0;
    stats_io_t               stats;
    vpx_image_t              raw;
    const struct codec_item *codec = codecs;
    int                      frame_avail, got_data;
    int                      arg_usage = 0, arg_passes = 1, arg_deadline = 0;
    int                      arg_limit = 0;
    static const arg_def_t **ctrl_args = no_args;
    int                      verbose = 0;
    int                      arg_use_i420 = 1;
    double                   total_cpu_time_used = 0;
    int                      flags = 0;
    unsigned long            cx_time = 0;
    struct vpx_rational      arg_framerate = {30, 1};
    stereo_format_t          stereo_fmt = STEREO_FORMAT_MONO;

    ebml.last_pts_ms = -1;
    /* Populate encoder configuration */
    res = vpx_codec_enc_config_default(codec->iface, &cfg, arg_usage);
    if (res){
        tprintf(PRINT_STD, "Failed to get config: %s\n",
            vpx_codec_err_to_string(res));

        if (RunQCheck == 1)
            quant_out_file.close();

        return -1;
    }

    vpxt_core_config_to_api_config(oxcf, &cfg);

    // Set Mode Pass and bitrate Here
    cfg.rc_target_bitrate = bitrate;

    if (oxcf.Mode == kRealTime) // Real time Mode
        arg_deadline = 1;

    if (oxcf.Mode == kOnePassGoodQuality) // One Pass Good
        arg_deadline = 1000000;

    if (oxcf.Mode == kOnePassBestQuality) // One Pass Best
        arg_deadline = 0;

    if (oxcf.Mode == kTwoPassGoodQuality) // Two Pass Good
    {
        arg_passes = 2;
        arg_deadline = 1000000;
    }

    if (oxcf.Mode == kTwoPassBestQuality) // Two Pass Best
    {
        arg_passes = 2;
        arg_deadline = 0;
    }

    ///////////////////////////////////////////////////////////////////

    /* Handle codec specific options */
    if (codec->iface == &vpx_codec_vp8_cx_algo)
    {
        ctrl_args = vp8_args;
    }

    memset(&stats, 0, sizeof(stats));

    for (pass = 0; pass < arg_passes; pass++)
    {
        tprintf(PRINT_BTH, "\n\n Target Bit Rate: %d \n Max Quantizer: %d \n"
            " Min Quantizer %d \n %s: %d \n \n", oxcf.target_bandwidth,
            oxcf.worst_allowed_q, oxcf.best_allowed_q, comp_out_str,
            compress_int);

        int CharCount = 0;

        if (pass == 0 && arg_passes == 2)
            tprintf(PRINT_BTH, "\nFirst Pass - ");

        if (pass == 1 && arg_passes == 2)
            tprintf(PRINT_BTH, "\nSecond Pass - ");

        if (oxcf.Mode == kRealTime) // Real time Mode
            tprintf(PRINT_BTH, " RealTime\n\n");

        if (oxcf.Mode == kOnePassGoodQuality || oxcf.Mode == kTwoPassGoodQuality) // One Pass Good
            tprintf(PRINT_BTH, " GoodQuality\n\n");

        if (oxcf.Mode == kOnePassBestQuality || oxcf.Mode == kTwoPassBestQuality) // One Pass Best
            tprintf(PRINT_BTH, " BestQuality\n\n");

        int frames_in = 0, frames_out = 0;
        unsigned long nbytes = 0;
        struct detect_buffer detect;

        infile = strcmp(in_fn, "-") ? fopen(in_fn, "rb") :
            set_binary_mode(stdin);

        if (!infile)
        {
            tprintf(PRINT_BTH, "Failed to open input file: %s", in_fn);

            if (RunQCheck == 1)
                quant_out_file.close();

            return -1;
        }

        detect.buf_read = fread(detect.buf, 1, 4, infile);
        detect.position = 0;

        unsigned int rate;
        unsigned int scale;

        if (detect.buf_read == 4 && file_is_y4m(infile, &y4m, detect.buf))
        {
            if (y4m_input_open(&y4m, infile, detect.buf, 4) >= 0)
            {
                file_type = FILE_TYPE_Y4M;
                cfg.g_w = y4m.pic_w;
                cfg.g_h = y4m.pic_h;

                /* Use the frame rate from the file only if none was specified
                * on the command-line.
                */
                if (!arg_have_framerate)
                {
                    rate = y4m.fps_n;
                    scale = y4m.fps_d;
                    arg_framerate.num = y4m.fps_n;
                    arg_framerate.den = y4m.fps_d;
                }

                arg_use_i420 = 0;
            }
            else
            {
                fprintf(stderr, "Unsupported Y4M stream.\n");
                return EXIT_FAILURE;
            }
        }
        else if (detect.buf_read == 4 &&
                 file_is_ivf(infile, &fourcc, &cfg.g_w, &cfg.g_h,
                 &detect, &scale, &rate))
        {
            file_type = FILE_TYPE_IVF;

            switch (fourcc)
            {
            case 0x32315659:
                arg_use_i420 = 0;
                break;
            case 0x30323449:
                arg_use_i420 = 1;
                break;
            default:
                fprintf(stderr, "Unsupported fourcc (%08x) in IVF\n", fourcc);
                return EXIT_FAILURE;
            }
        }
        else
        {
            file_type = FILE_TYPE_RAW;
        }

        if (!cfg.g_w || !cfg.g_h)
        {
            fprintf(stderr, "Specify stream dimensions with --width (-w) "
                    " and --height (-h).\n");
            return EXIT_FAILURE;
        }

        std::string inputformat;
        std::string outputformat;

        if (write_webm == 1)
            outputformat = "Webm";

        if (write_webm == 0)
            outputformat = "IVF";

        if (file_type == FILE_TYPE_RAW)
            inputformat = "Raw";

        if (file_type == FILE_TYPE_Y4M)
            inputformat = "Y4M";

        if (file_type == FILE_TYPE_IVF)
            inputformat = "IVF";

        tprintf(PRINT_BTH, "API - Compressing Raw %s File to VP8 %s File: \n",
            inputformat.c_str(), outputformat.c_str());

        if (pass == (one_pass_only ? one_pass_only - 1 : 0))
        {
            if (file_type == FILE_TYPE_Y4M)
                /*The Y4M reader does its own allocation.
                Just initialize this here to avoid problems if we never read any
                frames.*/
                memset(&raw, 0, sizeof(raw));
            else
                vpx_img_alloc(&raw, arg_use_i420 ? VPX_IMG_FMT_I420 :
                VPX_IMG_FMT_YV12,
                              cfg.g_w, cfg.g_h, 1);
        }

        outfile = strcmp(out_fn, "-") ? fopen(out_fn, "wb") :
            set_binary_mode(stdout);

        if (!outfile)
        {
            tprintf(PRINT_BTH, "Failed to open output file: %s", out_fn);
            fclose(infile);

            if (RunQCheck == 1)
                quant_out_file.close();

            return -1;
        }

        if (write_webm && fseek(outfile, 0, SEEK_CUR))
        {
            fprintf(stderr, "WebM output to pipes not supported.\n");
            fclose(infile);
            fclose(outfile);

            if (RunQCheck == 1)
                quant_out_file.close();

            return -1;
        }

        if (stats_fn)
        {
            if (!stats_open_file(&stats, stats_fn, pass))
            {
                tprintf(PRINT_STD, "Failed to open statistics store\n");
                fclose(infile);
                fclose(outfile);

                if (RunQCheck == 1)
                    quant_out_file.close();

                return -1;
            }
        }
        else
        {
            if (!stats_open_mem(&stats, pass))
            {
                tprintf(PRINT_STD, "Failed to open statistics store\n");
                fclose(infile);
                fclose(outfile);

                if (RunQCheck == 1)
                    quant_out_file.close();

                return -1;
            }
        }

        cfg.g_pass = arg_passes == 2
                     ? pass ? VPX_RC_LAST_PASS : VPX_RC_FIRST_PASS
                 : VPX_RC_ONE_PASS;
#if VPX_ENCODER_ABI_VERSION > (1 + VPX_CODEC_ABI_VERSION)

        if (pass)
        {
            cfg.rc_twopass_stats_in = stats_get(&stats);
        }

#endif

        if (write_webm)
        {
            ebml.stream = outfile;
            write_webm_file_header(&ebml, &cfg, &arg_framerate, stereo_fmt);
        }
        else
            write_ivf_file_header(outfile, &cfg, codec->fourcc, 0);

        /* Construct Encoder Context */
        if (cfg.kf_min_dist == cfg.kf_max_dist)
            cfg.kf_mode = VPX_KF_FIXED;

        /*set timebase*/
        cfg.g_timebase.den = rate;
        cfg.g_timebase.num = scale;
        arg_framerate.num = rate;
        arg_framerate.den = scale;

        /*check timebase*/
        if(arg_framerate.num > cfg.g_timebase.den)
        {
            tprintf(PRINT_BTH,"Invalid timebase: %i/%i - changing to default:"
                                " %i/%i\n",cfg.g_timebase.num,cfg.g_timebase.den
                                , 1, arg_framerate.num);
            cfg.g_timebase.den = arg_framerate.num;
            cfg.g_timebase.num = 1;
        }

        /*check cq*/
        if(oxcf.cq_level < oxcf.best_allowed_q)
        {
            int new_cq_level = oxcf.best_allowed_q +
                0.2 * (oxcf.worst_allowed_q - oxcf.best_allowed_q);

            tprintf(PRINT_BTH, "cq_level: %i less than min q: %i - changing to:"
                               " %i\n", oxcf.cq_level, oxcf.best_allowed_q,
                               new_cq_level);

            oxcf.cq_level = new_cq_level;
        }
        if(oxcf.cq_level > oxcf.worst_allowed_q)
        {
            int new_cq_level = oxcf.best_allowed_q +
                0.2*(oxcf.worst_allowed_q-oxcf.best_allowed_q);

            tprintf(PRINT_BTH, "cq_level: %i greater than max q: %i - changing"
                               " to: %i\n", oxcf.cq_level, oxcf.worst_allowed_q,
                                new_cq_level);

            oxcf.cq_level = new_cq_level;
        }

        vpx_codec_enc_init(&encoder, codec->iface, &cfg, 0);
        /////////// Set Encoder Custom Settings /////////////////
        vpx_codec_control(&encoder, VP8E_SET_CPUUSED, oxcf.cpu_used);
        vpx_codec_control(&encoder, VP8E_SET_STATIC_THRESHOLD,
            oxcf.encode_breakout);
        vpx_codec_control(&encoder, VP8E_SET_ENABLEAUTOALTREF,
            oxcf.play_alternate);
        vpx_codec_control(&encoder, VP8E_SET_NOISE_SENSITIVITY,
            oxcf.noise_sensitivity);
        vpx_codec_control(&encoder, VP8E_SET_SHARPNESS, oxcf.Sharpness);
        vpx_codec_control(&encoder, VP8E_SET_TOKEN_PARTITIONS,
            (vp8e_token_partitions) oxcf.token_partitions);
        vpx_codec_control(&encoder, VP8E_SET_ARNR_MAXFRAMES,
            oxcf.arnr_max_frames);
        vpx_codec_control(&encoder, VP8E_SET_ARNR_STRENGTH, oxcf.arnr_strength);
        vpx_codec_control(&encoder, VP8E_SET_ARNR_TYPE, oxcf.arnr_type);
        vpx_codec_control(&encoder, VP8E_SET_CQ_LEVEL, oxcf.cq_level);
        vpx_codec_control(&encoder, VP8E_SET_MAX_INTRA_BITRATE_PCT,
            oxcf.rc_max_intra_bitrate_pct);
        ///////////////////////////////////////////////////////
        if (ctx_exit_on_error_tester(&encoder, "Failed to initialize encoder")
            == -1)
        {
            fclose(infile);
            fclose(outfile);

            if (RunQCheck == 1)
                quant_out_file.close();

            return -1;
        }

        frame_avail = 1;
        got_data = 0;

        if ((arg_passes == 2 && pass == 1) || arg_passes == 1)
        {
            ////////////////////// OUTPUT PARAMATERS ///////////////////////////
            std::string OutputsettingsFile;
            int extLength = vpxt_remove_file_extension(outputFile2,
                OutputsettingsFile);
            OutputsettingsFile += "parameters_core.txt";
            vpxt_output_settings(OutputsettingsFile.c_str(),  oxcf);
            /////////////////////// OUTPUT PARAMATERS API //////////////////////
            OutputsettingsFile.erase(OutputsettingsFile.length() -
                (15 + extLength), 15 + extLength);
            OutputsettingsFile += "_parameters_vpx.txt";
            vpxt_output_settings_api(OutputsettingsFile.c_str(),  cfg);
            ////////////////////////////////////////////////////////////////////
        }

        int forceKeyFrameTracker = forceKeyFrame;

        while (frame_avail || got_data)
        {
            vpx_codec_iter_t iter = NULL;
            const vpx_codec_cx_pkt_t *pkt;
            struct vpx_usec_timer timer;
            int64_t frame_start, next_frame_start;

            if (!arg_limit || frames_in < arg_limit)
            {

                frame_avail = read_frame_enc(infile, &raw, file_type, &y4m,
                                             &detect);

                if (frame_avail)
                    frames_in++;

                if (CharCount == 79)
                {
                    tprintf(PRINT_BTH, "\n");
                    CharCount = 0;
                }

                CharCount++;
                tprintf(PRINT_BTH, ".");
            }
            else
                frame_avail = 0;

            if (frames_in - 1 == forceKeyFrameTracker)
            {
                flags |= VPX_EFLAG_FORCE_KF;
                forceKeyFrameTracker = forceKeyFrameTracker + forceKeyFrame;
            }
            else
            {
                flags &= ~VPX_EFLAG_FORCE_KF;
            }

            frame_start = (cfg.g_timebase.den * (int64_t)(frames_in - 1)
                           * arg_framerate.den) / cfg.g_timebase.num /
                           arg_framerate.num;
            next_frame_start = (cfg.g_timebase.den * (int64_t)(frames_in)
                                * arg_framerate.den)
                               / cfg.g_timebase.num / arg_framerate.num;

            vpx_usec_timer_start(&timer);
            vpx_codec_encode(&encoder, frame_avail ? &raw : NULL, frame_start,
                next_frame_start - frame_start, flags, arg_deadline);
            vpx_usec_timer_mark(&timer);
            cx_time += vpx_usec_timer_elapsed(&timer);

            ctx_exit_on_error_tester(&encoder, "Failed to encode frame");
            got_data = 0;

            if (RunQCheck == 1)
            {
                if ((arg_passes == 2 && pass == 1) || arg_passes == 1)
                {
                    int lastQuantizerValue = 0;
                    vpx_codec_control(&encoder, VP8E_GET_LAST_QUANTIZER_64,
                        &lastQuantizerValue);
                    quant_out_file << frames_out << " " << lastQuantizerValue
                        << "\n";
                }
            }

            if (RunQCheck == 2)
            {
                // Print Quantizers
                int lastQuantizerValue = 0;
                vpx_codec_control(&encoder, VP8E_GET_LAST_QUANTIZER_64,
                    &lastQuantizerValue);
                tprintf(PRINT_STD, "frame %i Quantizer: %i\n", frames_out,
                    lastQuantizerValue);
            }

            while ((pkt = vpx_codec_get_cx_data(&encoder, &iter)))
            {
                got_data = 1;

                switch (pkt->kind)
                {
                case VPX_CODEC_CX_FRAME_PKT:
                    frames_out++;

                    if (write_webm)
                    {
                        if (!ebml.debug)
                            hash = murmur(pkt->data.frame.buf,
                                          pkt->data.frame.sz, hash);

                        write_webm_block(&ebml, &cfg, pkt);
                    }
                    else
                    {
                        write_ivf_frame_header(outfile, pkt);

                        if (fwrite(pkt->data.frame.buf, 1,
                                   pkt->data.frame.sz, outfile));
                    }

                    nbytes += pkt->data.raw.sz;
                    break;
                case VPX_CODEC_STATS_PKT:
                    frames_out++;
                    stats_write(&stats,
                                pkt->data.twopass_stats.buf,
                                pkt->data.twopass_stats.sz);
                    nbytes += pkt->data.raw.sz;
                    break;
                case VPX_CODEC_PSNR_PKT:

                    if (0)
                    {
                        int i;

                        for (i = 0; i < 4; i++)
                            fprintf(stderr, "%.3lf ", pkt->data.psnr.psnr[i]);
                    }

                    break;
                default:
                    break;
                }
            }

            fflush(stdout);
        }

        /* this bitrate calc is simplified and relies on the fact that this
        * application uses 1/timebase for framerate.
        */
        {
            uint64_t temp = (uint64_t)frames_in * 1000000;

        }
        vpx_codec_destroy(&encoder);

        fclose(infile);
        if (file_type == FILE_TYPE_Y4M)
            y4m_input_close(&y4m);

        if (write_webm)
        {
            write_webm_file_footer(&ebml, hash);
        }
        else
        {
            if (!fseek(outfile, 0, SEEK_SET))
                write_ivf_file_header(outfile, &cfg, codec->fourcc, frames_out);
        }

        fclose(outfile);
        stats_close(&stats, arg_passes - 1);
        tprintf(PRINT_BTH, "\n");
    }

    vpx_img_free(&raw);
    free(ebml.cue_list);

    if (RunQCheck == 1)
        quant_out_file.close();

    return 0;
}
int vpxt_compress_recon_buffer_check(const char *input_file,
                                     const char *outputFile2,
                                     int speed, int bitrate,
                                     VP8_CONFIG &oxcf,
                                     const char *comp_out_str,
                                     int compress_int,
                                     int RunQCheck,
                                     int OutputRaw,
                                     std::string EncFormat)
{
    std::string outputFile2Str = outputFile2;
    int write_webm = 1;
    vpxt_lower_case_string(EncFormat);

    if (EncFormat.compare("ivf") == 0)
        write_webm = 0;


    // RunQCheck - Signifies if the quantizers should be check to make sure
    // theyre working properly during an encode
    // RunQCheck = 0 = Do not save q values
    // RunQCheck = 1 = Save q values
    // RunQCheck = 2 = display q values only
    std::ofstream quant_out_file;

    if (RunQCheck == 1)
    {
        std::string QuantOutStr;
        vpxt_remove_file_extension(outputFile2, QuantOutStr);
        QuantOutStr += "_quantizers.txt";
        quant_out_file.open(QuantOutStr.c_str());
    }

    ////////////////////////// DELETE ME TEMP MEASURE //////////////////////////
    if (oxcf.Mode == 3) // Real time Mode
    {
        if (RunQCheck == 1)
            quant_out_file.close();

        return 0;
    }
    ////////////////////////////////////////////////////////////////////////////

    unsigned int             file_type, fourcc;
    y4m_input                y4m;
    int                      arg_have_framerate = 0;
    EbmlGlobal               ebml = {0};
    uint32_t                 hash = 0;
    vpx_codec_ctx_t          encoder;
    const char              *in_fn = input_file, *out_fn =outputFile2Str.c_str();
    const char              *stats_fn = NULL;
    FILE                    *infile, *outfile;
    vpx_codec_enc_cfg_t      cfg;
    vpx_codec_err_t          res;
    int                      pass, one_pass_only = 0;
    stats_io_t               stats;
    vpx_image_t              raw;
    const struct codec_item *codec = codecs;
    int                      frame_avail, got_data;
    int                      arg_usage = 0, arg_passes = 1, arg_deadline = 0;
    int                      arg_limit = 0;
    static const arg_def_t **ctrl_args = no_args;
    int                      verbose = 0;
    int                      arg_use_i420 = 1;
    double                   total_cpu_time_used = 0;
    unsigned long            cx_time = 0;
    struct vpx_rational      arg_framerate = {30, 1};
    stereo_format_t          stereo_fmt = STEREO_FORMAT_MONO;

    ebml.last_pts_ms = -1;
    // outfile = encoded ivf file
    void *out; // all raw preview frames
    void *out2; // all raw decoded frames
    void *out3; // individual raw preview frames
    void *out4; // individual decoded preview frames

    /* Populate encoder configuration */
    res = vpx_codec_enc_config_default(codec->iface, &cfg, arg_usage);
    if (res){
        tprintf(PRINT_STD, "Failed to get config: %s\n",
            vpx_codec_err_to_string(res));

        if (RunQCheck == 1)
            quant_out_file.close();

        return -1;
    }

    vpxt_core_config_to_api_config(oxcf, &cfg);


    // Set Mode Pass and bitrate Here
    cfg.rc_target_bitrate = bitrate;

    if (oxcf.Mode == kRealTime) // Real time Mode
        arg_deadline = 1;

    if (oxcf.Mode == kOnePassGoodQuality) // One Pass Good
        arg_deadline = 1000000;

    if (oxcf.Mode == kOnePassBestQuality) // One Pass Best
        arg_deadline = 0;

    if (oxcf.Mode == kTwoPassGoodQuality) // Two Pass Good
    {
        arg_passes = 2;
        arg_deadline = 1000000;
    }

    if (oxcf.Mode == kTwoPassBestQuality) // Two Pass Best
    {
        arg_passes = 2;
        arg_deadline = 0;
    }

    ///////////////////////////////////////////////////////////////////

    /* Handle codec specific options */
    if (codec->iface == &vpx_codec_vp8_cx_algo)
    {
        ctrl_args = vp8_args;
    }
    memset(&stats, 0, sizeof(stats));

    std::string out_fn4STRb;
    vpxt_remove_file_extension(out_fn, out_fn4STRb);
    out_fn4STRb += "DecodeFrame" + slashCharStr();

    std::string CreateDir3b = out_fn4STRb;
    CreateDir3b.insert(0, "mkdir \"");
    CreateDir3b += "\"";
    system(CreateDir3b.c_str());

    std::string out_fn3STRb;
    vpxt_remove_file_extension(out_fn, out_fn3STRb);
    out_fn3STRb += "PreviewFrame" + slashCharStr();

    std::string CreateDir2b = out_fn3STRb;
    CreateDir2b.insert(0, "mkdir \"");
    CreateDir2b += "\"";
    system(CreateDir2b.c_str());


    std::ofstream recon_out_file;
    std::string recon_out_str;
    vpxt_remove_file_extension(outputFile2, recon_out_str);
    recon_out_str += "ReconFrameState.txt";
    recon_out_file.open(recon_out_str.c_str());

    for (pass = 0; pass < arg_passes; pass++)
    {
        tprintf(PRINT_BTH, "\n\n Target Bit Rate: %d \n Max Quantizer: %d \n"
            " Min Quantizer %d \n %s: %d \n \n", oxcf.target_bandwidth,
            oxcf.worst_allowed_q, oxcf.best_allowed_q, comp_out_str,
            compress_int);

        int CharCount = 0;

        if (pass == 0 && arg_passes == 2)
            tprintf(PRINT_BTH, "\nFirst Pass - ");

        if (pass == 1 && arg_passes == 2)
            tprintf(PRINT_BTH, "\nSecond Pass - ");

        if (oxcf.Mode == kRealTime) // Real time Mode
            tprintf(PRINT_BTH, " RealTime\n\n");

        if (oxcf.Mode == kOnePassGoodQuality ||
            oxcf.Mode == kTwoPassGoodQuality) // One Pass Good
            tprintf(PRINT_BTH, " GoodQuality\n\n");

        if (oxcf.Mode == kOnePassBestQuality ||
            oxcf.Mode == kTwoPassBestQuality) // One Pass Best
            tprintf(PRINT_BTH, " BestQuality\n\n");

        int frames_in = 0, frames_out = 0;
        unsigned long nbytes = 0;
        struct detect_buffer detect;

        infile = strcmp(in_fn, "-") ? fopen(in_fn, "rb") :
            set_binary_mode(stdin);

        if (!infile)
        {
            tprintf(PRINT_BTH, "Failed to open input file: %s", in_fn);
            recon_out_file.close();

            if (RunQCheck == 1)
                quant_out_file.close();

            return -1;
        }

        detect.buf_read = fread(detect.buf, 1, 4, infile);
        detect.position = 0;

        unsigned int rate;
        unsigned int scale;

        if (detect.buf_read == 4 && file_is_y4m(infile, &y4m, detect.buf))
        {
            if (y4m_input_open(&y4m, infile, detect.buf, 4) >= 0)
            {
                file_type = FILE_TYPE_Y4M;
                cfg.g_w = y4m.pic_w;
                cfg.g_h = y4m.pic_h;

                /* Use the frame rate from the file only if none was specified
                * on the command-line.
                */
                if (!arg_have_framerate)
                {
                    rate = y4m.fps_n;
                    scale = y4m.fps_d;
                    arg_framerate.num = y4m.fps_n;
                    arg_framerate.den = y4m.fps_d;
                }

                arg_use_i420 = 0;
            }
            else
            {
                fprintf(stderr, "Unsupported Y4M stream.\n");
                return EXIT_FAILURE;
            }
        }
        else if (detect.buf_read == 4 &&
                 file_is_ivf(infile, &fourcc, &cfg.g_w, &cfg.g_h, &detect,
                 &scale, &rate))
        {
            file_type = FILE_TYPE_IVF;

            switch (fourcc)
            {
            case 0x32315659:
                arg_use_i420 = 0;
                break;
            case 0x30323449:
                arg_use_i420 = 1;
                break;
            default:
                fprintf(stderr, "Unsupported fourcc (%08x) in IVF\n", fourcc);
                return EXIT_FAILURE;
            }
        }
        else
        {
            file_type = FILE_TYPE_RAW;
        }

        if (!cfg.g_w || !cfg.g_h)
        {
            fprintf(stderr, "Specify stream dimensions with --width (-w) "
                    " and --height (-h).\n");
            return EXIT_FAILURE;
        }

        std::string inputformat;
        std::string outputformat;

        if (write_webm == 1)
            outputformat = "Webm";

        if (write_webm == 0)
            outputformat = "IVF";

        if (file_type == FILE_TYPE_RAW)
            inputformat = "Raw";

        if (file_type == FILE_TYPE_Y4M)
            inputformat = "Y4M";

        if (file_type == FILE_TYPE_IVF)
            inputformat = "IVF";

        tprintf(PRINT_BTH, "API - Compressing Raw %s File to VP8 %s File: \n",
            inputformat.c_str(), outputformat.c_str());

        if (pass == (one_pass_only ? one_pass_only - 1 : 0))
        {
            if (file_type == FILE_TYPE_Y4M)
                /*The Y4M reader does its own allocation.
                Just initialize this here to avoid problems if we never read any
                frames.*/
                memset(&raw, 0, sizeof(raw));
            else
                vpx_img_alloc(&raw, arg_use_i420 ? VPX_IMG_FMT_I420 :
                VPX_IMG_FMT_YV12,
                              cfg.g_w, cfg.g_h, 1);
        }

        outfile = strcmp(out_fn, "-") ? fopen(out_fn, "wb") :
            set_binary_mode(stdout);

        std::string out_fn2STR = out_fn;
        out_fn2STR += "_Preview.raw";

        if (!OutputRaw)
            out = out_open(out_fn2STR.c_str(), 0);

        std::string out_fn3STR = out_fn;
        out_fn3STR += "_Decode.raw";

        if (!OutputRaw)
            out2 = out_open(out_fn3STR.c_str(), 0);

        ///////////////////////////////////////////////////////////////////

        if (!outfile)
        {
            tprintf(PRINT_BTH, "Failed to open output file: %s", out_fn);
            recon_out_file.close();
            fclose(infile);
            out_close(out, out_fn2STR.c_str(), 0);
            out_close(out2, out_fn2STR.c_str(), 0);

            if (RunQCheck == 1)
                quant_out_file.close();

            return -1;
        }

        if (write_webm && fseek(outfile, 0, SEEK_CUR))
        {
            fprintf(stderr, "WebM output to pipes not supported.\n");
            fclose(infile);
            fclose(outfile);

            if (RunQCheck == 1)
                quant_out_file.close();

            return -1;
        }

        if (stats_fn)
        {
            if (!stats_open_file(&stats, stats_fn, pass))
            {
                tprintf(PRINT_STD, "Failed to open statistics store\n");
                recon_out_file.close();
                fclose(infile);
                fclose(outfile);
                out_close(out, out_fn2STR.c_str(), 0);
                out_close(out2, out_fn2STR.c_str(), 0);

                if (RunQCheck == 1)
                    quant_out_file.close();

                return -1;
            }
        }
        else
        {
            if (!stats_open_mem(&stats, pass))
            {
                tprintf(PRINT_STD, "Failed to open statistics store\n");
                recon_out_file.close();
                fclose(infile);
                fclose(outfile);
                out_close(out, out_fn2STR.c_str(), 0);
                out_close(out2, out_fn2STR.c_str(), 0);

                if (RunQCheck == 1)
                    quant_out_file.close();

                return -1;
            }
        }

        cfg.g_pass = arg_passes == 2
                     ? pass ? VPX_RC_LAST_PASS : VPX_RC_FIRST_PASS
                 : VPX_RC_ONE_PASS;
#if VPX_ENCODER_ABI_VERSION > (1 + VPX_CODEC_ABI_VERSION)

        if (pass)
        {
            cfg.rc_twopass_stats_in = stats_get(&stats);
        }

#endif

        if (write_webm)
        {
            ebml.stream = outfile;
            write_webm_file_header(&ebml, &cfg, &arg_framerate, stereo_fmt);
        }
        else
            write_ivf_file_header(outfile, &cfg, codec->fourcc, 0);

        /* Construct Encoder Context */
        if (cfg.kf_min_dist == cfg.kf_max_dist)
            cfg.kf_mode = VPX_KF_FIXED;

        /*set timebase*/
        cfg.g_timebase.den = rate;
        cfg.g_timebase.num = scale;
        arg_framerate.num = rate;
        arg_framerate.den = scale;

        /*check timebase*/
        if(arg_framerate.num > cfg.g_timebase.den)
        {
            tprintf(PRINT_BTH,"Invalid timebase: %i/%i - changing to default:"
                                " %i/%i\n",cfg.g_timebase.num,cfg.g_timebase.den
                                , 1, arg_framerate.num);
            cfg.g_timebase.den = arg_framerate.num;
            cfg.g_timebase.num = 1;
        }

        /*check cq*/
        if(oxcf.cq_level < oxcf.best_allowed_q)
        {
            int new_cq_level = oxcf.best_allowed_q +
                0.2*(oxcf.worst_allowed_q-oxcf.best_allowed_q);

            tprintf(PRINT_BTH, "cq_level: %i less than min q: %i - changing to:"
                               " %i\n", oxcf.cq_level, oxcf.best_allowed_q,
                               new_cq_level);

            oxcf.cq_level = new_cq_level;
        }
        if(oxcf.cq_level > oxcf.worst_allowed_q)
        {
            int new_cq_level = oxcf.best_allowed_q +
                0.2*(oxcf.worst_allowed_q-oxcf.best_allowed_q);

            tprintf(PRINT_BTH, "cq_level: %i greater than max q: %i - changing"
                               " to: %i\n", oxcf.cq_level, oxcf.worst_allowed_q,
                                new_cq_level);

            oxcf.cq_level = new_cq_level;
        }

        vpx_codec_enc_init(&encoder, codec->iface, &cfg, 0);
        /////////// Set Encoder Custom Settings /////////////////
        vpx_codec_control(&encoder, VP8E_SET_CPUUSED, oxcf.cpu_used);
        vpx_codec_control(&encoder, VP8E_SET_STATIC_THRESHOLD,
            oxcf.encode_breakout);
        vpx_codec_control(&encoder, VP8E_SET_ENABLEAUTOALTREF,
            oxcf.play_alternate);
        vpx_codec_control(&encoder, VP8E_SET_NOISE_SENSITIVITY,
            oxcf.noise_sensitivity);
        vpx_codec_control(&encoder, VP8E_SET_SHARPNESS, oxcf.Sharpness);
        vpx_codec_control(&encoder, VP8E_SET_TOKEN_PARTITIONS,
            (vp8e_token_partitions) oxcf.token_partitions);
        vpx_codec_control(&encoder, VP8E_SET_ARNR_MAXFRAMES,
            oxcf.arnr_max_frames);
        vpx_codec_control(&encoder, VP8E_SET_ARNR_STRENGTH, oxcf.arnr_strength);
        vpx_codec_control(&encoder, VP8E_SET_ARNR_TYPE, oxcf.arnr_type);
        vpx_codec_control(&encoder, VP8E_SET_CQ_LEVEL, oxcf.cq_level);
        vpx_codec_control(&encoder, VP8E_SET_MAX_INTRA_BITRATE_PCT,
            oxcf.rc_max_intra_bitrate_pct);
        ///////////////////////////////////////////////////////
        if (ctx_exit_on_error_tester(&encoder, "Failed to initialize encoder")
            == -1)
        {
            fclose(infile);
            fclose(outfile);
            recon_out_file.close();
            out_close(out, out_fn2STR.c_str(), 0);
            out_close(out2, out_fn2STR.c_str(), 0);

            if (RunQCheck == 1)
                quant_out_file.close();

            return -1;
        }

        frame_avail = 1;
        got_data = 0;

        if ((arg_passes == 2 && pass == 1) || arg_passes == 1)
        {
            //////////////////////// OUTPUT PARAMATERS /////////////////////////
            std::string OutputsettingsFile;
            int extLength = vpxt_remove_file_extension(outputFile2,
                OutputsettingsFile);
            OutputsettingsFile += "parameters_core.txt";
            vpxt_output_settings(OutputsettingsFile.c_str(),  oxcf);
            ////////////////// OUTPUT PARAMATERS API ///////////////////////////
            OutputsettingsFile.erase(OutputsettingsFile.length() -
                (15 + extLength), 15 + extLength);
            OutputsettingsFile += "_parameters_vpx.txt";
            vpxt_output_settings_api(OutputsettingsFile.c_str(),  cfg);
            ////////////////////////////////////////////////////////////////////
        }

        ///////////////////////////////// INI DECODER //////////////////////////
        vpx_codec_ctx_t       decoder;
        vp8_postproc_cfg_t      vp8_pp_cfg = {0};
        vpx_codec_iface_t       *iface = ifaces[0].iface;
        int postproc = 0;
        vpx_codec_dec_cfg_t     cfgdec = {0};

        if (vpx_codec_dec_init(&decoder, iface ? iface :  ifaces[0].iface,
            &cfgdec, postproc ? VPX_CODEC_USE_POSTPROC : 0))
        {
            tprintf(PRINT_STD, "Failed to initialize decoder: %s\n",
                vpx_codec_error(&decoder));
            recon_out_file.close();
            fclose(infile);
            fclose(outfile);
            out_close(out, out_fn2STR.c_str(), 0);
            out_close(out2, out_fn2STR.c_str(), 0);

            if (RunQCheck == 1)
                quant_out_file.close();

            return -1;
        }

        if (vp8_pp_cfg.post_proc_flag
            && vpx_codec_control(&decoder, VP8_SET_POSTPROC, &vp8_pp_cfg))
        {
            fprintf(stderr, "Failed to configure postproc: %s\n",
                vpx_codec_error(&decoder));
            recon_out_file.close();
            fclose(infile);
            fclose(outfile);
            out_close(out, out_fn2STR.c_str(), 0);
            out_close(out2, out_fn2STR.c_str(), 0);

            if (RunQCheck == 1)
            {
                quant_out_file.close();
            }

            return -1;
        }

        ////////////////////////////////////////////////////////////////////////
        while (frame_avail || got_data)
        {
            vpx_codec_iter_t iter = NULL;
            const vpx_codec_cx_pkt_t *pkt;
            struct vpx_usec_timer timer;
            int64_t frame_start, next_frame_start;

            if (!arg_limit || frames_in < arg_limit)
            {

                frame_avail = read_frame_enc(infile, &raw, file_type, &y4m,
                                             &detect);

                if (frame_avail)
                    frames_in++;

                if (CharCount == 79)
                {
                    tprintf(PRINT_BTH, "\n");
                    CharCount = 0;
                }

                CharCount++;
                tprintf(PRINT_BTH, ".");
            }
            else
                frame_avail = 0;

            frame_start = (cfg.g_timebase.den * (int64_t)(frames_in - 1)
                           * arg_framerate.den) / cfg.g_timebase.num /
                           arg_framerate.num;
            next_frame_start = (cfg.g_timebase.den * (int64_t)(frames_in)
                                * arg_framerate.den)
                               / cfg.g_timebase.num / arg_framerate.num;

            vpx_usec_timer_start(&timer);
            vpx_codec_encode(&encoder, frame_avail ? &raw : NULL, frame_start,
                next_frame_start - frame_start, 0, arg_deadline);
            vpx_usec_timer_mark(&timer);
            cx_time += vpx_usec_timer_elapsed(&timer);
            ctx_exit_on_error_tester(&encoder, "Failed to encode frame");
            got_data = 0;

            if (RunQCheck == 1)
            {
                if ((arg_passes == 2 && pass == 1) || arg_passes == 1)
                {
                    int lastQuantizerValue = 0;
                    vpx_codec_control(&encoder, VP8E_GET_LAST_QUANTIZER_64,
                        &lastQuantizerValue);
                    quant_out_file << frames_out << " " << lastQuantizerValue
                        << "\n";
                }
            }

            if (RunQCheck == 2)
            {
                // Print Quantizers
                int lastQuantizerValue = 0;
                vpx_codec_control(&encoder, VP8E_GET_LAST_QUANTIZER_64,
                    &lastQuantizerValue);
                tprintf(PRINT_STD, "frame %i Quantizer: %i\n", frames_out,
                    lastQuantizerValue);
            }

            while ((pkt = vpx_codec_get_cx_data(&encoder, &iter)))
            {
                got_data = 1;
                vpx_codec_iter_t  iterdec = NULL;

                int MemCheckY = 0;
                int MemCheckU = 0;
                int MemCheckV = 0;

                switch (pkt->kind)
                {
                case VPX_CODEC_CX_FRAME_PKT:
                    frames_out++;

                    if (write_webm)
                    {
                        if (!ebml.debug)
                            hash = murmur(pkt->data.frame.buf,
                                          pkt->data.frame.sz, hash);

                        write_webm_block(&ebml, &cfg, pkt);
                    }
                    else
                    {
                        write_ivf_frame_header(outfile, pkt);

                        if (fwrite(pkt->data.frame.buf, 1,
                                   pkt->data.frame.sz, outfile));
                    }

                    nbytes += pkt->data.raw.sz;
                    const vpx_image_t    *imgPreview;
                    const vpx_image_t    *imgDecode;

                    imgPreview = vpx_codec_get_preview_frame(&encoder);

                    vpx_codec_decode(&decoder,
                        (const uint8_t *)pkt->data.frame.buf,
                        pkt->data.frame.sz, NULL, 0);
                    imgDecode = vpx_codec_get_frame(&decoder, &iterdec);

                    if (imgPreview && imgDecode)
                    {
                        std::string out_fn3STR;
                        vpxt_remove_file_extension(out_fn, out_fn3STR);
                        out_fn3STR += "PreviewFrame" + slashCharStr();

                        char intchar[56];
                        vpxt_itoa_custom(frames_out, intchar, 10);
                        out_fn3STR += intchar;
                        out_fn3STR += ".raw";

                        if (!OutputRaw)
                            out3 = out_open(out_fn3STR.c_str(), 0);

                        std::string out_fn4STR;
                        vpxt_remove_file_extension(out_fn, out_fn4STR);
                        out_fn4STR += "DecodeFrame" + slashCharStr();

                        char intchar2[56];
                        vpxt_itoa_custom(frames_out, intchar2, 10);
                        out_fn4STR += intchar2;
                        out_fn4STR += ".raw";

                        if (!OutputRaw)
                            out4 = out_open(out_fn4STR.c_str(), 0);

                        unsigned int y;
                        uint8_t *bufPreview;
                        uint8_t *bufDecode;

                        bufPreview = imgPreview->planes[PLANE_Y];
                        bufDecode = imgDecode->planes[PLANE_Y];

                        for (y = 0; y < imgDecode->d_h; y++)
                        {
                            if (!OutputRaw)
                                out_put(out3, bufPreview, imgDecode->d_w, 0);

                            bufPreview += imgPreview->stride[PLANE_Y];

                            if (!OutputRaw)
                                out_put(out4, bufDecode, imgDecode->d_w, 0);

                            bufDecode += imgDecode->stride[PLANE_Y];

                            MemCheckY |= memcmp(bufPreview, bufDecode,
                                imgDecode->d_w);
                        }

                        bufPreview = imgPreview->planes[PLANE_U];
                        bufDecode = imgDecode->planes[PLANE_U];

                        for (y = 0; y < (imgDecode->d_h + 1) / 2; y++)
                        {
                            if (!OutputRaw)
                                out_put(out3, bufPreview,
                                (imgDecode->d_w + 1) / 2, 0);

                            bufPreview += imgPreview->stride[PLANE_U];

                            if (!OutputRaw)
                                out_put(out4, bufDecode,
                                (imgDecode->d_w + 1) / 2, 0);

                            bufDecode += imgDecode->stride[PLANE_U];

                            MemCheckU |= memcmp(bufPreview, bufDecode,
                                (imgDecode->d_w + 1) / 2);
                        }

                        bufPreview = imgPreview->planes[PLANE_V];
                        bufDecode = imgDecode->planes[PLANE_V];

                        for (y = 0; y < (imgDecode->d_h + 1) / 2; y++)
                        {
                            if (!OutputRaw)
                                out_put(out3, bufPreview,
                                (imgDecode->d_w + 1) / 2, 0);

                            bufPreview += imgPreview->stride[PLANE_V];

                            if (!OutputRaw)
                                out_put(out4, bufDecode,
                                (imgDecode->d_w + 1) / 2, 0);

                            bufDecode += imgDecode->stride[PLANE_V];

                            MemCheckV |= memcmp(bufPreview, bufDecode,
                                (imgDecode->d_w + 1) / 2);
                        }


                        if (MemCheckY != 0)
                        {
                            recon_out_file << frames_out << " Y " << 0 << "\n";
                        }
                        else
                        {
                            recon_out_file << frames_out << " Y " << 1 << "\n";
                        }

                        if (MemCheckU != 0)
                        {
                            recon_out_file << frames_out << " U " << 0 << "\n";
                        }
                        else
                        {
                            recon_out_file << frames_out << " U " << 1 << "\n";
                        }

                        if (MemCheckV != 0)
                        {
                            recon_out_file << frames_out << " V " << 0 << "\n";
                        }
                        else
                        {
                            recon_out_file << frames_out << " V " << 1 << "\n";
                        }

                        if (!OutputRaw)
                        {
                            out_close(out3, out_fn3STR.c_str(), 0);
                            out_close(out4, out_fn4STR.c_str(), 0);
                        }
                    }

                    if (imgPreview && !OutputRaw)
                    {
                        unsigned int y;
                        uint8_t *buf;

                        buf = imgPreview->planes[PLANE_Y];

                        for (y = 0; y < imgDecode->d_h; y++)
                        {
                            out_put(out, buf, imgDecode->d_w, 0);
                            buf += imgPreview->stride[PLANE_Y];
                        }

                        buf = imgPreview->planes[PLANE_U];

                        for (y = 0; y < (imgDecode->d_h + 1) / 2; y++)
                        {
                            out_put(out, buf, (imgDecode->d_w + 1) / 2, 0);
                            buf += imgPreview->stride[PLANE_U];
                        }

                        buf = imgPreview->planes[PLANE_V];

                        for (y = 0; y < (imgDecode->d_h + 1) / 2; y++)
                        {
                            out_put(out, buf, (imgDecode->d_w + 1) / 2, 0);
                            buf += imgPreview->stride[PLANE_V];
                        }
                    }

                    if (imgDecode && !OutputRaw)
                    {
                        unsigned int y;
                        uint8_t *buf;

                        buf = imgDecode->planes[PLANE_Y];

                        for (y = 0; y < imgDecode->d_h; y++)
                        {
                            out_put(out2, buf, imgDecode->d_w, 0);
                            buf += imgDecode->stride[PLANE_Y];
                        }

                        buf = imgDecode->planes[PLANE_U];

                        for (y = 0; y < (imgDecode->d_h + 1) / 2; y++)
                        {
                            out_put(out2, buf, (imgDecode->d_w + 1) / 2, 0);
                            buf += imgDecode->stride[PLANE_U];
                        }

                        buf = imgDecode->planes[PLANE_V];

                        for (y = 0; y < (imgDecode->d_h + 1) / 2; y++)
                        {
                            out_put(out2, buf, (imgDecode->d_w + 1) / 2, 0);
                            buf += imgDecode->stride[PLANE_V];
                        }
                    }

                    break;
                case VPX_CODEC_STATS_PKT:
                    frames_out++;
                    stats_write(&stats,
                                pkt->data.twopass_stats.buf,
                                pkt->data.twopass_stats.sz);
                    nbytes += pkt->data.raw.sz;
                    break;
                case VPX_CODEC_PSNR_PKT:

                    if (0)
                    {
                        int i;

                        for (i = 0; i < 4; i++)
                            fprintf(stderr, "%.3lf ", pkt->data.psnr.psnr[i]);
                    }

                    break;
                default:
                    break;
                }
            }

            fflush(stdout);
        }

        /* this bitrate calc is simplified and relies on the fact that this
        * application uses 1/timebase for framerate.
        */
        {
            uint64_t temp = (uint64_t)frames_in * 1000000;

        }
        vpx_codec_destroy(&encoder);
        vpx_codec_destroy(&decoder);

        fclose(infile);
        if (file_type == FILE_TYPE_Y4M)
            y4m_input_close(&y4m);

        if (write_webm)
        {
            write_webm_file_footer(&ebml, hash);
        }
        else
        {
            if (!fseek(outfile, 0, SEEK_SET))
                write_ivf_file_header(outfile, &cfg, codec->fourcc, frames_out);
        }

        fclose(outfile);
        stats_close(&stats, arg_passes - 1);

        if (!OutputRaw)
        {
            out_close(out, out_fn2STR.c_str(), 0);
            out_close(out2, out_fn2STR.c_str(), 0);
        }

        tprintf(PRINT_BTH, "\n");
    }

    vpx_img_free(&raw);
    free(ebml.cue_list);

    recon_out_file.close();

    if (RunQCheck == 1)
        quant_out_file.close();

    return 0;
}
unsigned int vpxt_compress_multi_resolution(const char *input_file,
                                            const char *outputFile2,
                                            int speed, int bitrate,
                                            VP8_CONFIG &oxcf,
                                            const char *comp_out_str,
                                            int compress_int,
                                            int RunQCheck,
                                            std::string EncFormat)
{
    const char *outfile_name[NUM_ENCODERS];

    int                      write_webm = 1;
    vpxt_lower_case_string(EncFormat);

    if (EncFormat.compare("ivf") == 0)
        write_webm = 0;

    // RunQCheck - Signifies if the quantizers should be check to make sure
    // theyre working properly during an encode
    // RunQCheck = 0 = Do not save q values
    // RunQCheck = 1 = Save q values
    // RunQCheck = 2 = display q values only
    std::ofstream quant_out_file;

    if (RunQCheck == 1)
    {
        std::string QuantOutStr;
        vpxt_remove_file_extension(outputFile2, QuantOutStr);
        QuantOutStr += "quantizers.txt";
        quant_out_file.open(QuantOutStr.c_str());
    }

    /////////////////////////// DELETE ME TEMP MEASURE /////////////////////////
    if (oxcf.Mode == 3) // Real time Mode
    {
        if (RunQCheck == 1)
            quant_out_file.close();

        return 0;
    }

    unsigned int             file_type, fourcc;
    y4m_input                y4m;
    int                      arg_have_framerate = 0;
    EbmlGlobal               ebml[NUM_ENCODERS] = {0};
    uint32_t                 hash = 0;
    struct vpx_rational      arg_framerate = {30, 1};
    int                      arg_use_i420 = 1;
    stereo_format_t          stereo_fmt = STEREO_FORMAT_MONO;

    FILE                *infile, *outfile[NUM_ENCODERS];
    vpx_codec_ctx_t      codec[NUM_ENCODERS];
    vpx_codec_enc_cfg_t  cfg[NUM_ENCODERS];
    vpx_codec_pts_t      frame_cnt = 0;
    vpx_image_t          raw[NUM_ENCODERS];
    vpx_codec_err_t      res[NUM_ENCODERS];

    int                  i;
    int                  frame_avail;
    int                  got_data;
    int                  flags = 0;
    unsigned long        cx_time = 0;
    unsigned int         total_cpu_time_used = 0;

    for(i = 0; i < NUM_ENCODERS; i++)
        ebml[i].last_pts_ms = -1;

    /*Currently, only realtime mode is supported in multi-resolution encoding.*/
    int                  arg_deadline = VPX_DL_REALTIME;

    /* Set show_psnr to 1/0 to show/not show PSNR. Choose show_psnr=0 if you
    don't need to know PSNR, which will skip PSNR calculation and save
    encoding time. */
    int                  show_psnr = 0;
    uint64_t             psnr_sse_total[NUM_ENCODERS] = {0};
    uint64_t             psnr_samples_total[NUM_ENCODERS] = {0};
    double               psnr_totals[NUM_ENCODERS][4] = {{0,0}};
    int                  psnr_count[NUM_ENCODERS] = {0};

    /* Set the required target bitrates for each resolution level. */
    unsigned int         target_bitrate[NUM_ENCODERS]={1400, 500, 100};
    /* Enter the frame rate of the input video */
    int                  framerate = 30;
    /* Set down-sampling factor for each resolution level.
    dsf[0] controls down sampling from level 0 to level 1;
    dsf[1] controls down sampling from level 1 to level 2;
    dsf[2] is not used. */
    vpx_rational_t dsf[NUM_ENCODERS] = {{2, 1}, {2, 1}, {1, 1}};
    /* Encode starting from which resolution level. Normally it is 0 that
     * means the original(highest) resolution. */
    int                  s_lvl = 0;

    /* Open input video file for encoding */
    if(!(infile = fopen(input_file, "rb")))
        return 0;

    show_psnr = 0;//strtol(argv[NUM_ENCODERS + 4], NULL, 0);
    const struct codec_item  *codec_test = codecs;

    /* Check to see if we need to encode all resolution levels */
    for (i=0; i<NUM_ENCODERS; i++)
    {
        if (target_bitrate[i])
            break;
        else
            s_lvl += 1;
    }

    if (s_lvl >= NUM_ENCODERS)
    {
        printf("No encoding: total number of encoders is 0!");
        return 0;
    }

    /* Populate default encoder configuration */
    for (i=0; i< NUM_ENCODERS; i++)
    {
        res[i] = vpx_codec_enc_config_default(codec_test->iface, &cfg[i], 0);
        if(res[i]) {
            printf("Failed to get config: %s\n", vpx_codec_err_to_string(res[i]));
            return EXIT_FAILURE;
        }
    }

    struct detect_buffer detect;

    detect.buf_read = fread(detect.buf, 1, 4, infile);
    detect.position = 0;

    unsigned int rate;
    unsigned int scale;

    if (detect.buf_read == 4 && file_is_y4m(infile, &y4m, detect.buf))
    {
        if (y4m_input_open(&y4m, infile, detect.buf, 4) >= 0)
        {
            file_type = FILE_TYPE_Y4M;
            cfg[0].g_w = y4m.pic_w;
            cfg[0].g_h = y4m.pic_h;

            /* Use the frame rate from the file only if none was specified
            * on the command-line.
            */
            if (!arg_have_framerate)
            {
                rate = y4m.fps_n;
                scale = y4m.fps_d;
                arg_framerate.num = y4m.fps_n;
                arg_framerate.den = y4m.fps_d;
            }

            arg_use_i420 = 0;
        }
        else
        {
            fprintf(stderr, "Unsupported Y4M stream.\n");
            return EXIT_FAILURE;
        }
    }
    else if (detect.buf_read == 4 && file_is_ivf(infile, &fourcc, &cfg[0].g_w,
        &cfg[0].g_h, &detect, &scale, &rate))
    {
        file_type = FILE_TYPE_IVF;

        switch (fourcc)
        {
        case 0x32315659:
            arg_use_i420 = 0;
            break;
        case 0x30323449:
            arg_use_i420 = 1;
            break;
        default:
            fprintf(stderr, "Unsupported fourcc (%08x) in IVF\n", fourcc);
            return EXIT_FAILURE;
        }
    }
    else
    {
        file_type = FILE_TYPE_RAW;
    }

    if (!cfg[0].g_w || !cfg[0].g_h)
    {
        fprintf(stderr, "Specify stream dimensions with --width (-w) "
            " and --height (-h).\n");
        return EXIT_FAILURE;
    }

    arg_framerate.num = rate;
    arg_framerate.den = scale;

    for (i=0; i< NUM_ENCODERS; i++)
    {
        /*set timebase*/
        cfg[i].g_timebase.den = rate;
        cfg[i].g_timebase.num = scale;

        /*check timebase*/
        if(arg_framerate.num > cfg[i].g_timebase.den)
        {
            tprintf(PRINT_BTH,"Invalid timebase: %i/%i - changing to default:"
                " %i/%i\n",cfg[i].g_timebase.num,cfg[i].g_timebase.den
                , 1, arg_framerate.num);
            cfg[i].g_timebase.den = arg_framerate.num;
            cfg[i].g_timebase.num = 1;
        }
    }

    vpxt_core_config_to_api_config(oxcf, &cfg[0]);

    std::string inputformat;
    std::string outputformat;

    if (write_webm == 1)
        outputformat = "Webm";

    if (write_webm == 0)
        outputformat = "IVF";

    if (file_type == FILE_TYPE_RAW)
        inputformat = "Raw";

    if (file_type == FILE_TYPE_Y4M)
        inputformat = "Y4M";

    if (file_type == FILE_TYPE_IVF)
        inputformat = "IVF";

    /* Other-resolution encoder settings */
    for (i=1; i< NUM_ENCODERS; i++)
    {
        memcpy(&cfg[i], &cfg[0], sizeof(vpx_codec_enc_cfg_t));

        cfg[i].g_threads = 1;                       /* number of threads used */
        cfg[i].rc_target_bitrate = target_bitrate[i];

        /* Note: Width & height of other-resolution encoders are calculated
        * from the highest-resolution encoder's size and the corresponding
        * down_sampling_factor.
        */
        {
            unsigned int iw = cfg[i-1].g_w*dsf[i-1].den + dsf[i-1].num - 1;
            unsigned int ih = cfg[i-1].g_h*dsf[i-1].den + dsf[i-1].num - 1;
            cfg[i].g_w = iw/dsf[i-1].num;
            cfg[i].g_h = ih/dsf[i-1].num;
        }

        /* Make width & height to be multiplier of 2. */
        // Should support odd size ???
        if((cfg[i].g_w)%2)cfg[i].g_w++;
        if((cfg[i].g_h)%2)cfg[i].g_h++;
    }

    /* Open output file for each encoder to output bitstreams */
    for (i=0; i< NUM_ENCODERS; i++)
    {
        char WidthChar[10];
        vpxt_itoa_custom(cfg[i].g_w, WidthChar, 10);
        char HeightChar[10];
        vpxt_itoa_custom(cfg[i].g_h, HeightChar, 10);

        std::string out_str = outputFile2;
        out_str += "-";
        out_str += WidthChar;
        out_str += "x";
        out_str += HeightChar;

        if(write_webm == 0)
            out_str += ".ivf";
        else if(write_webm == 1)
            out_str += ".webm";

        outfile_name[i] = out_str.c_str();

        if(!(outfile[i] = fopen(outfile_name[i], "wb")))
            return 0;

        if (write_webm && fseek(outfile[i], 0, SEEK_CUR))
        {
            fprintf(stderr, "WebM output to pipes not supported.\n");
            return -1;
        }

        if (write_webm)
        {
            ebml[i].stream = outfile[i];
            write_webm_file_header(&ebml[i], &cfg[i], &arg_framerate,
                stereo_fmt);
        }
        else
            write_ivf_file_header(outfile[i], &cfg[i], fourcc, 0);
    }

    /* Allocate image for each encoder */
    if(file_type == FILE_TYPE_Y4M)
    {
        for (i=0; i< NUM_ENCODERS; i++)
        {
            if(i == 0)
                memset(&raw[i], 0, sizeof(raw[i]));
            else
                vpx_img_alloc(&raw[i], arg_use_i420 ? VPX_IMG_FMT_I420 :
                VPX_IMG_FMT_YV12, cfg[i].g_w, cfg[i].g_h,
                32);
        }
    }
    else
    {
        for (i=0; i< NUM_ENCODERS; i++)
            if(!vpx_img_alloc(&raw[i], arg_use_i420 ? VPX_IMG_FMT_I420 :
                VPX_IMG_FMT_YV12, cfg[i].g_w, cfg[i].g_h,
                32))
                return 0;
    }

    /* Initialize multi-encoder */
    if(vpx_codec_enc_init_multi(&codec[0], codec_test->iface, &cfg[0],
        NUM_ENCODERS,(show_psnr ? VPX_CODEC_USE_PSNR : 0), &dsf[0]))
        return 0;

    /* The extra encoding configuration parameters can be set as follows. */
    /* Set encoding speed */
    for ( i=0; i<NUM_ENCODERS; i++)
    {
        int speed = -6;
        if(vpx_codec_control(&codec[i], VP8E_SET_CPUUSED, speed))
            return 0;
    }
    /* Set static thresh for highest-resolution encoder. Set it to 1000 for
    * better performance. */
    {
        unsigned int static_thresh = 1000;
        if(vpx_codec_control(&codec[0], VP8E_SET_STATIC_THRESHOLD,
            static_thresh))
            return 0;
    }
    /* Set static thresh = 0 for other encoders for better quality */
    for ( i=1; i<NUM_ENCODERS; i++)
    {
        unsigned int static_thresh = 0;
        if(vpx_codec_control(&codec[i], VP8E_SET_STATIC_THRESHOLD,
            static_thresh))
            return 0;
    }

    frame_avail = 1;
    got_data = 0;
    int print_count = 0;

    tprintf(PRINT_BTH, "\n\n Target Bit Rate: %d \n Max Quantizer: %d \n"
        " Min Quantizer %d \n %s: %d \n \n", oxcf.target_bandwidth,
        oxcf.worst_allowed_q, oxcf.best_allowed_q, comp_out_str,
        compress_int);
    tprintf(PRINT_BTH, "API Multi Resolution - Compressing Raw %s File to VP8"
        " %s Files: \n", inputformat.c_str(), outputformat.c_str());

    while(frame_avail || got_data)
    {
        vpx_codec_iter_t iter[NUM_ENCODERS]={NULL};
        const vpx_codec_cx_pkt_t *pkt[NUM_ENCODERS];
        struct vpx_usec_timer timer;
        unsigned int start, end;

        flags = 0;

        frame_avail = read_frame_enc(infile, &raw[0], file_type, &y4m, &detect);

        if(frame_avail)
        {
            for ( i=1; i<NUM_ENCODERS; i++)
            {
                /*Scale the image down a number of times by downsampling factor*/
                /* FilterMode 1 or 2 give better psnr than FilterMode 0. */
                libyuv::I420Scale(raw[i-1].planes[VPX_PLANE_Y],
                    raw[i-1].stride[VPX_PLANE_Y], raw[i-1].planes[VPX_PLANE_U],
                    raw[i-1].stride[VPX_PLANE_U], raw[i-1].planes[VPX_PLANE_V],
                    raw[i-1].stride[VPX_PLANE_V], raw[i-1].d_w, raw[i-1].d_h,
                    raw[i].planes[VPX_PLANE_Y], raw[i].stride[VPX_PLANE_Y],
                    raw[i].planes[VPX_PLANE_U], raw[i].stride[VPX_PLANE_U],
                    raw[i].planes[VPX_PLANE_V], raw[i].stride[VPX_PLANE_V],
                    raw[i].d_w, raw[i].d_h, (libyuv::FilterMode) 1);
            }
        }

        /* Encode each frame at multi-levels */
        start = vpxt_get_cpu_tick();
        vpx_usec_timer_start(&timer);
        if(vpx_codec_encode(&codec[0], frame_avail? &raw[0] : NULL, frame_cnt,
            1, flags, arg_deadline))
            return 0;
        end = vpxt_get_cpu_tick();
        cx_time += vpx_usec_timer_elapsed(&timer);

        total_cpu_time_used = total_cpu_time_used + (end - start);

        for (i=NUM_ENCODERS-1; i>=0 ; i--)
        {
            got_data = 0;

            while( (pkt[i] = vpx_codec_get_cx_data(&codec[i], &iter[i])) )
            {
                got_data = 1;
                switch(pkt[i]->kind) {
    case VPX_CODEC_CX_FRAME_PKT:
        if (write_webm)
        {
            if (!ebml[i].debug)
                hash = murmur(pkt[i]->data.frame.buf,
                pkt[i]->data.frame.sz, hash);

            write_webm_block(&ebml[i], &cfg[i], pkt[i]);
        }
        else
        {
            write_ivf_frame_header(outfile[i], pkt[i]);

            if (fwrite(pkt[i]->data.frame.buf, 1,
                pkt[i]->data.frame.sz, outfile[i]));
        }
        break;
    case VPX_CODEC_PSNR_PKT:
        if (show_psnr)
        {
            int j;

            psnr_sse_total[i] += pkt[i]->data.psnr.sse[0];
            psnr_samples_total[i] += pkt[i]->data.psnr.samples[0];
            for (j = 0; j < 4; j++)
            {
                psnr_totals[i][j] += pkt[i]->data.psnr.psnr[j];
            }
            psnr_count[i]++;
        }

        break;
    default:
        break;
                }
                tprintf(PRINT_BTH, pkt[i]->kind == VPX_CODEC_CX_FRAME_PKT &&
                    (pkt[i]->data.frame.flags & VPX_FRAME_IS_KEY)? "K":".");

                print_count++;
                if(print_count >= 79){
                    tprintf(PRINT_BTH, "\n");
                    print_count = 0;
                }

                fflush(stdout);
            }
        }
        frame_cnt++;
    }
    printf("\n");

    fclose(infile);
    if (file_type == FILE_TYPE_Y4M)
        y4m_input_close(&y4m);

    for (i=0; i< NUM_ENCODERS; i++)
    {
        if(vpx_codec_destroy(&codec[i]))
            return 0;

        /* Try to rewrite the file header with the actual frame count */
        if (write_webm)
        {
            write_webm_file_footer(&ebml[i], hash);
        }
        else
        {
            if (!fseek(outfile[i], 0, SEEK_SET))
                write_ivf_file_header(outfile[i], &cfg[i], fourcc, frame_cnt-1);
        }

        fclose(outfile[i]);

        vpx_img_free(&raw[i]);

        free(ebml[i].cue_list);
    }

    tprintf(PRINT_BTH, "\n File completed: time in Microseconds: %u, Fps: %d "
        "\n", total_cpu_time_used,
        1000 * frame_cnt / (total_cpu_time_used / 1000));
    tprintf(PRINT_BTH, " Total CPU Ticks: %u\n", cx_time);

    return cx_time;
}

unsigned int vpxt_compress_scalable_patterns(const char *input_file,
                                             const char *outputFile2,
                                             int speed, VP8_CONFIG &oxcf,
                                             const char *comp_out_str,
                                             int compress_int,
                                             int RunQCheck,
                                             std::string EncFormat,
                                             int layering_mode,
                                             int ScaleBitRate0,
                                             int ScaleBitRate1,
                                             int ScaleBitRate2,
                                             int ScaleBitRate3,
                                             int ScaleBitRate4)
{

    int write_webm = 1;
    vpxt_lower_case_string(EncFormat);

    if (EncFormat.compare("ivf") == 0)
        write_webm = 0;

    FILE                     *infile, *outfile[MAX_LAYERS];
    vpx_codec_ctx_t           codec_enc;
    const struct codec_item  *codec = codecs;
    int                       arg_usage = 0;
    vpx_codec_enc_cfg_t       cfg;
    int                       frame_cnt = 0;
    vpx_image_t               raw;
    vpx_codec_err_t           res;
    unsigned int              width;
    unsigned int              height;
    int                       frame_avail;
    int                       got_data;
    int                       flags = 0;
    int                       i;
    int                       pts = 0;              // PTS starts at 0
    int                       frame_duration = 1;   // 1 timebase tick per frame
    int                       frames_in_layer[MAX_LAYERS] = {0};
    int                       layer_flags[MAX_PERIODICITY] = {0};
    y4m_input                 y4m;
    const char               *in_fn = input_file, *out_fn = outputFile2;
    const char               *stats_fn = NULL;
    unsigned int              file_type, fourcc;
    int                       arg_have_framerate = 0;
    struct vpx_rational       arg_framerate = {30, 1};
    int                       arg_use_i420 = 1;
    EbmlGlobal                ebml[TEMP_SCALE_ENCODERS] = {0};
    stereo_format_t           stereo_fmt = STEREO_FORMAT_MONO;
    uint32_t                  hash = 0;
    unsigned long             cx_time = 0;
    unsigned int              total_cpu_time_used = 0;
    int                       print_count=0;
    int                       flag_periodicity;
    int                       max_intra_size_pct;

    for(i = 0; i < TEMP_SCALE_ENCODERS; i++)
        ebml[i].last_pts_ms = -1;

    // Populate encoder configuration
    res = vpx_codec_enc_config_default(codec->iface, &cfg, 0);
    if(res) {
        printf("Failed to get config: %s\n", vpx_codec_err_to_string(res));
        return EXIT_FAILURE;
    }

    // These shoudl end up being passed in via cfg or oxcf eventualy.
    cfg.ts_target_bitrate[0] = ScaleBitRate0;
    cfg.ts_target_bitrate[1] = ScaleBitRate1;
    cfg.ts_target_bitrate[2] = ScaleBitRate2;
    cfg.ts_target_bitrate[3] = ScaleBitRate3;
    cfg.ts_target_bitrate[4] = ScaleBitRate4;

    // Timebase format e.g. 30fps: numerator=1, demoninator=30
    if (!sscanf ("1", "%d", &cfg.g_timebase.num ))
        return 0;
    if (!sscanf ("30", "%d", &cfg.g_timebase.den ))
        return 0;

    // Real time parameters
    cfg.rc_dropframe_thresh = 0;  // 30
    cfg.rc_end_usage        = VPX_CBR;
    cfg.rc_resize_allowed   = 0;
    cfg.rc_min_quantizer    = 8;
    cfg.rc_max_quantizer    = 56;
    cfg.rc_undershoot_pct   = 100;
    cfg.rc_overshoot_pct    = 15;
    cfg.rc_buf_initial_sz   = 500;
    cfg.rc_buf_optimal_sz   = 600;
    cfg.rc_buf_sz           = 1000;

    // Enable error resilient mode
    cfg.g_error_resilient = 1;
    cfg.g_lag_in_frames   = 0;
    cfg.kf_mode           = VPX_KF_DISABLED;

    // Disable automatic keyframe placement
    cfg.kf_min_dist = cfg.kf_max_dist = 1000;

    // Temporal scaling parameters:
    // NOTE: The 3 prediction frames cannot be used interchangeably due to
    // differences in the way they are handled throughout the code. The
    // frames should be allocated to layers in the order LAST, GF, ARF.
    // Other combinations work, but may produce slightly inferior results.
    switch (layering_mode)
    {

    case 0:
    {
        // 2-layers, 2-frame period
        int ids[2] = {0,1};
        cfg.ts_number_layers     = 2;
        cfg.ts_periodicity       = 2;
        cfg.ts_rate_decimator[0] = 2;
        cfg.ts_rate_decimator[1] = 1;
        memcpy(cfg.ts_layer_id, ids, sizeof(ids));

        flag_periodicity = cfg.ts_periodicity;
#if 1
        // 0=L, 1=GF, Intra-layer prediction enabled
        layer_flags[0] = VPX_EFLAG_FORCE_KF  |
                         VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF |
                         VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_REF_ARF;
        layer_flags[1] = VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST |
                         VP8_EFLAG_NO_REF_ARF;
#else
        // 0=L, 1=GF, Intra-layer prediction disabled
        layer_flags[0] = VPX_EFLAG_FORCE_KF  |
                         VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF |
                         VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_REF_ARF;
        layer_flags[1] = VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST |
                         VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_REF_LAST;
#endif
        break;
    }

    case 1:
    {
        // 2-layers, 3-frame period
        int ids[3] = {0,1,1};
        cfg.ts_number_layers     = 2;
        cfg.ts_periodicity       = 3;
        cfg.ts_rate_decimator[0] = 3;
        cfg.ts_rate_decimator[1] = 1;
        memcpy(cfg.ts_layer_id, ids, sizeof(ids));

        flag_periodicity = cfg.ts_periodicity;

        // 0=L, 1=GF, Intra-layer prediction enabled
        layer_flags[0] = VPX_EFLAG_FORCE_KF  |
                         VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_REF_ARF |
                         VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF;
        layer_flags[1] =
        layer_flags[2] = VP8_EFLAG_NO_REF_GF  |
                         VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_UPD_ARF |
                                                VP8_EFLAG_NO_UPD_LAST;
        break;
    }

    case 2:
    {
        // 3-layers, 6-frame period
        int ids[6] = {0,2,2,1,2,2};
        cfg.ts_number_layers     = 3;
        cfg.ts_periodicity       = 6;
        cfg.ts_rate_decimator[0] = 6;
        cfg.ts_rate_decimator[1] = 3;
        cfg.ts_rate_decimator[2] = 1;
        memcpy(cfg.ts_layer_id, ids, sizeof(ids));

        flag_periodicity = cfg.ts_periodicity;

        // 0=L, 1=GF, 2=ARF, Intra-layer prediction enabled
        layer_flags[0] = VPX_EFLAG_FORCE_KF  |
                         VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_REF_ARF |
                         VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF;
        layer_flags[3] = VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_UPD_ARF |
                                                VP8_EFLAG_NO_UPD_LAST;
        layer_flags[1] =
        layer_flags[2] =
        layer_flags[4] =
        layer_flags[5] = VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_LAST;
        break;
    }

    case 3:
    {
        // 3-layers, 4-frame period
        int ids[4] = {0,2,1,2};
        cfg.ts_number_layers     = 3;
        cfg.ts_periodicity       = 4;
        cfg.ts_rate_decimator[0] = 4;
        cfg.ts_rate_decimator[1] = 2;
        cfg.ts_rate_decimator[2] = 1;
        memcpy(cfg.ts_layer_id, ids, sizeof(ids));

        flag_periodicity = cfg.ts_periodicity;

        // 0=L, 1=GF, 2=ARF, Intra-layer prediction disabled
        layer_flags[0] = VPX_EFLAG_FORCE_KF  |
                         VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_REF_ARF |
                         VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF;
        layer_flags[2] = VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_REF_ARF |
                         VP8_EFLAG_NO_UPD_ARF |
                         VP8_EFLAG_NO_UPD_LAST;
        layer_flags[1] =
        layer_flags[3] = VP8_EFLAG_NO_REF_ARF |
                         VP8_EFLAG_NO_UPD_LAST | VP8_EFLAG_NO_UPD_GF |
                         VP8_EFLAG_NO_UPD_ARF;
        break;
    }

    case 4:
    {
        // 3-layers, 4-frame period
        int ids[4] = {0,2,1,2};
        cfg.ts_number_layers     = 3;
        cfg.ts_periodicity       = 4;
        cfg.ts_rate_decimator[0] = 4;
        cfg.ts_rate_decimator[1] = 2;
        cfg.ts_rate_decimator[2] = 1;
        memcpy(cfg.ts_layer_id, ids, sizeof(ids));

        flag_periodicity = cfg.ts_periodicity;

        // 0=L, 1=GF, 2=ARF, Intra-layer prediction enabled in layer 1,
        // disabled in layer 2
        layer_flags[0] = VPX_EFLAG_FORCE_KF  |
                         VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_REF_ARF |
                         VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF;
        layer_flags[2] = VP8_EFLAG_NO_REF_ARF |
                         VP8_EFLAG_NO_UPD_LAST | VP8_EFLAG_NO_UPD_ARF;
        layer_flags[1] =
        layer_flags[3] = VP8_EFLAG_NO_REF_ARF |
                         VP8_EFLAG_NO_UPD_LAST | VP8_EFLAG_NO_UPD_GF |
                         VP8_EFLAG_NO_UPD_ARF;
        break;
    }

    case 5:
    {
        // 3-layers, 4-frame period
        int ids[4] = {0,2,1,2};
        cfg.ts_number_layers     = 3;
        cfg.ts_periodicity       = 4;
        cfg.ts_rate_decimator[0] = 4;
        cfg.ts_rate_decimator[1] = 2;
        cfg.ts_rate_decimator[2] = 1;
        memcpy(cfg.ts_layer_id, ids, sizeof(ids));

        flag_periodicity = cfg.ts_periodicity;

        // 0=L, 1=GF, 2=ARF, Intra-layer prediction enabled
        layer_flags[0] = VPX_EFLAG_FORCE_KF  |
                         VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_REF_ARF |
                         VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF;
        layer_flags[2] = VP8_EFLAG_NO_REF_ARF |
                         VP8_EFLAG_NO_UPD_LAST | VP8_EFLAG_NO_UPD_ARF;
        layer_flags[1] =
        layer_flags[3] = VP8_EFLAG_NO_UPD_LAST | VP8_EFLAG_NO_UPD_GF;
        break;
    }

    case 6:
    {
        // NOTE: Probably of academic interest only

        // 5-layers, 16-frame period
        int ids[16] = {0,4,3,4,2,4,3,4,1,4,3,4,2,4,3,4};
        cfg.ts_number_layers     = 5;
        cfg.ts_periodicity       = 16;
        cfg.ts_rate_decimator[0] = 16;
        cfg.ts_rate_decimator[1] = 8;
        cfg.ts_rate_decimator[2] = 4;
        cfg.ts_rate_decimator[3] = 2;
        cfg.ts_rate_decimator[4] = 1;
        memcpy(cfg.ts_layer_id, ids, sizeof(ids));

        flag_periodicity = cfg.ts_periodicity;

        layer_flags[0]  = VPX_EFLAG_FORCE_KF;
        layer_flags[1]  =
        layer_flags[3]  =
        layer_flags[5]  =
        layer_flags[7]  =
        layer_flags[9]  =
        layer_flags[11] =
        layer_flags[13] =
        layer_flags[15] = VP8_EFLAG_NO_UPD_LAST |
                          VP8_EFLAG_NO_UPD_GF   |
                          VP8_EFLAG_NO_UPD_ARF;
        layer_flags[2]  =
        layer_flags[6]  =
        layer_flags[10] =
        layer_flags[14] = VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_GF;
        layer_flags[4]  =
        layer_flags[12] = VP8_EFLAG_NO_REF_LAST |
                          VP8_EFLAG_NO_UPD_ARF;
        layer_flags[8]  = VP8_EFLAG_NO_REF_LAST | VP8_EFLAG_NO_REF_GF;
        break;
    }

    case 7:
    {
        // 2-layers
        int ids[2] = {0,1};
        cfg.ts_number_layers     = 2;
        cfg.ts_periodicity       = 2;
        cfg.ts_rate_decimator[0] = 2;
        cfg.ts_rate_decimator[1] = 1;
        memcpy(cfg.ts_layer_id, ids, sizeof(ids));

        flag_periodicity = 8;

        // 0=L, 1=GF
        layer_flags[0] = VPX_EFLAG_FORCE_KF  |
                         VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_REF_ARF |
                         VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF;
        layer_flags[1] = VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_REF_ARF |
                         VP8_EFLAG_NO_UPD_LAST | VP8_EFLAG_NO_UPD_ARF;
        layer_flags[2] =
        layer_flags[4] =
        layer_flags[6] = VP8_EFLAG_NO_REF_GF  | VP8_EFLAG_NO_REF_ARF |
                         VP8_EFLAG_NO_UPD_GF  | VP8_EFLAG_NO_UPD_ARF;
        layer_flags[3] =
        layer_flags[5] = VP8_EFLAG_NO_REF_ARF |
                         VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST;
        layer_flags[7] = VP8_EFLAG_NO_REF_ARF |
                         VP8_EFLAG_NO_UPD_LAST | VP8_EFLAG_NO_UPD_GF |
                         VP8_EFLAG_NO_UPD_ARF |
                         VP8_EFLAG_NO_UPD_ENTROPY;
        break;
    }

    case 8:
    {
        // 3-layers
        int ids[4] = {0,2,1,2};
        cfg.ts_number_layers     = 3;
        cfg.ts_periodicity       = 4;
        cfg.ts_rate_decimator[0] = 4;
        cfg.ts_rate_decimator[1] = 2;
        cfg.ts_rate_decimator[2] = 1;
        memcpy(cfg.ts_layer_id, ids, sizeof(ids));

        flag_periodicity = 8;

        // 0=L, 1=GF, 2=ARF
        layer_flags[0] = VPX_EFLAG_FORCE_KF  |
                         VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_REF_ARF |
                         VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF;
        layer_flags[1] = VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_REF_ARF |
                         VP8_EFLAG_NO_UPD_LAST | VP8_EFLAG_NO_UPD_GF;
        layer_flags[2] = VP8_EFLAG_NO_REF_GF   | VP8_EFLAG_NO_REF_ARF |
                         VP8_EFLAG_NO_UPD_LAST | VP8_EFLAG_NO_UPD_ARF;
        layer_flags[3] =
        layer_flags[5] = VP8_EFLAG_NO_UPD_LAST | VP8_EFLAG_NO_UPD_GF;
        layer_flags[4] = VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_REF_ARF |
                         VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF;
        layer_flags[6] = VP8_EFLAG_NO_REF_ARF |
                         VP8_EFLAG_NO_UPD_LAST | VP8_EFLAG_NO_UPD_ARF;
        layer_flags[7] = VP8_EFLAG_NO_UPD_LAST | VP8_EFLAG_NO_UPD_GF |
                         VP8_EFLAG_NO_UPD_ARF |
                         VP8_EFLAG_NO_UPD_ENTROPY;
        break;
    }
    default:
        break;
    }

    struct detect_buffer detect;

        infile = strcmp(in_fn, "-") ? fopen(in_fn, "rb") :
            set_binary_mode(stdin);

        if (!infile)
        {
            tprintf(PRINT_BTH, "Failed to open input file: %s", in_fn);
            return -1;
        }

        detect.buf_read = fread(detect.buf, 1, 4, infile);
        detect.position = 0;

        unsigned int rate;
        unsigned int scale;

        if (detect.buf_read == 4 && file_is_y4m(infile, &y4m, detect.buf))
        {
            if (y4m_input_open(&y4m, infile, detect.buf, 4) >= 0)
            {
                file_type = FILE_TYPE_Y4M;
                cfg.g_w = y4m.pic_w;
                cfg.g_h = y4m.pic_h;

                /* Use the frame rate from the file only if none was specified
                * on the command-line.
                */
                if (!arg_have_framerate)
                {
                    rate = y4m.fps_n;
                    scale = y4m.fps_d;
                    arg_framerate.num = y4m.fps_n;
                    arg_framerate.den = y4m.fps_d;
                }

                arg_use_i420 = 0;
            }
            else
            {
                fprintf(stderr, "Unsupported Y4M stream.\n");
                return EXIT_FAILURE;
            }
        }
        else if (detect.buf_read == 4 && file_is_ivf(infile, &fourcc, &cfg.g_w,
            &cfg.g_h, &detect, &scale, &rate))
        {
            file_type = FILE_TYPE_IVF;

            switch (fourcc)
            {
            case 0x32315659:
                arg_use_i420 = 0;
                break;
            case 0x30323449:
                arg_use_i420 = 1;
                break;
            default:
                fprintf(stderr, "Unsupported fourcc (%08x) in IVF\n", fourcc);
                return EXIT_FAILURE;
            }
        }
        else
        {
            file_type = FILE_TYPE_RAW;
        }

        if (!cfg.g_w || !cfg.g_h)
        {
            fprintf(stderr, "Specify stream dimensions with --width (-w) "
                    " and --height (-h).\n");
            return EXIT_FAILURE;
        }

    width = cfg.g_w;
    height = cfg.g_h;

    /*set timebase*/
    cfg.g_timebase.den = rate;
    cfg.g_timebase.num = scale;
    arg_framerate.num = rate;
    arg_framerate.den = scale;

    /*check timebase*/
    if(arg_framerate.num > cfg.g_timebase.den)
    {
        tprintf(PRINT_BTH,"Invalid timebase: %i/%i - changing to default:"
            " %i/%i\n",cfg.g_timebase.num,cfg.g_timebase.den
            , 1, arg_framerate.num);
        cfg.g_timebase.den = arg_framerate.num;
        cfg.g_timebase.num = 1;
    }

    std::string inputformat;
    std::string outputformat;

    if (write_webm == 1)
        outputformat = "Webm";

    if (write_webm == 0)
        outputformat = "IVF";

    if (file_type == FILE_TYPE_RAW)
        inputformat = "Raw";

    if (file_type == FILE_TYPE_Y4M)
        inputformat = "Y4M";

    if (file_type == FILE_TYPE_IVF)
        inputformat = "IVF";

        if (file_type == FILE_TYPE_Y4M)
            /*The Y4M reader does its own allocation.
            Just initialize this here to avoid problems if we never read any
            frames.*/
            memset(&raw, 0, sizeof(raw));
        else
            vpx_img_alloc(&raw, arg_use_i420 ? VPX_IMG_FMT_I420 :
            VPX_IMG_FMT_YV12,
            cfg.g_w, cfg.g_h, 1);

        for (i=0; i<cfg.ts_number_layers; i++)
        {
            char file_name[512];
            sprintf (file_name, "%s_%d.%s", outputFile2, i, EncFormat.c_str());
            outfile[i] = fopen(file_name, "wb");

            if (!outfile[i]){
                tprintf(PRINT_BTH, "Failed to open output file: %s", out_fn);
                fclose(infile);
                return -1;
            }
            if (write_webm){
                ebml[i].stream = outfile[i];
                write_webm_file_header(&ebml[i], &cfg, &arg_framerate,
                    stereo_fmt);
            }
            else
                write_ivf_file_header(outfile[i], &cfg, codec->fourcc, 0);
        }

    // Initialize codec
    if (vpx_codec_enc_init (&codec_enc, codec->iface, &cfg, 0))
        return 0;

    // Cap CPU & first I-frame size
    vpx_codec_control (&codec_enc, VP8E_SET_CPUUSED,                -6);
    vpx_codec_control (&codec_enc, VP8E_SET_STATIC_THRESHOLD,      800);

    max_intra_size_pct = (int) (((double)cfg.rc_buf_optimal_sz * 0.5)
                         * ((double) cfg.g_timebase.den / cfg.g_timebase.num)
                         / 10.0);

    vpx_codec_control(&codec_enc, VP8E_SET_MAX_INTRA_BITRATE_PCT,
                      max_intra_size_pct);

    vpxt_api_config_to_core_config(cfg, oxcf);
    tprintf(PRINT_BTH, "\n\n Target Bit Rate: %d \n Max Quantizer: %d \n"
        " Min Quantizer %d \n %s: %d \n \n", oxcf.target_bandwidth,
        oxcf.worst_allowed_q, oxcf.best_allowed_q, comp_out_str, compress_int);
    tprintf(PRINT_BTH, "API Temporal Scalability - Compressing Raw %s File to"
        " VP8 %s Files: \n", inputformat.c_str(), outputformat.c_str());

    /////////////////////////////// OUTPUT PARAMATERS //////////////////////////
    std::string OutputsettingsFile = outputFile2;
    OutputsettingsFile += "_parameters_core.txt";
    vpxt_output_settings(OutputsettingsFile.c_str(),  oxcf);
    ///////////////////////////// OUTPUT PARAMATERS API ////////////////////////
    OutputsettingsFile.erase(OutputsettingsFile.length() - 20, 20);
    OutputsettingsFile  += "_parameters_vpx.txt";
    vpxt_output_settings_api(OutputsettingsFile.c_str(),  cfg);
    ////////////////////////////////////////////////////////////////////////////

    frame_avail = 1;
    while (frame_avail || got_data) {
        unsigned int start, end;
        struct vpx_usec_timer timer;
        vpx_codec_iter_t iter = NULL;
        const vpx_codec_cx_pkt_t *pkt;

        flags = layer_flags[frame_cnt % flag_periodicity];

        frame_avail = read_frame_enc(infile, &raw, file_type, &y4m, &detect);

        start = vpxt_get_cpu_tick();
        vpx_usec_timer_start(&timer);
        if (vpx_codec_encode(&codec_enc, frame_avail? &raw : NULL, pts,
                            1, flags, VPX_DL_REALTIME))
                            return 0;
        vpx_usec_timer_mark(&timer);
        cx_time += vpx_usec_timer_elapsed(&timer);
        end = vpxt_get_cpu_tick();

        total_cpu_time_used = total_cpu_time_used + (end - start);

        // Reset KF flag
        if (layering_mode != 6)
            layer_flags[0] &= ~VPX_EFLAG_FORCE_KF;

        got_data = 0;
        while ( (pkt = vpx_codec_get_cx_data(&codec_enc, &iter)) ) {
            got_data = 1;
            switch (pkt->kind) {
            case VPX_CODEC_CX_FRAME_PKT:
                for (i=cfg.ts_layer_id[frame_cnt % cfg.ts_periodicity];
                                              i<cfg.ts_number_layers; i++)
                {
                    if (write_webm)
                    {
                        if (!ebml[i].debug)
                            hash = murmur(pkt->data.frame.buf,
                                          pkt->data.frame.sz, hash);

                        write_webm_block(&ebml[i], &cfg, pkt);
                    }
                    else
                    {
                        write_ivf_frame_header(outfile[i], pkt);

                        if (fwrite(pkt->data.frame.buf, 1,
                                   pkt->data.frame.sz, outfile[i]));
                    }
                }
                break;
            default:
                break;
            }
            tprintf (PRINT_BTH, pkt->kind == VPX_CODEC_CX_FRAME_PKT
                && (pkt->data.frame.flags & VPX_FRAME_IS_KEY)? "K":".");

            print_count++;
            if(print_count >= 79){
                tprintf(PRINT_BTH, "\n");
                print_count = 0;
            }

            fflush (stdout);
        }
        frame_cnt++;
        pts += frame_duration;
    }
    printf ("\n");
    fclose (infile);

    printf ("Processed %d frames.\n",frame_cnt-1);
    if (vpx_codec_destroy(&codec_enc))
        return 0;

    // Try to rewrite the output file headers with the actual frame count
    if (file_type == FILE_TYPE_Y4M)
            y4m_input_close(&y4m);
    else
        vpx_img_free(&raw);

    for (i=0; i<cfg.ts_number_layers; i++)
    {
        if (write_webm){
            write_webm_file_footer(&ebml[i], hash);
        }
        else{
            if (!fseek(outfile[i], 0, SEEK_SET))
                write_ivf_file_header(outfile[i], &cfg, codec->fourcc,
                frames_in_layer[i]);
        }
        fclose (outfile[i]);
        free(ebml[i].cue_list);
    }

    tprintf(PRINT_BTH,"\n File completed: time in Microseconds: %u, Fps: %d \n",
            total_cpu_time_used,
            1000 * frame_cnt / (total_cpu_time_used / 1000));
    tprintf(PRINT_BTH, " Total CPU Ticks: %u\n", cx_time);

    return cx_time;
}
int vpxt_decompress(const char *inputchar,
                    const char *outputchar,
                    std::string DecFormat,
                    int threads)
{
    int use_y4m = 1;
    vpxt_lower_case_string(DecFormat);

    if (DecFormat.compare("ivf") == 0)
        use_y4m = 2;

    vpx_codec_ctx_t        decoder;
    const char             *fn = inputchar;
    int                     i;
    uint8_t                *buf = NULL;
    size_t                  buf_sz = 0, buf_alloc_sz = 0;
    FILE                   *infile;
    int                     frame_in = 0, frame_out = 0, flipuv = 0, noblit = 0, do_md5 = 0, progress = 0;
    int                     stop_after = 0, postproc = 0, summary = 0, quiet = 1;
    vpx_codec_iface_t      *iface = NULL;
    unsigned int            fourcc;
    unsigned long           dx_time = 0;
    const char             *outfile_pattern = 0;
    char                    outfile[PATH_MAX];
    int                     single_file;
    unsigned int            width;
    unsigned int            height;
    unsigned int            fps_den;
    unsigned int            fps_num;
    void                   *out = NULL;
    vpx_codec_dec_cfg_t     cfg = {0};
#if CONFIG_VP8_DECODER
    vp8_postproc_cfg_t      vp8_pp_cfg = {0};
    int                     vp8_dbg_color_ref_frame = 0;
    int                     vp8_dbg_color_mb_modes = 0;
    int                     vp8_dbg_color_b_modes = 0;
    int                     vp8_dbg_display_mv = 0;
#endif
    struct input_ctx        input;

    input.chunk = 0;
    input.chunks = 0;
    input.infile = NULL;
    input.kind = RAW_FILE;
    input.nestegg_ctx = 0;
    input.pkt = 0;
    input.video_track = 0;

    int CharCount = 0;

    /* Open file */
    infile = strcmp(fn, "-") ? fopen(fn, "rb") : set_binary_mode(stdin);
    if (!infile){
        tprintf(PRINT_BTH, "Failed to open input file: %s", fn);
        return -1;
    }

    input.infile = infile;
    if (file_is_ivf_dec(infile, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = IVF_FILE;
    else if (file_is_webm(&input, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = WEBM_FILE;
    else if (file_is_raw(infile, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = RAW_FILE;
    else
    {
        fprintf(stderr, "Unrecognized input file type.\n");
        return EXIT_FAILURE;
    }

    std::string compformat;
    std::string decformat;

    if (input.kind == IVF_FILE)
        compformat = "IVF";

    if (input.kind == WEBM_FILE)
        compformat = "Webm";

    if (input.kind == RAW_FILE)
        compformat = "Raw";

    if (use_y4m == 0)
        decformat = "Raw";

    if (use_y4m == 1)
        decformat = "Y4M";

    if (use_y4m == 2)
        decformat = "IVF";

    tprintf(PRINT_BTH, "\nAPI - Decompressing VP8 %s File to Raw %s File: \n",
        compformat.c_str(), decformat.c_str());

    outfile_pattern = outfile_pattern ? outfile_pattern : "-";
    single_file = 1;
    {
        const char *p = outfile_pattern;

        do
        {
            p = strchr(p, '%');

            if (p && p[1] >= '1' && p[1] <= '9')
            {
                single_file = 0;
                break;
            }

            if (p)
                p++;
        }
        while (p);
    }

    if (single_file && !noblit)
    {
        generate_filename(outfile_pattern, outfile, sizeof(outfile) - 1,
                          width, height, 0);
        strncpy(outfile, outputchar, PATH_MAX);
        // outfile = outputchar;
        out = out_open(outfile, do_md5);
    }

    if (use_y4m && !noblit)
    {
        char buffer[128];

        if (!single_file)
        {
            fprintf(stderr, "YUV4MPEG2 not supported with output patterns,"
                    " try --i420 or --yv12.\n");
            return EXIT_FAILURE;
        }

        if (input.kind == WEBM_FILE)
            if (webm_guess_framerate(&input, &fps_den, &fps_num))
            {
                fprintf(stderr, "Failed to guess framerate -- error parsing "
                        "webm file?\n");
                return EXIT_FAILURE;
            }

        /*Note: We can't output an aspect ratio here because IVF doesn't
        store one, and neither does VP8.
        That will have to wait until these tools support WebM natively.*/
        sprintf(buffer, "YUV4MPEG2 C%s W%u H%u F%u:%u I%c\n",
                "420jpeg", width, height, fps_num, fps_den, 'p');
        out_put(out, (unsigned char *)buffer, strlen(buffer), do_md5);
    }

    /* Try to determine the codec from the fourcc. */
    for (i = 0; i < sizeof(ifaces) / sizeof(ifaces[0]); i++)
        if ((fourcc & ifaces[i].fourcc_mask) == ifaces[i].fourcc)
        {
            vpx_codec_iface_t  *ivf_iface = ifaces[i].iface;

            if (iface && iface != ivf_iface)
                tprintf(PRINT_STD, "Notice -- IVF header indicates codec: %s\n",
                        ifaces[i].name);
            else
                iface = ivf_iface;

            break;
        }

    unsigned int FrameSize = (width * height * 3) / 2;
    uint64_t TimeStamp = 0;

    cfg.threads = threads;

    if (vpx_codec_dec_init(&decoder, iface ? iface :  ifaces[0].iface, &cfg,
        postproc ? VPX_CODEC_USE_POSTPROC : 0))
    {
        tprintf(PRINT_STD, "Failed to initialize decoder: %s\n",
            vpx_codec_error(&decoder));
        fclose(infile);

        return -1;
    }

    uint64_t timestamp = 0;

    /* Decode file */
    while (!read_frame_dec(&input, &buf, &buf_sz, &buf_alloc_sz, &timestamp))
    {
        vpx_codec_iter_t  iter = NULL;
        vpx_image_t    *img;
        struct vpx_usec_timer timer;

        vpx_usec_timer_start(&timer);

        if (vpx_codec_decode(&decoder, buf, buf_sz, NULL, 0))
        {
            const char *detail = vpx_codec_error_detail(&decoder);
            tprintf(PRINT_STD, "Failed to decode frame: %s\n",
                vpx_codec_error(&decoder));

            if (detail)
                tprintf(PRINT_STD, "  Additional information: %s\n", detail);

            goto fail;
        }

        vpx_usec_timer_mark(&timer);
        dx_time += vpx_usec_timer_elapsed(&timer);

        ++frame_in;

        if ((img = vpx_codec_get_frame(&decoder, &iter)))
            ++frame_out;

        if (progress)
        {
        }

        if (!noblit)
        {
            if (img)
            {
                unsigned int y;
                char out_fn[PATH_MAX];
                uint8_t *buf;

                if (!single_file)
                {
                    size_t len = sizeof(out_fn) - 1;
                    out_fn[len] = '\0';
                    generate_filename(outfile_pattern, out_fn, len - 1,
                                      img->d_w, img->d_h, frame_in);
                    out = out_open(out_fn, do_md5);
                }
                else if (use_y4m)
                    out_put(out, (unsigned char *)"FRAME\n", 6, do_md5);

                if (!use_y4m)
                    ivf_write_frame_header((FILE *)out, 0, img->d_h * img->d_w);

                if (CharCount == 79)
                {
                    tprintf(PRINT_BTH, "\n");
                    CharCount = 0;
                }

                CharCount++;
                tprintf(PRINT_BTH, ".");

                buf = img->planes[VPX_PLANE_Y];

                for (y = 0; y < img->d_h; y++)
                {
                    out_put(out, buf, img->d_w, do_md5);
                    buf += img->stride[VPX_PLANE_Y];
                }

                buf = img->planes[flipuv?VPX_PLANE_V:VPX_PLANE_U];

                for (y = 0; y < (1 + img->d_h) / 2; y++)
                {
                    out_put(out, buf, (1 + img->d_w) / 2, do_md5);
                    buf += img->stride[VPX_PLANE_U];
                }

                buf = img->planes[flipuv?VPX_PLANE_U:VPX_PLANE_V];

                for (y = 0; y < (1 + img->d_h) / 2; y++)
                {
                    out_put(out, buf, (1 + img->d_w) / 2, do_md5);
                    buf += img->stride[VPX_PLANE_V];
                }

                if (!single_file)
                    out_close(out, out_fn, do_md5);
            }
        }

        if (stop_after && frame_in >= stop_after)
            break;
    }

    if (summary || progress)
    {
        show_progress(frame_in, frame_out, dx_time);
        fprintf(stderr, "\n");
    }

fail:

    if (vpx_codec_destroy(&decoder))
    {
        tprintf(PRINT_STD, "Failed to destroy decoder: %s\n",
            vpx_codec_error(&decoder));
        fclose(infile);

        return -1;
    }

    if (single_file && !noblit)
        out_close(out, outfile, do_md5);

    if (input.nestegg_ctx)
        nestegg_destroy(input.nestegg_ctx);

    if (input.kind != WEBM_FILE)
        free(buf);

    fclose(infile);

    return 0;
}
int vpxt_decompress_copy_set(const char *inputchar,
                             const char *outputchar,
                             const char *outfile2,
                             std::string DecFormat,
                             int threads,
                             int firstClone,
                             int printVar)
{
    int use_y4m = 1;
    vpxt_lower_case_string(DecFormat);

    if (DecFormat.compare("ivf") == 0)
        use_y4m = 2;

    vpx_codec_ctx_t  decoder_clone;
    vpx_img_fmt_t    fmt;
    unsigned int     d_w = 0;
    unsigned int     d_h = 0;
    int              cloned = 0;
    int notprinted = 1;
    int dec_cpy_set_retval = 1;

    vpx_codec_ctx_t         decoder;
    const char             *fn = inputchar;
    int                     i;
    uint8_t                *buf = NULL;
    size_t                  buf_sz = 0, buf_alloc_sz = 0;
    FILE                   *infile;
    int                     frame_in = 0, frame_out = 0, flipuv = 0, noblit = 0;
    int                     do_md5 = 0, progress = 0;
    int                     stop_after = 0, postproc = 0, summary = 0, quiet =1;
    vpx_codec_iface_t      *iface = NULL;
    unsigned int            fourcc;
    unsigned long           dx_time = 0;
    const char             *outfile_pattern = 0;
    char                    outfile[PATH_MAX];
    int                     single_file;
    unsigned int            width;
    unsigned int            height;
    unsigned int            fps_den;
    unsigned int            fps_num;
    void                   *out = NULL;
    void                   *out2 = NULL;
    vpx_codec_dec_cfg_t     cfg = {0};
#if CONFIG_VP8_DECODER
    vp8_postproc_cfg_t      vp8_pp_cfg = {0};
    int                     vp8_dbg_color_ref_frame = 0;
    int                     vp8_dbg_color_mb_modes = 0;
    int                     vp8_dbg_color_b_modes = 0;
    int                     vp8_dbg_display_mv = 0;
#endif
    struct input_ctx        input;

    input.chunk = 0;
    input.chunks = 0;
    input.infile = NULL;
    input.kind = RAW_FILE;
    input.nestegg_ctx = 0;
    input.pkt = 0;
    input.video_track = 0;

    int CharCount = 0;

    /* Open file */
    infile = strcmp(fn, "-") ? fopen(fn, "rb") : set_binary_mode(stdin);
    if (!infile){
        tprintf(printVar, "Failed to open input file: %s", fn);
        return -1;
    }

    input.infile = infile;
    if (file_is_ivf_dec(infile, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = IVF_FILE;
    else if (file_is_webm(&input, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = WEBM_FILE;
    else if (file_is_raw(infile, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = RAW_FILE;
    else
    {
        fprintf(stderr, "Unrecognized input file type.\n");
        return EXIT_FAILURE;
    }

    std::string compformat;
    std::string decformat;

    if (input.kind == IVF_FILE)
        compformat = "IVF";

    if (input.kind == WEBM_FILE)
        compformat = "Webm";

    if (input.kind == RAW_FILE)
        compformat = "Raw";

    if (use_y4m == 0)
        decformat = "Raw";

    if (use_y4m == 1)
        decformat = "Y4M";

    if (use_y4m == 2)
        decformat = "IVF";

    tprintf(printVar, "\nAPI - Decompressing VP8 %s File to Raw %s File: \n",
        compformat.c_str(), decformat.c_str());

    outfile_pattern = outfile_pattern ? outfile_pattern : "-";
    single_file = 1;
    {
        const char *p = outfile_pattern;

        do
        {
            p = strchr(p, '%');

            if (p && p[1] >= '1' && p[1] <= '9')
            {
                single_file = 0;
                break;
            }

            if (p)
                p++;
        }
        while (p);
    }

    if (single_file && !noblit)
    {
        generate_filename(outfile_pattern, outfile, sizeof(outfile) - 1,
                          width, height, 0);
        strncpy(outfile, outputchar, PATH_MAX);
        out = out_open(outfile, do_md5);
        out2 = out_open(outfile2, do_md5);
    }

    if (use_y4m && !noblit)
    {
        char buffer[128];

        if (!single_file)
        {
            fprintf(stderr, "YUV4MPEG2 not supported with output patterns,"
                    " try --i420 or --yv12.\n");
            return EXIT_FAILURE;
        }

        if (input.kind == WEBM_FILE)
            if (webm_guess_framerate(&input, &fps_den, &fps_num))
            {
                fprintf(stderr, "Failed to guess framerate -- error parsing "
                        "webm file?\n");
                return EXIT_FAILURE;
            }

        /*Note: We can't output an aspect ratio here because IVF doesn't
        store one, and neither does VP8.
        That will have to wait until these tools support WebM natively.*/
        sprintf(buffer, "YUV4MPEG2 C%s W%u H%u F%u:%u I%c\n",
                "420jpeg", width, height, fps_num, fps_den, 'p');
        out_put(out, (unsigned char *)buffer, strlen(buffer), do_md5);
        out_put(out2, (unsigned char *)buffer, strlen(buffer), do_md5);
    }

    /* Try to determine the codec from the fourcc. */
    for (i = 0; i < sizeof(ifaces) / sizeof(ifaces[0]); i++)
        if ((fourcc & ifaces[i].fourcc_mask) == ifaces[i].fourcc)
        {
            vpx_codec_iface_t  *ivf_iface = ifaces[i].iface;

            if (iface && iface != ivf_iface)
                tprintf(printVar, "Notice -- IVF header indicates codec: %s\n",
                        ifaces[i].name);
            else
                iface = ivf_iface;

            break;
        }

    unsigned int FrameSize = (width * height * 3) / 2;
    uint64_t TimeStamp = 0;

    cfg.threads = threads;

    if (vpx_codec_dec_init(&decoder, iface ? iface : ifaces[0].iface, &cfg,
        postproc ? VPX_CODEC_USE_POSTPROC : 0))
    {
        tprintf(printVar, "Failed to initialize decoder: %s\n",
            vpx_codec_error(&decoder));
        fclose(infile);

        return -1;
    }

    if (vpx_codec_dec_init(&decoder_clone, iface ? iface : ifaces[0].iface,
        &cfg, postproc ? VPX_CODEC_USE_POSTPROC : 0))
    {
        tprintf(printVar, "Failed to initialize decoder clone: %s\n",
            vpx_codec_error(&decoder));
        fclose(infile);

        return -1;
    }

    int clonethedecoder = 0;
    uint64_t timestamp = 0;

    /* Decode file */
    while (!read_frame_dec(&input, &buf, &buf_sz, &buf_alloc_sz, &timestamp))
    {
        vpx_codec_iter_t  iter = NULL;
        vpx_image_t    *img;
        vpx_image_t    *img2;
        struct vpx_usec_timer timer;

        if (frame_in == firstClone && d_w > 0)
        {
            /* Clone the decoder */
            vpx_ref_frame_t ref_frame;
            clonethedecoder = 1;

            int ref_frame_w = d_w;
            int ref_frame_h = d_h;

            ref_frame_w = (ref_frame_w + 15) & ~15;
            ref_frame_h = (ref_frame_h + 15) & ~15;

            /* Allocate memory for image copy */
            if (!vpx_img_alloc(&ref_frame.img, fmt, ref_frame_w, ref_frame_h,1))
            {
                tprintf(PRINT_BTH,"Failed to allocate reference frame storage");
                dec_cpy_set_retval = -1;
                break;
            }

            /* Copy VP8_LAST_FRAME */
            ref_frame.frame_type = VP8_LAST_FRAME;

            if (vpx_codec_control(&decoder, VP8_COPY_REFERENCE, &ref_frame))
            {
                tprintf(PRINT_BTH, "Failed to get reference frame");
                vpx_img_free(&ref_frame.img);
                dec_cpy_set_retval = -1;
                return -1;
            }

            if (vpx_codec_control(&decoder_clone, VP8_SET_REFERENCE,&ref_frame))
            {
                tprintf(PRINT_BTH, "Failed to set reference frame");
                vpx_img_free(&ref_frame.img);
                dec_cpy_set_retval = -1;
                break;
            }

            /* Copy VP8_GOLD_FRAME */
            ref_frame.frame_type = VP8_GOLD_FRAME;

            if (vpx_codec_control(&decoder, VP8_COPY_REFERENCE, &ref_frame))
            {
                tprintf(PRINT_BTH, "Failed to get reference frame");
                vpx_img_free(&ref_frame.img);
                dec_cpy_set_retval = -1;
                break;
            }

            if (vpx_codec_control(&decoder_clone, VP8_SET_REFERENCE,&ref_frame))
            {
                tprintf(PRINT_BTH, "Failed to set reference frame");
                vpx_img_free(&ref_frame.img);
                dec_cpy_set_retval = -1;
                break;
            }

            /* Copy VP8_ALTR_FRAME */
            ref_frame.frame_type = VP8_ALTR_FRAME;

            if (vpx_codec_control(&decoder, VP8_COPY_REFERENCE, &ref_frame))
            {
                tprintf(PRINT_BTH, "Failed to get reference frame");
                vpx_img_free(&ref_frame.img);
                dec_cpy_set_retval = -1;
                break;
            }

            if (vpx_codec_control(&decoder_clone, VP8_SET_REFERENCE,&ref_frame))
            {
                tprintf(PRINT_BTH, "Failed to set reference frame");
                vpx_img_free(&ref_frame.img);
                dec_cpy_set_retval = -1;
                break;
            }

            cloned = 1;
            vpx_img_free(&ref_frame.img);
        }

        vpx_usec_timer_start(&timer);

        if (vpx_codec_decode(&decoder, buf, buf_sz, NULL, 0))
        {
            const char *detail = vpx_codec_error_detail(&decoder);
            tprintf(printVar, "Failed to decode frame: %s\n",
                vpx_codec_error(&decoder));

            if (detail)
                tprintf(printVar, "  Additional information: %s\n", detail);

            goto fail;
        }

        if (cloned || ((buf[0] & 0x01) == 0))
        {
            if (vpx_codec_decode(&decoder_clone, buf, buf_sz, NULL, 0))
            {
                const char *detail = vpx_codec_error_detail(&decoder);
                tprintf(printVar, "Failed to decode frame: %s\n",
                    vpx_codec_error(&decoder));

                if (detail)
                    tprintf(printVar, "  Additional information: %s\n", detail);

                goto fail;
            }
        }

        vpx_usec_timer_mark(&timer);
        dx_time += vpx_usec_timer_elapsed(&timer);

        ++frame_in;

        if ((img = vpx_codec_get_frame(&decoder, &iter)))
        {
            ++frame_out;
            fmt = img->fmt;
            d_w = img->d_w;
            d_h = img->d_h;

            /* Check if secondary decoder is active */
            if (cloned)
            {
                vpx_codec_iter_t  iter2 = NULL;
                unsigned int      plane, z, x;

                /* Get frame from secondary decoder */
                img2 = vpx_codec_get_frame(&decoder_clone, &iter2);

                /* Compare outputs */
                if (img->fmt != img2->fmt)
                    tprintf(printVar, "Clone output fmt differs");

                if (img->w != img2->w)
                    tprintf(printVar, "Clone output w differs");

                if (img->h != img2->h)
                    tprintf(printVar, "Clone output h differs");

                if (img->d_w != img2->d_w)
                    tprintf(printVar, "Clone output d_w differs");

                if (img->d_h != img2->d_h)
                    tprintf(printVar, "Clone output d_h differs");

                if (img->x_chroma_shift != img2->x_chroma_shift)
                    tprintf(printVar, "Clone output x_chroma_shift differs");

                if (img->y_chroma_shift != img2->y_chroma_shift)
                    tprintf(printVar, "Clone output y_chroma_shift differs");

                notprinted = 1;

                for (plane = 0; plane < 3; plane++)
                {
                    unsigned char *buf = img->planes[plane];
                    unsigned char *buf2 = img2->planes[plane];

                    for (z = 0; z < img->d_h >> (plane ? 1 : 0); z++)
                    {
                        for (x = 0; x < img->d_w >> (plane ? 1 : 0); x++)
                        {
                            if (buf[x] != buf2[x])
                            {
                                if (notprinted == 1)
                                {
                                    if (dec_cpy_set_retval == 1)
                                        tprintf(printVar, "\n");

                                    tprintf(printVar, "%i - Clone image differs"
                                        "\n", frame_out);
                                    notprinted = 0;
                                    dec_cpy_set_retval = 0;
                                }
                            }
                            else if (notprinted == 1)
                            {
                                // tprintf(printVar, "%i Good\n", frame_out);
                                // notprinted = 0;
                            }
                        }

                        buf += img->stride[plane];
                        buf2 += img->stride[plane];
                    }
                }
            }
        }

        if (!noblit)
        {
            if (img)
            {
                unsigned int y;
                char out_fn[PATH_MAX];
                uint8_t *buf;

                if (!single_file)
                {
                    size_t len = sizeof(out_fn) - 1;
                    out_fn[len] = '\0';
                    generate_filename(outfile_pattern, out_fn, len - 1,
                                      img->d_w, img->d_h, frame_in);
                    out = out_open(out_fn, do_md5);
                }
                else if (use_y4m)
                    out_put(out, (unsigned char *)"FRAME\n", 6, do_md5);

                if (!use_y4m)
                    ivf_write_frame_header((FILE *)out, 0, img->d_h * img->d_w);

                if (CharCount == 79)
                {
                    tprintf(printVar, "\n");
                    CharCount = 0;
                }

                if (notprinted == 0)
                    CharCount = 0;

                if (notprinted == 1)
                {
                    if (clonethedecoder != 1)
                        tprintf(printVar, ".");
                    else
                        tprintf(printVar, "*");

                    CharCount++;
                }

                buf = img->planes[VPX_PLANE_Y];

                for (y = 0; y < img->d_h; y++)
                {
                    out_put(out, buf, img->d_w, do_md5);
                    buf += img->stride[VPX_PLANE_Y];
                }

                buf = img->planes[flipuv?VPX_PLANE_V:VPX_PLANE_U];

                for (y = 0; y < (1 + img->d_h) / 2; y++)
                {
                    out_put(out, buf, (1 + img->d_w) / 2, do_md5);
                    buf += img->stride[VPX_PLANE_U];
                }

                buf = img->planes[flipuv?VPX_PLANE_U:VPX_PLANE_V];

                for (y = 0; y < (1 + img->d_h) / 2; y++)
                {
                    out_put(out, buf, (1 + img->d_w) / 2, do_md5);
                    buf += img->stride[VPX_PLANE_V];
                }

                if (!single_file)
                    out_close(out, out_fn, do_md5);
            }

            if (cloned)
            {
                unsigned int y;
                char out_fn[PATH_MAX];
                uint8_t *buf;

                std::string out_fnSTR = out_fn;
                out_fnSTR += "2";

                if (!single_file)
                {
                    size_t len = sizeof(out_fn) - 1;
                    out_fn[len] = '\0';
                    generate_filename(outfile_pattern, out_fn, len - 1,
                                      img2->d_w, img2->d_h, frame_in);
                    out2 = out_open(out_fnSTR.c_str(), do_md5);
                }
                else if (use_y4m)
                    out_put(out2, (unsigned char *)"FRAME\n", 6, do_md5);

                if (!use_y4m)
                    ivf_write_frame_header((FILE *)out2, 0,
                    img2->d_h * img2->d_w);

                buf = img2->planes[VPX_PLANE_Y];

                for (y = 0; y < img2->d_h; y++)
                {
                    out_put(out2, buf, img2->d_w, do_md5);
                    buf += img2->stride[VPX_PLANE_Y];
                }

                buf = img2->planes[flipuv?VPX_PLANE_V:VPX_PLANE_U];

                for (y = 0; y < (1 + img2->d_h) / 2; y++)
                {
                    out_put(out2, buf, (1 + img2->d_w) / 2, do_md5);
                    buf += img2->stride[VPX_PLANE_U];
                }

                buf = img2->planes[flipuv?VPX_PLANE_U:VPX_PLANE_V];

                for (y = 0; y < (1 + img2->d_h) / 2; y++)
                {
                    out_put(out2, buf, (1 + img2->d_w) / 2, do_md5);
                    buf += img2->stride[VPX_PLANE_V];
                }
            }
        }

        if (stop_after && frame_in >= stop_after)
            break;
    }

    if (summary || progress)
    {

        show_progress(frame_in, frame_out, dx_time);
        fprintf(stderr, "\n");
    }

fail:

    if (vpx_codec_destroy(&decoder))
    {
        tprintf(PRINT_STD, "Failed to destroy decoder: %s\n",
            vpx_codec_error(&decoder));
        fclose(infile);
        vpx_codec_destroy(&decoder_clone);

        return -1;
    }

    if (vpx_codec_destroy(&decoder_clone))
    {
        tprintf(PRINT_STD, "Failed to destroy decoder clone: %s\n",
            vpx_codec_error(&decoder));
        fclose(infile);

        return -1;
    }

    if (single_file && !noblit)
    {
        out_close(out, outfile, do_md5);
        out_close(out2, outfile, do_md5);
    }

    if (input.nestegg_ctx)
        nestegg_destroy(input.nestegg_ctx);

    if (input.kind != WEBM_FILE)
        free(buf);

    fclose(infile);

    if (clonethedecoder == 1)
        return dec_cpy_set_retval;
    else
        return 2;
}
int vpxt_decompress_partial_drops(const char *inputchar,
                                  const char *outputchar,
                                  std::string DecFormat,
                                  int threads,
                                  int n,
                                  int m,
                                  int mode,
                                  int printVar,
                                  int outputParDropEnc)
{
    std::string ParDropEncStr;
    vpxt_remove_file_extension(inputchar, ParDropEncStr);
    ParDropEncStr += "with_part_frame_drops.ivf";
    int                     use_y4m = 1;
    vpxt_lower_case_string(DecFormat);

    if (DecFormat.compare("ivf") == 0)
        use_y4m = 2;

    vpx_codec_ctx_t         decoder;
    const char             *fn = inputchar;
    int                     i;
    uint8_t                *buf = NULL;
    size_t                  buf_sz = 0, buf_alloc_sz = 0;
    FILE                   *infile;
    FILE                   *outfile_drops;
    int                     frame_in = 0, frame_out = 0, flipuv = 0, noblit = 0;
    int                     do_md5 = 0, progress = 0;
    int                     stop_after = 0, postproc = 0, summary = 0,quiet = 1;
    vpx_codec_iface_t      *iface = NULL;
    unsigned int            fourcc;
    unsigned long           dx_time = 0;
    const char             *outfile_pattern = 0;
    char                    outfile[PATH_MAX];
    int                     single_file;
    unsigned int            width;
    unsigned int            height;
    unsigned int            fps_den;
    unsigned int            fps_num;
    void                   *out = NULL;
    vpx_codec_dec_cfg_t     cfg = {0};
    vpx_codec_enc_cfg_t     enc_cfg = {0};
#if CONFIG_VP8_DECODER
    vp8_postproc_cfg_t      vp8_pp_cfg = {0};
    int                     vp8_dbg_color_ref_frame = 0;
    int                     vp8_dbg_color_mb_modes = 0;
    int                     vp8_dbg_color_b_modes = 0;
    int                     vp8_dbg_display_mv = 0;
#endif
    struct input_ctx        input;
    int                     flags = 0, frame_cnt = 0;

////////////////////////////////////////////////////////////////////////////////
    // int              n, m, mode;                                           //
    unsigned int     seed;                                                    //
    int              thrown = 0, kept = 0;                                    //
    int              thrown_frame = 0, kept_frame = 0;                        //
////////////////////////////////////////////////////////////////////////////////
    // char *nptr;                                                            //
    // n = strtol("3,5", &nptr, 0);                                           //
    // mode = (*nptr == '\0' || *nptr == ',') ? 2 : (*nptr == '-') ? 1 : 0;   //
    //                                                                        //
    // m = strtol(nptr+1, NULL, 0);                                           //
    // if((!n && !m) || (*nptr != '-' && *nptr != '/' &&                      //
    //    *nptr != '\0' && *nptr != ','))                                     //
    //    die_dec("Couldn't parse pattern %s\n", "3,5");                      //
    //                                                                        //
    seed = (m > 0) ? m : (unsigned int)time(NULL);                            //
    srand(seed);                                                              //
    thrown_frame = 0;                                                         //
    // printf("Seed: %u\n", seed);                                            //
////////////////////////////////////////////////////////////////////////////////

    input.chunk = 0;
    input.chunks = 0;
    input.infile = NULL;
    input.kind = RAW_FILE;
    input.nestegg_ctx = 0;
    input.pkt = 0;
    input.video_track = 0;

    int CharCount = 0;

    /* Open file */
    infile = strcmp(fn, "-") ? fopen(fn, "rb") : set_binary_mode(stdin);
    if (!infile){
        tprintf(PRINT_BTH, "Failed to open input file: %s", fn);
        return -1;
    }

    input.infile = infile;
    if (file_is_ivf_dec(infile, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = IVF_FILE;
    else if (file_is_webm(&input, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = WEBM_FILE;
    else if (file_is_raw(infile, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = RAW_FILE;
    else
    {
        fprintf(stderr, "Unrecognized input file type.\n");
        return EXIT_FAILURE;
    }

    std::string compformat;
    std::string decformat;

    if (input.kind == IVF_FILE)
        compformat = "IVF";

    if (input.kind == WEBM_FILE)
        compformat = "Webm";

    if (input.kind == RAW_FILE)
        compformat = "Raw";

    if (use_y4m == 0)
        decformat = "Raw";

    if (use_y4m == 1)
        decformat = "Y4M";

    if (use_y4m == 2)
        decformat = "IVF";

    tprintf(printVar, "\nAPI - Decompressing VP8 %s File to Raw %s File - With"
        " Partial Drops (seed = %i): \n", compformat.c_str(), decformat.c_str(),
        seed);

    outfile_pattern = outfile_pattern ? outfile_pattern : "-";
    single_file = 1;
    {
        const char *p = outfile_pattern;

        do
        {
            p = strchr(p, '%');

            if (p && p[1] >= '1' && p[1] <= '9')
            {
                single_file = 0;
                break;
            }

            if (p)
                p++;
        }
        while (p);
    }

    if (single_file && !noblit)
    {
        generate_filename(outfile_pattern, outfile, sizeof(outfile) - 1,
                          width, height, 0);
        strncpy(outfile, outputchar, PATH_MAX);
        // outfile = outputchar;
        out = out_open(outfile, do_md5);
    }

    if (use_y4m && !noblit)
    {
        char buffer[128];

        if (!single_file)
        {
            fprintf(stderr, "YUV4MPEG2 not supported with output patterns,"
                    " try --i420 or --yv12.\n");
            return EXIT_FAILURE;
        }

        if (input.kind == WEBM_FILE)
            if (webm_guess_framerate(&input, &fps_den, &fps_num))
            {
                fprintf(stderr, "Failed to guess framerate -- error parsing "
                        "webm file?\n");
                return EXIT_FAILURE;
            }

        /*Note: We can't output an aspect ratio here because IVF doesn't
        store one, and neither does VP8.
        That will have to wait until these tools support WebM natively.*/
        sprintf(buffer, "YUV4MPEG2 C%s W%u H%u F%u:%u I%c\n",
                "420jpeg", width, height, fps_num, fps_den, 'p');
        out_put(out, (unsigned char *)buffer, strlen(buffer), do_md5);
    }

    /* Try to determine the codec from the fourcc. */
    for (i = 0; i < sizeof(ifaces) / sizeof(ifaces[0]); i++)
        if ((fourcc & ifaces[i].fourcc_mask) == ifaces[i].fourcc)
        {
            vpx_codec_iface_t  *ivf_iface = ifaces[i].iface;

            if (iface && iface != ivf_iface)
                tprintf(printVar, "Notice -- IVF header indicates codec: %s\n",
                        ifaces[i].name);
            else
                iface = ivf_iface;

            break;
        }

    unsigned int FrameSize = (width * height * 3) / 2;
    uint64_t TimeStamp = 0;

    cfg.threads = threads;

    if (vpx_codec_dec_init(&decoder, iface ? iface :  ifaces[0].iface, &cfg,
        VPX_CODEC_USE_ERROR_CONCEALMENT))
    {
        tprintf(printVar, "Failed to initialize decoder: %s\n",
            vpx_codec_error(&decoder));
        fclose(infile);

        return -1;
    }

    if (outputParDropEnc)
    {
        enc_cfg.g_w = width;
        enc_cfg.g_h = height;
        enc_cfg.g_timebase.den = fps_num;
        enc_cfg.g_timebase.num = fps_den;
        outfile_drops = fopen(ParDropEncStr.c_str(), "wb");
        write_ivf_file_header(outfile_drops, &enc_cfg, VP8_FOURCC, 0);
    }

    uint64_t timestamp = 0;

    /* Decode file */
    while (!read_frame_dec(&input, &buf, &buf_sz, &buf_alloc_sz, &timestamp))
    {
        vpx_codec_iter_t  iter = NULL;
        vpx_image_t    *img;
        struct vpx_usec_timer timer;

        int frame_sz = buf_sz;

        ////////////////////////////////////////////////////////////////////////
        /* Decide whether to throw parts of the frame or the whole frame      //
           depending on the drop mode */                                      //
        thrown_frame = 0;                                                     //
        kept_frame = 0;                                                       //

        switch (mode)                                                         //
        {
            //
        case 0:                                                               //

            if (m - (frame_in - 1) % m <= n)                                  //
            {
                //
                frame_sz = 0;                                                 //
            }                                                                 //

            break;                                                            //
        case 1:                                                               //

            if (frame_in >= n && frame_in <= m)                               //
            {
                //
                frame_sz = 0;                                                 //
            }                                                                 //

            break;                                                            //
        case 2:                                                               //
            throw_packets(buf, &frame_sz, n, &thrown_frame, &kept_frame,
                printVar);                                                    //
            break;                                                            //
        default:
            break;                                                            //
        }                                                                     //

        if (mode < 2)                                                         //
        {
            //
            if (frame_sz == 0)                                                //
            {
                tprintf(printVar, "X");                                       //
                //putc('X', stdout);                                          //
                thrown_frame++;                                               //
            }                                                                 //
            else                                                              //
            {
                tprintf(printVar, ".");                                       //
                //putc('.', stdout);                                          //
                kept_frame++;                                                 //
            }                                                                 //
        }                                                                     //

        thrown += thrown_frame;                                               //
        kept += kept_frame;                                                   //
        ////////////////////////////////////////////////////////////////////////

        if (outputParDropEnc)
        {
            char header_drop[12];
            mem_put_le32(header_drop, frame_sz);
            mem_put_le32(header_drop + 4, 0);
            mem_put_le32(header_drop + 8, 0);
            fwrite(header_drop, 1, 12, outfile_drops);
            fwrite(buf, 1, frame_sz, outfile_drops);
        }

        vpx_usec_timer_start(&timer);

        if (vpx_codec_decode(&decoder, buf, frame_sz, NULL, 0))
        {
            const char *detail = vpx_codec_error_detail(&decoder);
            tprintf(printVar, "Failed to decode frame: %s\n",
                vpx_codec_error(&decoder));

            if (detail)
                tprintf(printVar, "  Additional information: %s\n", detail);

            goto fail;
        }

        vpx_usec_timer_mark(&timer);
        dx_time += vpx_usec_timer_elapsed(&timer);

        ++frame_in;

        if ((img = vpx_codec_get_frame(&decoder, &iter)))
            ++frame_out;

        if (progress)
        {
        }

        if (!noblit)
        {
            if (img)
            {
                unsigned int y;
                char out_fn[PATH_MAX];
                uint8_t *buf;

                if (!single_file)
                {
                    size_t len = sizeof(out_fn) - 1;
                    out_fn[len] = '\0';
                    generate_filename(outfile_pattern, out_fn, len - 1,
                                      img->d_w, img->d_h, frame_in);
                    out = out_open(out_fn, do_md5);
                }
                else if (use_y4m)
                    out_put(out, (unsigned char *)"FRAME\n", 6, do_md5);

                if (!use_y4m)
                    ivf_write_frame_header((FILE *)out, 0, img->d_h * img->d_w);

                buf = img->planes[VPX_PLANE_Y];

                for (y = 0; y < img->d_h; y++)
                {
                    out_put(out, buf, img->d_w, do_md5);
                    buf += img->stride[VPX_PLANE_Y];
                }

                buf = img->planes[flipuv?VPX_PLANE_V:VPX_PLANE_U];

                for (y = 0; y < (1 + img->d_h) / 2; y++)
                {
                    out_put(out, buf, (1 + img->d_w) / 2, do_md5);
                    buf += img->stride[VPX_PLANE_U];
                }

                buf = img->planes[flipuv?VPX_PLANE_U:VPX_PLANE_V];

                for (y = 0; y < (1 + img->d_h) / 2; y++)
                {
                    out_put(out, buf, (1 + img->d_w) / 2, do_md5);
                    buf += img->stride[VPX_PLANE_V];
                }

                if (!single_file)
                    out_close(out, out_fn, do_md5);
            }
        }

        if (stop_after && frame_in >= stop_after)
            break;
    }

    if (summary || progress)
    {
        show_progress(frame_in, frame_out, dx_time);
        fprintf(stderr, "\n");
    }

fail:

    if (vpx_codec_destroy(&decoder))
    {
        tprintf(printVar, "Failed to destroy decoder: %s\n",
            vpx_codec_error(&decoder));
        fclose(infile);

        return -1;
    }

    if (single_file && !noblit)
        out_close(out, outfile, do_md5);

    if (input.nestegg_ctx)
        nestegg_destroy(input.nestegg_ctx);

    if (input.kind != WEBM_FILE)
        free(buf);

    fclose(infile);

    if (outputParDropEnc)
    {
        if (!fseek(outfile_drops, 0, SEEK_SET))
            write_ivf_file_header(outfile_drops, &enc_cfg, VP8_FOURCC,
            frame_out);

        fclose(outfile_drops);
    }

    return 0;
}
int vpxt_decompress_resize(const char *inputchar,
                           const char *outputchar,
                           std::string DecFormat,
                           int threads)
{
    int use_y4m = 1;
    vpxt_lower_case_string(DecFormat);

    if (DecFormat.compare("ivf") == 0)
        use_y4m = 2;

    vpx_codec_ctx_t         decoder;
    const char             *fn = inputchar;
    int                     i;
    uint8_t                *buf = NULL;
    size_t                  buf_sz = 0, buf_alloc_sz = 0;
    FILE                   *infile;
    int                     frame_in = 0, frame_out = 0, flipuv = 0, noblit = 0;
    int                     do_md5 = 0, progress = 0;
    int                     stop_after = 0, postproc = 0, summary = 0, quiet =1;
    vpx_codec_iface_t      *iface = NULL;
    unsigned int            fourcc;
    unsigned long           dx_time = 0;
    const char             *outfile_pattern = 0;
    char                    outfile[PATH_MAX];
    int                     single_file;
    unsigned int            width;
    unsigned int            height;
    unsigned int            fps_den;
    unsigned int            fps_num;
    void                   *out = NULL;
    vpx_codec_dec_cfg_t     cfg = {0};
#if CONFIG_VP8_DECODER
    vp8_postproc_cfg_t      vp8_pp_cfg = {0};
    int                     vp8_dbg_color_ref_frame = 0;
    int                     vp8_dbg_color_mb_modes = 0;
    int                     vp8_dbg_color_b_modes = 0;
    int                     vp8_dbg_display_mv = 0;
#endif
    struct input_ctx        input;

    input.chunk = 0;
    input.chunks = 0;
    input.infile = NULL;
    input.kind = RAW_FILE;
    input.nestegg_ctx = 0;
    input.pkt = 0;
    input.video_track = 0;

    int CharCount = 0;

    /* Open file */
    infile = strcmp(fn, "-") ? fopen(fn, "rb") : set_binary_mode(stdin);
    if (!infile){
        tprintf(PRINT_STD, "Failed to open input file: %s", fn);
        return -1;
    }

    input.infile = infile;
    if (file_is_ivf_dec(infile, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = IVF_FILE;
    else if (file_is_webm(&input, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = WEBM_FILE;
    else if (file_is_raw(infile, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = RAW_FILE;
    else
    {
        fprintf(stderr, "Unrecognized input file type.\n");
        return EXIT_FAILURE;
    }

    std::string compformat;
    std::string decformat;

    if (input.kind == IVF_FILE)
        compformat = "IVF";

    if (input.kind == WEBM_FILE)
        compformat = "Webm";

    if (input.kind == RAW_FILE)
        compformat = "Raw";

    if (use_y4m == 0)
        decformat = "Raw";

    if (use_y4m == 1)
        decformat = "Y4M";

    if (use_y4m == 2)
        decformat = "IVF";

    tprintf(PRINT_STD, "\nAPI - Decompressing VP8 %s File to Raw %s File: \n",
        compformat.c_str(), decformat.c_str());

    outfile_pattern = outfile_pattern ? outfile_pattern : "-";
    single_file = 1;
    {
        const char *p = outfile_pattern;

        do
        {
            p = strchr(p, '%');

            if (p && p[1] >= '1' && p[1] <= '9')
            {
                single_file = 0;
                break;
            }

            if (p)
                p++;
        }
        while (p);
    }

    if (single_file && !noblit)
    {
        generate_filename(outfile_pattern, outfile, sizeof(outfile) - 1,
                          width, height, 0);
        strncpy(outfile, outputchar, PATH_MAX);
        out = out_open(outfile, do_md5);
    }

    if (use_y4m && !noblit)
    {
        char buffer[128];

        if (!single_file)
        {
            fprintf(stderr, "YUV4MPEG2 not supported with output patterns,"
                    " try --i420 or --yv12.\n");
            return EXIT_FAILURE;
        }

        if (input.kind == WEBM_FILE)
            if (webm_guess_framerate(&input, &fps_den, &fps_num))
            {
                fprintf(stderr, "Failed to guess framerate -- error parsing "
                        "webm file?\n");
                return EXIT_FAILURE;
            }

        /*Note: We can't output an aspect ratio here because IVF doesn't
        store one, and neither does VP8.
        That will have to wait until these tools support WebM natively.*/
        sprintf(buffer, "YUV4MPEG2 C%s W%u H%u F%u:%u I%c\n",
                "420jpeg", width, height, fps_num, fps_den, 'p');
        out_put(out, (unsigned char *)buffer, strlen(buffer), do_md5);
    }

    /* Try to determine the codec from the fourcc. */
    for (i = 0; i < sizeof(ifaces) / sizeof(ifaces[0]); i++)
        if ((fourcc & ifaces[i].fourcc_mask) == ifaces[i].fourcc)
        {
            vpx_codec_iface_t  *ivf_iface = ifaces[i].iface;

            if (iface && iface != ivf_iface)
                tprintf(PRINT_STD, "Notice -- IVF header indicates codec: %s\n",
                        ifaces[i].name);
            else
                iface = ivf_iface;

            break;
        }

    unsigned int FrameSize = (width * height * 3) / 2;
    uint64_t TimeStamp = 0;

    cfg.threads = threads;

    if (vpx_codec_dec_init(&decoder, iface ? iface :  ifaces[0].iface, &cfg,
        postproc ? VPX_CODEC_USE_POSTPROC : 0))
    {
        tprintf(PRINT_STD, "Failed to initialize decoder: %s\n",
            vpx_codec_error(&decoder));
        fclose(infile);

        return -1;
    }

    /////////////////// Setup yv12_buffer_dest to Resize if nessicary //////////
    initialize_scaler();
    YV12_BUFFER_CONFIG yv12_buffer_source;
    memset(&yv12_buffer_source, 0, sizeof(yv12_buffer_source));
    YV12_BUFFER_CONFIG yv12_buffer_dest;
    memset(&yv12_buffer_dest, 0, sizeof(yv12_buffer_dest));
    vp8_yv12_alloc_frame_buffer(&yv12_buffer_dest, width, height,
        VP8BORDERINPIXELS);
    ////////////////////////////////////////////////////////////////////////////

    /* Decode file */

    uint64_t timestamp = 0;
    while (!read_frame_dec(&input, &buf, &buf_sz, &buf_alloc_sz, &timestamp))
    {
        vpx_codec_iter_t  iter = NULL;
        vpx_image_t    *img;
        struct vpx_usec_timer timer;
        int resized = 0;

        vpx_usec_timer_start(&timer);

        if (vpx_codec_decode(&decoder, buf, buf_sz, NULL, 0))
        {
            const char *detail = vpx_codec_error_detail(&decoder);
            tprintf(PRINT_STD, "Failed to decode frame: %s\n",
                vpx_codec_error(&decoder));

            if (detail)
                tprintf(PRINT_STD, "  Additional information: %s\n", detail);

            goto fail;
        }

        vpx_usec_timer_mark(&timer);
        dx_time += vpx_usec_timer_elapsed(&timer);

        ++frame_in;

        if ((img = vpx_codec_get_frame(&decoder, &iter)))
        {
            ///////////////////// Inflate Dec Img if needed ////////////////////
            image2yuvconfig(img, &yv12_buffer_source);

            // if frame not correct size resize it
            if (img->d_w != width || img->d_h != height)
            {
                resized = 1;
                int GCDInt1 = vpxt_gcd(img->d_w, width);
                int GCDInt2 = vpxt_gcd(img->d_h, height);

                vp8_yv12_scale_or_center(&yv12_buffer_source, &yv12_buffer_dest,
                    width, height, 0, (width / GCDInt1), (img->d_w / GCDInt1),
                    (height / GCDInt2), (img->d_h / GCDInt2));

                yuvconfig2image(img, &yv12_buffer_dest, 0);
            }

            ////////////////////////////////////////////////////////////////////
            ++frame_out;
        }

        if (progress)
        {
        }

        if (!noblit)
        {
            if (img)
            {
                unsigned int y;
                char out_fn[PATH_MAX];
                uint8_t *buf;

                if (!single_file)
                {
                    size_t len = sizeof(out_fn) - 1;
                    out_fn[len] = '\0';
                    generate_filename(outfile_pattern, out_fn, len - 1,
                                      img->d_w, img->d_h, frame_in);
                    out = out_open(out_fn, do_md5);
                }
                else if (use_y4m)
                    out_put(out, (unsigned char *)"FRAME\n", 6, do_md5);

                if (!use_y4m)
                    ivf_write_frame_header((FILE *)out, 0, img->d_h * img->d_w);

                if (CharCount == 79)
                {
                    tprintf(PRINT_STD, "\n");
                    CharCount = 0;
                }

                CharCount++;

                if (resized == 1)
                    tprintf(PRINT_STD, "*");
                else
                    tprintf(PRINT_STD, ".");

                buf = img->planes[VPX_PLANE_Y];

                for (y = 0; y < img->d_h; y++)
                {
                    out_put(out, buf, img->d_w, do_md5);
                    buf += img->stride[VPX_PLANE_Y];
                }

                buf = img->planes[flipuv?VPX_PLANE_V:VPX_PLANE_U];

                for (y = 0; y < (1 + img->d_h) / 2; y++)
                {
                    out_put(out, buf, (1 + img->d_w) / 2, do_md5);
                    buf += img->stride[VPX_PLANE_U];
                }

                buf = img->planes[flipuv?VPX_PLANE_U:VPX_PLANE_V];

                for (y = 0; y < (1 + img->d_h) / 2; y++)
                {
                    out_put(out, buf, (1 + img->d_w) / 2, do_md5);
                    buf += img->stride[VPX_PLANE_V];
                }

                if (!single_file)
                    out_close(out, out_fn, do_md5);
            }
        }

        if (stop_after && frame_in >= stop_after)
            break;
    }

    if (summary || progress)
    {
        show_progress(frame_in, frame_out, dx_time);
        fprintf(stderr, "\n");
    }

fail:

    if (vpx_codec_destroy(&decoder))
    {
        tprintf(PRINT_STD, "Failed to destroy decoder: %s\n",
            vpx_codec_error(&decoder));
        fclose(infile);

        return -1;
    }

    vp8_yv12_de_alloc_frame_buffer(&yv12_buffer_dest);

    if (single_file && !noblit)
        out_close(out, outfile, do_md5);

    if (input.nestegg_ctx)
        nestegg_destroy(input.nestegg_ctx);

    if (input.kind != WEBM_FILE)
        free(buf);

    fclose(infile);

    return 0;
}
int vpxt_decompress_to_raw(const char *inputchar,
                           const char *outputchar,
                           int threads)
{
    int                     use_y4m = 1;
    vpx_codec_ctx_t         decoder;
    const char             *fn = inputchar;
    int                     i;
    uint8_t                *buf = NULL;
    size_t                  buf_sz = 0, buf_alloc_sz = 0;
    FILE                   *infile;
    int                     frame_in = 0, frame_out = 0, flipuv = 0, noblit = 0;
    int                     do_md5 = 0, progress = 0;
    int                     stop_after = 0, postproc = 0, summary = 0, quiet =1;
    vpx_codec_iface_t      *iface = NULL;
    unsigned int            fourcc;
    unsigned long           dx_time = 0;
    const char             *outfile_pattern = 0;
    char                    outfile[PATH_MAX];
    int                     single_file;
    unsigned int            width;
    unsigned int            height;
    unsigned int            fps_den;
    unsigned int            fps_num;
    void                   *out = NULL;
    vpx_codec_dec_cfg_t     cfg = {0};
#if CONFIG_VP8_DECODER
    vp8_postproc_cfg_t      vp8_pp_cfg = {0};
    int                     vp8_dbg_color_ref_frame = 0;
    int                     vp8_dbg_color_mb_modes = 0;
    int                     vp8_dbg_color_b_modes = 0;
    int                     vp8_dbg_display_mv = 0;
#endif
    struct input_ctx        input;

    input.chunk = 0;
    input.chunks = 0;
    input.infile = NULL;
    input.kind = RAW_FILE;
    input.nestegg_ctx = 0;
    input.pkt = 0;
    input.video_track = 0;

    int CharCount = 0;

    /* Open file */
    infile = strcmp(fn, "-") ? fopen(fn, "rb") : set_binary_mode(stdin);
    if (!infile){
        tprintf(PRINT_BTH, "Failed to open input file: %s", fn);
        return -1;
    }

    input.infile = infile;
    if (file_is_ivf_dec(infile, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = IVF_FILE;
    else if (file_is_webm(&input, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = WEBM_FILE;
    else if (file_is_raw(infile, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = RAW_FILE;
    else
    {
        fprintf(stderr, "Unrecognized input file type.\n");
        return EXIT_FAILURE;
    }

    std::string compformat;
    std::string decformat;

    if (input.kind == IVF_FILE)
        compformat = "IVF";

    if (input.kind == WEBM_FILE)
        compformat = "Webm";

    if (input.kind == RAW_FILE)
        compformat = "Raw";

    if (use_y4m == 0)
        decformat = "Raw";

    if (use_y4m == 1)
        decformat = "Y4M";

    if (use_y4m == 2)
        decformat = "IVF";

    tprintf(PRINT_BTH, "\nAPI - Decompressing VP8 %s File to Raw File: \n",
        compformat.c_str());

    outfile_pattern = outfile_pattern ? outfile_pattern : "-";
    single_file = 1;
    {
        const char *p = outfile_pattern;

        do
        {
            p = strchr(p, '%');

            if (p && p[1] >= '1' && p[1] <= '9')
            {
                single_file = 0;
                break;
            }

            if (p)
                p++;
        }
        while (p);
    }

    if (single_file && !noblit)
    {
        generate_filename(outfile_pattern, outfile, sizeof(outfile) - 1,
                          width, height, 0);
        strncpy(outfile, outputchar, PATH_MAX);
        // outfile = outputchar;
        out = out_open(outfile, do_md5);
    }

    if (use_y4m && !noblit)
    {
        char buffer[128];

        if (!single_file)
        {
            fprintf(stderr, "YUV4MPEG2 not supported with output patterns,"
                    " try --i420 or --yv12.\n");
            return EXIT_FAILURE;
        }

        if (input.kind == WEBM_FILE)
            if (webm_guess_framerate(&input, &fps_den, &fps_num))
            {
                fprintf(stderr, "Failed to guess framerate -- error parsing "
                        "webm file?\n");
                return EXIT_FAILURE;
            }

        /*Note: We can't output an aspect ratio here because IVF doesn't
        store one, and neither does VP8.
        That will have to wait until these tools support WebM natively.*/
        sprintf(buffer, "YUV4MPEG2 C%s W%u H%u F%u:%u I%c\n",
                "420jpeg", width, height, fps_num, fps_den, 'p');
        out_put(out, (unsigned char *)buffer, strlen(buffer), do_md5);
    }

    /* Try to determine the codec from the fourcc. */
    for (i = 0; i < sizeof(ifaces) / sizeof(ifaces[0]); i++)
        if ((fourcc & ifaces[i].fourcc_mask) == ifaces[i].fourcc)
        {
            vpx_codec_iface_t  *ivf_iface = ifaces[i].iface;

            if (iface && iface != ivf_iface)
                tprintf(PRINT_STD, "Notice -- IVF header indicates codec: %s\n",
                        ifaces[i].name);
            else
                iface = ivf_iface;

            break;
        }

    unsigned int FrameSize = (width * height * 3) / 2;
    uint64_t TimeStamp = 0;

    cfg.threads = threads;

    if (vpx_codec_dec_init(&decoder, iface ? iface :  ifaces[0].iface, &cfg,
        postproc ? VPX_CODEC_USE_POSTPROC : 0))
    {
        tprintf(PRINT_STD, "Failed to initialize decoder: %s\n",
            vpx_codec_error(&decoder));
        fclose(infile);

        return -1;
    }

    /* Decode file */
    uint64_t timestamp = 0;
    while (!read_frame_dec(&input, &buf, &buf_sz, &buf_alloc_sz, &timestamp))
    {
        vpx_codec_iter_t  iter = NULL;
        vpx_image_t    *img;
        struct vpx_usec_timer timer;

        vpx_usec_timer_start(&timer);

        if (vpx_codec_decode(&decoder, buf, buf_sz, NULL, 0))
        {
            const char *detail = vpx_codec_error_detail(&decoder);
            tprintf(PRINT_STD, "Failed to decode frame: %s\n",
                vpx_codec_error(&decoder));

            if (detail)
                tprintf(PRINT_STD, "  Additional information: %s\n", detail);

            goto fail;
        }

        vpx_usec_timer_mark(&timer);
        dx_time += vpx_usec_timer_elapsed(&timer);

        ++frame_in;

        if ((img = vpx_codec_get_frame(&decoder, &iter)))
            ++frame_out;

        if (progress)
        {
        }

        if (!noblit)
        {
            if (img)
            {
                unsigned int y;
                char out_fn[PATH_MAX];
                uint8_t *buf;

                if (!single_file)
                {
                    size_t len = sizeof(out_fn) - 1;
                    out_fn[len] = '\0';
                    generate_filename(outfile_pattern, out_fn, len - 1,
                                      img->d_w, img->d_h, frame_in);
                    out = out_open(out_fn, do_md5);
                }
                else if (use_y4m)
                    out_put(out, (unsigned char *)"FRAME\n", 6, do_md5);

                if (CharCount == 79)
                {
                    tprintf(PRINT_BTH, "\n");
                    CharCount = 0;
                }

                CharCount++;
                tprintf(PRINT_BTH, ".");

                buf = img->planes[VPX_PLANE_Y];

                for (y = 0; y < img->d_h; y++)
                {
                    out_put(out, buf, img->d_w, do_md5);
                    buf += img->stride[VPX_PLANE_Y];
                }

                buf = img->planes[flipuv?VPX_PLANE_V:VPX_PLANE_U];

                for (y = 0; y < (1 + img->d_h) / 2; y++)
                {
                    out_put(out, buf, (1 + img->d_w) / 2, do_md5);
                    buf += img->stride[VPX_PLANE_U];
                }

                buf = img->planes[flipuv?VPX_PLANE_U:VPX_PLANE_V];

                for (y = 0; y < (1 + img->d_h) / 2; y++)
                {
                    out_put(out, buf, (1 + img->d_w) / 2, do_md5);
                    buf += img->stride[VPX_PLANE_V];
                }

                if (!single_file)
                    out_close(out, out_fn, do_md5);
            }
        }

        if (stop_after && frame_in >= stop_after)
            break;
    }

    if (summary || progress)
    {
        show_progress(frame_in, frame_out, dx_time);
        fprintf(stderr, "\n");
    }

fail:

    if (vpx_codec_destroy(&decoder))
    {
        tprintf(PRINT_STD, "Failed to destroy decoder: %s\n",
            vpx_codec_error(&decoder));
        fclose(infile);

        return -1;
    }

    if (single_file && !noblit)
        out_close(out, outfile, do_md5);

    if (input.nestegg_ctx)
        nestegg_destroy(input.nestegg_ctx);

    if (input.kind != WEBM_FILE)
        free(buf);

    fclose(infile);

    return 0;
}
int vpxt_decompress_to_raw_no_error_output(const char *inputchar,
                                           const char *outputchar,
                                           int threads)
{
    int                     use_y4m = 1;
    vpx_codec_ctx_t         decoder;
    const char             *fn = inputchar;
    int                     i;
    uint8_t                *buf = NULL;
    size_t                  buf_sz = 0, buf_alloc_sz = 0;
    FILE                   *infile;
    int                     frame_in = 0, frame_out = 0, flipuv = 0, noblit = 0;
    int                     do_md5 = 0, progress = 0;
    int                     stop_after = 0, postproc = 0, summary = 0, quiet =1;
    vpx_codec_iface_t      *iface = NULL;
    unsigned int            fourcc;
    unsigned long           dx_time = 0;
    const char             *outfile_pattern = 0;
    char                    outfile[PATH_MAX];
    int                     single_file;
    unsigned int            width;
    unsigned int            height;
    unsigned int            fps_den;
    unsigned int            fps_num;
    void                   *out = NULL;
    vpx_codec_dec_cfg_t     cfg = {0};
#if CONFIG_VP8_DECODER
    vp8_postproc_cfg_t      vp8_pp_cfg = {0};
    int                     vp8_dbg_color_ref_frame = 0;
    int                     vp8_dbg_color_mb_modes = 0;
    int                     vp8_dbg_color_b_modes = 0;
    int                     vp8_dbg_display_mv = 0;
#endif
    struct input_ctx        input;

    input.chunk = 0;
    input.chunks = 0;
    input.infile = NULL;
    input.kind = RAW_FILE;
    input.nestegg_ctx = 0;
    input.pkt = 0;
    input.video_track = 0;

    int CharCount = 0;

    /* Open file */
    infile = strcmp(fn, "-") ? fopen(fn, "rb") : set_binary_mode(stdin);
    if (!infile){
        tprintf(PRINT_STD, "Failed to open input file: %s", fn);
        return -1;
    }

    input.infile = infile;
    if (file_is_ivf_dec(infile, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = IVF_FILE;
    else if (file_is_webm(&input, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = WEBM_FILE;
    else if (file_is_raw(infile, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = RAW_FILE;
    else
    {
        fprintf(stderr, "Unrecognized input file type.\n");
        return EXIT_FAILURE;
    }

    std::string compformat;
    std::string decformat;

    if (input.kind == IVF_FILE)
        compformat = "IVF";

    if (input.kind == WEBM_FILE)
        compformat = "Webm";

    if (input.kind == RAW_FILE)
        compformat = "Raw";

    if (use_y4m == 0)
        decformat = "Raw";

    if (use_y4m == 1)
        decformat = "Y4M";

    if (use_y4m == 2)
        decformat = "IVF";

    tprintf(PRINT_STD, "\nAPI - Decompressing VP8 %s File to Raw File: \n",
        compformat.c_str());

    outfile_pattern = outfile_pattern ? outfile_pattern : "-";
    single_file = 1;
    {
        const char *p = outfile_pattern;

        do
        {
            p = strchr(p, '%');

            if (p && p[1] >= '1' && p[1] <= '9')
            {
                single_file = 0;
                break;
            }

            if (p)
                p++;
        }
        while (p);
    }

    if (single_file && !noblit)
    {
        generate_filename(outfile_pattern, outfile, sizeof(outfile) - 1,
                          width, height, 0);
        strncpy(outfile, outputchar, PATH_MAX);
        out = out_open(outfile, do_md5);
    }

    if (use_y4m && !noblit)
    {
        char buffer[128];

        if (!single_file)
        {
            fprintf(stderr, "YUV4MPEG2 not supported with output patterns,"
                    " try --i420 or --yv12.\n");
            return EXIT_FAILURE;
        }

        if (input.kind == WEBM_FILE)
            if (webm_guess_framerate(&input, &fps_den, &fps_num))
            {
                fprintf(stderr, "Failed to guess framerate -- error parsing "
                        "webm file?\n");
                return EXIT_FAILURE;
            }

        /*Note: We can't output an aspect ratio here because IVF doesn't
        store one, and neither does VP8.
        That will have to wait until these tools support WebM natively.*/
        sprintf(buffer, "YUV4MPEG2 C%s W%u H%u F%u:%u I%c\n",
                "420jpeg", width, height, fps_num, fps_den, 'p');
        out_put(out, (unsigned char *)buffer, strlen(buffer), do_md5);
    }

    /* Try to determine the codec from the fourcc. */
    for (i = 0; i < sizeof(ifaces) / sizeof(ifaces[0]); i++)
        if ((fourcc & ifaces[i].fourcc_mask) == ifaces[i].fourcc)
        {
            vpx_codec_iface_t  *ivf_iface = ifaces[i].iface;

            if (iface && iface != ivf_iface)
                tprintf(PRINT_STD, "Notice -- IVF header indicates codec: %s\n",
                        ifaces[i].name);
            else
                iface = ivf_iface;

            break;
        }

    unsigned int FrameSize = (width * height * 3) / 2;
    uint64_t TimeStamp = 0;

    cfg.threads = threads;

    if (vpx_codec_dec_init(&decoder, iface ? iface :  ifaces[0].iface, &cfg,
        postproc ? VPX_CODEC_USE_POSTPROC : 0))
    {
        tprintf(PRINT_STD, "Failed to initialize decoder: %s\n",
            vpx_codec_error(&decoder));
        fclose(infile);

        return -1;
    }

    /* Decode file */
    uint64_t timestamp = 0;
    while (!read_frame_dec(&input, &buf, &buf_sz, &buf_alloc_sz, &timestamp))
    {
        vpx_codec_iter_t  iter = NULL;
        vpx_image_t    *img;
        struct vpx_usec_timer timer;

        vpx_usec_timer_start(&timer);

        if (vpx_codec_decode(&decoder, buf, buf_sz, NULL, 0))
        {
            const char *detail = vpx_codec_error_detail(&decoder);
            tprintf(PRINT_STD, "Failed to decode frame: %s\n",
                vpx_codec_error(&decoder));

            if (detail)
                tprintf(PRINT_STD, "  Additional information: %s\n", detail);

            goto fail;
        }

        vpx_usec_timer_mark(&timer);
        dx_time += vpx_usec_timer_elapsed(&timer);

        ++frame_in;

        if ((img = vpx_codec_get_frame(&decoder, &iter)))
            ++frame_out;

        if (progress)
        {
        }

        if (!noblit)
        {
            if (img)
            {
                unsigned int y;
                char out_fn[PATH_MAX];
                uint8_t *buf;

                if (!single_file)
                {
                    size_t len = sizeof(out_fn) - 1;
                    out_fn[len] = '\0';
                    generate_filename(outfile_pattern, out_fn, len - 1,
                                      img->d_w, img->d_h, frame_in);
                    out = out_open(out_fn, do_md5);
                }
                else if (use_y4m)
                    out_put(out, (unsigned char *)"FRAME\n", 6, do_md5);

                if (!use_y4m)
                    ivf_write_frame_header((FILE *)out, 0, img->d_h * img->d_w);

                if (CharCount == 79)
                {
                    tprintf(PRINT_STD, "\n");
                    CharCount = 0;
                }

                CharCount++;
                tprintf(PRINT_STD, ".");

                buf = img->planes[VPX_PLANE_Y];

                for (y = 0; y < img->d_h; y++)
                {
                    out_put(out, buf, img->d_w, do_md5);
                    buf += img->stride[VPX_PLANE_Y];
                }

                buf = img->planes[flipuv?VPX_PLANE_V:VPX_PLANE_U];

                for (y = 0; y < (1 + img->d_h) / 2; y++)
                {
                    out_put(out, buf, (1 + img->d_w) / 2, do_md5);
                    buf += img->stride[VPX_PLANE_U];
                }

                buf = img->planes[flipuv?VPX_PLANE_U:VPX_PLANE_V];

                for (y = 0; y < (1 + img->d_h) / 2; y++)
                {
                    out_put(out, buf, (1 + img->d_w) / 2, do_md5);
                    buf += img->stride[VPX_PLANE_V];
                }

                if (!single_file)
                    out_close(out, out_fn, do_md5);
            }
        }

        if (stop_after && frame_in >= stop_after)
            break;
    }

    if (summary || progress)
    {
        show_progress(frame_in, frame_out, dx_time);
        fprintf(stderr, "\n");
    }

fail:

    if (vpx_codec_destroy(&decoder))
    {
        tprintf(PRINT_STD, "Failed to destroy decoder: %s\n",
            vpx_codec_error(&decoder));
        fclose(infile);

        return -1;
    }

    if (single_file && !noblit)
        out_close(out, outfile, do_md5);

    if (input.nestegg_ctx)
        nestegg_destroy(input.nestegg_ctx);

    if (input.kind != WEBM_FILE)
        free(buf);

    fclose(infile);

    return 0;
}
int vpxt_decompress_no_output(const char *inputchar,
                              const char *outputchar,
                              std::string DecFormat,
                              int threads)
{
    int use_y4m = 1;
    vpxt_lower_case_string(DecFormat);

    if (DecFormat.compare("ivf") == 0)
        use_y4m = 2;

    vpx_codec_ctx_t         decoder;
    const char             *fn = inputchar;
    int                     i;
    uint8_t                *buf = NULL;
    size_t                  buf_sz = 0, buf_alloc_sz = 0;
    FILE                   *infile;
    int                     frame_in = 0, frame_out = 0, flipuv = 0, noblit = 0;
    int                     do_md5 = 0, progress = 0;
    int                     stop_after = 0, postproc = 0, summary = 0, quiet =1;
    vpx_codec_iface_t      *iface = NULL;
    unsigned int            fourcc;
    unsigned long           dx_time = 0;
    const char             *outfile_pattern = 0;
    char                    outfile[PATH_MAX];
    int                     single_file;
    unsigned int            width;
    unsigned int            height;
    unsigned int            fps_den;
    unsigned int            fps_num;
    void                   *out = NULL;
    vpx_codec_dec_cfg_t     cfg = {0};
#if CONFIG_VP8_DECODER
    vp8_postproc_cfg_t      vp8_pp_cfg = {0};
    int                     vp8_dbg_color_ref_frame = 0;
    int                     vp8_dbg_color_mb_modes = 0;
    int                     vp8_dbg_color_b_modes = 0;
    int                     vp8_dbg_display_mv = 0;
#endif
    struct input_ctx        input;

    input.chunk = 0;
    input.chunks = 0;
    input.infile = NULL;
    input.kind = RAW_FILE;
    input.nestegg_ctx = 0;
    input.pkt = 0;
    input.video_track = 0;

    int CharCount = 0;

    /* Open file */
    infile = strcmp(fn, "-") ? fopen(fn, "rb") : set_binary_mode(stdin);
    if (!infile){
        tprintf(PRINT_STD, "Failed to open input file: %s", fn);
        return -1;
    }

    input.infile = infile;
    if (file_is_ivf_dec(infile, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = IVF_FILE;
    else if (file_is_webm(&input, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = WEBM_FILE;
    else if (file_is_raw(infile, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = RAW_FILE;
    else
    {
        fprintf(stderr, "Unrecognized input file type.\n");
        return EXIT_FAILURE;
    }

    std::string compformat;
    std::string decformat;

    if (input.kind == IVF_FILE)
        compformat = "IVF";

    if (input.kind == WEBM_FILE)
        compformat = "Webm";

    if (input.kind == RAW_FILE)
        compformat = "Raw";

    if (use_y4m == 0)
        decformat = "Raw";

    if (use_y4m == 1)
        decformat = "Y4M";

    if (use_y4m == 2)
        decformat = "IVF";

    tprintf(PRINT_STD, "\nAPI - Decompressing VP8 %s File to Raw %s File: \n",
        compformat.c_str(), decformat.c_str());

    outfile_pattern = outfile_pattern ? outfile_pattern : "-";
    single_file = 1;
    {
        const char *p = outfile_pattern;

        do
        {
            p = strchr(p, '%');

            if (p && p[1] >= '1' && p[1] <= '9')
            {
                single_file = 0;
                break;
            }

            if (p)
                p++;
        }
        while (p);
    }

    if (single_file && !noblit)
    {
        generate_filename(outfile_pattern, outfile, sizeof(outfile) - 1,
                          width, height, 0);
        strncpy(outfile, outputchar, PATH_MAX);
        out = out_open(outfile, do_md5);
    }

    if (use_y4m && !noblit)
    {
        char buffer[128];

        if (!single_file)
        {
            fprintf(stderr, "YUV4MPEG2 not supported with output patterns,"
                    " try --i420 or --yv12.\n");
            return EXIT_FAILURE;
        }

        if (input.kind == WEBM_FILE)
            if (webm_guess_framerate(&input, &fps_den, &fps_num))
            {
                fprintf(stderr, "Failed to guess framerate -- error parsing "
                        "webm file?\n");
                return EXIT_FAILURE;
            }

        /*Note: We can't output an aspect ratio here because IVF doesn't
        store one, and neither does VP8.
        That will have to wait until these tools support WebM natively.*/
        sprintf(buffer, "YUV4MPEG2 C%s W%u H%u F%u:%u I%c\n",
                "420jpeg", width, height, fps_num, fps_den, 'p');
        out_put(out, (unsigned char *)buffer, strlen(buffer), do_md5);
    }

    /* Try to determine the codec from the fourcc. */
    for (i = 0; i < sizeof(ifaces) / sizeof(ifaces[0]); i++)
        if ((fourcc & ifaces[i].fourcc_mask) == ifaces[i].fourcc)
        {
            vpx_codec_iface_t  *ivf_iface = ifaces[i].iface;

            if (iface && iface != ivf_iface)
                tprintf(PRINT_STD, "Notice -- IVF header indicates codec: %s\n",
                        ifaces[i].name);
            else
                iface = ivf_iface;

            break;
        }

    unsigned int FrameSize = (width * height * 3) / 2;
    uint64_t TimeStamp = 0;

    cfg.threads = threads;

    if (vpx_codec_dec_init(&decoder, iface ? iface :  ifaces[0].iface, &cfg,
        postproc ? VPX_CODEC_USE_POSTPROC : 0))
    {
        tprintf(PRINT_STD, "Failed to initialize decoder: %s\n",
            vpx_codec_error(&decoder));
        fclose(infile);

        return -1;
    }

    /* Decode file */
    uint64_t timestamp = 0;
    while (!read_frame_dec(&input, &buf, &buf_sz, &buf_alloc_sz, &timestamp))
    {
        vpx_codec_iter_t  iter = NULL;
        vpx_image_t    *img;
        struct vpx_usec_timer timer;

        vpx_usec_timer_start(&timer);

        if (vpx_codec_decode(&decoder, buf, buf_sz, NULL, 0))
        {
            const char *detail = vpx_codec_error_detail(&decoder);
            tprintf(PRINT_STD, "Failed to decode frame: %s\n",
                vpx_codec_error(&decoder));

            if (detail)
                tprintf(PRINT_STD, "  Additional information: %s\n", detail);

            goto fail;
        }

        vpx_usec_timer_mark(&timer);
        dx_time += vpx_usec_timer_elapsed(&timer);

        ++frame_in;

        if ((img = vpx_codec_get_frame(&decoder, &iter)))
            ++frame_out;

        if (progress)
        {
        }

        if (!noblit)
        {
            if (img)
            {
                unsigned int y;
                char out_fn[PATH_MAX];
                uint8_t *buf;

                if (!single_file)
                {
                    size_t len = sizeof(out_fn) - 1;
                    out_fn[len] = '\0';
                    generate_filename(outfile_pattern, out_fn, len - 1,
                                      img->d_w, img->d_h, frame_in);
                    out = out_open(out_fn, do_md5);
                }
                else if (use_y4m)
                    out_put(out, (unsigned char *)"FRAME\n", 6, do_md5);

                if (!use_y4m)
                    ivf_write_frame_header((FILE *)out, 0, img->d_h * img->d_w);

                if (CharCount == 79)
                {
                    tprintf(PRINT_STD, "\n");
                    CharCount = 0;
                }

                CharCount++;
                tprintf(PRINT_STD, ".");

                buf = img->planes[VPX_PLANE_Y];

                for (y = 0; y < img->d_h; y++)
                {
                    out_put(out, buf, img->d_w, do_md5);
                    buf += img->stride[VPX_PLANE_Y];
                }

                buf = img->planes[flipuv?VPX_PLANE_V:VPX_PLANE_U];

                for (y = 0; y < (1 + img->d_h) / 2; y++)
                {
                    out_put(out, buf, (1 + img->d_w) / 2, do_md5);
                    buf += img->stride[VPX_PLANE_U];
                }

                buf = img->planes[flipuv?VPX_PLANE_U:VPX_PLANE_V];

                for (y = 0; y < (1 + img->d_h) / 2; y++)
                {
                    out_put(out, buf, (1 + img->d_w) / 2, do_md5);
                    buf += img->stride[VPX_PLANE_V];
                }

                if (!single_file)
                    out_close(out, out_fn, do_md5);
            }
        }

        if (stop_after && frame_in >= stop_after)
            break;
    }

    if (summary || progress)
    {
        show_progress(frame_in, frame_out, dx_time);
        fprintf(stderr, "\n");
    }

fail:

    if (vpx_codec_destroy(&decoder))
    {
        tprintf(PRINT_STD, "Failed to destroy decoder: %s\n",
            vpx_codec_error(&decoder));
        fclose(infile);

        return -1;
    }

    if (single_file && !noblit)
        out_close(out, outfile, do_md5);

    if (input.nestegg_ctx)
        nestegg_destroy(input.nestegg_ctx);

    if (input.kind != WEBM_FILE)
        free(buf);

    fclose(infile);

    return 0;
}
unsigned int vpxt_time_decompress(const char *inputchar,
                                  const char *outputchar,
                                  unsigned int &CPUTick,
                                  std::string DecFormat,
                                  int threads)
{
    int use_y4m = 1;
    vpxt_lower_case_string(DecFormat);
    if (DecFormat.compare("ivf") == 0)
        use_y4m = 2;

    // time Decompress is not supposed to save output that is the only
    // difference between it and vpxt_decompress_ivf_to_ivf_time_and_output
    vpx_codec_ctx_t         decoder;
    uint64_t                total_cpu_time_used = 0;
    const char             *fn = inputchar;
    int                     i;
    uint8_t                *buf = NULL;
    size_t                  buf_sz = 0, buf_alloc_sz = 0;
    FILE                   *infile;
    int                     frame_in = 0, frame_out = 0, flipuv = 0, noblit = 1;
    int                     do_md5 = 0, progress = 0;
    int                     stop_after = 0, postproc = 0, summary = 0, quiet =1;
    vpx_codec_iface_t      *iface = NULL;
    unsigned int            fourcc;
    unsigned long           dx_time = 0;
    const char             *outfile_pattern = 0;
    char                    outfile[PATH_MAX];
    int                     single_file;
    unsigned int            width;
    unsigned int            height;
    unsigned int            fps_den;
    unsigned int            fps_num;
    void                   *out = NULL;
    vpx_codec_dec_cfg_t     cfg = {0};
#if CONFIG_VP8_DECODER
    vp8_postproc_cfg_t      vp8_pp_cfg = {0};
    int                     vp8_dbg_color_ref_frame = 0;
    int                     vp8_dbg_color_mb_modes = 0;
    int                     vp8_dbg_color_b_modes = 0;
    int                     vp8_dbg_display_mv = 0;
#endif
    struct input_ctx        input;

    input.chunk = 0;
    input.chunks = 0;
    input.infile = NULL;
    input.kind = RAW_FILE;
    input.nestegg_ctx = 0;
    input.pkt = 0;
    input.video_track = 0;

    int CharCount = 0;

    /* Open file */
    infile = strcmp(fn, "-") ? fopen(fn, "rb") : set_binary_mode(stdin);
    if (!infile){
        tprintf(PRINT_BTH, "Failed to open input file: %s", fn);
        return -1;
    }

    input.infile = infile;
    if (file_is_ivf_dec(infile, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = IVF_FILE;
    else if (file_is_webm(&input, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = WEBM_FILE;
    else if (file_is_raw(infile, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = RAW_FILE;
    else
    {
        fprintf(stderr, "Unrecognized input file type.\n");
        return EXIT_FAILURE;
    }

    std::string compformat;
    std::string decformat;

    if (input.kind == IVF_FILE)
        compformat = "IVF";

    if (input.kind == WEBM_FILE)
        compformat = "Webm";

    if (input.kind == RAW_FILE)
        compformat = "Raw";

    if (use_y4m == 0)
        decformat = "Raw";

    if (use_y4m == 1)
        decformat = "Y4M";

    if (use_y4m == 2)
        decformat = "IVF";

    tprintf(PRINT_BTH, "\nAPI - Decompressing VP8 %s \n", compformat.c_str());

    outfile_pattern = outfile_pattern ? outfile_pattern : "-";
    single_file = 1;
    {
        const char *p = outfile_pattern;

        do
        {
            p = strchr(p, '%');

            if (p && p[1] >= '1' && p[1] <= '9')
            {
                single_file = 0;
                break;
            }

            if (p)
                p++;
        }
        while (p);
    }

    if (single_file && !noblit)
    {
        generate_filename(outfile_pattern, outfile, sizeof(outfile) - 1,
                          width, height, 0);
        strncpy(outfile, outputchar, PATH_MAX);
        // outfile = outputchar;
        out = out_open(outfile, do_md5);
    }

    if (use_y4m && !noblit)
    {
        char buffer[128];

        if (!single_file)
        {
            fprintf(stderr, "YUV4MPEG2 not supported with output patterns,"
                    " try --i420 or --yv12.\n");
            return EXIT_FAILURE;
        }

        if (input.kind == WEBM_FILE)
            if (webm_guess_framerate(&input, &fps_den, &fps_num))
            {
                fprintf(stderr, "Failed to guess framerate -- error parsing "
                        "webm file?\n");
                return EXIT_FAILURE;
            }

        /*Note: We can't output an aspect ratio here because IVF doesn't
        store one, and neither does VP8.
        That will have to wait until these tools support WebM natively.*/
        sprintf(buffer, "YUV4MPEG2 C%s W%u H%u F%u:%u I%c\n",
                "420jpeg", width, height, fps_num, fps_den, 'p');
        out_put(out, (unsigned char *)buffer, strlen(buffer), do_md5);
    }

    /* Try to determine the codec from the fourcc. */
    for (i = 0; i < sizeof(ifaces) / sizeof(ifaces[0]); i++)
        if ((fourcc & ifaces[i].fourcc_mask) == ifaces[i].fourcc)
        {
            vpx_codec_iface_t  *ivf_iface = ifaces[i].iface;

            if (iface && iface != ivf_iface)
                tprintf(PRINT_STD, "Notice -- IVF header indicates codec: %s\n",
                        ifaces[i].name);
            else
                iface = ivf_iface;

            break;
        }

    unsigned int FrameSize = (width * height * 3) / 2;
    uint64_t TimeStamp = 0;

    cfg.threads = threads;

    if (vpx_codec_dec_init(&decoder, iface ? iface : ifaces[0].iface, &cfg,
        postproc ? VPX_CODEC_USE_POSTPROC : 0))
    {
        tprintf(PRINT_STD, "Failed to initialize decoder: %s\n",
            vpx_codec_error(&decoder));
        fclose(infile);

        return -1;
    }

    /* Decode file */
    uint64_t timestamp = 0;
    while (!read_frame_dec(&input, &buf, &buf_sz, &buf_alloc_sz, &timestamp))
    {
        vpx_codec_iter_t  iter = NULL;
        vpx_image_t    *img;
        struct vpx_usec_timer timer;
        uint64_t start, end;

        start = vpxt_get_cpu_tick();
        vpx_usec_timer_start(&timer);

        if (vpx_codec_decode(&decoder, buf, buf_sz, NULL, 0))
        {
            const char *detail = vpx_codec_error_detail(&decoder);
            tprintf(PRINT_STD, "Failed to decode frame: %s\n",
                vpx_codec_error(&decoder));

            if (detail)
                tprintf(PRINT_STD, "  Additional information: %s\n", detail);

            goto fail;
        }

        vpx_usec_timer_mark(&timer);
        end  = vpxt_get_cpu_tick();
        dx_time += vpx_usec_timer_elapsed(&timer);

        total_cpu_time_used = total_cpu_time_used + (end - start);
        ++frame_in;

        if (CharCount == 79)
        {
            tprintf(PRINT_BTH, "\n");
            CharCount = 0;
        }

        CharCount++;
        tprintf(PRINT_BTH, ".");

        if ((img = vpx_codec_get_frame(&decoder, &iter)))
            ++frame_out;

        if (progress)
        {
        }

        if (!noblit)
        {
            if (img)
            {
                unsigned int y;
                char out_fn[PATH_MAX];
                uint8_t *buf;
                const char *sfx = flipuv ? "yv12" : "i420";

                if (!single_file)
                {
                    size_t len = sizeof(out_fn) - 1;
                    out_fn[len] = '\0';
                    generate_filename(outfile_pattern, out_fn, len - 1,
                                      img->d_w, img->d_h, frame_in);
                    out = out_open(out_fn, do_md5);
                }
                else if (use_y4m)
                    out_put(out, (unsigned char *)"FRAME\n", 6, do_md5);

                if (!use_y4m)
                    ivf_write_frame_header((FILE *)out, 0, img->d_h * img->d_w);

                if (CharCount == 79)
                {
                    tprintf(PRINT_BTH, "\n");
                    CharCount = 0;
                }

                CharCount++;
                tprintf(PRINT_BTH, ".");

                buf = img->planes[VPX_PLANE_Y];

                for (y = 0; y < img->d_h; y++)
                {
                    out_put(out, buf, img->d_w, do_md5);
                    buf += img->stride[VPX_PLANE_Y];
                }

                buf = img->planes[flipuv?VPX_PLANE_V:VPX_PLANE_U];

                for (y = 0; y < (1 + img->d_h) / 2; y++)
                {
                    out_put(out, buf, (1 + img->d_w) / 2, do_md5);
                    buf += img->stride[VPX_PLANE_U];
                }

                buf = img->planes[flipuv?VPX_PLANE_U:VPX_PLANE_V];

                for (y = 0; y < (1 + img->d_h) / 2; y++)
                {
                    out_put(out, buf, (1 + img->d_w) / 2, do_md5);
                    buf += img->stride[VPX_PLANE_V];
                }

                if (!single_file)
                    out_close(out, out_fn, do_md5);
            }
        }

        if (stop_after && frame_in >= stop_after)
            break;
    }

    if (summary)
    {
        tprintf(PRINT_BTH, "\n\n Decoded %d frames in %lu us (%.2f fps)\n",
            frame_in, dx_time, (float)frame_in * 1000000.0 / (float)dx_time);
        tprintf(PRINT_BTH, " Total CPU Ticks: %i\n", total_cpu_time_used);
    }

fail:

    if (vpx_codec_destroy(&decoder))
    {
        tprintf(PRINT_STD, "Failed to destroy decoder: %s\n",
            vpx_codec_error(&decoder));
        fclose(infile);

        return -1;
    }

    if (single_file && !noblit)
        out_close(out, outfile, do_md5);

    if (input.nestegg_ctx)
        nestegg_destroy(input.nestegg_ctx);

    if (input.kind != WEBM_FILE)
        free(buf);

    fclose(infile);

    std::string FullNameMs;
    std::string FullNameCpu;

    vpxt_remove_file_extension(outputchar, FullNameMs);
    vpxt_remove_file_extension(outputchar, FullNameCpu);

    FullNameMs += "decompression_time.txt";

    std::ofstream FullNameMsFile(FullNameMs.c_str());
    FullNameMsFile << dx_time;
    FullNameMsFile.close();

    FullNameCpu += "decompression_cpu_tick.txt";

    std::ofstream FullNameCpuFile(FullNameCpu.c_str());
    FullNameCpuFile << total_cpu_time_used;
    FullNameCpuFile.close();

    CPUTick = total_cpu_time_used;
    return dx_time;
}
unsigned int vpxt_decompress_time_and_output(const char *inputchar,
                                             const char *outputchar,
                                             unsigned int &CPUTick,
                                             std::string DecFormat,
                                             int threads)
{

    int use_y4m = 1;
    vpxt_lower_case_string(DecFormat);

    if (DecFormat.compare("ivf") == 0)
        use_y4m = 2;

    vpx_codec_ctx_t         decoder;
    uint64_t                total_cpu_time_used = 0;
    const char             *fn = inputchar;
    int                     i;
    uint8_t                *buf = NULL;
    size_t                  buf_sz = 0, buf_alloc_sz = 0;
    FILE                   *infile;
    int                     frame_in = 0, frame_out = 0, flipuv = 0, noblit = 0;
    int                     do_md5 = 0, progress = 0;
    int                     stop_after = 0, postproc = 0, summary = 0, quiet =1;
    vpx_codec_iface_t      *iface = NULL;
    unsigned int            fourcc;
    unsigned long           dx_time = 0;
    const char             *outfile_pattern = 0;
    char                    outfile[PATH_MAX];
    int                     single_file;
    unsigned int            width;
    unsigned int            height;
    unsigned int            fps_den;
    unsigned int            fps_num;
    void                   *out = NULL;
    vpx_codec_dec_cfg_t     cfg = {0};
#if CONFIG_VP8_DECODER
    vp8_postproc_cfg_t      vp8_pp_cfg = {0};
    int                     vp8_dbg_color_ref_frame = 0;
    int                     vp8_dbg_color_mb_modes = 0;
    int                     vp8_dbg_color_b_modes = 0;
    int                     vp8_dbg_display_mv = 0;
#endif
    struct input_ctx        input;

    input.chunk = 0;
    input.chunks = 0;
    input.infile = NULL;
    input.kind = RAW_FILE;
    input.nestegg_ctx = 0;
    input.pkt = 0;
    input.video_track = 0;

    int CharCount = 0;

    /* Open file */
    infile = strcmp(fn, "-") ? fopen(fn, "rb") : set_binary_mode(stdin);
    if (!infile){
        tprintf(PRINT_BTH, "Failed to open input file: %s", fn);
        return -1;
    }

    input.infile = infile;
    if (file_is_ivf_dec(infile, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = IVF_FILE;
    else if (file_is_webm(&input, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = WEBM_FILE;
    else if (file_is_raw(infile, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = RAW_FILE;
    else
    {
        fprintf(stderr, "Unrecognized input file type.\n");
        return EXIT_FAILURE;
    }

    std::string compformat;
    std::string decformat;

    if (input.kind == IVF_FILE)
        compformat = "IVF";

    if (input.kind == WEBM_FILE)
        compformat = "Webm";

    if (input.kind == RAW_FILE)
        compformat = "Raw";

    if (use_y4m == 0)
        decformat = "Raw";

    if (use_y4m == 1)
        decformat = "Y4M";

    if (use_y4m == 2)
        decformat = "IVF";

    tprintf(PRINT_BTH, "\nAPI - Decompressing VP8 %s File to Raw %s File: \n",
        compformat.c_str(), decformat.c_str());

    outfile_pattern = outfile_pattern ? outfile_pattern : "-";
    single_file = 1;
    {
        const char *p = outfile_pattern;

        do
        {
            p = strchr(p, '%');

            if (p && p[1] >= '1' && p[1] <= '9')
            {
                single_file = 0;
                break;
            }

            if (p)
                p++;
        }
        while (p);
    }

    if (single_file && !noblit)
    {
        generate_filename(outfile_pattern, outfile, sizeof(outfile) - 1,
                          width, height, 0);
        strncpy(outfile, outputchar, PATH_MAX);
        // outfile = outputchar;
        out = out_open(outfile, do_md5);
    }

    if (use_y4m && !noblit)
    {
        char buffer[128];

        if (!single_file)
        {
            fprintf(stderr, "YUV4MPEG2 not supported with output patterns,"
                    " try --i420 or --yv12.\n");
            return EXIT_FAILURE;
        }

        if (input.kind == WEBM_FILE)
            if (webm_guess_framerate(&input, &fps_den, &fps_num))
            {
                fprintf(stderr, "Failed to guess framerate -- error parsing "
                        "webm file?\n");
                return EXIT_FAILURE;
            }

        /*Note: We can't output an aspect ratio here because IVF doesn't
        store one, and neither does VP8.
        That will have to wait until these tools support WebM natively.*/
        sprintf(buffer, "YUV4MPEG2 C%s W%u H%u F%u:%u I%c\n",
                "420jpeg", width, height, fps_num, fps_den, 'p');
        out_put(out, (unsigned char *)buffer, strlen(buffer), do_md5);
    }

    /* Try to determine the codec from the fourcc. */
    for (i = 0; i < sizeof(ifaces) / sizeof(ifaces[0]); i++)
        if ((fourcc & ifaces[i].fourcc_mask) == ifaces[i].fourcc)
        {
            vpx_codec_iface_t  *ivf_iface = ifaces[i].iface;

            if (iface && iface != ivf_iface)
                tprintf(PRINT_STD, "Notice -- IVF header indicates codec: %s\n",
                        ifaces[i].name);
            else
                iface = ivf_iface;

            break;
        }

    unsigned int FrameSize = (width * height * 3) / 2;
    uint64_t TimeStamp = 0;

    cfg.threads = threads;

    if (vpx_codec_dec_init(&decoder, iface ? iface :  ifaces[0].iface, &cfg,
        postproc ? VPX_CODEC_USE_POSTPROC : 0))
    {
        tprintf(PRINT_STD, "Failed to initialize decoder: %s\n",
            vpx_codec_error(&decoder));
        fclose(infile);

        return -1;
    }

    /* Decode file */
    uint64_t timestamp = 0;
    while (!read_frame_dec(&input, &buf, &buf_sz, &buf_alloc_sz, &timestamp))
    {
        vpx_codec_iter_t  iter = NULL;
        vpx_image_t    *img;
        struct vpx_usec_timer timer;
        uint64_t start, end;

        start = vpxt_get_cpu_tick();
        vpx_usec_timer_start(&timer);

        if (vpx_codec_decode(&decoder, buf, buf_sz, NULL, 0))
        {
            const char *detail = vpx_codec_error_detail(&decoder);
            tprintf(PRINT_STD, "Failed to decode frame: %s\n",
                vpx_codec_error(&decoder));

            if (detail)
                tprintf(PRINT_STD, "  Additional information: %s\n", detail);

            goto fail;
        }

        vpx_usec_timer_mark(&timer);
        end  = vpxt_get_cpu_tick();
        dx_time += vpx_usec_timer_elapsed(&timer);
        total_cpu_time_used = total_cpu_time_used + (end - start);
        ++frame_in;

        if ((img = vpx_codec_get_frame(&decoder, &iter)))
            ++frame_out;

        if (progress)
        {
        }

        if (!noblit)
        {
            if (img)
            {
                unsigned int y;
                char out_fn[PATH_MAX];
                uint8_t *buf;
                const char *sfx = flipuv ? "yv12" : "i420";

                if (!single_file)
                {
                    size_t len = sizeof(out_fn) - 1;
                    out_fn[len] = '\0';
                    generate_filename(outfile_pattern, out_fn, len - 1,
                                      img->d_w, img->d_h, frame_in);
                    out = out_open(out_fn, do_md5);
                }
                else if (use_y4m)
                    out_put(out, (unsigned char *)"FRAME\n", 6, do_md5);

                if (!use_y4m)
                    ivf_write_frame_header((FILE *)out, 0, img->d_h * img->d_w);

                if (CharCount == 79)
                {
                    tprintf(PRINT_BTH, "\n");
                    CharCount = 0;
                }

                CharCount++;
                tprintf(PRINT_BTH, ".");

                buf = img->planes[VPX_PLANE_Y];

                for (y = 0; y < img->d_h; y++)
                {
                    out_put(out, buf, img->d_w, do_md5);
                    buf += img->stride[VPX_PLANE_Y];
                }

                buf = img->planes[flipuv?VPX_PLANE_V:VPX_PLANE_U];

                for (y = 0; y < (1 + img->d_h) / 2; y++)
                {
                    out_put(out, buf, (1 + img->d_w) / 2, do_md5);
                    buf += img->stride[VPX_PLANE_U];
                }

                buf = img->planes[flipuv?VPX_PLANE_U:VPX_PLANE_V];

                for (y = 0; y < (1 + img->d_h) / 2; y++)
                {
                    out_put(out, buf, (1 + img->d_w) / 2, do_md5);
                    buf += img->stride[VPX_PLANE_V];
                }

                if (!single_file)
                    out_close(out, out_fn, do_md5);
            }
        }

        if (stop_after && frame_in >= stop_after)
            break;
    }

    if (summary)
    {
        tprintf(PRINT_BTH, "\n\n Decoded %d frames in %lu us (%.2f fps)\n",
            frame_in, dx_time, (float)frame_in * 1000000.0 / (float)dx_time);
        tprintf(PRINT_BTH, " Total CPU Ticks: %i\n", total_cpu_time_used);
    }

fail:

    if (vpx_codec_destroy(&decoder))
    {
        tprintf(PRINT_STD, "Failed to destroy decoder: %s\n",
            vpx_codec_error(&decoder));
        fclose(infile);

        return -1;
    }

    if (single_file && !noblit)
        out_close(out, outfile, do_md5);

    if (input.nestegg_ctx)
        nestegg_destroy(input.nestegg_ctx);

    if (input.kind != WEBM_FILE)
        free(buf);

    fclose(infile);

    std::string FullNameMs;
    std::string FullNameCpu;

    vpxt_remove_file_extension(outputchar, FullNameMs);
    vpxt_remove_file_extension(outputchar, FullNameCpu);

    FullNameMs += "decompression_time.txt";

    std::ofstream FullNameMsFile(FullNameMs.c_str());
    FullNameMsFile << dx_time;
    FullNameMsFile.close();

    FullNameCpu += "decompression_cpu_tick.txt";

    std::ofstream FullNameCpuFile(FullNameCpu.c_str());
    FullNameCpuFile << total_cpu_time_used;
    FullNameCpuFile.close();

    CPUTick = total_cpu_time_used;
    return dx_time;
}
int vpxt_dec_compute_md5(const char *inputchar, const char *outputchar)
{
    int                     use_y4m = 0;
    vpx_codec_ctx_t         decoder;
    const char             *fn = inputchar;
    int                     i;
    uint8_t                *buf = NULL;
    size_t                  buf_sz = 0, buf_alloc_sz = 0;
    FILE                   *infile;
    int                     frame_in = 0, frame_out = 0, flipuv = 0, noblit = 0;
    int                     do_md5 = 1, progress = 0;
    int                     stop_after = 0, postproc = 0, summary = 0, quiet =1;
    vpx_codec_iface_t      *iface = NULL;
    unsigned int            fourcc;
    unsigned long           dx_time = 0;
    const char             *outfile_pattern = 0;
    char                    outfile[PATH_MAX];
    int                     single_file;
    unsigned int            width;
    unsigned int            height;
    unsigned int            fps_den;
    unsigned int            fps_num;
    void                   *out = NULL;
    vpx_codec_dec_cfg_t     cfg = {0};
#if CONFIG_VP8_DECODER
    vp8_postproc_cfg_t      vp8_pp_cfg = {0};
    int                     vp8_dbg_color_ref_frame = 0;
    int                     vp8_dbg_color_mb_modes = 0;
    int                     vp8_dbg_color_b_modes = 0;
    int                     vp8_dbg_display_mv = 0;
#endif
    struct input_ctx        input;

    input.chunk = 0;
    input.chunks = 0;
    input.infile = NULL;
    input.kind = RAW_FILE;
    input.nestegg_ctx = 0;
    input.pkt = 0;
    input.video_track = 0;

    int CharCount = 0;

    /* Open file */
    infile = strcmp(fn, "-") ? fopen(fn, "rb") : set_binary_mode(stdin);
    if (!infile){
        tprintf(PRINT_BTH, "Failed to open input file: %s", fn);
        return -1;
    }

    input.infile = infile;
    if (file_is_ivf_dec(infile, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = IVF_FILE;
    else if (file_is_webm(&input, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = WEBM_FILE;
    else if (file_is_raw(infile, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = RAW_FILE;
    else
    {
        fprintf(stderr, "Unrecognized input file type.\n");
        return EXIT_FAILURE;
    }

    outfile_pattern = outfile_pattern ? outfile_pattern : "-";
    single_file = 1;
    {
        const char *p = outfile_pattern;

        do
        {
            p = strchr(p, '%');

            if (p && p[1] >= '1' && p[1] <= '9')
            {
                single_file = 0;
                break;
            }

            if (p)
                p++;
        }
        while (p);
    }

    if (single_file && !noblit)
    {
        generate_filename(outfile_pattern, outfile, sizeof(outfile) - 1, width,
            height, 0);
        strncpy(outfile, outputchar, PATH_MAX);
        out = out_open(outfile, do_md5);
    }

    if (use_y4m && !noblit)
    {
        char buffer[128];

        if (!single_file)
        {
            fprintf(stderr, "YUV4MPEG2 not supported with output patterns,"
                    " try --i420 or --yv12.\n");
            return EXIT_FAILURE;
        }

        if (input.kind == WEBM_FILE)
            if (webm_guess_framerate(&input, &fps_den, &fps_num))
            {
                fprintf(stderr, "Failed to guess framerate -- error parsing "
                        "webm file?\n");
                return EXIT_FAILURE;
            }

        /*Note: We can't output an aspect ratio here because IVF doesn't
        store one, and neither does VP8.
        That will have to wait until these tools support WebM natively.*/
        sprintf(buffer, "YUV4MPEG2 C%s W%u H%u F%u:%u I%c\n",
                "420jpeg", width, height, fps_num, fps_den, 'p');
        out_put(out, (unsigned char *)buffer, strlen(buffer), do_md5);
    }

    /* Try to determine the codec from the fourcc. */
    for (i = 0; i < sizeof(ifaces) / sizeof(ifaces[0]); i++)
        if ((fourcc & ifaces[i].fourcc_mask) == ifaces[i].fourcc)
        {
            vpx_codec_iface_t  *ivf_iface = ifaces[i].iface;

            if (iface && iface != ivf_iface)
                tprintf(PRINT_STD, "Notice -- IVF header indicates codec: %s\n",
                        ifaces[i].name);
            else
                iface = ivf_iface;

            break;
        }

    unsigned int FrameSize = (width * height * 3) / 2;
    uint64_t TimeStamp = 0;

    if (vpx_codec_dec_init(&decoder, iface ? iface :  ifaces[0].iface, &cfg,
        postproc ? VPX_CODEC_USE_POSTPROC : 0))
    {
        tprintf(PRINT_STD, "Failed to initialize decoder: %s\n",
            vpx_codec_error(&decoder));
        fclose(infile);

        return -1;
    }

    /* Decode file */
    uint64_t timestamp = 0;
    while (!read_frame_dec(&input, &buf, &buf_sz, &buf_alloc_sz, &timestamp))
    {
        vpx_codec_iter_t  iter = NULL;
        vpx_image_t    *img;
        struct vpx_usec_timer timer;

        vpx_usec_timer_start(&timer);

        if (vpx_codec_decode(&decoder, buf, buf_sz, NULL, 0))
        {
            const char *detail = vpx_codec_error_detail(&decoder);
            tprintf(PRINT_STD, "Failed to decode frame: %s\n",
                vpx_codec_error(&decoder));

            if (detail)
                tprintf(PRINT_STD, "  Additional information: %s\n", detail);

            goto fail;
        }

        vpx_usec_timer_mark(&timer);
        dx_time += vpx_usec_timer_elapsed(&timer);

        ++frame_in;

        if ((img = vpx_codec_get_frame(&decoder, &iter)))
            ++frame_out;

        if (progress)
        {
        }

        if (!noblit)
        {
            if (img)
            {
                unsigned int y;
                char out_fn[PATH_MAX];
                uint8_t *buf;

                if (!single_file)
                {
                    size_t len = sizeof(out_fn) - 1;
                    out_fn[len] = '\0';
                    generate_filename(outfile_pattern, out_fn, len - 1,
                                      img->d_w, img->d_h, frame_in);
                    out = out_open(out_fn, do_md5);
                }
                else if (use_y4m)
                    out_put(out, (unsigned char *)"FRAME\n", 6, do_md5);

                buf = img->planes[VPX_PLANE_Y];

                for (y = 0; y < img->d_h; y++)
                {
                    out_put(out, buf, img->d_w, do_md5);
                    buf += img->stride[VPX_PLANE_Y];
                }

                buf = img->planes[flipuv?VPX_PLANE_V:VPX_PLANE_U];

                for (y = 0; y < (1 + img->d_h) / 2; y++)
                {
                    out_put(out, buf, (1 + img->d_w) / 2, do_md5);
                    buf += img->stride[VPX_PLANE_U];
                }

                buf = img->planes[flipuv?VPX_PLANE_U:VPX_PLANE_V];

                for (y = 0; y < (1 + img->d_h) / 2; y++)
                {
                    out_put(out, buf, (1 + img->d_w) / 2, do_md5);
                    buf += img->stride[VPX_PLANE_V];
                }

                if (!single_file)
                    out_close(out, out_fn, do_md5);
            }
        }

        if (stop_after && frame_in >= stop_after)
            break;
    }

    if (summary || progress)
    {
        tprintf(PRINT_STD, "Decoded %d frames in %lu us (%.2f fps)\n",
                frame_in, dx_time, (float)frame_in * 1000000.0 /(float)dx_time);
    }

fail:

    if (vpx_codec_destroy(&decoder))
    {
        tprintf(PRINT_STD, "Failed to destroy decoder: %s\n",
            vpx_codec_error(&decoder));
        fclose(infile);

        return -1;
    }

    if (single_file && !noblit)
        out_close(out, outfile, do_md5);

    if (input.nestegg_ctx)
        nestegg_destroy(input.nestegg_ctx);

    if (input.kind != WEBM_FILE)
        free(buf);

    fclose(infile);

    return 0;
}
#endif
// --------------------------------Tools----------------------------------------
int vpxt_cut_clip(const char *input_file,
                  const char *output_file,
                  int StartingFrame,
                  int EndingFrame)
{
    bool verbose = 1;

    FILE *in = fopen(input_file, "rb");

    if (in == NULL)
    {
        tprintf(PRINT_STD, "\nInput file does not exist");
        return 0;
    }

    FILE *out = fopen(output_file, "wb");

    if (out == NULL)
    {
        tprintf(PRINT_STD, "\nOutput file does not exist");
        fclose(in);
        return 0;
    }

    int currentVideoFrame = 0;
    int CharCount = 0;

    y4m_input                y4m;
    unsigned int file_type, fourcc;
    unsigned long nbytes = 0;
    struct detect_buffer detect;

    unsigned int width;
    unsigned int height;
    unsigned int rate;
    unsigned int scale;

    detect.buf_read = fread(detect.buf, 1, 4, in);
    detect.position = 0;

    if (detect.buf_read == 4 && file_is_y4m(in, &y4m, detect.buf))
    {
        if (y4m_input_open(&y4m, in, detect.buf, 4) >= 0)
        {
            file_type = FILE_TYPE_Y4M;
            width = y4m.pic_w;
            height = y4m.pic_h;

            /* Use the frame rate from the file only if none was specified
            * on the command-line.
            */
            if (1)
            {
                rate = y4m.fps_n;
                scale = y4m.fps_d;
            }
        }
        else
        {
            fprintf(stderr, "Unsupported Y4M stream.\n");
            return EXIT_FAILURE;
        }
    }
    else if (detect.buf_read == 4 && file_is_ivf(in, &fourcc, &width, &height,
        &detect, &scale, &rate))
    {
        file_type = FILE_TYPE_IVF;

        switch (fourcc)
        {
        case 0x32315659:
            // arg_use_i420 = 0;
            break;
        case 0x30323449:
            // arg_use_i420 = 1;
            break;
        default:
            fprintf(stderr, "Unsupported fourcc (%08x) in IVF\n", fourcc);
            return EXIT_FAILURE;
        }
    }
    else
    {
        file_type = FILE_TYPE_RAW;
    }

    if (!width || !height)
    {
        fprintf(stderr, "Specify stream dimensions with --width (-w) "
                " and --height (-h).\n");
        return EXIT_FAILURE;
    }

    std::string inputformat;

    if (file_type == FILE_TYPE_RAW)
        inputformat = "Raw";

    if (file_type == FILE_TYPE_Y4M)
        inputformat = "Y4M";

    if (file_type == FILE_TYPE_IVF)
        inputformat = "IVF";

    tprintf(PRINT_STD, "\nCut %s file to %s file: \n", inputformat.c_str(),
        inputformat.c_str());

    if (file_type == FILE_TYPE_Y4M)
    {
        // copy and output y4m header
        fpos_t position;
        fgetpos(in, &position);
        rewind(in);

        char buffer[128];
        fread(buffer, 1, 128, in);

        std::string bufferStr = buffer;
        std::string bufferStr2 = bufferStr.substr(0, bufferStr.find("FRAME"));
        char buffer2[128];
        strncpy(buffer2, bufferStr2.c_str(), 128);
        out_put(out, (unsigned char *)buffer2, strlen(buffer2), 0);
        fsetpos(in, &position);
    }

    if (file_type == FILE_TYPE_IVF)
    {
        rewind(in);
        IVF_HEADER ivf_h_raw;
        InitIVFHeader(&ivf_h_raw);
        fread(&ivf_h_raw, 1, sizeof(ivf_h_raw), in);
        fwrite((char *)& ivf_h_raw, 1, sizeof(ivf_h_raw), out);
    }

    currentVideoFrame = 1;

    vpx_image_t raw;
    vpx_img_alloc(&raw, VPX_IMG_FMT_I420, width, height, 1);
    int frame_avail = 1;
    int framesWritten = 0;

    while (frame_avail)
    {

        if (currentVideoFrame >= StartingFrame && currentVideoFrame <=
            EndingFrame)
        {
            if (CharCount == 79)
            {
                tprintf(PRINT_STD, "\n");
                CharCount = 0;
            }

            tprintf(PRINT_STD, "*");
            CharCount++;

            frame_avail = read_frame_enc(in, &raw, file_type, &y4m, &detect);

            if (!frame_avail)
                break;

            if (file_type == FILE_TYPE_Y4M)
            {
                out_put(out, (unsigned char *)"FRAME\n", 6, 0);
            }

            if (file_type == FILE_TYPE_IVF)
            {
                ivf_write_frame_header(out, 0, width * height * 3 / 2);
            }

            unsigned int y;
            uint8_t *buf;

            buf = raw.planes[VPX_PLANE_Y];

            for (y = 0; y < raw.d_h; y++)
            {
                out_put(out, buf, raw.d_w, 0);
                buf += raw.stride[VPX_PLANE_Y];
            }

            buf = raw.planes[VPX_PLANE_U];

            for (y = 0; y < (1 + raw.d_h) / 2; y++)
            {
                out_put(out, buf, (1 + raw.d_w) / 2, 0);
                buf += raw.stride[VPX_PLANE_U];
            }

            buf = raw.planes[VPX_PLANE_V];

            for (y = 0; y < (1 + raw.d_h) / 2; y++)
            {
                out_put(out, buf, (1 + raw.d_w) / 2, 0);
                buf += raw.stride[VPX_PLANE_V];
            }

            framesWritten++;
        }
        else
        {
            if (CharCount == 79)
            {
                tprintf(PRINT_STD, "\n");
                CharCount = 0;
            }

            tprintf(PRINT_STD, ".");
            CharCount++;

            frame_avail = read_frame_enc(in, &raw, file_type, &y4m, &detect);
        }

        currentVideoFrame++;
    }

    if (file_type == FILE_TYPE_IVF)
    {
        rewind(in);
        rewind(out);
        IVF_HEADER ivf_h_raw;
        InitIVFHeader(&ivf_h_raw);
        fread(&ivf_h_raw, 1, sizeof(ivf_h_raw), in);
        ivf_h_raw.length = framesWritten;
        fwrite((char *)& ivf_h_raw, 1, sizeof(ivf_h_raw), out);
    }

    fclose(in);
    fclose(out);
    vpx_img_free(&raw);

    return(0);
}
int vpxt_crop_raw_clip(const char *input_file,
                       const char *output_file,
                       int xoffset, int yoffset,
                       int newFrameWidth,
                       int newFrameHeight,
                       int FileIsIVF,
                       int OutputToFile)
{
    bool verbose = 1;

    FILE *in = fopen(input_file, "rb");

    if (in == NULL)
    {
        tprintf(PRINT_STD, "\nInput file does not exist");
        return 0;
    }

    FILE *out = fopen(output_file, "wb");

    if (out == NULL)
    {
        tprintf(PRINT_STD, "\nOutput file does not exist");
        fclose(in);
        return 0;
    }

    int currentVideoFrame = 0;
    int CharCount = 0;

    y4m_input                y4m;
    unsigned int file_type, fourcc;
    unsigned long nbytes = 0;
    struct detect_buffer detect;

    unsigned int width;
    unsigned int height;
    unsigned int rate;
    unsigned int scale;
    //int arg_use_i420 = 1;

    detect.buf_read = fread(detect.buf, 1, 4, in);
    detect.position = 0;

    if (detect.buf_read == 4 && file_is_y4m(in, &y4m, detect.buf))
    {
        if (y4m_input_open(&y4m, in, detect.buf, 4) >= 0)
        {
            file_type = FILE_TYPE_Y4M;
            width = y4m.pic_w;
            height = y4m.pic_h;

            /* Use the frame rate from the file only if none was specified
            * on the command-line.
            */
            if (1)
            {
                rate = y4m.fps_n;
                scale = y4m.fps_d;
            }

            // arg_use_i420 = 0;
        }
        else
        {
            fprintf(stderr, "Unsupported Y4M stream.\n");
            return EXIT_FAILURE;
        }
    }
    else if (detect.buf_read == 4 && file_is_ivf(in, &fourcc, &width, &height,
        &detect, &scale, &rate))
    {
        file_type = FILE_TYPE_IVF;

        switch (fourcc)
        {
        case 0x32315659:
            // arg_use_i420 = 0;
            break;
        case 0x30323449:
            // arg_use_i420 = 1;
            break;
        default:
            fprintf(stderr, "Unsupported fourcc (%08x) in IVF\n", fourcc);
            return EXIT_FAILURE;
        }
    }
    else
    {
        file_type = FILE_TYPE_RAW;
    }

    if (!width || !height)
    {
        fprintf(stderr, "Specify stream dimensions with --width (-w) "
                " and --height (-h).\n");
        return EXIT_FAILURE;
    }

    std::string inputformat;

    if (file_type == FILE_TYPE_RAW)
        inputformat = "Raw";

    if (file_type == FILE_TYPE_Y4M)
        inputformat = "Y4M";

    if (file_type == FILE_TYPE_IVF)
        inputformat = "IVF";

    tprintf(PRINT_STD, "\nCrop %s file to %s file: \n", inputformat.c_str(),
        inputformat.c_str());

    if (file_type == FILE_TYPE_Y4M)
    {
        // copy and output y4m header
        fpos_t position;
        fgetpos(in, &position);
        rewind(in);

        char buffer[128];
        fread(buffer, 1, 128, in);

        std::string bufferStr = buffer;
        size_t WidthLoc = 0;
        size_t WidthLocEnd = 0;
        size_t HeightLoc = 0;
        size_t HeightLocEnd = 0;
        size_t FrameLoc = 0;

        char widthChar[32];
        char heightChar[32];
        vpxt_itoa_custom(newFrameWidth, widthChar, 10);
        vpxt_itoa_custom(newFrameHeight, heightChar, 10);

        WidthLoc = bufferStr.find("W");
        WidthLocEnd = bufferStr.find(" ", WidthLoc + 1);
        HeightLoc = bufferStr.find("H");
        HeightLocEnd = bufferStr.find(" ", HeightLoc + 1);
        FrameLoc = bufferStr.find("FRAME");

        std::string bufferStr2 = bufferStr.substr(0, WidthLoc) + "W" + widthChar
            + " H" + heightChar + bufferStr.substr(HeightLocEnd, FrameLoc -
            HeightLocEnd);

        char buffer2[128];
        strncpy(buffer2, bufferStr2.c_str(), 128);
        out_put(out, (unsigned char *)buffer2, strlen(buffer2), 0);
        fsetpos(in, &position);

    }

    if (file_type == FILE_TYPE_IVF)
    {
        rewind(in);
        IVF_HEADER ivf_h_raw;
        InitIVFHeader(&ivf_h_raw);
        fread(&ivf_h_raw, 1, sizeof(ivf_h_raw), in);
        fwrite((char *)& ivf_h_raw, 1, sizeof(ivf_h_raw), out);
    }

    currentVideoFrame = 1;

    int frame_avail = 1;
    int framesWritten = 0;

    while (frame_avail)
    {
        vpx_image_t raw;
        vpx_img_alloc(&raw, VPX_IMG_FMT_I420, width, height, 1);

        if (CharCount == 79)
        {
            tprintf(PRINT_STD, "\n");
            CharCount = 0;
        }

        tprintf(PRINT_STD, ".");
        CharCount++;

        frame_avail = read_frame_enc(in, &raw, file_type, &y4m, &detect);

        if (!frame_avail)
        {
            vpx_img_free(&raw);
            break;
        }

        if (vpx_img_set_rect(&raw, xoffset, yoffset, newFrameWidth,
            newFrameHeight) != 0)
        {
            tprintf(PRINT_STD, "ERROR: INVALID RESIZE\n");

            if (OutputToFile)
            {
                fprintf(stderr, "ERROR: INVALID RESIZE\n");
            }

            vpx_img_free(&raw);
            break;
        }

        if (!frame_avail)
        {
            vpx_img_free(&raw);
            break;
        }

        if (file_type == FILE_TYPE_Y4M)
        {
            out_put(out, (unsigned char *)"FRAME\n", 6, 0);
        }

        if (file_type == FILE_TYPE_IVF)
        {
            ivf_write_frame_header(out, 0, newFrameWidth * newFrameHeight +
                ((newFrameWidth + 1) / 2) *((newFrameHeight + 1) / 2) +
                ((newFrameWidth + 1) / 2) *((newFrameHeight + 1) / 2));
        }

        unsigned int y;
        uint8_t *buf;

        buf = raw.planes[VPX_PLANE_Y];

        for (y = 0; y < raw.d_h; y++)
        {
            out_put(out, buf, raw.d_w, 0);
            buf += raw.stride[VPX_PLANE_Y];
        }

        buf = raw.planes[VPX_PLANE_U];

        for (y = 0; y < (1 + raw.d_h) / 2; y++)
        {
            out_put(out, buf, (1 + raw.d_w) / 2, 0);
            buf += raw.stride[VPX_PLANE_U];
        }

        buf = raw.planes[VPX_PLANE_V];

        for (y = 0; y < (1 + raw.d_h) / 2; y++)
        {
            out_put(out, buf, (1 + raw.d_w) / 2, 0);
            buf += raw.stride[VPX_PLANE_V];
        }

        framesWritten++;
        currentVideoFrame++;

        vpx_img_free(&raw);
    }

    if (file_type == FILE_TYPE_IVF)
    {
        rewind(in);
        rewind(out);
        IVF_HEADER ivf_h_raw;
        InitIVFHeader(&ivf_h_raw);
        fread(&ivf_h_raw, 1, sizeof(ivf_h_raw), in);
        ivf_h_raw.length = framesWritten;
        ivf_h_raw.width = newFrameWidth;
        ivf_h_raw.height = newFrameHeight;
        fwrite((char *)& ivf_h_raw, 1, sizeof(ivf_h_raw), out);
    }

    fclose(in);
    fclose(out);

    return(0);
}
int vpxt_pad_raw_clip(const char *input_file,
                      const char *output_file,
                      int newFrameWidth,
                      int newFrameHeight,
                      int FileIsIVF,
                      int OutputToFile)
{
    bool verbose = 1;

    FILE *in = fopen(input_file, "rb");

    if (in == NULL)
    {
        tprintf(PRINT_STD, "\nInput file does not exist");
        return 0;
    }

    FILE *out = fopen(output_file, "wb");

    if (out == NULL)
    {
        tprintf(PRINT_STD, "\nOutput file does not exist");
        fclose(in);
        return 0;
    }

    int currentVideoFrame = 0;
    int CharCount = 0;

    y4m_input                y4m;
    unsigned int file_type, fourcc;
    unsigned long nbytes = 0;
    struct detect_buffer detect;

    unsigned int width;
    unsigned int height;
    unsigned int rate;
    unsigned int scale;

    detect.buf_read = fread(detect.buf, 1, 4, in);
    detect.position = 0;

    if (detect.buf_read == 4 && file_is_y4m(in, &y4m, detect.buf))
    {
        if (y4m_input_open(&y4m, in, detect.buf, 4) >= 0)
        {
            file_type = FILE_TYPE_Y4M;
            width = y4m.pic_w;
            height = y4m.pic_h;

            /* Use the frame rate from the file only if none was specified
            * on the command-line.
            */
            if (1)
            {
                rate = y4m.fps_n;
                scale = y4m.fps_d;
            }

            // arg_use_i420 = 0;
        }
        else
        {
            fprintf(stderr, "Unsupported Y4M stream.\n");
            return EXIT_FAILURE;
        }
    }
    else if (detect.buf_read == 4 && file_is_ivf(in, &fourcc, &width, &height,
        &detect, &scale, &rate))
    {
        file_type = FILE_TYPE_IVF;

        switch (fourcc)
        {
        case 0x32315659:
            // arg_use_i420 = 0;
            break;
        case 0x30323449:
            // arg_use_i420 = 1;
            break;
        default:
            fprintf(stderr, "Unsupported fourcc (%08x) in IVF\n", fourcc);
            return EXIT_FAILURE;
        }
    }
    else
    {
        file_type = FILE_TYPE_RAW;
    }

    if (!width || !height)
    {
        fprintf(stderr, "Specify stream dimensions with --width (-w) "
                " and --height (-h).\n");
        return EXIT_FAILURE;
    }

    std::string inputformat;

    if (file_type == FILE_TYPE_RAW)
        inputformat = "Raw";

    if (file_type == FILE_TYPE_Y4M)
        inputformat = "Y4M";

    if (file_type == FILE_TYPE_IVF)
        inputformat = "IVF";

    tprintf(PRINT_STD, "\nCrop %s file to %s file: \n", inputformat.c_str(),
        inputformat.c_str());

    if (file_type == FILE_TYPE_Y4M)
    {
        // copy and output y4m header
        fpos_t position;
        fgetpos(in, &position);
        rewind(in);

        char buffer[128];
        fread(buffer, 1, 128, in);

        std::string bufferStr = buffer;
        size_t WidthLoc = 0;
        size_t WidthLocEnd = 0;
        size_t HeightLoc = 0;
        size_t HeightLocEnd = 0;
        size_t FrameLoc = 0;

        char widthChar[32];
        char heightChar[32];
        vpxt_itoa_custom(newFrameWidth, widthChar, 10);
        vpxt_itoa_custom(newFrameHeight, heightChar, 10);

        WidthLoc = bufferStr.find("W");
        WidthLocEnd = bufferStr.find(" ", WidthLoc + 1);
        HeightLoc = bufferStr.find("H");
        HeightLocEnd = bufferStr.find(" ", HeightLoc + 1);
        FrameLoc = bufferStr.find("FRAME");

        std::string bufferStr2 = bufferStr.substr(0, WidthLoc) + "W" + widthChar
            + " H" + heightChar + bufferStr.substr(HeightLocEnd, FrameLoc -
            HeightLocEnd);

        char buffer2[128];
        strncpy(buffer2, bufferStr2.c_str(), 128);
        out_put(out, (unsigned char *)buffer2, strlen(buffer2), 0);
        fsetpos(in, &position);
    }

    if (file_type == FILE_TYPE_IVF)
    {
        rewind(in);
        IVF_HEADER ivf_h_raw;
        InitIVFHeader(&ivf_h_raw);
        fread(&ivf_h_raw, 1, sizeof(ivf_h_raw), in);
        fwrite((char *)& ivf_h_raw, 1, sizeof(ivf_h_raw), out);
    }

    currentVideoFrame = 1;

    int frame_avail = 1;
    int framesWritten = 0;

    vpx_image_t raw;
    vpx_img_alloc(&raw, VPX_IMG_FMT_I420, width, height, 1);

    while (frame_avail)
    {
        if (CharCount == 79)
        {
            tprintf(PRINT_STD, "\n");
            CharCount = 0;
        }

        tprintf(PRINT_STD, ".");
        CharCount++;

        frame_avail = read_frame_enc(in, &raw, file_type, &y4m, &detect);

        if (!frame_avail)
            break;

        if (file_type == FILE_TYPE_Y4M)
        {
            out_put(out, (unsigned char *)"FRAME\n", 6, 0);
        }

        if (file_type == FILE_TYPE_IVF)
        {
            ivf_write_frame_header(out, 0, newFrameWidth * newFrameHeight +
                ((newFrameWidth + 1) / 2) *((newFrameHeight + 1) / 2) +
                ((newFrameWidth + 1) / 2) *((newFrameHeight + 1) / 2));
        }

        unsigned int y;
        unsigned int z;
        uint8_t *buf;

        int size = 0;
        int offset = 0;

        int FrameSizeTrack = 0;

        buf = raw.planes[PLANE_Y];

        for (y = 0; y < raw.d_h; y++)
        {
            out_put(out, buf, raw.d_w, 0);
            buf += raw.stride[PLANE_Y];

            for (z = raw.d_w; z < newFrameWidth; z++)
            {
                char padchar = 'a';
                uint8_t padbuf = padchar;
                out_put(out, &padbuf, 1, 0);
            }
        }

        for (y = raw.d_h; y < newFrameHeight; y++)
        {
            char padchar = 'a';
            uint8_t padbuf = padchar;

            for (z = 0; z < newFrameWidth; z++)
            {
                char padchar = 'a';
                uint8_t padbuf = padchar;
                out_put(out, &padbuf, 1, 0);
            }
        }

        buf = raw.planes[PLANE_U];

        for (y = 0; y < (1 + raw.d_h) / 2; y++)
        {
            out_put(out, buf, (1 + raw.d_w) / 2, 0);
            buf += raw.stride[PLANE_U];

            for (z = (1 + raw.d_w) / 2; z < (1 + newFrameWidth) / 2; z++)
            {
                char padchar = 'a';
                uint8_t padbuf = padchar;
                out_put(out, &padbuf, 1, 0);
            }
        }

        for (y = (1 + raw.d_h) / 2; y < (1 + newFrameHeight) / 2; y++)
        {
            char padchar = 'a';
            uint8_t padbuf = padchar;

            for (z = 0 / 2; z < (1 + newFrameWidth) / 2; z++)
            {
                char padchar = 'a';
                uint8_t padbuf = padchar;
                out_put(out, &padbuf, 1, 0);
            }
        }

        buf = raw.planes[PLANE_V];

        for (y = 0; y < (1 + raw.d_h) / 2; y++)
        {
            out_put(out, buf, (1 + raw.d_w) / 2, 0);
            buf += raw.stride[PLANE_V];

            for (z = (1 + raw.d_w) / 2; z < (1 + newFrameWidth) / 2; z++)
            {
                char padchar = 'a';
                uint8_t padbuf = padchar;
                out_put(out, &padbuf, 1, 0);
            }
        }

        for (y = (1 + raw.d_h) / 2; y < (1 + newFrameHeight) / 2; y++)
        {
            char padchar = 'a';
            uint8_t padbuf = padchar;

            for (z = 0; z < (1 + newFrameWidth) / 2; z++)
            {
                char padchar = 'a';
                uint8_t padbuf = padchar;
                out_put(out, &padbuf, 1, 0);
            }
        }

        framesWritten++;
        currentVideoFrame++;
    }

    if (file_type == FILE_TYPE_IVF)
    {
        rewind(in);
        rewind(out);
        IVF_HEADER ivf_h_raw;
        InitIVFHeader(&ivf_h_raw);
        fread(&ivf_h_raw, 1, sizeof(ivf_h_raw), in);
        ivf_h_raw.length = framesWritten;
        ivf_h_raw.width = newFrameWidth;
        ivf_h_raw.height = newFrameHeight;
        fwrite((char *)& ivf_h_raw, 1, sizeof(ivf_h_raw), out);
    }

    fclose(in);
    fclose(out);
    vpx_img_free(&raw);


    return(0);
}
int vpxt_paste_clip(const char *inputFile1,
                    const char *inputFile2,
                    const char *output_file,
                    int StartingFrame)
{
    bool verbose = 1;

    FILE *in1 = fopen(inputFile1, "rb");

    if (in1 == NULL)
    {
        tprintf(PRINT_STD, "\nInput file does not exist");
        return 0;
    }

    FILE *in2 = fopen(inputFile2, "rb");

    if (in2 == NULL)
    {
        tprintf(PRINT_STD, "\nInput file does not exist");
        return 0;
    }

    FILE *out = fopen(output_file, "wb");

    if (out == NULL)
    {
        tprintf(PRINT_STD, "\nOutput file does not exist");
        fclose(in1);
        return 0;
    }

    int currentVideoFrame = 0;
    int CharCount = 0;

    y4m_input                y4m1;
    unsigned int file_type1, fourcc1;
    unsigned long nbytes1 = 0;
    struct detect_buffer detect1;

    unsigned int width_1;
    unsigned int height_1;
    unsigned int rate1;
    unsigned int scale1;

    detect1.buf_read = fread(detect1.buf, 1, 4, in1);
    detect1.position = 0;

    if (detect1.buf_read == 4 && file_is_y4m(in1, &y4m1, detect1.buf))
    {
        if (y4m_input_open(&y4m1, in1, detect1.buf, 4) >= 0)
        {
            file_type1 = FILE_TYPE_Y4M;
            width_1 = y4m1.pic_w;
            height_1 = y4m1.pic_h;

            /* Use the frame rate from the file only if none was specified
            * on the command-line.
            */
            if (1)
            {
                rate1 = y4m1.fps_n;
                scale1 = y4m1.fps_d;
            }
        }
        else
        {
            fprintf(stderr, "Unsupported Y4M stream.\n");
            return EXIT_FAILURE;
        }
    }
    else if (detect1.buf_read == 4 && file_is_ivf(in1, &fourcc1, &width_1,
        &height_1, &detect1, &scale1, &rate1))
    {
        file_type1 = FILE_TYPE_IVF;

        switch (fourcc1)
        {
        case 0x32315659:
            // arg_use_i420 = 0;
            break;
        case 0x30323449:
            // arg_use_i420 = 1;
            break;
        default:
            fprintf(stderr, "Unsupported fourcc (%08x) in IVF\n", fourcc1);
            return EXIT_FAILURE;
        }
    }
    else
    {
        file_type1 = FILE_TYPE_RAW;
    }

    if (!width_1 || !height_1)
    {
        fprintf(stderr, "Specify stream dimensions with --width (-w) "
                " and --height (-h).\n");
        return EXIT_FAILURE;
    }

    y4m_input                y4m2;
    unsigned int file_type2, fourcc2;
    unsigned long nbytes2 = 0;
    struct detect_buffer detect2;

    unsigned int width_2;
    unsigned int height_2;
    unsigned int rate2;
    unsigned int scale2;

    detect2.buf_read = fread(detect2.buf, 1, 4, in2);
    detect2.position = 0;

    if (detect2.buf_read == 4 && file_is_y4m(in2, &y4m2, detect2.buf))
    {
        if (y4m_input_open(&y4m2, in2, detect2.buf, 4) >= 0)
        {
            file_type2 = FILE_TYPE_Y4M;
            width_2 = y4m2.pic_w;
            height_2 = y4m2.pic_h;

            /* Use the frame rate from the file only if none was specified
            * on the command-line.
            */
            if (1)
            {
                rate2 = y4m2.fps_n;
                scale2 = y4m2.fps_d;
            }

            // arg_use_i420 = 0;
        }
        else
        {
            fprintf(stderr, "Unsupported Y4M stream.\n");
            return EXIT_FAILURE;
        }
    }
    else if (detect2.buf_read == 4 && file_is_ivf(in2, &fourcc2, &width_2,
        &height_2, &detect2, &scale2, &rate2))
    {
        file_type2 = FILE_TYPE_IVF;

        switch (fourcc2)
        {
        case 0x32315659:
            // arg_use_i420 = 0;
            break;
        case 0x30323449:
            // arg_use_i420 = 1;
            break;
        default:
            fprintf(stderr, "Unsupported fourcc (%08x) in IVF\n", fourcc2);
            return EXIT_FAILURE;
        }
    }
    else
    {
        file_type2 = FILE_TYPE_RAW;
    }

    if (!width_2 || !height_2)
    {
        fprintf(stderr, "Specify stream dimensions with --width (-w) "
                " and --height (-h).\n");
        return EXIT_FAILURE;
    }

    if (width_1 != width_2)
    {
        printf("width_1 %i != width_2 %i\n", width_1, width_2);
        return 0;
    }

    if (height_1 != height_2)
    {
        printf("height_1 %i != height_2 %i\n", height_1, height_2);
        return 0;
    }

    if (file_type1 != file_type2)
    {
        printf("file_type1 %i != file_type2 %i\n", file_type1, file_type2);
        return 0;
    }

    std::string inputformat1;
    std::string inputformat2;

    if (file_type1 == FILE_TYPE_RAW)
        inputformat1 = "Raw";

    if (file_type1 == FILE_TYPE_Y4M)
        inputformat1 = "Y4M";

    if (file_type1 == FILE_TYPE_IVF)
        inputformat1 = "IVF";

    if (file_type2 == FILE_TYPE_RAW)
        inputformat2 = "Raw";

    if (file_type2 == FILE_TYPE_Y4M)
        inputformat2 = "Y4M";

    if (file_type2 == FILE_TYPE_IVF)
        inputformat2 = "IVF";

    tprintf(PRINT_STD, "\nPaste %s file into %s file: \n", inputformat2.c_str(),
        inputformat1.c_str());

    if (file_type1 == FILE_TYPE_Y4M)
    {
        //copy and output y4m header
        fpos_t position;
        fgetpos(in1, &position);
        rewind(in1);

        char buffer[128];
        fread(buffer, 1, 128, in1);

        std::string bufferStr = buffer;
        std::string bufferStr2 = bufferStr.substr(0, bufferStr.find("FRAME"));
        char buffer2[128];
        strncpy(buffer2, bufferStr2.c_str(), 128);
        out_put(out, (unsigned char *)buffer2, strlen(buffer2), 0);
        fsetpos(in1, &position);
    }

    if (file_type1 == FILE_TYPE_IVF)
    {
        rewind(in1);
        IVF_HEADER ivf_h_raw;
        InitIVFHeader(&ivf_h_raw);
        fread(&ivf_h_raw, 1, sizeof(ivf_h_raw), in1);
        fwrite((char *)& ivf_h_raw, 1, sizeof(ivf_h_raw), out);
    }

    vpx_image_t raw;
    vpx_img_alloc(&raw, VPX_IMG_FMT_I420, width_1, height_1, 1);
    int frame_avail1 = 1;
    int frame_avail2 = 1;
    int framesWritten = 0;

    while (frame_avail1)
    {
        if (CharCount == 79)
        {
            tprintf(PRINT_STD, "\n");
            CharCount = 0;
        }

        CharCount++;

        if (framesWritten >= StartingFrame && frame_avail2)
        {
            tprintf(PRINT_STD, "*");
            frame_avail2 = read_frame_enc(in2, &raw, file_type1,&y4m1,&detect1);
        }
        else
        {
            tprintf(PRINT_STD, ".");
            frame_avail1 = read_frame_enc(in1, &raw, file_type1,&y4m1,&detect1);
        }

        if (!frame_avail2)
            frame_avail1 = read_frame_enc(in1, &raw, file_type1,&y4m1,&detect1);

        if (!frame_avail1)
            break;

        if (file_type1 == FILE_TYPE_Y4M)
        {
            out_put(out, (unsigned char *)"FRAME\n", 6, 0);
        }

        if (file_type1 == FILE_TYPE_IVF)
        {
            ivf_write_frame_header(out, 0, width_1 * height_1 * 3 / 2);
        }

        unsigned int y;
        uint8_t *buf;

        buf = raw.planes[VPX_PLANE_Y];

        for (y = 0; y < raw.d_h; y++)
        {
            out_put(out, buf, raw.d_w, 0);
            buf += raw.stride[VPX_PLANE_Y];
        }

        buf = raw.planes[VPX_PLANE_U];

        for (y = 0; y < (1 + raw.d_h) / 2; y++)
        {
            out_put(out, buf, (1 + raw.d_w) / 2, 0);
            buf += raw.stride[VPX_PLANE_U];
        }

        buf = raw.planes[VPX_PLANE_V];

        for (y = 0; y < (1 + raw.d_h) / 2; y++)
        {
            out_put(out, buf, (1 + raw.d_w) / 2, 0);
            buf += raw.stride[VPX_PLANE_V];
        }

        framesWritten++;
    }

    if (file_type1 == FILE_TYPE_IVF)
    {
        rewind(in1);
        rewind(out);
        IVF_HEADER ivf_h_raw;
        InitIVFHeader(&ivf_h_raw);
        fread(&ivf_h_raw, 1, sizeof(ivf_h_raw), in1);
        ivf_h_raw.length = framesWritten;
        fwrite((char *)& ivf_h_raw, 1, sizeof(ivf_h_raw), out);
    }

    fclose(in1);
    fclose(in2);
    fclose(out);
    vpx_img_free(&raw);

    return(0);
}
int vpxt_formatted_to_raw(const std::string input_file,
                          const std::string output_file)
{
    bool verbose = 1;

    FILE *in = fopen(input_file.c_str(), "rb");

    if (in == NULL)
    {
        tprintf(PRINT_STD, "\nInput file does not exist");
        return 0;
    }

    FILE *out = fopen(output_file.c_str(), "wb");

    if (out == NULL)
    {
        tprintf(PRINT_STD, "\nOutput file does not exist");
        fclose(in);
        return 0;
    }

    int currentVideoFrame = 0;
    int CharCount = 0;

    y4m_input                y4m;
    unsigned int file_type, fourcc;
    unsigned long nbytes = 0;
    struct detect_buffer detect;

    unsigned int width;
    unsigned int height;
    unsigned int rate;
    unsigned int scale;

    detect.buf_read = fread(detect.buf, 1, 4, in);
    detect.position = 0;

    if (detect.buf_read == 4 && file_is_y4m(in, &y4m, detect.buf))
    {
        if (y4m_input_open(&y4m, in, detect.buf, 4) >= 0)
        {
            file_type = FILE_TYPE_Y4M;
            width = y4m.pic_w;
            height = y4m.pic_h;

            /* Use the frame rate from the file only if none was specified
            * on the command-line.
            */
            if (1)
            {
                rate = y4m.fps_n;
                scale = y4m.fps_d;
            }

        }
        else
        {
            fprintf(stderr, "Unsupported Y4M stream.\n");
            return EXIT_FAILURE;
        }
    }
    else if (detect.buf_read == 4 && file_is_ivf(in, &fourcc, &width, &height,
        &detect, &scale, &rate))
    {
        file_type = FILE_TYPE_IVF;

        switch (fourcc)
        {
        case 0x32315659:
            // arg_use_i420 = 0;
            break;
        case 0x30323449:
            // arg_use_i420 = 1;
            break;
        default:
            fprintf(stderr, "Unsupported fourcc (%08x) in IVF\n", fourcc);
            return EXIT_FAILURE;
        }
    }
    else
    {
        file_type = FILE_TYPE_RAW;
    }

    if (!width || !height)
    {
        fprintf(stderr, "Specify stream dimensions with --width (-w) "
                " and --height (-h).\n");
        return EXIT_FAILURE;
    }

    std::string inputformat;

    if (file_type == FILE_TYPE_RAW)
        inputformat = "Raw";

    if (file_type == FILE_TYPE_Y4M)
        inputformat = "Y4M";

    if (file_type == FILE_TYPE_IVF)
        inputformat = "IVF";

    currentVideoFrame = 1;

    vpx_image_t raw;
    vpx_img_alloc(&raw, VPX_IMG_FMT_I420, width, height, 1);
    int frame_avail = 1;
    int framesWritten = 0;

    tprintf(PRINT_STD, "\n");

    while (frame_avail)
    {
        if (CharCount == 79)
        {
            tprintf(PRINT_STD, "\n");
            CharCount = 0;
        }

        tprintf(PRINT_STD, ".");
        CharCount++;

        frame_avail = read_frame_enc(in, &raw, file_type, &y4m, &detect);

        if (!frame_avail)
            break;

        unsigned int y;
        uint8_t *buf;

        buf = raw.planes[VPX_PLANE_Y];

        for (y = 0; y < raw.d_h; y++)
        {
            out_put(out, buf, raw.d_w, 0);
            buf += raw.stride[VPX_PLANE_Y];
        }

        buf = raw.planes[VPX_PLANE_U];

        for (y = 0; y < (1 + raw.d_h) / 2; y++)
        {
            out_put(out, buf, (1 + raw.d_w) / 2, 0);
            buf += raw.stride[VPX_PLANE_U];
        }

        buf = raw.planes[VPX_PLANE_V];

        for (y = 0; y < (1 + raw.d_h) / 2; y++)
        {
            out_put(out, buf, (1 + raw.d_w) / 2, 0);
            buf += raw.stride[VPX_PLANE_V];
        }

        framesWritten++;
        currentVideoFrame++;
    }

    fclose(in);
    fclose(out);
    vpx_img_free(&raw);

    return(0);
}
int vpxt_formatted_to_raw_frames(const std::string input_file,
                                 const std::string output_file)
{
    int frameLength = vpxt_get_number_of_frames(input_file.c_str());
    int LastFrameDecPlaces = vpxt_decimal_places(frameLength);

    bool verbose = 1;

    FILE *in = fopen(input_file.c_str(), "rb");

    if (in == NULL)
    {
        tprintf(PRINT_STD, "\nInput file does not exist");
        return 0;
    }

    int currentVideoFrame = 0;
    int CharCount = 0;

    y4m_input                y4m;
    unsigned int file_type, fourcc;
    unsigned long nbytes = 0;

    struct detect_buffer detect;

    unsigned int width;
    unsigned int height;
    unsigned int rate;
    unsigned int scale;

    detect.buf_read = fread(detect.buf, 1, 4, in);
    detect.position = 0;

    if (detect.buf_read == 4 && file_is_y4m(in, &y4m, detect.buf))
    {
        if (y4m_input_open(&y4m, in, detect.buf, 4) >= 0)
        {
            file_type = FILE_TYPE_Y4M;
            width = y4m.pic_w;
            height = y4m.pic_h;

            /* Use the frame rate from the file only if none was specified
            * on the command-line.
            */
            if (1)
            {
                rate = y4m.fps_n;
                scale = y4m.fps_d;
            }
        }
        else
        {
            fprintf(stderr, "Unsupported Y4M stream.\n");
            return EXIT_FAILURE;
        }
    }
    else if (detect.buf_read == 4 && file_is_ivf(in, &fourcc, &width, &height,
        &detect, &scale, &rate))
    {
        file_type = FILE_TYPE_IVF;

        switch (fourcc)
        {
        case 0x32315659:
            // arg_use_i420 = 0;
            break;
        case 0x30323449:
            // arg_use_i420 = 1;
            break;
        default:
            fprintf(stderr, "Unsupported fourcc (%08x) in IVF\n", fourcc);
            return EXIT_FAILURE;
        }
    }
    else
    {
        file_type = FILE_TYPE_RAW;
    }

    if (!width || !height)
    {
        fprintf(stderr, "\nunsupported file type.");
        return EXIT_FAILURE;
    }

    std::string inputformat;

    if (file_type == FILE_TYPE_RAW)
        inputformat = "Raw";

    if (file_type == FILE_TYPE_Y4M)
        inputformat = "Y4M";

    if (file_type == FILE_TYPE_IVF)
        inputformat = "IVF";

    currentVideoFrame = 1;

    vpx_image_t raw;
    vpx_img_alloc(&raw, VPX_IMG_FMT_I420, width, height, 1);
    int frame_avail = 1;
    int framesWritten = 0;

    tprintf(PRINT_STD, "\n");

    while (frame_avail)
    {
        char currentVideoFrameStr[10];
        char widthchar[10];
        char heightchar[10];
        vpxt_itoa_custom(currentVideoFrame, currentVideoFrameStr, 10);
        vpxt_itoa_custom(width, widthchar, 10);
        vpxt_itoa_custom(height, heightchar, 10);

        std::string FrameFileName;
        vpxt_remove_file_extension(output_file.c_str(), FrameFileName);
        FrameFileName.erase(FrameFileName.length() - 1, 1);
        FrameFileName += "-";
        FrameFileName += widthchar;
        FrameFileName += "x";
        FrameFileName += heightchar;
        FrameFileName += "-Frame-";

        int CurNumDecPlaces = vpxt_decimal_places(currentVideoFrame);

        // add zeros for increasing frames
        while (CurNumDecPlaces < LastFrameDecPlaces)
        {
            FrameFileName += "0";
            CurNumDecPlaces++;
        }

        FrameFileName += currentVideoFrameStr;
        FrameFileName += ".raw";

        FILE *out = fopen(FrameFileName.c_str(), "wb");
        if (out == NULL)
        {
            tprintf(PRINT_STD, "\nOutput file does not exist");
            fclose(in);
            return 0;
        }

        if (CharCount == 79)
        {
            tprintf(PRINT_STD, "\n");
            CharCount = 0;
        }

        tprintf(PRINT_STD, ".");
        CharCount++;

        frame_avail = read_frame_enc(in, &raw, file_type, &y4m, &detect);

        if (!frame_avail)
            break;

        unsigned int y;
        uint8_t *buf;

        buf = raw.planes[VPX_PLANE_Y];

        for (y = 0; y < raw.d_h; y++)
        {
            out_put(out, buf, raw.d_w, 0);
            buf += raw.stride[VPX_PLANE_Y];
        }

        buf = raw.planes[VPX_PLANE_U];

        for (y = 0; y < (1 + raw.d_h) / 2; y++)
        {
            out_put(out, buf, (1 + raw.d_w) / 2, 0);
            buf += raw.stride[VPX_PLANE_U];
        }

        buf = raw.planes[VPX_PLANE_V];

        for (y = 0; y < (1 + raw.d_h) / 2; y++)
        {
            out_put(out, buf, (1 + raw.d_w) / 2, 0);
            buf += raw.stride[VPX_PLANE_V];
        }

        fclose(out);
        currentVideoFrame++;
    }

    fclose(in);
    vpx_img_free(&raw);

    return(0);
}
int vpxt_display_header_info(int argc, const char** argv)
{
    char input_file[256] = "";
    strncpy(input_file, argv[2], 256);
    int extrafileinfo = 0;

    FILE *fp;

    if (argc > 3)
        extrafileinfo = atoi(argv[3]);

    if (argc > 4)
    {
        char output_file[256] = "";
        strncpy(output_file, argv[4], 256);

        if ((fp = freopen(output_file, "w", stderr)) == NULL)
        {
            tprintf(PRINT_STD, "Cannot open out put file: %s\n", output_file);
            exit(1);
        }
    }

    unsigned char  signature[4];    // ='DKIF';
    unsigned short version = 0;     //  -
    unsigned short headersize = 0;  //  -
    unsigned int fourcc = 0;        // good
    unsigned int width = 0;         // good
    unsigned int height = 0;        // good
    unsigned int rate = 0;          // good
    unsigned int scale = 0;         // good
    unsigned int length = 0;        // other measure
    unsigned char unused[4];        //  -

    signature[0] = ' ';
    signature[1] = ' ';
    signature[2] = ' ';
    signature[3] = ' ';

    std::vector<unsigned int>     frameSize;
    std::vector<uint64_t> timeStamp;

    FILE *infile = strcmp(input_file, "-") ? fopen(input_file, "rb") :
        set_binary_mode(stdin);

    if (!infile)
    {
        tprintf(PRINT_BTH, "Failed to open input file: %s", input_file);
        return -1;
    }

    struct input_ctx        input;

    input.chunk = 0;
    input.chunks = 0;
    input.infile = NULL;
    input.kind = RAW_FILE;
    input.nestegg_ctx = 0;
    input.pkt = 0;
    input.video_track = 0;
    input.infile = infile;

    if (file_is_ivf_dec(infile, &fourcc, &width, &height, &scale, &rate))
        input.kind = IVF_FILE;
    else if (file_is_webm(&input, &fourcc, &width, &height, &scale, &rate))
        input.kind = WEBM_FILE;
    else if (file_is_raw(infile, &fourcc, &width, &height, &scale, &rate))
        input.kind = RAW_FILE;
    else
    {
        fprintf(stderr, "Unrecognized input file type.\n");
        return EXIT_FAILURE;
    }

    if (input.kind == WEBM_FILE)
        if (webm_guess_framerate(&input, &scale, &rate))
        {
            fprintf(stderr, "Failed to guess framerate -- error parsing "
                    "webm file?\n");
            return EXIT_FAILURE;
        }

    std::string compformat;

    if (input.kind == IVF_FILE)
        compformat = "IVF";

    if (input.kind == WEBM_FILE)
        compformat = "Webm";

    if (input.kind == RAW_FILE)
        compformat = "Raw";

    if (input.kind == IVF_FILE)
    {
        rewind(infile);
        IVF_HEADER ivf_h_raw;
        InitIVFHeader(&ivf_h_raw);
        fread(&ivf_h_raw, 1, sizeof(ivf_h_raw), infile);

        version = ivf_h_raw.version;
        headersize = ivf_h_raw.headersize;
        fourcc = ivf_h_raw.four_cc;
        width = ivf_h_raw.width;
        height = ivf_h_raw.height;
        rate = ivf_h_raw.rate;
        scale = ivf_h_raw.scale;
        length = ivf_h_raw.length;

        signature[0] = ivf_h_raw.signature[0];
        signature[1] = ivf_h_raw.signature[1];
        signature[2] = ivf_h_raw.signature[2];
        signature[3] = ivf_h_raw.signature[3];
    }

    uint8_t               *buf = NULL;
    size_t               buf_sz = 0, buf_alloc_sz = 0;
    unsigned int currentVideoFrame = 0;
    int frame_avail = 0;
    uint64_t timestamp = 0;

    while (!frame_avail)
    {
        frame_avail = skim_frame_dec(&input, &buf, &buf_sz, &buf_alloc_sz,
            &timestamp);

        if (!frame_avail && (extrafileinfo == 1 || currentVideoFrame == 0))
        {
            frameSize.push_back(buf_sz);
            timeStamp.push_back(timestamp);
        }
        else
            frame_avail = 1;

        currentVideoFrame++;
    }

    if (input.kind == WEBM_FILE)
        length = currentVideoFrame;

    tprintf(PRINT_STD, "\nFILE TYPE IS: %s\n", compformat.c_str());
    tprintf(PRINT_STD, "\n"
            "FILE HEADER \n\n"
            "File Header            - %c%c%c%c \n"
            "File Format Version    - %i \n"
            "File Header Size       - %i \n"
            "Video Data FourCC      - %i \n"
            "Video Image Width      - %i \n"
            "Video Image Height     - %i \n"
            "Frame Rate Rate        - %i \n"
            "Frame Rate Scale       - %i \n"
            "Video Length in Frames - %i \n"
            "Unused                 - %c \n"
            "\n\n"
            , signature[0], signature[1], signature[2], signature[3]
            , version, headersize, fourcc, width, height, rate, scale, length,
                unused);

    if (argc > 4)
    {
        tprintf(PRINT_ERR, "\nFILE TYPE IS: %s\n", compformat.c_str());
        tprintf(PRINT_ERR, "\n"
                "FILE HEADER \n\n"
                "File Header            - %c%c%c%c \n"
                "File Format Version    - %i \n"
                "File Header Size       - %i \n"
                "Video Data FourCC      - %i \n"
                "Video Image Width      - %i \n"
                "Video Image Height     - %i \n"
                "Frame Rate Rate        - %i \n"
                "Frame Rate Scale       - %i \n"
                "Video Length in Frames - %i \n"
                "Unused                 - %c \n"
                "\n\n"
                , signature[0], signature[1], signature[2], signature[3]
                , version, headersize, fourcc, width, height, rate, scale,
                    length, unused);
    }

    currentVideoFrame = 0;

    while (currentVideoFrame < frameSize.size())
    {
        tprintf(PRINT_STD, "FRAME HEADER %i\n"
                "Frame Size            - %i \n"
                "time Stamp            - %llu \n"
                "\n"

                , currentVideoFrame, frameSize[currentVideoFrame],
                timeStamp[currentVideoFrame]);

        if (argc > 4)
        {
            tprintf(PRINT_ERR, "FRAME HEADER %i\n"
                    "Frame Size            - %i \n"
                    "time Stamp            - %llu \n"
                    "\n"

                    , currentVideoFrame, frameSize[currentVideoFrame],
                    timeStamp[currentVideoFrame]);
        }

        currentVideoFrame++;
    }

    fclose(infile);

    if (argc > 4)
        fclose(fp);

    return 0;
}
int vpxt_compare_header_info(int argc, const char** argv)
{
    const char *inputFile1 = argv[2];
    const char *inputFile2 = argv[3];

    const char *outputfile = argv[5];

    FILE *fp;

    if (argc == 6)
    {
        if ((fp = freopen(outputfile, "w", stderr)) == NULL)
        {
            tprintf(PRINT_STD, "Cannot open out put file: %s\n", outputfile);
            exit(1);
        }
    }

    int extrafileinfo;

    if (argc > 3)
    {
        extrafileinfo = atoi(argv[4]);
    }
    else
    {
        extrafileinfo = 0;
    }

    std::string strinputFile1 = inputFile1;
    std::string strinputFile2 = inputFile2;

    int returnval = -1;

    unsigned char  signature_1[4];      // ='DKIF';
    unsigned short version_1 = 0;       //  -
    unsigned short headersize_1 = 0;    //  -
    unsigned int fourcc_1 = 0;          // good
    unsigned int width_1 = 0;           // good
    unsigned int height_1 = 0;          // good
    unsigned int rate_1 = 0;            // good
    unsigned int scale_1 = 0;           // good
    unsigned int length_1 = 0;          // other measure
    unsigned char unused_1[4];          //  -

    unsigned char  signature_2[4];      // ='DKIF';
    unsigned short version_2 = 0;       //  -
    unsigned short headersize_2 = 0;    //  -
    unsigned int fourcc_2 = 0;          // good
    unsigned int width_2 = 0;           // good
    unsigned int height_2 = 0;          // good
    unsigned int rate_2 = 0;            // good
    unsigned int scale_2 = 0;           // good
    unsigned int length_2 = 0;          // other measure
    unsigned char unused_2[4];          //  -

    signature_1[0] = ' ';
    signature_1[1] = ' ';
    signature_1[2] = ' ';
    signature_1[3] = ' ';

    signature_2[0] = ' ';
    signature_2[1] = ' ';
    signature_2[2] = ' ';
    signature_2[3] = ' ';

    FILE *infile_1 = strcmp(inputFile1, "-") ? fopen(inputFile1, "rb") :
        set_binary_mode(stdin);
    FILE *infile_2 = strcmp(inputFile2, "-") ? fopen(inputFile2, "rb") :
        set_binary_mode(stdin);

    if (!infile_1)
    {
        tprintf(PRINT_BTH, "Failed to open input file: %s", inputFile1);
        return -1;
    }

    struct input_ctx        input_1;

    struct input_ctx        input_2;

    input_1.chunk = 0;

    input_1.chunks = 0;

    input_1.infile = NULL;

    input_1.kind = RAW_FILE;

    input_1.nestegg_ctx = 0;

    input_1.pkt = 0;

    input_1.video_track = 0;

    input_2.chunk = 0;

    input_2.chunks = 0;

    input_2.infile = NULL;

    input_2.kind = RAW_FILE;

    input_2.nestegg_ctx = 0;

    input_2.pkt = 0;

    input_2.video_track = 0;

    input_1.infile = infile_1;

    if (file_is_ivf_dec(infile_1, &fourcc_1, &width_1, &height_1, &scale_1,
        &rate_1))
        input_1.kind = IVF_FILE;
    else if (file_is_webm(&input_1, &fourcc_1, &width_1, &height_1, &scale_1,
        &rate_1))
        input_1.kind = WEBM_FILE;
    else if (file_is_raw(infile_1, &fourcc_1, &width_1, &height_1, &scale_1,
        &rate_1))
        input_1.kind = RAW_FILE;
    else
    {
        fprintf(stderr, "Unrecognized input file type.\n");
        return EXIT_FAILURE;
    }

    if (input_1.kind == WEBM_FILE)
        if (webm_guess_framerate(&input_1, &scale_1, &rate_1))
        {
            fprintf(stderr, "Failed to guess framerate -- error parsing "
                    "webm file?\n");
            return EXIT_FAILURE;
        }

    input_2.infile = infile_2;

    if (file_is_ivf_dec(infile_2, &fourcc_2, &width_2, &height_2, &scale_2,
        &rate_2))
        input_2.kind = IVF_FILE;
    else if (file_is_webm(&input_2, &fourcc_2, &width_2, &height_2, &scale_2,
        &rate_2))
        input_2.kind = WEBM_FILE;
    else if (file_is_raw(infile_2, &fourcc_2, &width_2, &height_2, &scale_2,
        &rate_2))
        input_2.kind = RAW_FILE;
    else
    {
        fprintf(stderr, "Unrecognized input file type.\n");
        return EXIT_FAILURE;
    }

    if (input_2.kind == WEBM_FILE)
        if (webm_guess_framerate(&input_2, &scale_2, &rate_2))
        {
            fprintf(stderr, "Failed to guess framerate -- error parsing "
                    "webm file?\n");
            return EXIT_FAILURE;
        }

    std::string compformat_1;
    std::string compformat_2;

    if (input_1.kind == IVF_FILE)
        compformat_1 = "IVF";

    if (input_1.kind == WEBM_FILE)
        compformat_1 = "Webm";

    if (input_1.kind == RAW_FILE)
        compformat_1 = "Raw";

    if (input_2.kind == IVF_FILE)
        compformat_2 = "IVF";

    if (input_2.kind == WEBM_FILE)
        compformat_2 = "Webm";

    if (input_2.kind == RAW_FILE)
        compformat_2 = "Raw";

    if (input_1.kind == IVF_FILE)
    {
        rewind(infile_1);
        IVF_HEADER ivfhRaw_1;
        InitIVFHeader(&ivfhRaw_1);
        fread(&ivfhRaw_1, 1, sizeof(ivfhRaw_1), infile_1);

        version_1 = ivfhRaw_1.version;
        headersize_1 = ivfhRaw_1.headersize;
        fourcc_1 = ivfhRaw_1.four_cc;
        width_1 = ivfhRaw_1.width;
        height_1 = ivfhRaw_1.height;
        rate_1 = ivfhRaw_1.rate;
        scale_1 = ivfhRaw_1.scale;
        length_1 = ivfhRaw_1.length;

        signature_1[0] = ivfhRaw_1.signature[0];
        signature_1[1] = ivfhRaw_1.signature[1];
        signature_1[2] = ivfhRaw_1.signature[2];
        signature_1[3] = ivfhRaw_1.signature[3];
    }

    if (input_2.kind == IVF_FILE)
    {
        rewind(infile_2);
        IVF_HEADER ivfhRaw_2;
        InitIVFHeader(&ivfhRaw_2);
        fread(&ivfhRaw_2, 1, sizeof(ivfhRaw_2), infile_2);

        version_2 = ivfhRaw_2.version;
        headersize_2 = ivfhRaw_2.headersize;
        fourcc_2 = ivfhRaw_2.four_cc;
        width_2 = ivfhRaw_2.width;
        height_2 = ivfhRaw_2.height;
        rate_2 = ivfhRaw_2.rate;
        scale_2 = ivfhRaw_2.scale;
        length_2 = ivfhRaw_2.length;

        signature_2[0] = ivfhRaw_2.signature[0];
        signature_2[1] = ivfhRaw_2.signature[1];
        signature_2[2] = ivfhRaw_2.signature[2];
        signature_2[3] = ivfhRaw_2.signature[3];
    }

    uint8_t               *buf_1 = NULL;
    uint8_t               *buf_2 = NULL;
    size_t               buf_sz_1 = 0, buf_alloc_sz_1 = 0;
    size_t               buf_sz_2 = 0, buf_alloc_sz_2 = 0;
    int currentVideoFrame = 0;
    int frame_avail_1 = 0;
    int frame_avail_2 = 0;

    tprintf(PRINT_STD, "\n"
            "        FILE HEADER1                        FILE HEADER2\n\n"
            "File Header            - %c%c%c%-*cFile Header            "
            "- %c%c%c%c\n"
            "File Format Version    - %-*iFile Format Version    - %i\n"
            "File Header Size       - %-*iFile Header Size       - %i\n"
            "Video Data FourCC      - %-*iVideo Data FourCC      - %i\n"
            "Video Image Width      - %-*iVideo Image Width      - %i\n"
            "Video Image Height     - %-*iVideo Image Height     - %i\n"
            "Frame Rate Rate        - %-*iFrame Rate Rate        - %i\n"
            "Frame Rate Scale       - %-*iFrame Rate Scale       - %i\n"
            "Video Length in Frames - %-*iVideo Length in Frames - %i\n"
            "Unused                 - %-*cUnused                 - %c\n"
            "\n\n"
            , signature_1[0], signature_1[1], signature_1[2], 9, signature_1[3]
            , signature_2, signature_2[1], signature_2[2], signature_2[3]
            , 12, version_1, version_2
            , 12, headersize_1, headersize_2
            , 12, fourcc_1, fourcc_2
            , 12, width_1, width_2
            , 12, height_1, height_2
            , 12, rate_1, rate_2
            , 12, scale_1, scale_2
            , 12, length_1, length_2
            , 12, unused_1, unused_2);

    if (argc == 6)
    {
        tprintf(PRINT_ERR, "\n"
                "        FILE HEADER1                        FILE HEADER2\n\n"
                "File Header            - %c%c%c%-*cFile Header            "
                "- %c%c%c%c\n"
                "File Format Version    - %-*iFile Format Version    - %i\n"
                "File Header Size       - %-*iFile Header Size       - %i\n"
                "Video Data FourCC      - %-*iVideo Data FourCC      - %i\n"
                "Video Image Width      - %-*iVideo Image Width      - %i\n"
                "Video Image Height     - %-*iVideo Image Height     - %i\n"
                "Frame Rate Rate        - %-*iFrame Rate Rate        - %i\n"
                "Frame Rate Scale       - %-*iFrame Rate Scale       - %i\n"
                "Video Length in Frames - %-*iVideo Length in Frames - %i\n"
                "Unused                 - %-*cUnused                 - %c\n"
                "\n\n"
                , signature_1[0], signature_1[1], signature_1[2], 9
                ,signature_1[3], signature_2, signature_2[1], signature_2[2]
                , signature_2[3], 12, version_1, version_2
                , 12, headersize_1, headersize_2
                , 12, fourcc_1, fourcc_2
                , 12, width_1, width_2
                , 12, height_1, height_2
                , 12, rate_1, rate_2
                , 12, scale_1, scale_2
                , 12, length_1, length_2
                , 12, unused_1, unused_2);
    }
    uint64_t timestamp1 = 0;
    uint64_t timestamp2 = 0;

    while (!frame_avail_1 && !frame_avail_2)
    {
        frame_avail_1 = skim_frame_dec(&input_1, &buf_1, &buf_sz_1,
            &buf_alloc_sz_1, &timestamp1);
        frame_avail_2 = skim_frame_dec(&input_2, &buf_2, &buf_sz_2,
            &buf_alloc_sz_2, &timestamp2);

        if (frame_avail_1 && frame_avail_2)
        {
            break;
        }

        if (!frame_avail_1 && frame_avail_2)
        {
            tprintf(PRINT_STD,
                    "FRAME HEADER1 %-*i\n"
                    "Frame Size            - %-*i\n"
                    "time Stamp            - %-*llu\n"
                    "\n"

                    , 22, currentVideoFrame
                    , 12, buf_sz_1
                    , 12, timestamp1);

            if (argc == 6)
            {
                tprintf(PRINT_ERR,
                        "FRAME HEADER1 %-*i\n"
                        "Frame Size            - %-*i\n"
                        "time Stamp            - %-*llu\n"
                        "\n"

                        , 22, currentVideoFrame
                        , 12, buf_sz_1
                        , 12, timestamp1);
            }
        }

        if (!frame_avail_2 && frame_avail_1)
        {
            tprintf(PRINT_STD,
                    "                                    FRAME HEADER2 %i\n"
                    "                                    Frame Size            "
                    "- %i\n"
                    "                                    time Stamp            "
                    "- %llu\n"
                    "\n"

                    , currentVideoFrame
                    , buf_sz_2
                    , timestamp2);

            if (argc == 6)
            {
                tprintf(PRINT_ERR,
                        "                                    FRAME HEADER2 %i\n"
                        "                                    Frame Size        "
                        "- %i\n"
                        "                                    time Stamp        "
                        "- %llu\n"
                        "\n"

                        , currentVideoFrame
                        , buf_sz_2
                        , timestamp2);
            }
        }

        if (!frame_avail_1 && !frame_avail_2)
        {
            tprintf(PRINT_STD, "FRAME HEADER1 %-*iFRAME HEADER2 %i\n"
                    "Frame Size            - %-*iFrame Size            - %i\n"
                    "time Stamp            - %-*lluTime Stamp            - %llu"
                    "\n"
                    "\n"

                    , 22, currentVideoFrame, currentVideoFrame
                    , 12, buf_sz_1, buf_sz_2
                    , 12, timestamp1, timestamp2);

            if (argc == 6)
            {
                tprintf(PRINT_ERR, "FRAME HEADER1 %-*iFRAME HEADER2 %i\n"
                        "Frame Size            - %-*iFrame Size            - "
                        "%i\n"
                        "time Stamp            - %-*lluTime Stamp            - "
                        "%llu\n"
                        "\n"

                        , 22, currentVideoFrame, currentVideoFrame
                        , 12, buf_sz_1, buf_sz_2
                        , 12, timestamp1, timestamp2);
            }
        }

        currentVideoFrame++;
    }

    if (input_1.nestegg_ctx)
        nestegg_destroy(input_1.nestegg_ctx);

    if (input_2.nestegg_ctx)
        nestegg_destroy(input_2.nestegg_ctx);

    if (input_1.kind != WEBM_FILE)
        free(buf_1);

    if (input_2.kind != WEBM_FILE)
        free(buf_2);

    fclose(infile_1);
    fclose(infile_2);

    if (argc == 6)
        fclose(fp);

    return returnval;
}
int vpxt_compare_dec(const char *inputFile1, const char *inputFile2)
{
    // Returns:
    // -1 if files are identical
    // -2 if file 2 ends before File 1
    // -3 if file 1 ends before File 2
    // and >= 0 where the number the function returns is the frame that they
    // differ first on.

    int returnval = -1;

    FILE *in_1 = fopen(inputFile1, "rb");

    if (in_1 == NULL)
    {
        tprintf(PRINT_STD, "\nInput file does not exist");
        return 0;
    }

    FILE *in_2 = fopen(inputFile2, "rb");

    if (in_2 == NULL)
    {
        tprintf(PRINT_STD, "\nInput file does not exist");
        return 0;
    }

    int currentVideoFrame_1 = 0;
    int CharCount_1 = 0;

    y4m_input                y4m_1;
    unsigned int file_type_1, fourcc_1;
    unsigned long nbytes_1 = 0;
    struct detect_buffer detect_1;

    y4m_input                y4m_2;
    unsigned int file_type_2, fourcc_2;
    unsigned long nbytes_2 = 0;
    struct detect_buffer detect_2;

    unsigned int width_1;
    unsigned int height_1;
    unsigned int rate_1;
    unsigned int scale_1;

    unsigned int width_2;
    unsigned int height_2;
    unsigned int rate_2;
    unsigned int scale_2;

    detect_1.buf_read = fread(detect_1.buf, 1, 4, in_1);
    detect_1.position = 0;

    detect_2.buf_read = fread(detect_2.buf, 1, 4, in_2);
    detect_2.position = 0;

    if (detect_1.buf_read == 4 && file_is_y4m(in_1, &y4m_1, detect_1.buf))
    {
        if (y4m_input_open(&y4m_1, in_1, detect_1.buf, 4) >= 0)
        {
            file_type_1 = FILE_TYPE_Y4M;
            width_1 = y4m_1.pic_w;
            height_1 = y4m_1.pic_h;

            /* Use the frame rate from the file only if none was specified
            * on the command-line.
            */
            if (1)
            {
                rate_1 = y4m_1.fps_n;
                scale_1 = y4m_1.fps_d;
            }

            // arg_use_i420 = 0;
        }
        else
        {
            fprintf(stderr, "Unsupported Y4M stream.\n");
            return EXIT_FAILURE;
        }
    }
    else if (detect_1.buf_read == 4 && file_is_ivf(in_1, &fourcc_1, &width_1,
        &height_1, &detect_1, &scale_1, &rate_1))
    {
        file_type_1 = FILE_TYPE_IVF;

        switch (fourcc_1)
        {
        case 0x32315659:
            // arg_use_i420 = 0;
            break;
        case 0x30323449:
            // arg_use_i420 = 1;
            break;
        default:
            fprintf(stderr, "Unsupported fourcc (%08x) in IVF\n", fourcc_1);
            return EXIT_FAILURE;
        }
    }
    else
    {
        file_type_1 = FILE_TYPE_RAW;
    }

    if (!width_1 || !height_1)
    {
        fprintf(stderr, "Specify stream dimensions with --width (-w) "
                " and --height (-h).\n");
        return EXIT_FAILURE;
    }

    if (detect_2.buf_read == 4 && file_is_y4m(in_2, &y4m_2, detect_2.buf))
    {
        if (y4m_input_open(&y4m_2, in_2, detect_2.buf, 4) >= 0)
        {
            file_type_2 = FILE_TYPE_Y4M;
            width_2 = y4m_2.pic_w;
            height_2 = y4m_2.pic_h;

            /* Use the frame rate from the file only if none was specified
            * on the command-line.
            */
            if (1)
            {
                rate_2 = y4m_2.fps_n;
                scale_2 = y4m_2.fps_d;
            }

            // arg_use_i420 = 0;
        }
        else
        {
            fprintf(stderr, "Unsupported Y4M stream.\n");
            return EXIT_FAILURE;
        }
    }
    else if (detect_2.buf_read == 4 && file_is_ivf(in_2, &fourcc_2, &width_2,
        &height_2, &detect_2, &scale_2, &rate_2))
    {
        file_type_2 = FILE_TYPE_IVF;

        switch (fourcc_2)
        {
        case 0x32315659:
            // arg_use_i420 = 0;
            break;
        case 0x30323449:
            // arg_use_i420 = 1;
            break;
        default:
            fprintf(stderr, "Unsupported fourcc (%08x) in IVF\n", fourcc_2);
            return EXIT_FAILURE;
        }
    }
    else
    {
        file_type_2 = FILE_TYPE_RAW;
    }

    if (!width_2 || !height_2)
    {
        fprintf(stderr, "Specify stream dimensions with --width (-w) "
                " and --height (-h).\n");
        return EXIT_FAILURE;
    }

    std::string inputformat_1;
    std::string inputformat_2;

    if (file_type_1 == FILE_TYPE_RAW)
        inputformat_1 = "Raw";

    if (file_type_1 == FILE_TYPE_Y4M)
        inputformat_1 = "Y4M";

    if (file_type_1 == FILE_TYPE_IVF)
        inputformat_1 = "IVF";

    if (file_type_2 == FILE_TYPE_RAW)
        inputformat_2 = "Raw";

    if (file_type_2 == FILE_TYPE_Y4M)
        inputformat_2 = "Y4M";

    if (file_type_2 == FILE_TYPE_IVF)
        inputformat_2 = "IVF";

    int currentVideoFrame = 1;

    int frame_avail_1 = 1;
    int frame_avail_2 = 1;
    vpx_image_t raw_2;
    vpx_image_t raw_1;

    if (file_type_1 == FILE_TYPE_Y4M)
        /*The Y4M reader does its own allocation.
        Just initialize this here to avoid problems if we never read any
        frames.*/
        memset(&raw_1, 0, sizeof(raw_1));
    else
        vpx_img_alloc(&raw_1, VPX_IMG_FMT_I420, width_1, height_1, 1);

    if (file_type_2 == FILE_TYPE_Y4M)
        /*The Y4M reader does its own allocation.
        Just initialize this here to avoid problems if we never read any
        frames.*/
        memset(&raw_2, 0, sizeof(raw_2));
    else
        vpx_img_alloc(&raw_2, VPX_IMG_FMT_I420, width_2, height_2, 1);

    while (frame_avail_1 && frame_avail_2)
    {
        frame_avail_1 = read_frame_enc(in_1, &raw_1, file_type_1, &y4m_1,
            &detect_1);
        frame_avail_2 = read_frame_enc(in_2, &raw_2, file_type_2, &y4m_2,
            &detect_2);

        if (!frame_avail_1 || !frame_avail_2)
        {
            if (file_type_1 == FILE_TYPE_IVF)
                vpx_img_free(&raw_1);

            if (file_type_1 == FILE_TYPE_IVF)
                vpx_img_free(&raw_2);

            fclose(in_1);
            fclose(in_2);

            if (file_type_1 == FILE_TYPE_Y4M)
                y4m_input_close(&y4m_1);

            if (file_type_2 == FILE_TYPE_Y4M)
                y4m_input_close(&y4m_2);

            if (frame_avail_1 == frame_avail_2)
                return -1;
            else
                return currentVideoFrame + 1;
        }

        if (raw_1.d_w != raw_2.d_w || raw_1.d_h != raw_2.d_h)
        {
            if (file_type_1 == FILE_TYPE_IVF)
                vpx_img_free(&raw_1);

            if (file_type_1 == FILE_TYPE_IVF)
                vpx_img_free(&raw_2);

            fclose(in_1);
            fclose(in_2);

            if (file_type_1 == FILE_TYPE_Y4M)
                y4m_input_close(&y4m_1);

            if (file_type_2 == FILE_TYPE_Y4M)
                y4m_input_close(&y4m_2);

            return currentVideoFrame + 1;
        }

        unsigned int y;
        uint8_t *buf_1;
        uint8_t *buf_2;

        buf_1 = raw_1.planes[VPX_PLANE_Y];
        buf_2 = raw_2.planes[VPX_PLANE_Y];

        for (y = 0; y < raw_1.d_h; y++)
        {
            if (memcmp(buf_1, buf_2, raw_1.d_w) != 0)
            {
                if (file_type_1 == FILE_TYPE_IVF)
                    vpx_img_free(&raw_1);

                if (file_type_1 == FILE_TYPE_IVF)
                    vpx_img_free(&raw_2);

                fclose(in_1);
                fclose(in_2);

                if (file_type_1 == FILE_TYPE_Y4M)
                    y4m_input_close(&y4m_1);

                if (file_type_2 == FILE_TYPE_Y4M)
                    y4m_input_close(&y4m_2);

                return currentVideoFrame + 1;
            }

            buf_1 += raw_1.stride[VPX_PLANE_Y];
            buf_2 += raw_2.stride[VPX_PLANE_Y];
        }

        buf_1 = raw_1.planes[VPX_PLANE_U];
        buf_2 = raw_2.planes[VPX_PLANE_U];

        for (y = 0; y < (1 + raw_1.d_h) / 2; y++)
        {
            if (memcmp(buf_1, buf_2, (1 + raw_1.d_w) / 2) != 0)
            {
                if (file_type_1 == FILE_TYPE_IVF)
                    vpx_img_free(&raw_1);

                if (file_type_1 == FILE_TYPE_IVF)
                    vpx_img_free(&raw_2);

                fclose(in_1);
                fclose(in_2);

                if (file_type_1 == FILE_TYPE_Y4M)
                    y4m_input_close(&y4m_1);

                if (file_type_2 == FILE_TYPE_Y4M)
                    y4m_input_close(&y4m_2);

                return currentVideoFrame + 1;
            }

            buf_1 += raw_1.stride[VPX_PLANE_U];
            buf_2 += raw_2.stride[VPX_PLANE_U];
        }

        buf_1 = raw_1.planes[VPX_PLANE_V];
        buf_2 = raw_2.planes[VPX_PLANE_V];

        for (y = 0; y < (1 + raw_1.d_h) / 2; y++)
        {
            if (memcmp(buf_1, buf_2, (1 + raw_1.d_w) / 2) != 0)
            {
                if (file_type_1 == FILE_TYPE_IVF)
                    vpx_img_free(&raw_1);

                if (file_type_1 == FILE_TYPE_IVF)
                    vpx_img_free(&raw_2);

                fclose(in_1);
                fclose(in_2);

                if (file_type_1 == FILE_TYPE_Y4M)
                    y4m_input_close(&y4m_1);

                if (file_type_2 == FILE_TYPE_Y4M)
                    y4m_input_close(&y4m_2);

                return currentVideoFrame + 1;
            }

            buf_1 += raw_1.stride[VPX_PLANE_V];
            buf_2 += raw_2.stride[VPX_PLANE_V];
        }

        currentVideoFrame++;
    }


    if (file_type_1 == FILE_TYPE_IVF)
        vpx_img_free(&raw_1);

    if (file_type_1 == FILE_TYPE_IVF)
        vpx_img_free(&raw_2);

    fclose(in_1);
    fclose(in_2);

    if (file_type_1 == FILE_TYPE_Y4M)
        y4m_input_close(&y4m_1);

    if (file_type_2 == FILE_TYPE_Y4M)
        y4m_input_close(&y4m_2);

    return returnval;
}
int vpxt_compare_enc(const char *inputFile1,
                     const char *inputFile2,
                     int fullcheck)
{
    // Returns:
    // -1 if files are identical
    // -2 if file 2 ends before File 1
    // -3 if file 1 ends before File 2
    // and >= 0 where the number the function returns is the frame that they
    // differ first on.

    int returnval = -1;

    unsigned char  signature_1[4];      // ='DKIF';
    unsigned short version_1 = 0;       //  -
    unsigned short headersize_1 = 0;    //  -
    unsigned int fourcc_1 = 0;          // good
    unsigned int width_1 = 0;           // good
    unsigned int height_1 = 0;          // good
    unsigned int rate_1 = 0;            // good
    unsigned int scale_1 = 0;           // good
    unsigned int length_1 = 0;          // other measure

    unsigned char  signature_2[4];      // ='DKIF';
    unsigned short version_2 = 0;       //  -
    unsigned short headersize_2 = 0;    //  -
    unsigned int fourcc_2 = 0;          // good
    unsigned int width_2 = 0;           // good
    unsigned int height_2 = 0;          // good
    unsigned int rate_2 = 0;            // good
    unsigned int scale_2 = 0;           // good
    unsigned int length_2 = 0;          // other measure

    signature_1[0] = ' ';
    signature_1[1] = ' ';
    signature_1[2] = ' ';
    signature_1[3] = ' ';

    signature_2[0] = ' ';
    signature_2[1] = ' ';
    signature_2[2] = ' ';
    signature_2[3] = ' ';

    FILE *infile_1 = strcmp(inputFile1, "-") ? fopen(inputFile1, "rb") :
        set_binary_mode(stdin);
    FILE *infile_2 = strcmp(inputFile2, "-") ? fopen(inputFile2, "rb") :
        set_binary_mode(stdin);

    if (!infile_1)
    {
        tprintf(PRINT_BTH, "Failed to open input file: %s", inputFile1);
        return -1;
    }

    struct input_ctx        input_1;

    struct input_ctx        input_2;

    input_1.chunk = 0;

    input_1.chunks = 0;

    input_1.infile = NULL;

    input_1.kind = RAW_FILE;

    input_1.nestegg_ctx = 0;

    input_1.pkt = 0;

    input_1.video_track = 0;

    input_2.chunk = 0;

    input_2.chunks = 0;

    input_2.infile = NULL;

    input_2.kind = RAW_FILE;

    input_2.nestegg_ctx = 0;

    input_2.pkt = 0;

    input_2.video_track = 0;

    input_1.infile = infile_1;

    if (file_is_ivf_dec(infile_1, &fourcc_1, &width_1, &height_1, &scale_1,
        &rate_1))
        input_1.kind = IVF_FILE;
    else if (file_is_webm(&input_1, &fourcc_1, &width_1, &height_1, &scale_1,
        &rate_1))
        input_1.kind = WEBM_FILE;
    else if (file_is_raw(infile_1, &fourcc_1, &width_1, &height_1, &scale_1,
        &rate_1))
        input_1.kind = RAW_FILE;
    else
    {
        fprintf(stderr, "Unrecognized input file type.\n");
        return -5;
    }

    if (input_1.kind == WEBM_FILE)
        if (webm_guess_framerate(&input_1, &scale_1, &rate_1))
        {
            fprintf(stderr, "Failed to guess framerate -- error parsing "
                    "webm file?\n");
            return EXIT_FAILURE;
        }

    input_2.infile = infile_2;

    if (file_is_ivf_dec(infile_2, &fourcc_2, &width_2, &height_2, &scale_2,
        &rate_2))
        input_2.kind = IVF_FILE;
    else if (file_is_webm(&input_2, &fourcc_2, &width_2, &height_2, &scale_2,
        &rate_2))
        input_2.kind = WEBM_FILE;
    else if (file_is_raw(infile_2, &fourcc_2, &width_2, &height_2, &scale_2,
        &rate_2))
        input_2.kind = RAW_FILE;
    else
    {
        fprintf(stderr, "Unrecognized input file type.\n");
        return -5;
    }

    if (input_2.kind == WEBM_FILE)
        if (webm_guess_framerate(&input_2, &scale_2, &rate_2))
        {
            fprintf(stderr, "Failed to guess framerate -- error parsing "
                    "webm file?\n");
            return EXIT_FAILURE;
        }

    std::string compformat_1;
    std::string compformat_2;

    if (input_1.kind == IVF_FILE)
        compformat_1 = "IVF";

    if (input_1.kind == WEBM_FILE)
        compformat_1 = "Webm";

    if (input_1.kind == RAW_FILE)
        compformat_1 = "Raw";

    if (input_2.kind == IVF_FILE)
        compformat_2 = "IVF";

    if (input_2.kind == WEBM_FILE)
        compformat_2 = "Webm";

    if (input_2.kind == RAW_FILE)
        compformat_2 = "Raw";

    if (input_1.kind == IVF_FILE)
    {
        rewind(infile_1);
        IVF_HEADER ivfhRaw_1;
        InitIVFHeader(&ivfhRaw_1);
        fread(&ivfhRaw_1, 1, sizeof(ivfhRaw_1), infile_1);

        version_1 = ivfhRaw_1.version;
        headersize_1 = ivfhRaw_1.headersize;
        fourcc_1 = ivfhRaw_1.four_cc;
        width_1 = ivfhRaw_1.width;
        height_1 = ivfhRaw_1.height;
        rate_1 = ivfhRaw_1.rate;
        scale_1 = ivfhRaw_1.scale;
        length_1 = ivfhRaw_1.length;

        signature_1[0] = ivfhRaw_1.signature[0];
        signature_1[1] = ivfhRaw_1.signature[1];
        signature_1[2] = ivfhRaw_1.signature[2];
        signature_1[3] = ivfhRaw_1.signature[3];
    }

    if (input_2.kind == IVF_FILE)
    {
        rewind(infile_2);
        IVF_HEADER ivfhRaw_2;
        InitIVFHeader(&ivfhRaw_2);
        fread(&ivfhRaw_2, 1, sizeof(ivfhRaw_2), infile_2);

        version_2 = ivfhRaw_2.version;
        headersize_2 = ivfhRaw_2.headersize;
        fourcc_2 = ivfhRaw_2.four_cc;
        width_2 = ivfhRaw_2.width;
        height_2 = ivfhRaw_2.height;
        rate_2 = ivfhRaw_2.rate;
        scale_2 = ivfhRaw_2.scale;
        length_2 = ivfhRaw_2.length;

        signature_2[0] = ivfhRaw_2.signature[0];
        signature_2[1] = ivfhRaw_2.signature[1];
        signature_2[2] = ivfhRaw_2.signature[2];
        signature_2[3] = ivfhRaw_2.signature[3];
    }

    uint8_t               *buf_1 = NULL;
    uint8_t               *buf_2 = NULL;
    size_t               buf_sz_1 = 0, buf_alloc_sz_1 = 0;
    size_t               buf_sz_2 = 0, buf_alloc_sz_2 = 0;
    int currentVideoFrame = 0;
    int frame_avail_1 = 0;
    int frame_avail_2 = 0;
    uint64_t timestamp1 = 0;
    uint64_t timestamp2 = 0;

    while (!frame_avail_1 && !frame_avail_2)
    {
        frame_avail_1 = read_frame_dec(&input_1, &buf_1, &buf_sz_1,
            &buf_alloc_sz_1, &timestamp1);
        frame_avail_2 = read_frame_dec(&input_2, &buf_2, &buf_sz_2,
            &buf_alloc_sz_2, &timestamp2);

        if (frame_avail_1 || frame_avail_2)
        {
            if (input_1.nestegg_ctx)
                nestegg_destroy(input_1.nestegg_ctx);

            if (input_2.nestegg_ctx)
                nestegg_destroy(input_2.nestegg_ctx);

            if (input_1.kind != WEBM_FILE)
                free(buf_1);

            if (input_2.kind != WEBM_FILE)
                free(buf_2);

            fclose(infile_1);
            fclose(infile_2);

            if (frame_avail_1 == frame_avail_2)
                return returnval;
            else
                return currentVideoFrame + 1;
        }

        if (memcmp(buf_1, buf_2, buf_sz_1) != 0)
        {
            if(fullcheck)
            {
                printf("\nFrame: %i Not Identical",currentVideoFrame);

                if(returnval == -1)
                    returnval = currentVideoFrame + 1;

                char intchar[56];
                vpxt_itoa_custom(currentVideoFrame, intchar, 10);

                // write different frames out
                std::string first_file_frame;
                vpxt_remove_file_extension(inputFile1, first_file_frame);
                first_file_frame += "Frame";
                first_file_frame += intchar;
                first_file_frame += ".raw";
                FILE *first_file_frame_file = fopen(first_file_frame.c_str(),
                    "w");

                std::string second_file_frame;
                vpxt_remove_file_extension(inputFile2, second_file_frame);
                second_file_frame += "Frame";
                second_file_frame += intchar;
                second_file_frame += ".raw";
                FILE *second_file_frame_file = fopen(second_file_frame.c_str(),
                    "w");

                fwrite(buf_1, buf_sz_1, 1, first_file_frame_file);
                fwrite(buf_2, buf_sz_2, 1, second_file_frame_file);

                fclose(first_file_frame_file);
                fclose(second_file_frame_file);
            }
            else
            {
                if (input_1.nestegg_ctx)
                    nestegg_destroy(input_1.nestegg_ctx);

                if (input_2.nestegg_ctx)
                    nestegg_destroy(input_2.nestegg_ctx);

                if (input_1.kind != WEBM_FILE)
                    free(buf_1);

                if (input_2.kind != WEBM_FILE)
                    free(buf_2);

                fclose(infile_1);
                fclose(infile_2);
                return currentVideoFrame + 1;
            }
        }

        currentVideoFrame++;
    }

    if (input_1.nestegg_ctx)
        nestegg_destroy(input_1.nestegg_ctx);

    if (input_2.nestegg_ctx)
        nestegg_destroy(input_2.nestegg_ctx);

    if (input_1.kind != WEBM_FILE)
        free(buf_1);

    if (input_2.kind != WEBM_FILE)
        free(buf_2);

    fclose(infile_1);
    fclose(infile_2);

    return returnval;
}
int vpxt_display_droped_frames(const char *inputchar, int PrintSwitch)
{
    int dropedframecount = 0;

    std::string DropedInStr;
    vpxt_remove_file_extension(inputchar, DropedInStr);
    DropedInStr += "aprox_droped_frames.txt";

    FILE *in = fopen(inputchar, "rb");
    FILE *out;

    ///////////////////////////////////
    if (in == NULL)
    {
        tprintf(PRINT_BTH, "\nInput file does not exist");
        return 0;
    }

    if (PrintSwitch == 1)
    {
        out = fopen(DropedInStr.c_str(), "w");

        if (out == NULL)
        {
            tprintf(PRINT_BTH, "\nOutput file does not exist");
            fclose(in);
            return 0;
        }
    }

    int currentVideoFrame = 0;
    int frameCount = 0;
    int byteRec = 0;

    IVF_HEADER ivf_h_raw;

    InitIVFHeader(&ivf_h_raw);
    fread(&ivf_h_raw, 1, sizeof(ivf_h_raw), in);
    vpxt_format_ivf_header_read(&ivf_h_raw);


    IVF_FRAME_HEADER ivf_fhRaw;

    fread(&ivf_fhRaw.frameSize, 1, 4, in);
    fread(&ivf_fhRaw.timeStamp, 1, 8, in);
    vpxt_format_frame_header_read(ivf_fhRaw);

    frameCount = ivf_h_raw.length;

    long nSamples = frameCount;
    long lRateNum = ivf_h_raw.rate;
    long lRateDenom = ivf_h_raw.scale;

    long nSamplesPerBlock = 1;

    long nBytes = 0;
    long nBytesMin = 999999;
    long nBytesMax = 0;

    fpos_t position;
    fgetpos(in, &position);

    __int64 priorTimeStamp = 0;

    while (currentVideoFrame < frameCount)
    {
        fseek(in , ivf_fhRaw.frameSize , SEEK_CUR);

        fread(&ivf_fhRaw.frameSize, 1, 4, in);
        fread(&ivf_fhRaw.timeStamp, 1, 8, in);
        vpxt_format_frame_header_read(ivf_fhRaw);

        while (ivf_fhRaw.timeStamp != priorTimeStamp + 2 && !feof(in))
        {
            if (PrintSwitch == 1)
                fprintf(out, "%i\n", currentVideoFrame);

            priorTimeStamp = priorTimeStamp + 2;
            dropedframecount++;
        }

        priorTimeStamp = ivf_fhRaw.timeStamp;

        currentVideoFrame ++;
    }

    if (PrintSwitch == 1)
        fclose(out);

    fclose(in);

    return dropedframecount;
}
int vpxt_display_resized_frames(const char *inputchar, int PrintSwitch)
{
    std::string ResizeInStr;
    vpxt_remove_file_extension(inputchar, ResizeInStr);
    ResizeInStr += "resized_frames.txt";

    FILE *out;

    if (PrintSwitch == 1)
    {
        out = fopen(ResizeInStr.c_str(), "w");
    }

    int                     use_y4m = 1;
    vpx_codec_ctx_t         decoder;
    const char             *fn = inputchar;
    int                     i;
    uint8_t                *buf = NULL;
    size_t                  buf_sz = 0, buf_alloc_sz = 0;
    FILE                   *infile;
    int                     frame_in = 0, frame_out = 0, flipuv = 0, noblit = 0;
    int                     do_md5 = 0, progress = 0;
    int                     stop_after = 0, postproc = 0, summary = 0, quiet =1;
    vpx_codec_iface_t      *iface = NULL;
    unsigned int            fourcc;
    unsigned long           dx_time = 0;
    const char             *outfile_pattern = 0;
    unsigned int            width;
    unsigned int            height;
    unsigned int            fps_den;
    unsigned int            fps_num;
    vpx_codec_dec_cfg_t     cfg = {0};
#if CONFIG_VP8_DECODER
    vp8_postproc_cfg_t      vp8_pp_cfg = {0};
    int                     vp8_dbg_color_ref_frame = 0;
    int                     vp8_dbg_color_mb_modes = 0;
    int                     vp8_dbg_color_b_modes = 0;
    int                     vp8_dbg_display_mv = 0;
#endif
    struct input_ctx        input;

    input.chunk = 0;
    input.chunks = 0;
    input.infile = NULL;
    input.kind = RAW_FILE;
    input.nestegg_ctx = 0;
    input.pkt = 0;
    input.video_track = 0;

    int CharCount = 0;

    /* Open file */
    infile = strcmp(fn, "-") ? fopen(fn, "rb") : set_binary_mode(stdin);
    if (!infile){
        tprintf(PRINT_BTH, "Failed to open input file: %s", fn);
        return -1;
    }

    input.infile = infile;

    if (file_is_ivf_dec(infile, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = IVF_FILE;
    else if (file_is_webm(&input, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = WEBM_FILE;
    else if (file_is_raw(infile, &fourcc, &width, &height, &fps_den, &fps_num))
        input.kind = RAW_FILE;
    else
    {
        fprintf(stderr, "Unrecognized input file type.\n");
        return EXIT_FAILURE;
    }

    std::string compformat;

    if (input.kind == IVF_FILE)
        compformat = "IVF";

    if (input.kind == WEBM_FILE)
        compformat = "Webm";

    if (input.kind == RAW_FILE)
        compformat = "Raw";

    /* Try to determine the codec from the fourcc. */
    for (i = 0; i < sizeof(ifaces) / sizeof(ifaces[0]); i++)
        if ((fourcc & ifaces[i].fourcc_mask) == ifaces[i].fourcc)
        {
            vpx_codec_iface_t  *ivf_iface = ifaces[i].iface;

            if (iface && iface != ivf_iface)
                tprintf(PRINT_STD, "Notice -- IVF header indicates codec: %s\n",
                        ifaces[i].name);
            else
                iface = ivf_iface;

            break;
        }

    unsigned int FrameSize = (width * height * 3) / 2;
    uint64_t TimeStamp = 0;

    if (vpx_codec_dec_init(&decoder, iface ? iface :  ifaces[0].iface, &cfg,
        postproc ? VPX_CODEC_USE_POSTPROC : 0))
    {
        tprintf(PRINT_STD, "Failed to initialize decoder: %s\n",
            vpx_codec_error(&decoder));
        fclose(infile);

        return -1;
    }

    int resizedIMGCount = 0;
    int frame = 0;
    uint64_t timestamp = 0;

    /* Decode file */
    while (!skim_frame_dec(&input, &buf, &buf_sz, &buf_alloc_sz, &timestamp))
    {
        vpx_codec_iter_t  iter = NULL;
        vpx_image_t    *img;
        struct vpx_usec_timer timer;

        vpx_usec_timer_start(&timer);

        if (vpx_codec_decode(&decoder, buf, buf_sz, NULL, 0))
        {
            const char *detail = vpx_codec_error_detail(&decoder);
            tprintf(PRINT_STD, "Failed to decode frame: %s\n",
                vpx_codec_error(&decoder));

            if (detail)
                tprintf(PRINT_STD, "  Additional information: %s\n", detail);

            goto fail;
        }

        vpx_usec_timer_mark(&timer);
        dx_time += vpx_usec_timer_elapsed(&timer);

        ++frame_in;

        if (!noblit)
        {
            if (img = vpx_codec_get_frame(&decoder, &iter))
            {
                if (img->d_w != width || img->d_h != height) // CheckFrameSize
                {
                    if (PrintSwitch == 0)
                    {
                        tprintf(PRINT_STD, "%i %i %i \n", frame, img->d_w,
                            img->d_h);
                    }

                    if (PrintSwitch == 1)
                    {
                        fprintf(out, "%i %i %i\n", frame, img->d_w, img->d_h);
                    }

                    resizedIMGCount++;
                }

                ++frame_out;
            }
        }

        ++frame;

        if (stop_after && frame_in >= stop_after)
            break;
    }

    if (summary || progress)
    {
        show_progress(frame_in, frame_out, dx_time);
        fprintf(stderr, "\n");
    }

fail:

    if (vpx_codec_destroy(&decoder))
    {
        tprintf(PRINT_STD, "Failed to destroy decoder: %s\n",
            vpx_codec_error(&decoder));
        fclose(infile);

        return -1;
    }

    if (input.nestegg_ctx)
        nestegg_destroy(input.nestegg_ctx);

    if (input.kind != WEBM_FILE)
        free(buf);

    fclose(infile);

    if (PrintSwitch == 1)
    {
        fclose(out);
    }

    return resizedIMGCount;
}
int vpxt_display_visible_frames(const char *input_file, int Selector)
{
    // 0 = just display
    // 1 = write to file
    std::string VisInStr;
    vpxt_remove_file_extension(input_file, VisInStr);
    VisInStr += "visible_frames.txt";

    int VisableCount = 0;

    std::ofstream outfile;

    if (Selector == 1)
    {
        outfile.open(VisInStr.c_str());

        if (!outfile.good())
        {
            tprintf(PRINT_BTH, "\nERROR: Could not open output file: %s\n",
                VisInStr.c_str());
            return 0;
        }
    }

    unsigned char  signature[4];    // ='DKIF';
    unsigned short version = 0;     //  -
    unsigned short headersize = 0;  //  -
    unsigned int fourcc = 0;        // good
    unsigned int width = 0;         // good
    unsigned int height = 0;        // good
    unsigned int rate = 0;          // good
    unsigned int scale = 0;         // good
    unsigned int length = 0;        // other measure

    signature[0] = ' ';
    signature[1] = ' ';
    signature[2] = ' ';
    signature[3] = ' ';

    std::vector<unsigned int>     frameSize;
    std::vector<uint64_t> timeStamp;

    FILE *infile = strcmp(input_file, "-") ? fopen(input_file, "rb") :
        set_binary_mode(stdin);

    if (!infile)
    {
        tprintf(PRINT_BTH, "Failed to open input file: %s", input_file);
        return -1;
    }

    struct input_ctx        input;

    input.chunk = 0;

    input.chunks = 0;

    input.infile = NULL;

    input.kind = RAW_FILE;

    input.nestegg_ctx = 0;

    input.pkt = 0;

    input.video_track = 0;

    input.infile = infile;

    if (file_is_ivf_dec(infile, &fourcc, &width, &height, &scale, &rate))
        input.kind = IVF_FILE;
    else if (file_is_webm(&input, &fourcc, &width, &height, &scale, &rate))
        input.kind = WEBM_FILE;
    else if (file_is_raw(infile, &fourcc, &width, &height, &scale, &rate))
        input.kind = RAW_FILE;
    else
    {
        fprintf(stderr, "Unrecognized input file type.\n");
        return EXIT_FAILURE;
    }

    if (input.kind == WEBM_FILE)
        if (webm_guess_framerate(&input, &scale, &rate))
        {
            fprintf(stderr, "Failed to guess framerate -- error parsing "
                    "webm file?\n");
            return EXIT_FAILURE;
        }

    std::string compformat;

    if (input.kind == IVF_FILE)
        compformat = "IVF";

    if (input.kind == WEBM_FILE)
        compformat = "Webm";

    if (input.kind == RAW_FILE)
        compformat = "Raw";

    if (input.kind == IVF_FILE)
    {
        rewind(infile);
        IVF_HEADER ivf_h_raw;
        InitIVFHeader(&ivf_h_raw);
        fread(&ivf_h_raw, 1, sizeof(ivf_h_raw), infile);

        version = ivf_h_raw.version;
        headersize = ivf_h_raw.headersize;
        fourcc = ivf_h_raw.four_cc;
        width = ivf_h_raw.width;
        height = ivf_h_raw.height;
        rate = ivf_h_raw.rate;
        scale = ivf_h_raw.scale;
        length = ivf_h_raw.length;

        signature[0] = ivf_h_raw.signature[0];
        signature[1] = ivf_h_raw.signature[1];
        signature[2] = ivf_h_raw.signature[2];
        signature[3] = ivf_h_raw.signature[3];
    }

    uint8_t               *buf = NULL;
    size_t               buf_sz = 0, buf_alloc_sz = 0;
    int currentVideoFrame = 0;
    int frame_avail = 0;
    uint64_t timestamp = 0;

    while (!frame_avail)
    {
        frame_avail = read_frame_dec(&input, &buf, &buf_sz, &buf_alloc_sz,
            &timestamp);

        if (frame_avail)
            break;

        // check to see if visible frame
        VP8_HEADER oz;

#if defined(_PPC)
        {
            int v = ((int *)buf)[0];
            v = swap4(v);
            oz.type = v & 1;
            oz.version = (v >> 1) & 7;
            oz.show_frame = (v >> 4) & 1;
            oz.first_partition_length_in_bytes = (v >> 5) & 0x7ffff;
        }
#else
        memcpy(&oz, buf, 3); // copy 3 bytes;
#endif
        unsigned int type = oz.type;
        unsigned int showFrame = oz.show_frame;
        unsigned int version = oz.version;

        if (showFrame == 1)
        {
            VisableCount++;

            if (Selector == 0)
                tprintf(PRINT_STD, "\n%i\n", currentVideoFrame);

            if (Selector == 1)
                outfile << currentVideoFrame << "\n";
        }

        currentVideoFrame ++;
    }


fail:

    if (input.nestegg_ctx)
        nestegg_destroy(input.nestegg_ctx);

    if (input.kind != WEBM_FILE)
        free(buf);

    fclose(infile);

    if (Selector == 1)
        outfile.close();

    return VisableCount;
}
int vpxt_display_alt_ref_frames(const char *input_file, int Selector)
{
    // 0 = just display
    // 1 = write to file

    std::string AltRefInStr;
    vpxt_remove_file_extension(input_file, AltRefInStr);
    AltRefInStr += "alt_ref_frames.txt";
    char output_file[255] = "";
    snprintf(output_file, 255, "%s", AltRefInStr.c_str());

    int AltRefCount = 0;

    std::ofstream outfile;

    if (Selector == 1)
    {
        outfile.open(AltRefInStr.c_str());

        if (!outfile.good())
        {
            tprintf(PRINT_BTH, "\nERROR: Could not open output file: %s\n",
                AltRefInStr.c_str());
            return 0;
        }
    }

    unsigned char  signature[4];    // ='DKIF';
    unsigned short version = 0;     //  -
    unsigned short headersize = 0;  //  -
    unsigned int fourcc = 0;        // good
    unsigned int width = 0;         // good
    unsigned int height = 0;        // good
    unsigned int rate = 0;          // good
    unsigned int scale = 0;         // good
    unsigned int length = 0;        // other measure

    signature[0] = ' ';
    signature[1] = ' ';
    signature[2] = ' ';
    signature[3] = ' ';

    std::vector<unsigned int>     frameSize;
    std::vector<uint64_t> timeStamp;

    FILE *infile = strcmp(input_file, "-") ? fopen(input_file, "rb") :
        set_binary_mode(stdin);

    if (!infile)
    {
        tprintf(PRINT_BTH, "Failed to open input file: %s", input_file);
        return -1;
    }

    struct input_ctx        input;

    input.chunk = 0;

    input.chunks = 0;

    input.infile = NULL;

    input.kind = RAW_FILE;

    input.nestegg_ctx = 0;

    input.pkt = 0;

    input.video_track = 0;

    input.infile = infile;

    if (file_is_ivf_dec(infile, &fourcc, &width, &height, &scale, &rate))
        input.kind = IVF_FILE;
    else if (file_is_webm(&input, &fourcc, &width, &height, &scale, &rate))
        input.kind = WEBM_FILE;
    else if (file_is_raw(infile, &fourcc, &width, &height, &scale, &rate))
        input.kind = RAW_FILE;
    else
    {
        fprintf(stderr, "Unrecognized input file type.\n");
        return EXIT_FAILURE;
    }

    if (input.kind == WEBM_FILE)
        if (webm_guess_framerate(&input, &scale, &rate))
        {
            fprintf(stderr, "Failed to guess framerate -- error parsing "
                    "webm file?\n");
            return EXIT_FAILURE;
        }

    std::string compformat;

    if (input.kind == IVF_FILE)
        compformat = "IVF";

    if (input.kind == WEBM_FILE)
        compformat = "Webm";

    if (input.kind == RAW_FILE)
        compformat = "Raw";

    if (input.kind == IVF_FILE)
    {
        rewind(infile);
        IVF_HEADER ivf_h_raw;
        InitIVFHeader(&ivf_h_raw);
        fread(&ivf_h_raw, 1, sizeof(ivf_h_raw), infile);

        version = ivf_h_raw.version;
        headersize = ivf_h_raw.headersize;
        fourcc = ivf_h_raw.four_cc;
        width = ivf_h_raw.width;
        height = ivf_h_raw.height;
        rate = ivf_h_raw.rate;
        scale = ivf_h_raw.scale;
        length = ivf_h_raw.length;

        signature[0] = ivf_h_raw.signature[0];
        signature[1] = ivf_h_raw.signature[1];
        signature[2] = ivf_h_raw.signature[2];
        signature[3] = ivf_h_raw.signature[3];
    }

    uint8_t               *buf = NULL;
    size_t               buf_sz = 0, buf_alloc_sz = 0;
    int currentVideoFrame = 0;
    int frame_avail = 0;
    uint64_t timestamp = 0;

    while (!frame_avail)
    {
        frame_avail = read_frame_dec(&input, &buf, &buf_sz, &buf_alloc_sz,
            &timestamp);

        if (frame_avail)
            break;

        // check to see if visible frame
        VP8_HEADER oz;

#if defined(_PPC)
        {
            int v = ((int *)buf)[0];
            v = swap4(v);
            oz.type = v & 1;
            oz.version = (v >> 1) & 7;
            oz.show_frame = (v >> 4) & 1;
            oz.first_partition_length_in_bytes = (v >> 5) & 0x7ffff;
        }
#else
        memcpy(&oz, buf, 3); // copy 3 bytes;
#endif
        unsigned int type = oz.type;
        unsigned int showFrame = oz.show_frame;
        unsigned int version = oz.version;

        if (showFrame == 0)
        {
            AltRefCount++;

            if (Selector == 0)
                tprintf(PRINT_STD, "\n%i\n", currentVideoFrame);

            if (Selector == 1)
                outfile << currentVideoFrame << "\n";
        }

        currentVideoFrame ++;
    }


fail:

    if (input.nestegg_ctx)
        nestegg_destroy(input.nestegg_ctx);

    if (input.kind != WEBM_FILE)
        free(buf);

    fclose(infile);

    if (Selector == 1)
        outfile.close();

    return AltRefCount;
}
int vpxt_display_key_frames(const char *input_file, int Selector)
{
    int keyframecount = 0;

    std::string KeyInStr;
    vpxt_remove_file_extension(input_file, KeyInStr);
    KeyInStr += "key_frames.txt";
    char output_file[255] = "";
    snprintf(output_file, 255, "%s", KeyInStr.c_str());

    std::ofstream outfile;

    if (Selector == 1)
    {
        outfile.open(KeyInStr.c_str());

        if (!outfile.good())
        {
            tprintf(PRINT_BTH, "\nERROR: Could not open output file: %s\n",
                KeyInStr.c_str());
            return 0;
        }
    }

    unsigned char  signature[4];    // ='DKIF';
    unsigned short version = 0;     //  -
    unsigned short headersize = 0;  //  -
    unsigned int fourcc = 0;        // good
    unsigned int width = 0;         // good
    unsigned int height = 0;        // good
    unsigned int rate = 0;          // good
    unsigned int scale = 0;         // good
    unsigned int length = 0;        // other measure

    signature[0] = ' ';
    signature[1] = ' ';
    signature[2] = ' ';
    signature[3] = ' ';

    FILE *infile = strcmp(input_file, "-") ? fopen(input_file, "rb") :
        set_binary_mode(stdin);

    if (!infile)
    {
        tprintf(PRINT_BTH, "Failed to open input file: %s", input_file);
        return -1;
    }

    struct input_ctx        input;

    input.chunk = 0;

    input.chunks = 0;

    input.infile = NULL;

    input.kind = RAW_FILE;

    input.nestegg_ctx = 0;

    input.pkt = 0;

    input.video_track = 0;

    input.infile = infile;

    if (file_is_ivf_dec(infile, &fourcc, &width, &height, &scale, &rate))
        input.kind = IVF_FILE;
    else if (file_is_webm(&input, &fourcc, &width, &height, &scale, &rate))
        input.kind = WEBM_FILE;
    else if (file_is_raw(infile, &fourcc, &width, &height, &scale, &rate))
        input.kind = RAW_FILE;
    else
    {
        fprintf(stderr, "Unrecognized input file type.\n");
        return EXIT_FAILURE;
    }

    if (input.kind == WEBM_FILE)
        if (webm_guess_framerate(&input, &scale, &rate))
        {
            fprintf(stderr, "Failed to guess framerate -- error parsing "
                    "webm file?\n");
            return EXIT_FAILURE;
        }

    std::string compformat;

    if (input.kind == IVF_FILE)
        compformat = "IVF";

    if (input.kind == WEBM_FILE)
        compformat = "Webm";

    if (input.kind == RAW_FILE)
        compformat = "Raw";

    if (input.kind == IVF_FILE)
    {
        rewind(infile);
        IVF_HEADER ivf_h_raw;
        InitIVFHeader(&ivf_h_raw);
        fread(&ivf_h_raw, 1, sizeof(ivf_h_raw), infile);

        version = ivf_h_raw.version;
        headersize = ivf_h_raw.headersize;
        fourcc = ivf_h_raw.four_cc;
        width = ivf_h_raw.width;
        height = ivf_h_raw.height;
        rate = ivf_h_raw.rate;
        scale = ivf_h_raw.scale;
        length = ivf_h_raw.length;

        signature[0] = ivf_h_raw.signature[0];
        signature[1] = ivf_h_raw.signature[1];
        signature[2] = ivf_h_raw.signature[2];
        signature[3] = ivf_h_raw.signature[3];
    }

    uint8_t               *buf = NULL;
    size_t               buf_sz = 0, buf_alloc_sz = 0;
    int currentVideoFrame = 0;
    int frame_avail = 0;
    uint64_t timestamp = 0;

    while (!frame_avail)
    {
        frame_avail = read_frame_dec(&input, &buf, &buf_sz, &buf_alloc_sz,
            &timestamp);

        if (frame_avail)
            break;

        // check to see if visible frame
        VP8_HEADER oz;

#if defined(_PPC)
        {
            int v = ((int *)buf)[0];
            v = swap4(v);
            oz.type = v & 1;
            oz.version = (v >> 1) & 7;
            oz.show_frame = (v >> 4) & 1;
            oz.first_partition_length_in_bytes = (v >> 5) & 0x7ffff;
        }
#else
        memcpy(&oz, buf, 3); // copy 3 bytes;
#endif
        unsigned int type = oz.type;
        unsigned int showFrame = oz.show_frame;
        unsigned int version = oz.version;

        if (type == 0)
        {
            keyframecount++;

            if (Selector == 0)
                tprintf(PRINT_STD, "\n%i\n", currentVideoFrame);

            if (Selector == 1)
                outfile << currentVideoFrame << "\n";
        }

        currentVideoFrame ++;
    }

fail:

    if (input.nestegg_ctx)
        nestegg_destroy(input.nestegg_ctx);

    if (input.kind != WEBM_FILE)
        free(buf);

    fclose(infile);

    if (Selector == 1)
        outfile.close();

    return keyframecount;
}
int vpxt_lag_in_frames_check(const char *QuantInChar)
{
    std::ifstream Quantinfile(QuantInChar);

    if (!Quantinfile.good())
    {
        tprintf(PRINT_BTH, "\nError: Cannot open file: %s\n", QuantInChar);
        return -1;
    }

    int LagCounter = 0;
    int frame = 0;
    int quantizer = 0;
    int LagInFramesFound = 0;

    while (!Quantinfile.eof())
    {
        Quantinfile >> frame;
        Quantinfile >> quantizer;

        if (frame != 0)
        {
            if (LagInFramesFound == 1)
                LagInFramesFound = 0;

            break;
        }
        else
        {
            LagInFramesFound++;
        }
    }

    Quantinfile.close();
    return LagInFramesFound;
    ////////////////////////////////////////////////////////
}
int vpxt_dfwm_check(const char *InputFile, int printselect)
{
    // return 0 = pass
    // return 1 = fail
    // return -1 = error
    // return -3 = threshold never reached

    std::string CheckPBMThreshStr;
    vpxt_remove_file_extension(InputFile, CheckPBMThreshStr);
    CheckPBMThreshStr += "CheckPBMThresh.txt";

    std::string ResizeFramesStr;
    vpxt_remove_file_extension(InputFile, ResizeFramesStr);
    ResizeFramesStr += "resized_frames.txt";

    std::string KeyFramesStr;
    vpxt_remove_file_extension(InputFile, KeyFramesStr);
    KeyFramesStr += "key_frames.txt";

    std::ifstream CheckPBMFile(CheckPBMThreshStr.c_str());

    if (!CheckPBMFile.good())
    {
        tprintf(PRINT_BTH, "\nError: Cannot open file: %s\n",
            CheckPBMThreshStr.c_str());
        CheckPBMFile.close();
        return -1;
    }

    std::ifstream ResizeFramesFile(ResizeFramesStr.c_str());

    if (!ResizeFramesFile.good())
    {
        tprintf(PRINT_BTH, "\nError: Cannot open file: %s\n",
            ResizeFramesStr.c_str());
        ResizeFramesFile.close();
        CheckPBMFile.close();
        return -1;
    }

    std::ifstream KeyFramesFile(KeyFramesStr.c_str());

    if (!KeyFramesFile.good())
    {
        tprintf(PRINT_BTH, "\nError: Cannot open file: %s\n",
            KeyFramesStr.c_str());
        KeyFramesFile.close();
        ResizeFramesFile.close();
        CheckPBMFile.close();
        return -1;
    }

    int fail = 0;

    int firstResizedFrame = 0;
    ResizeFramesFile >> firstResizedFrame;

    int curkeyframe = 0;
    int garbage = 0;
    int CheckPBMStatus = -1;
    // if threshold never hit this is 0 if it is hit somewhere it is 1
    int ThresholdTrigger = 0;


    KeyFramesFile >> curkeyframe; // get past trivial 0 case

    while (curkeyframe < firstResizedFrame)
    {
        KeyFramesFile >> curkeyframe;

        int curThreshFrame = 0;
        CheckPBMFile.seekg(std::ios::beg);
        // get threshold status for frame just prior to keyframe
        while (!CheckPBMFile.eof() && curThreshFrame < curkeyframe)
        {
            CheckPBMFile >> garbage;
            CheckPBMFile >> CheckPBMStatus;

            if (CheckPBMStatus == 1)
            {
                ThresholdTrigger = 1;
            }

            curThreshFrame++;
        }
        // if keyframe is also first resized frame then the threshold status for
        // the frame prior to it shoudl be 1 if not it fails
        if (curkeyframe == firstResizedFrame)
        {
            if (CheckPBMStatus == 1)
            {
                if (printselect == 1)
                {
                    tprintf(PRINT_STD, "For Key Frame %4i; frame %4i under "
                        "buffer level for first resized frame -Pass\n",
                        curkeyframe, curkeyframe - 1);
                }
            }
            else
            {
                if (printselect == 1)
                {
                    tprintf(PRINT_STD, "For Key Frame %4i; frame %4i not under "
                        "buffer level for first resized frame -Fail\n",
                        curkeyframe, curkeyframe - 1);
                }

                fail = 1;
            }
        }
        else
        {
            // if key frame isnt first resized frame then the threshold status
            // for the frame prior to it should be 0 if not it fails
            if (CheckPBMStatus == 0)
            {
                if (printselect == 1)
                {
                    tprintf(PRINT_STD, "For Key Frame %4i; frame %4i not under "
                        "buffer level -Pass\n", curkeyframe, curkeyframe - 1);
                }
            }
            else
            {
                if (printselect == 1)
                {
                    tprintf(PRINT_STD, "For Key Frame %4i; frame %4i under "
                        "buffer level -Fail\n", curkeyframe, curkeyframe - 1);
                }

                fail = 1;
            }
        }
    }

    KeyFramesFile.close();
    ResizeFramesFile.close();
    CheckPBMFile.close();

    if (ThresholdTrigger == 1)
    {
        return fail;
    }
    else
    {
        return -3;
    }

    ////////////////////////////////////////////////////////
}
double vpxt_print_frame_statistics(const char *input_file1,
                 const char *input_file2,
                 const char *output_file,
                 int force_uvswap,
                 int print_out,
                 int print_embl,
                 int print_drop_frame,
                 int print_resized_frame,
                 int print_key_frame,
                 int print_non_visible_frame,
                 int print_frame_size)
{
    double summed_quality = 0;
    double summed_weights = 0;
    double summed_psnr = 0;
    double summed_ypsnr = 0;
    double summed_upsnr = 0;
    double summed_vpsnr = 0;
    double sum_sq_error = 0;
    double sum_bytes = 0;
    double sum_bytes2 = 0;

    unsigned int             decoded_frames = 0;
    int                      raw_offset = 0;
    int                      comp_offset = 0;
    unsigned int             maximumFrameCount = 0;
    vpx_image_t              raw_img;
    unsigned int             frameCount = 0;
    unsigned int             file_type = FILE_TYPE_RAW;
    unsigned int             fourcc    = 808596553;
    struct                   detect_buffer detect;
    unsigned int             raw_width = 0;
    unsigned int             raw_height = 0;
    unsigned int             raw_rate = 0;
    unsigned int             raw_scale = 0;
    y4m_input                y4m;
    int                      arg_have_framerate = 0;
    struct vpx_rational      arg_framerate = {30, 1};
    int                      arg_use_i420 = 1;
    FILE *out_file;

    if(output_file)
        out_file = fopen(output_file, "wb");

    force_uvswap = 0;
    //////////////////////// Initilize Raw File ////////////////////////
    FILE *raw_file = strcmp(input_file1, "-") ?
        fopen(input_file1, "rb") : set_binary_mode(stdin);

    if (!raw_file)
    {
        tprintf(print_out, "Failed to open input file: %s", input_file1);
        return -1;
    }

    detect.buf_read = fread(detect.buf, 1, 4, raw_file);
    detect.position = 0;

    if (detect.buf_read == 4 && file_is_y4m(raw_file, &y4m, detect.buf))
    {
        if (y4m_input_open(&y4m, raw_file, detect.buf, 4) >= 0)
        {
            file_type = FILE_TYPE_Y4M;
            raw_width = y4m.pic_w;
            raw_height = y4m.pic_h;
            raw_rate = y4m.fps_n;
            raw_scale = y4m.fps_d;

            /* Use the frame rate from the file only if none was specified
            * on the command-line.
            */
            if (!arg_have_framerate)
            {
                arg_framerate.num = y4m.fps_n;
                arg_framerate.den = y4m.fps_d;
            }

            arg_use_i420 = 0;
        }
        else
        {
            tprintf(print_out, "Unsupported Y4M stream.\n");
            return EXIT_FAILURE;
        }
    }
    else if (detect.buf_read == 4 &&
        file_is_ivf(raw_file, &fourcc, &raw_width, &raw_height, &detect,
            &raw_scale, &raw_rate))
    {
        switch (fourcc)
        {
        case 0x32315659:
            arg_use_i420 = 0;
            break;
        case 0x30323449:
            arg_use_i420 = 1;
            break;
        default:
            tprintf(print_out, "Unsupported fourcc (%08x) in IVF\n", fourcc);
            return EXIT_FAILURE;
        }

        file_type = FILE_TYPE_IVF;
    }
    else
    {
        file_type = FILE_TYPE_RAW;
    }

    if (!raw_width || !raw_height)
    {
        tprintf(print_out, "Specify stream dimensions with --width (-w) "
            " and --height (-h).\n");
        return EXIT_FAILURE;
    }

    if(file_type != FILE_TYPE_Y4M)
        vpx_img_alloc(&raw_img, IMG_FMT_I420, raw_width, raw_height, 1);

    // Burn Frames untill Raw frame offset reached - currently disabled by
    // override of raw_offset
    if (raw_offset > 0)
    {
        for (int i = 0; i < raw_offset; i++)
        {
        }
    }

    // I420 hex-0x30323449 dec-808596553
    // YV12 hex-0x32315659 dec-842094169
    // if YV12 Do not swap Frames
    if (fourcc == 842094169)
        force_uvswap = 1;

    //////////////////////// Initilize Compressed File ////////////////////////
    /* Open file */
    FILE *comp_file = strcmp(input_file2, "-") ?
        fopen(input_file2, "rb") : set_binary_mode(stdin);

    if (!comp_file)
    {
        tprintf(print_out, "Failed to open input file: %s", input_file2);
        return -1;
    }

    unsigned int            comp_fourcc;
    unsigned int            comp_width;
    unsigned int            comp_height;
    unsigned int            comp_scale;
    unsigned int            comp_rate;
    struct input_ctx        input;

    input.chunk = 0;
    input.chunks = 0;
    input.infile = NULL;
    input.kind = RAW_FILE;
    input.nestegg_ctx = 0;
    input.pkt = 0;
    input.video_track = 0;

    input.infile = comp_file;

    if (file_is_ivf_dec(comp_file, &comp_fourcc, &comp_width, &comp_height,
        &comp_scale, &comp_rate))
        input.kind = IVF_FILE;
    else if (file_is_webm(&input, &comp_fourcc, &comp_width, &comp_height,
        &comp_scale, &comp_rate))
        input.kind = WEBM_FILE;
    else if (file_is_raw(comp_file, &comp_fourcc, &comp_width, &comp_height,
        &comp_scale, &comp_rate))
        input.kind = RAW_FILE;
    else
    {
        tprintf(print_out, "Unrecognized input file type.\n");
        return EXIT_FAILURE;
    }

    if (input.kind == WEBM_FILE)
        if (webm_guess_framerate(&input, &comp_scale, &comp_rate))
        {
            tprintf(print_out, "Failed to guess framerate -- error parsing "
                "webm file?\n");
            return EXIT_FAILURE;
        }

        // Burn Frames untill Compressed frame offset reached - currently
        // disabled by override of comp_offset
        if (comp_offset > 0)
        {
        }

        ////////////////////////////////////////////////////////////////////////
        //////// Printing ////////
        if (print_embl)
            tprintf(print_out, "\n\n                        "
                "---------Computing PSNR---------");

        tprintf(print_out, "\n\nComparing %s to %s:\n                        \n"
            , input_file1, input_file2);
        ////////////////////////
        int deblock_level2 = 0;
        int noise_level2 = 0;
        int flags2 = 0;
        int currentFrame2 = 0;

        vpx_codec_ctx_t       decoder;
        vpx_codec_iface_t       *iface = NULL;
        const struct codec_item  *codec = codecs;
        vpx_codec_iface_t  *ivf_iface = ifaces[0].iface;
        vpx_codec_dec_cfg_t     cfg = {0};
        iface = ivf_iface;

        vp8_postproc_cfg_t ppcfg = {0};

        ppcfg.deblocking_level = deblock_level2;
        ppcfg.noise_level = noise_level2;
        ppcfg.post_proc_flag = flags2;

        if (vpx_codec_dec_init(&decoder, ifaces[0].iface, &cfg, 0))
        {
            tprintf(print_out, "Failed to initialize decoder: %s\n",
                vpx_codec_error(&decoder));

            fclose(raw_file);
            fclose(comp_file);
            vpx_img_free(&raw_img);
            return EXIT_FAILURE;
        }

        /////////// Setup Temp YV12 Buffer to Resize if nessicary //////////////
        initialize_scaler();

        YV12_BUFFER_CONFIG temp_yv12;
        YV12_BUFFER_CONFIG temp_yv12b;

        memset(&temp_yv12, 0, sizeof(temp_yv12));
        memset(&temp_yv12b, 0, sizeof(temp_yv12b));
        ////////////////////////////////////////////////////////////////////////

        vpx_codec_control(&decoder, VP8_SET_POSTPROC, &ppcfg);

        uint8_t *comp_buff = NULL;
        size_t buf_sz = 0, buf_alloc_sz = 0;
        int current_raw_frame = 0;
        int comp_frame_available = 1;

        uint64_t raw_timestamp = 0;
        uint64_t comp_timestamp = 0;

        while (comp_frame_available)
        {
            unsigned long lpdwFlags = 0;
            unsigned long lpckid = 0;
            long bytes1;
            long bytes2;
            int resized_frame = 0;
            int dropped_frame = 0;
            int non_visible_frame = 0;
            int key_frame = 0;
            buf_sz = 0;

            VP8_HEADER oz;

            if(read_frame_dec(&input, &comp_buff, &buf_sz, &buf_alloc_sz,
                &comp_timestamp)){
                    comp_frame_available = 0;
            }

            memcpy(&oz, comp_buff, 3);

            if(comp_frame_available && !oz.type)
                key_frame = 1;

            if(comp_frame_available && !oz.show_frame)
                non_visible_frame = 1;

            if (input.kind == WEBM_FILE){
                raw_timestamp = current_raw_frame * 1000 * raw_scale / raw_rate;
                raw_timestamp = raw_timestamp * 1000000;
            }
            else
                raw_timestamp = current_raw_frame;

            bytes2 = buf_sz;
            sum_bytes2 += bytes2;

            vpx_codec_iter_t  iter = NULL;
            vpx_image_t    *img;

            // make sure the timestamps sync otherwise process
            // last shown compressed frame
            while(raw_timestamp <= comp_timestamp || !comp_frame_available)
            {
                if(comp_timestamp == raw_timestamp)
                {
                    dropped_frame = 0;

                    if (vpx_codec_decode(&decoder, (uint8_t *) comp_buff,
                        buf_sz, NULL, 0))
                    {
                        const char *detail = vpx_codec_error_detail(&decoder);
                        tprintf(print_out, "Failed to decode frame: %s\n",
                            vpx_codec_error(&decoder));
                    }

                    if (img = vpx_codec_get_frame(&decoder, &iter))
                    {
                        ++decoded_frames;

                        // if frame not correct size flag as resized
                        if (img->d_w != raw_width || img->d_h != raw_height)
                            resized_frame = 1;
                    }
                }
                else
                    dropped_frame = 1;

                // keep getting frames from raw file
                if(img || dropped_frame || !comp_frame_available)
                {
                    ////////////////// Get YV12 Data For Raw File //////////////
                    // if end of uncompressed file break out
                    if(!read_frame_enc(raw_file, &raw_img, file_type, &y4m,
                        &detect))
                        break;

                    bytes1 = (raw_width * raw_height * 3) / 2;
                    sum_bytes += bytes1;

                    current_raw_frame = current_raw_frame + 1;
                    if (input.kind == WEBM_FILE){
                        raw_timestamp = current_raw_frame * 1000 * raw_scale /
                            raw_rate;
                        raw_timestamp = raw_timestamp * 1000000;
                    }
                    else
                        raw_timestamp = current_raw_frame;
                }
                else
                {
                    // if time stamps do not match find out why if droped
                    // frame use last frame keep trying subtract one unit
                    // to move along.  if invisible frame get next frame
                    // and keep going.
                    if (oz.show_frame){
                        current_raw_frame = current_raw_frame - 1;

                        if (input.kind == WEBM_FILE){
                            raw_timestamp = current_raw_frame * 1000 *
                                raw_scale / raw_rate;
                            raw_timestamp = raw_timestamp * 1000000;
                        }
                        else
                            raw_timestamp = current_raw_frame;
                    }
                    else{
                        if(read_frame_dec(&input, &comp_buff, &buf_sz,
                            &buf_alloc_sz, &comp_timestamp))
                                comp_frame_available = 0;

                        raw_timestamp = comp_timestamp;
                        non_visible_frame = 1;
                    }
                }

                //////// Printing ////////
                if(output_file)
                {
                    fprintf(out_file, "F:%5d", current_raw_frame);

                    if(print_frame_size)
                        fprintf(out_file, " 1:%6.0f 2:%6.0f",
                        bytes1 * 8.0,
                        bytes2 * 8.0);

                    if(dropped_frame && print_drop_frame)
                        fprintf(out_file, " D");

                    if(!dropped_frame && resized_frame && print_resized_frame)
                        fprintf(out_file, " R");

                    if(!dropped_frame && key_frame && print_key_frame)
                        fprintf(out_file, " K");

                    if(!dropped_frame && non_visible_frame &&
                        print_non_visible_frame)
                        fprintf(out_file, " N");

                    fprintf(out_file, "\n");
                }

                tprintf(print_out, "F:%5d", current_raw_frame);

                if(print_frame_size)
                    tprintf(print_out, " 1:%6.0f 2:%6.0f",
                    bytes1 * 8.0,
                    bytes2 * 8.0);

                if(dropped_frame && print_drop_frame)
                    tprintf(print_out, " D");

                if(!dropped_frame && resized_frame && print_resized_frame)
                    tprintf(print_out, " R");

                if(!dropped_frame && key_frame && print_key_frame)
                    tprintf(print_out, " K");

                if(!dropped_frame && non_visible_frame &&
                    print_non_visible_frame)
                    tprintf(print_out, " N");

                tprintf(print_out, "\n");
                ////////////////////////
            }
        }

        //////// Printing ////////
        if (print_embl)
            tprintf(print_out, "\n                        "
                "--------------------------------\n");
        ////////////////////////

        fclose(raw_file);
        fclose(comp_file);
        vp8_yv12_de_alloc_frame_buffer(&temp_yv12);
        vp8_yv12_de_alloc_frame_buffer(&temp_yv12b);

        if(file_type != FILE_TYPE_Y4M)
            vpx_img_free(&raw_img);

        if(file_type == FILE_TYPE_Y4M)
            y4m_input_close(&y4m);

        vpx_codec_destroy(&decoder);

        if (input.nestegg_ctx)
            nestegg_destroy(input.nestegg_ctx);

        if (input.kind != WEBM_FILE)
            free(comp_buff);

        fclose(out_file);

        return 0;
}
int vpxt_eval_frame_stats_temp_scale(const char *input_file, int pattern)
{
    int i;
    int dropped_count = pattern; // for first frame
    int dropped_current = 0;
    int line_count = 0;
    char buffer[256];
    std::fstream infile;
    infile.open(input_file);

    while (!infile.eof())
    {
        i = 0;
        dropped_current = 0;
        infile.getline(buffer, 256);
        line_count ++;

        // evaluate line look for 'D'
        while(buffer[i] && i < 256)
        {
            if(buffer[i] == 'D'){
                dropped_current = 1;
                dropped_count = dropped_count + 1;
                break;
            }
            i++;
        }
        // if 'D' found
        if(!dropped_current && buffer[0]){
            // if the correct number of frames not droped return fail
            if(dropped_count != pattern)
                return 0;
            else // otherwise reset and keep going
                dropped_count = 0;
        }

    }

    infile.close();
    return 1;
}
int vpxt_check_min_quantizer(const char *input_file, int MinQuantizer)
{
    char QuantDispNameChar[255] = "";
    vpxt_file_name(input_file, QuantDispNameChar, 0);

    tprintf(PRINT_BTH, "Checking %s min quantizer:\n", QuantDispNameChar);

    std::string QuantInStr;
    vpxt_remove_file_extension(input_file, QuantInStr);
    QuantInStr += "quantizers.txt";

    std::ifstream infile(QuantInStr.c_str());

    if (!infile.good())
    {
        tprintf(PRINT_BTH, "\nError: Cannot open file: %s\n",
            QuantInStr.c_str());
        return 0;
    }

    int frame = 0;
    int quantizer = 0;
    int result = 1;

    while (!infile.eof())
    {
        infile >> frame;
        infile >> quantizer;

        if (frame == 0)
        {
            if (quantizer != 0 && quantizer < MinQuantizer)
            {
                tprintf(PRINT_BTH, "     Quantizer not >= min for frame %i q=%i"
                    " - Failed", frame, quantizer);
                infile.close();
                return frame;
            }
        }
        else
        {
            if (quantizer < MinQuantizer)
            {
                tprintf(PRINT_BTH, "     Quantizer not >= min for frame %i q=%i"
                    " - Failed", frame, quantizer);
                infile.close();
                return frame;
            }
        }
    }

    tprintf(PRINT_BTH, "     All quantizers >= min for all frames - Passed");
    infile.close();
    return -1; // result > -1 -> fail | result = -1 pass
}
int vpxt_check_max_quantizer(const char *input_file, int MaxQuantizer)
{
    char QuantDispNameChar[255] = "";
    vpxt_file_name(input_file, QuantDispNameChar, 0);

    tprintf(PRINT_BTH, "Checking %s max quantizer:\n", QuantDispNameChar);

    std::string QuantInStr;
    vpxt_remove_file_extension(input_file, QuantInStr);
    QuantInStr += "quantizers.txt";

    std::ifstream infile(QuantInStr.c_str());

    if (!infile.good())
    {
        tprintf(PRINT_BTH, "\nError: Cannot open file: %s\n",
            QuantInStr.c_str());
        return 0;
    }

    int frame = 0;
    int quantizer = 0;
    int result = 1;

    while (!infile.eof())
    {
        infile >> frame;
        infile >> quantizer;

        if (frame == 0)
        {
            if (quantizer != 0 && quantizer > MaxQuantizer)
            {
                tprintf(PRINT_BTH, "     Quantizer not <= max for frame %i q=%i"
                    " - Failed", frame, quantizer);
                infile.close();
                return frame;
            }
        }
        else
        {
            if (quantizer > MaxQuantizer)
            {
                tprintf(PRINT_BTH, "     Quantizer not <= max for frame %i q=%i"
                    " - Failed", frame, quantizer);
                infile.close();
                return frame;
            }
        }
    }

    tprintf(PRINT_BTH, " All quantizers <= max for all frames");
    infile.close();
    return -1; // result > -1 -> fail | result = -1 pass
}
int vpxt_check_fixed_quantizer(const char *input_file, int FixedQuantizer)
{
    char QuantDispNameChar[255] = "";
    vpxt_file_name(input_file, QuantDispNameChar, 0);

    tprintf(PRINT_BTH, "Checking %s fixed quantizer:", QuantDispNameChar);

    std::string QuantInStr;
    vpxt_remove_file_extension(input_file, QuantInStr);
    QuantInStr += "quantizers.txt";

    std::ifstream infile(QuantInStr.c_str());

    if (!infile.good())
    {
        tprintf(PRINT_BTH, "\nError: Cannot open file: %s\n",
            QuantInStr.c_str());
        return 0;
    }

    int frame = 0;
    int quantizer = 0;
    int result = 1;

    while (!infile.eof())
    {
        infile >> frame;
        infile >> quantizer;

        if (frame == 0)
        {
            if (quantizer != 0 && quantizer != FixedQuantizer)
            {
                tprintf(PRINT_BTH, " not fixed for frame %i q=%i - Failed",
                    frame, quantizer);
                return frame;
            }
        }
        else
        {
            if (quantizer != FixedQuantizer)
            {
                tprintf(PRINT_BTH, " not fixed for frame %i q=%i", frame,
                    quantizer);
                return frame;
            }
        }
    }

    tprintf(PRINT_BTH, " fixed for all frames");

    return -1; // result > -1 -> fail | result = -1 pass
}
int vpxt_time_return(const char *infile, int FileType)
{
    int speed;

    std::string FullName;

    vpxt_remove_file_extension(infile, FullName);


    if (FileType == 0)
        FullName += "compression_time.txt";

    if (FileType == 1)
        FullName += "decompression_time.txt";

    std::ifstream infile2(FullName.c_str());

    if (!infile2.is_open())
    {
        tprintf(PRINT_BTH, "File: %s not opened", FullName.c_str());
        return 0;
    }

    infile2 >> speed;
    infile2.close();

    return speed;
}
int vpxt_cpu_tick_return(const char *infile, int FileType)
{
    int speed;

    std::string FullName;

    vpxt_remove_file_extension(infile, FullName);


    if (FileType == 0)
        FullName += "compression_cpu_tick.txt";

    if (FileType == 1)
        FullName += "decompression_cpu_tick.txt";

    std::ifstream infile2(FullName.c_str());

    if (!infile2.is_open())
    {
        tprintf(PRINT_BTH, "File: %s not opened", FullName.c_str());
        return 0;
    }

    infile2 >> speed;
    infile2.close();

    return speed;
}
int vpxt_check_force_key_frames(const char *KeyFrameoutputfile,
                                int ForceKeyFrameInt,
                                const char *ForceKeyFrame)
{
    // return - 1 if file not found
    // return 1 if failed
    // return 0 if passed

    std::ifstream infile(KeyFrameoutputfile);

    if (!infile.good())
    {
        tprintf(PRINT_BTH, "\nKey Frame File Not Present\n");
        // fclose(fp);
        return -1;
    }

    tprintf(PRINT_BTH, "\n\nResults:\n\n");

    int fail = 0;
    int selector2 = 0;
    int doOnce2 = 0;
    int x2;
    int y2;
    int framecount = 0;

    while (!infile.eof())
    {
        if (selector2 == 1)
        {
            infile >> x2;
            selector2 = 2;
        }
        else
        {
            infile >> y2;
            selector2 = 1;
        }

        if (doOnce2 == 0)
        {
            x2 = y2;
            infile >> y2;
            doOnce2 = 1;
            selector2 = 1;
        }

        if (vpxt_abs_int(y2 - x2) != ForceKeyFrameInt)
        {
            vpxt_formated_print(5, "Key Frames do not occur only when Force Key"
                " Frame dictates: %i - Failed", ForceKeyFrameInt);
            tprintf(PRINT_BTH, "\n");
            fail = 1;
        }
    }

    int maxKeyFrame = 0;

    if (x2 > y2)
    {
        maxKeyFrame = x2;
    }
    else
    {
        maxKeyFrame = y2;
    }

    int NumberofFrames = vpxt_get_number_of_frames(ForceKeyFrame);

    if (NumberofFrames - 1 >= (maxKeyFrame + ForceKeyFrameInt))
    {
        vpxt_formated_print(RESPRT, "Key Frames do not occur only when Force "
            "Key Frame dictates: %i - Failed", ForceKeyFrameInt);
        tprintf(PRINT_BTH, "\n");
        fail = 1;
    }

    return fail;
}
int vpxt_check_mem_state(const std::string FileName, std::string &bufferString)
{
    std::ifstream infile4(FileName.c_str());

    if (!infile4.good())
        return -1;

    char buffer4[256];

    infile4.getline(buffer4, 256);
    infile4.getline(buffer4, 256);
    infile4.getline(buffer4, 256);
    infile4.getline(buffer4, 256);

    bufferString = buffer4;

    return 0;
}
int vpxt_print_compare_ivf_results(int lng_rc, int printErr)
{
    // return 1 for files identical
    // retrun -1 for files not identical
    // return 0 on error

    if (lng_rc >= 0)
    {
        tprintf(PRINT_BTH, "Files differ at frame: %i\n", lng_rc);
        return -1;
    }

    if (lng_rc == -1)
    {
        tprintf(PRINT_BTH, "Files are identical\n");
        return 1;
    }

    if (lng_rc == -2)
    {
        tprintf(PRINT_BTH, "File 2 ends before File 1\n");
        return -1;
    }

    if (lng_rc == -3)
    {
        tprintf(PRINT_BTH, "File 1 ends before File 2\n");
        return -1;
    }

    return 0;
}
double vpxt_get_psnr(const char *compFileName)
{
    // Takes in a compression full path and looks for the psnr file
    // accociated with it.

    std::string TimeTextFileStr;
    vpxt_remove_file_extension(compFileName, TimeTextFileStr);
    TimeTextFileStr += "psnr.txt";

    double inputPSNR;
    std::ifstream infile(TimeTextFileStr.c_str());

    if (!infile.good())
    {
        tprintf(PRINT_BTH, "\nFailed to open input file: %s",
            TimeTextFileStr.c_str());
        return 0;
    }

    infile >> inputPSNR;
    infile.close();

    if (inputPSNR <= 0.0 || inputPSNR >= 60.0)
    {
        tprintf(PRINT_BTH, "\nPSNR value is not vailid: %f\n",
            inputPSNR);
        return 0;
    }

    return inputPSNR;
}