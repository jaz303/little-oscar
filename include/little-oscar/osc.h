#ifndef OSC_H
#define OSC_H

/*
 * little-oscar has minimal reliance on the C stdlib and should hence be
 * readily portable to various microcontroller platforms.
 *
 * If you're compiling on a platform with a stdlib there's no need for any
 * special configuration; the macro block below should work. It's a bug if
 * it doesn't.
 *
 * Otherwise, you'll need to define the following types/functions, as well
 * as the constant OSC_GOT_PAL. You can also #define OSC_HAVE_VARARG to enable
 * vararg support (requires the usual va_list, va_start(), va_end() etc).
 *
 * Required types: int32_t, int64_t, uint32_t, uint64_t
 * Macros/functions: strlen(), osc_hton32(), osc_hton64(), osc_ntoh32(), osc_ntoh64()
 */
#ifndef OSC_GOT_PAL
    #include <stdint.h>     /* int32_t, int64_t, uint32_t, uint64_t */
    #include <string.h>     /* strlen() */
    #include <stdarg.h>     /* va_list and friends */
    
    #define OSC_HAVE_VARARG 1

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

// Writing



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

typedef struct {
    char                    *data;
    int                     len;
    int                     pos;
    int                     msg_start;
    int                     type_pos;
    enum {
        OSC_WRITER_OUT          = 1,
        OSC_WRITER_BUNDLE       = 2,
        OSC_WRITER_MSG          = 4,
        OSC_WRITER_COMPLETE     = 8
    }                       state;
} osc_writer_t;

int osc_msg_writer_init(osc_writer_t *writer, void *buffer_or_writer, int len);
int osc_msg_writer_start_msg(osc_writer_t *writer, const char *address, int nargs);
int osc_msg_writer_end_msg(osc_writer_t *writer);
int osc_msg_writer_start_bundle(osc_writer_t *writer, osc_timetag_t timetag);
int osc_msg_writer_end_bundle(osc_writer_t *writer);
int osc_msg_write_null(osc_writer_t *writer);
int osc_msg_write_true(osc_writer_t *writer);
int osc_msg_write_false(osc_writer_t *writer);
int osc_msg_write_infinity(osc_writer_t *writer);
int osc_msg_write_int32(osc_writer_t *writer, int32_t val);
int osc_msg_write_int64(osc_writer_t *writer, int64_t val);
int osc_msg_write_timetag(osc_writer_t *writer, osc_timetag_t val);
int osc_msg_write_float(osc_writer_t *writer, float val);
int osc_msg_write_double(osc_writer_t *writer, double val);
int osc_msg_write_str(osc_writer_t *writer, const char *val);
int osc_msg_write_blob(osc_writer_t *writer, unsigned char *val, int32_t sz);

#ifdef OSC_HAVE_VARARG
int osc_writev(osc_writer_t *writer, const char *address, const char *typestring, va_list args);
int osc_write(osc_writer_t *writer, const char *address, const char *typestring, ...);
#endif

#ifdef __cplusplus
}
#endif

#endif
