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

int     osc_bundle_init(osc_bundle_t *bundle, char *buffer, size_t buffer_sz);
int     osc_bundle_start(osc_bundle_t *bundle, osc_timetag_t when);
int     osc_bundle_writev(osc_bundle_t *bundle, const char *address, const char *format, va_list args);
int     osc_bundle_write(osc_bundle_t *bundle, const char *address, const char *format, ...);

struct tuio_msg_2d {
    int32_t     session_id;
    int32_t     class_id;
    float       x, y, a;
    float       vx, vy, va;
    float       motion_acceleration;
    float       rotation_acceleration;
    float       width, height, area;
};

// http://www.tuio.org/?tuio11

int tuio_write_2Dobj(char *buffer, size_t buffer_len, struct tuio_msg_2d *msg);
int tuio_write_2Dcur(char *buffer, size_t buffer_len, struct tuio_msg_2d *msg);
int tuio_write_2Dblb(char *buffer, size_t buffer_len, struct tuio_msg_2d *msg);

#endif