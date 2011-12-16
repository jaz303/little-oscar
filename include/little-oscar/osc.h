#ifndef OSC_H
#define OSC_H

//
// PAL

#ifndef OSC_GOT_PAL
    #include <stdint.h>     /* int32_t, int64_t, uint32_t, uint64_t */
    #include <string.h>     /* strlen() */

    #if defined(__WIN32) || defined(__WIN32__) || defined(__WIN64) || defined(__WIN64__)
        #define OSC_LITTLE_ENDIAN
        #include <winsock2.h>   /* htonl(), ntohl() */
    #else
        #include <arpa/inet.h>  /* htonl(), ntohl() */

        #if defined BYTE_ORDER
            #if BYTE_ORDER == BIG_ENDIAN
                #define OSC_BIG_ENDIAN
            #elif BYTE_ORDER == LITTLE_ENDIAN
                #define OSC_LITTLE_ENDIAN
            #endif
        #endif
    #endif

    #if defined(OSC_BIG_ENDIAN)
        #define osc_hton32(i)   (i)
        #define osc_hton64(i)   (i)
        #define osc_ntoh32(i)   (i)
        #define osc_ntoh64(i)   (i)
    #elif defined(OSC_LITTLE_ENDIAN)
        #define osc_hton32(i)   htonl(i)
        #define osc_hton64(i)   (((uint64_t)htonl(i) << 32) | htonl(i >> 32))
        #define osc_ntoh32(i)   ntohl(i)
        #define osc_ntoh64(i)   (((uint64_t)ntohl(i) << 32) | ntohl(i >> 32))
    #else
        #error "Byte order is undefined!"
    #endif

    #undef OSC_BIG_ENDIAN
    #undef OSC_LITTLE_ENDIAN
#endif

#ifdef __cplusplus
extern "C" {
#endif

// End PAL
//

//
// Return codes

#define OSC_MESSAGE     2
#define OSC_BUNDLE      1
#define OSC_OK          0
#define OSC_ERROR      -1
#define OSC_END        -2

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

typedef struct {
    const char              *bundle_ptr;
    const char              *bundle_end;
    const char              *msg_ptr;
} osc_bundle_reader_t;

typedef struct {
    const char              *msg_ptr;
    const char              *msg_end;
    const char              *type_ptr;
    const char              *arg_ptr;
} osc_msg_reader_t;

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

int                 osc_packet_get_type(const char *buffer, int len);

int                 osc_bundle_reader_init(osc_bundle_reader_t *reader, const char *buffer, int len);
osc_timetag_t       osc_bundle_reader_get_timetag(osc_bundle_reader_t *reader);
int                 osc_bundle_reader_next(osc_bundle_reader_t *reader, const char **start, int32_t *len);

int                 osc_msg_reader_init(osc_msg_reader_t *reader, const char *buffer, int len);
int                 osc_msg_reader_is_typed(osc_msg_reader_t *reader);
const char *        osc_msg_reader_get_address(osc_msg_reader_t *reader);

/*
 * convenience function.
 * read the current message's next argument (and its type) into the given
 * structure and advance the argument pointer.
 * returns 1 if an argument was read, and 0 if there was no argument
 * remaining to read.
 * this function requires that the current message has a type tag.
 */
int                 osc_msg_reader_get_arg(osc_msg_reader_t *reader, osc_arg_t *arg);

/*
 * returns the type at the current message's arg pointer the advances the ptr to 
 * the next argument. returns 0 if there was no argument remaining to read.
 * (this function requires that the current message has a type tag)
 */
char                osc_msg_reader_next_arg(osc_msg_reader_t *reader);

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
int                 osc_msg_reader_get_arg_int32(osc_msg_reader_t *reader, int32_t *val);
int                 osc_msg_reader_get_arg_int64(osc_msg_reader_t *reader, int64_t *val);
int                 osc_msg_reader_get_arg_timetag(osc_msg_reader_t *reader, osc_timetag_t *val);
int                 osc_msg_reader_get_arg_float(osc_msg_reader_t *reader, float *val);
int                 osc_msg_reader_get_arg_double(osc_msg_reader_t *reader, double *val);
int                 osc_msg_reader_get_arg_str(osc_msg_reader_t *reader, const char **val);
int                 osc_msg_reader_get_arg_blob(osc_msg_reader_t *reader, void **val, int32_t *sz);

#ifdef __cplusplus
}
#endif

#endif
