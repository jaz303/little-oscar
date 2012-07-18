#include "little-oscar/osc_internal.h"

#include <stdarg.h>

typedef union {
    
} union32;

typedef union {
    
} union64;

#define PAD() \
    while (writer->pos & 3) { writer->data[writer->pos++] = '\0'; }

#define ENSURE_REMAIN(rlen) \
    if (writer->len - writer->pos < (rlen)) { return OSC_ERROR; }

#define WRITE_STRING(str) \
    while (*str) { writer->data[writer->pos++] = *(str++); } \
    writer->data[writer->pos++] = '\0'; \
    PAD();

#define WRITE_FIXED(type, bits, val) \
    uint##bits##_t raw = osc_hton##bits(val); \
    *((uint##bits##_t*)(&(writer->data[writer->pos]))) = raw; \
    writer->pos += sizeof(type)

#define WRITE_FIXED_VA(type, va_type, bits) \
    type real_val = (type) va_arg(args, va_type); \
    WRITE_FIXED(type, bits, real_val)

#define ADD_TYPE(t) writer->data[writer->type_pos++] = t

static const char *k_bundle = "#bundle";

int osc_msg_writer_init(osc_writer_t *writer, void *buffer_or_writer, int len) {
    if (len > 0) {
        writer->data = (char*)buffer_or_writer;
        writer->len = len;
    } else {
        osc_writer_t *parent = (osc_writer_t *)buffer_or_writer;
        writer->data = parent->data + parent->pos;
        writer->len  = parent->len - parent->pos;
    }
    writer->pos = 0;
    writer->state = OSC_WRITER_OUT;
    return OSC_OK;
}

int osc_msg_writer_start_msg(osc_writer_t *writer, const char *address, int nargs) {
    if (writer->state & OSC_WRITER_MSG) return OSC_ERROR;
    
    if (writer->state == OSC_WRITER_BUNDLE) {
        writer->pos += 4;
    }
    
    writer->msg_start = writer->pos;
    
    ENSURE_REMAIN(ROUND32(strlen(address)));
    WRITE_STRING(address);
    
    nargs = ROUND32(nargs);
    ENSURE_REMAIN(nargs);
    writer->type_pos = writer->pos;
    while (nargs--) writer->data[writer->pos++] = '\0';
    
    writer->state |= OSC_WRITER_MSG;
    
    return OSC_OK;
}

int osc_msg_writer_end_msg(osc_writer_t *writer) {
    if (!(writer->state & OSC_WRITER_MSG)) return OSC_ERROR;
    if (writer->state & OSC_WRITER_BUNDLE) {
        *((uint32_t*)&(writer->data[writer->msg_start - 4])) = writer->pos - writer->msg_start;
        writer->state &= ~OSC_WRITER_MSG;
    } else {
        writer->state = OSC_WRITER_COMPLETE;
    }
    return OSC_OK;
}

int osc_msg_writer_start_bundle(osc_writer_t *writer, osc_timetag_t timetag) {
    if (writer->state != OSC_WRITER_OUT) return OSC_ERROR;
    ENSURE_REMAIN(12);
    WRITE_STRING(k_bundle);
    WRITE_FIXED(osc_timetag_t, 64, timetag);
    writer->state = OSC_WRITER_BUNDLE;
    return OSC_OK;
}

int osc_msg_writer_end_bundle(osc_writer_t *writer) {
    writer->state = OSC_WRITER_COMPLETE;
    return OSC_OK;
}

int osc_msg_write_null(osc_writer_t *writer) {
    ADD_TYPE('N');
    return OSC_OK;
}

int osc_msg_write_true(osc_writer_t *writer) {
    ADD_TYPE('T');
    return OSC_OK;
}

int osc_msg_write_false(osc_writer_t *writer) {
    ADD_TYPE('F');
    return OSC_OK;
}

int osc_msg_write_infinity(osc_writer_t *writer) {
    ADD_TYPE('I');
    return OSC_OK;
}

int osc_msg_write_int32(osc_writer_t *writer, int32_t val) {
    ADD_TYPE('i');
    ENSURE_REMAIN(4);
    WRITE_FIXED(int32_t, 32, val);
    return OSC_OK;
}

int osc_msg_write_int64(osc_writer_t *writer, int64_t val) {
    ADD_TYPE('h');
    ENSURE_REMAIN(8);
    WRITE_FIXED(int64_t, 64, val);
    return OSC_OK;
}

int osc_msg_write_timetag(osc_writer_t *writer, osc_timetag_t val) {
    ADD_TYPE('t');
    ENSURE_REMAIN(8);
    WRITE_FIXED(osc_timetag_t, 64, val);
    return OSC_OK;
}

int osc_msg_write_float(osc_writer_t *writer, float val) {
    ADD_TYPE('f');
    ENSURE_REMAIN(4);
    WRITE_FIXED(float, 32, val);
    return OSC_OK;
}

int osc_msg_write_double(osc_writer_t *writer, double val) {
    ADD_TYPE('d');
    ENSURE_REMAIN(8);
    WRITE_FIXED(double, 64, val);
    return OSC_OK;
}

int osc_msg_write_str(osc_writer_t *writer, const char *val) {
    ADD_TYPE('s');
    ENSURE_REMAIN(ROUND32(strlen(val)));
    WRITE_STRING(val);
    return OSC_OK;
}

int osc_msg_write_blob(osc_writer_t *writer, unsigned char *val, int32_t sz) {
    ADD_TYPE('b');
    ENSURE_REMAIN(4 + ROUND32(sz));
    WRITE_FIXED(int32_t, 32, sz);
    while (sz--) writer->data[writer->pos++] = *(val++);
    PAD();
    return OSC_OK;
}

#ifndef OSC_HAVE_VARARG

int osc_writev(osc_writer_t *writer, const char *address, const char *typestring, va_list args) {
    int nargs = 0;
    const char *tmp = typestring;
    
    while (*(tmp++)) ++nargs;
    
    if (osc_msg_writer_start_msg(writer, address, nargs) != OSC_OK) {
        return OSC_ERROR;
    }
    
    while (*typestring) {
        ADD_TYPE(*typestring);
        
        switch (*typestring) {
            case 'T': /* fall-through */
            case 'F': /* fall-through */
            case 'N': /* fall-through */
            case 'I': break;
            case 'i': { ENSURE_REMAIN(4); WRITE_FIXED_VA(int32_t, int, 32);         break; }
            case 'h': { ENSURE_REMAIN(8); WRITE_FIXED_VA(int64_t, int64_t, 64);     break; }
            case 't': { ENSURE_REMAIN(8); WRITE_FIXED_VA(uint64_t, int64_t, 64);    break; }
            case 'f': { ENSURE_REMAIN(4); WRITE_FIXED_VA(float, double, 32);        break; }
            case 'd': { ENSURE_REMAIN(8); WRITE_FIXED_VA(double, double, 64);       break; }
            case 'k': /* fall through */
            case 's': /* fall through */
            case 'S':
            {
                const char *string_val = va_arg(args, char*);
                ENSURE_REMAIN(ROUND32(strlen(string_val)));
                WRITE_STRING(string_val);
                break;
            }
            case 'b':
            {
                int32_t     blob_len = va_arg(args, int32_t);
                const char* blob_data = va_arg(args, char*);
                ENSURE_REMAIN(4 + ROUND32(blob_len));
                WRITE_FIXED(int32_t, 32, blob_len);
                /* copy data */
                break;
            }
            default : return OSC_ERROR;   
        }
        
        typestring++;
    }
    
    if (osc_msg_writer_end_msg(writer) != OSC_OK) {
        return OSC_ERROR;
    }
    
    return OSC_OK;
}

int osc_write(osc_writer_t *writer, const char *address, const char *typestring, ...) {
    int s;
    va_list args;
    va_start(args, typestring);
    s = osc_writev(writer, address, typestring, args);
    va_end(args);
    return s;
}

#endif