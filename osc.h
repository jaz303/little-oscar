#ifndef OSC_H
#define OSC_H

/*
 * Define OSC_GOT_HAL if you are providing OSC's abstraction layer manually.
 * (this is probably only necessary if you're working on a microcontroller)
 *
 * You need to provide the following:
 *
 * Typedefs for fixed-size types: uint64_t, int64_t, int32_t
 * Endianness conversion functions: htobe32(), htobe64(), be32toh(), be64toh()
 * String functions: strlen(), memcpy()
 *
 * TODO: how to support va_args on micros?
 */
#ifndef OSC_GOT_HAL
    #include <stdint.h>
    #include <stdlib.h>
    #include <stdarg.h>
    #include <string.h>
    #include <netinet/in.h>
    #include <sys/types.h>
    
    #if BYTE_ORDER == BIG_ENDIAN
        #define htobe32(i)      (i)
        #define htobe64(i)      (i)
    #elif BYTE_ORDER == LITTLE_ENDIAN
        #define htobe32(i)      htonl(i)
        #define htobe64(i)      (((uint64_t)htonl(i) << 32) | htonl(i >> 32))
    #else
        #error "Byte order is undefined!"
    #endif
#endif

typedef uint64_t osc_timetag_t;

#define OSC_TIMETAG(seconds, fractional)    ((((uint64_t)seconds) << 32)|fractional)
#define OSC_TIME(seconds, fractional)       OSC_TIMETAG(seconds, (uint32_t)(((float)fractional)*0xffffffff))
#define OSC_NOW                             OSC_TIMETAG(0,1)

typedef struct osc_bundle {
    char    *buffer;
    char    *buffer_pos;
    char    *buffer_end;
    size_t  len;
} osc_bundle_t;

int     osc_writev(char *buffer, size_t buffer_len, const char *address, const char *format, va_list args);
int     osc_write(char *buffer, size_t buffer_len, const char *address, const char *format, ...);

void    osc_bundle_init(osc_bundle_t *bundle, char *buffer, size_t buffer_sz);
int     osc_bundle_start(osc_bundle_t *bundle, osc_timetag_t when);
int     osc_bundle_writev(osc_bundle_t *bundle, const char *address, const char *format, va_list args);
int     osc_bundle_write(osc_bundle_t *bundle, const char *address, const char *format, ...);

typedef enum {
    TUIO_2D_OBJ     = 1,
    TUIO_2D_CUR     = 2,
    TUIO_2D_BLB     = 3
} tuio_profile_t;

typedef struct tuio_bundle {
    osc_bundle_t        osc_bundle;
    tuio_profile_t      type;
} tuio_bundle_t;

typedef struct tuio_msg {
    int32_t     session_id;
    int32_t     class_id;
    float       x, y, a;
    float       vx, vy, va;
    float       motion_acceleration;
    float       rotation_acceleration;
    float       width, height, area;
} tuio_msg_t;

// http://www.tuio.org/?tuio11

void    tuio_bundle_init(tuio_bundle_t *bundle, char *buffer, size_t buffer_sz);
int     tuio_bundle_start(tuio_bundle_t *bundle, osc_timetag_t when, tuio_profile_t type);
int     tuio_bundle_source(tuio_bundle_t *bundle, const char *source);
int     tuio_bundle_alive(tuio_bundle_t *bundle, int32_t *s_ids, size_t count);
int     tuio_bundle_set(tuio_bundle_t *bundle, tuio_msg_t *msg);
int     tuio_bundle_fseq(tuio_bundle_t *bundle, int32_t fseq);

#endif