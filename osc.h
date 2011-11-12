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
 * String functions: strlen(), strcmp(), memcpy()
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
        #define betoh32(i)      (i)
        #define betoh64(i)      (i)
    #elif BYTE_ORDER == LITTLE_ENDIAN
        #define htobe32(i)      htonl(i)
        #define htobe64(i)      (((uint64_t)htonl(i) << 32) | htonl(i >> 32))
        #define betoh32(i)      ntohl(i)
        #define betoh64(i)      (((uint64_t)ntohl(i) << 32) | ntohl(i >> 32))
    #else
        #error "Byte order is undefined!"
    #endif
#endif

typedef uint64_t osc_timetag_t;

#define OSC_END     -2
#define OSC_ERROR   -1
#define OSC_OK      0

#define OSC_TIMETAG(seconds, fractional)    ((((uint64_t)seconds) << 32)|fractional)
#define OSC_TIME(seconds, fractional)       OSC_TIMETAG(seconds, (uint32_t)(((float)fractional)*0xffffffff))
#define OSC_NOW                             OSC_TIMETAG(0,1)

//
// Write

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

//
// Read

typedef struct osc_reader {
    char*               buffer;
    const char*         buffer_end;
    char                is_bundle;
    const char*         msg_ptr;
    const char*         msg_end;
    const char*         type_ptr;
    const char*         arg_ptr;
} osc_reader_t;

typedef struct osc_arg {
    char type;
    union {
        int32_t         val_int32;
        int64_t         val_int64;
        osc_timetag_t   val_timetag;
        float           val_float;
        double          val_double;
        const char*     val_str;
        union {
            void*       data;
            int32_t     len;
        } val_blob;
    } val;
} osc_arg_t;

/*
 * initialise an osc_reader with a buffer containing an OSC packet of a given
 * length. the packet may contain either a single OSC message or a message
 * bundle.
 *
 * for bundles, a quick sanity check will be performed to ensure that all
 * message sizes are sane.
 *
 * returns 1 on success, 0 on failure.
 *
 * NOTE: buffer *MUST* be padded with a single null-terminator character to
 *       prevent ill-formed OSC-strings from causing a buffer overflow.
 *       `message_len` MUST NOT include the null-terminator. 
 */
int             osc_reader_init(osc_reader_t *reader, char *buffer, size_t packet_len);

/* returns 1 if the packet is a bundle, 0 otherwise */
int             osc_reader_is_bundle(osc_reader_t *reader);

/* returns the bundle's OSC timetag, or OSC_NOW if packet is not a bundle */
osc_timetag_t   osc_reader_get_timetag(osc_reader_t *reader);

/*
 * start processing the next message in this packet.
 * returns 0 if no messages are left to process, 1 otherwise.
 */
int             osc_reader_start_msg(osc_reader_t *reader);

/* returns 1 if the current message has a type tag, 0 otherwise */
int             osc_reader_msg_is_typed(osc_reader_t *reader);

/* returns the address of the current message. */
const char*     osc_reader_get_msg_address(osc_reader_t *reader);

/*
 * convenience function.
 * read the current message's next argument (and its type) into the given
 * structure and advance the argument pointer.
 * returns 1 if an argument was read, and 0 if there was no argument
 * remaining to read.
 * this function requires that the current message has a type tag.
 */
int             osc_reader_get_arg(osc_reader_t *reader, osc_arg_t *arg);

/*
 * returns the type at the current message's arg pointer the advances the ptr to 
 * the next argument. returns 0 if there was no argument remaining to read.
 * (this function requires that the current message has a type tag)
 */
char            osc_reader_next_arg(osc_reader_t *reader);

/*
 * the following functions read data from the current message's argument pointer
 * then advance that pointer. no type checking is performed, so these functions
 * can be used either in the absence of a type-tag, or after an argument's type
 * has been read using `osc_reader_next_arg()`.
 * 
 * return 1 on success and 0 on failure. a failure is reported iff reading the
 * current argument would cause the argument pointer to advance beyond the end
 * of the current message.
 */
int             osc_reader_get_arg_int32(osc_reader_t *reader, int32_t *val);
int             osc_reader_get_arg_int64(osc_reader_t *reader, int64_t *val);
int             osc_reader_get_arg_timetag(osc_reader_t *reader, osc_timetag_t *val);
int             osc_reader_get_arg_float(osc_reader_t *reader, float *val);
int             osc_reader_get_arg_double(osc_reader_t *reader, double *val);
int             osc_reader_get_arg_str(osc_reader_t *reader, const char **val);
int             osc_reader_get_arg_blob(osc_reader_t *reader, void **val, int32_t *sz);

/* returns 0 if the reader object is in an error state, 1 otherwise */
int             osc_reader_ok(osc_reader_t *reader);

//
// TUIO

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