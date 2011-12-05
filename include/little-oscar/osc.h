#ifndef OSC_H
#define OSC_H

//
// HAL

#ifndef OSC_GOT_HAL

#include <stdint.h>     /* int32_t, int64_t, uint32_t, uint64_t */
#include <string.h>     /* strlen() */
#include <arpa/inet.h>  /* htonl(), ntohl() */

#if BYTE_ORDER == BIG_ENDIAN
    #define osc_hton32(i)   (i)
    #define osc_hton64(i)   (i)
    #define osc_ntoh32(i)   (i)
    #define osc_ntoh64(i)   (i)
#elif BYTE_ORDER == LITTLE_ENDIAN
    #define osc_hton32(i)   htonl(i)
    #define osc_hton64(i)   (((uint64_t)htonl(i) << 32) | htonl(i >> 32))
    #define osc_ntoh32(i)   ntohl(i)
    #define osc_ntoh64(i)   (((uint64_t)ntohl(i) << 32) | ntohl(i >> 32))
#else
    #error "Byte order is undefined!"
#endif

#endif

// End HAL
//

//
// Return codes

#define OSC_OK       0
#define OSC_ERROR   -1
#define OSC_END     -2

//
// Types

typedef uint64_t osc_timetag_t;

#define OSC_NOW     1

typedef union {
    int32_t         i32;
    uint32_t        u32;
    float           fl;
} osc_v32_t;

typedef union {
    int64_t         i64;
    uint64_t        u64;
    double          fl;
    osc_timetag_t   timetag;
} osc_v64_t;

//
// Reading

typedef struct osc_reader {
    char*               buffer;
    const char*         buffer_end;
    char                is_bundle;
    const char*         msg_ptr;
    const char*         msg_end;
    const char*         type_ptr;
    const char*         arg_ptr;
} osc_reader_t;

typedef struct {
    char type;
    union {
        int32_t         val_int32;
        int64_t         val_int64;
        osc_timetag_t   val_timetag;
        float           val_float;
        double          val_double;
        const char*     val_str;
        union {
            int32_t     len;
            void        *data;
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
 * returns OSC_OK on success.
 *
 * NOTE: buffer *MUST* be padded with a single null-terminator character to
 *       prevent ill-formed OSC-strings from causing a buffer overflow.
 *       `message_len` MUST NOT include the null-terminator. 
 */
int             osc_reader_init(osc_reader_t *reader, char *buffer, int len);

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

#endif