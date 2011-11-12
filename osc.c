#include "osc.h"

#define osc_hton32(i)   (htobe32(*((uint32_t*)(&i))))
#define osc_hton64(i)   (htobe64(*((uint64_t*)(&i))))

#define osc_ntoh32(i)   (betoh32(*((uint32_t*)(&i))))
#define osc_ntoh64(i)   (betoh64(*((uint64_t*)(&i))))

#define ROUND32(i)      ((i + 3) & ~0x03)

#ifndef NULL
#define NULL 0
#endif

#define WRITE_FIXED(type, va_type, bits) \
    if (end - pos < sizeof(type)) return -1; \
    type real_val = (type) va_arg(args, va_type); \
    uint##bits##_t bytes = osc_hton##bits(real_val); \
    memcpy(pos, &bytes, sizeof(type)); \
    pos += sizeof(type);

static inline char* osc_write_aligned(char *buffer, size_t remain, const void *source, size_t len, size_t already_written) {
    size_t unrounded    = len + already_written;
    size_t rounded      = ROUND32(unrounded);
    
    if (rounded - already_written > remain) return NULL;
    
    memcpy(buffer, source, len);
    buffer += len;
    switch (rounded - unrounded) {
        case 3: *(buffer++) = 0;
        case 2: *(buffer++) = 0;
        case 1: *(buffer++) = 0;
        case 0: break;
    }
    
    return buffer;
}

int osc_writev(char *buffer, size_t buffer_len, const char *address, const char *format, va_list args) {
    
    char *pos   = buffer,
         *end   = buffer + buffer_len;
         
    pos = osc_write_aligned(pos, end - pos, address, strlen(address) + 1, 0);
    if (!pos) return -1;
    
    if (end - pos < 1) return -1;
    
    *(pos++) = ',';
    pos = osc_write_aligned(pos, end - pos, format, strlen(format) + 1, 1);
    if (!pos) return -1;
    
    const char *curr = format;
    while (*curr) {
        /*
         * Not implemented:
         * c  - 32-bit ASCII char
         * r  - 32-bit RGBA color
         * m  - 4-byte MIDI message
         * [] - arrays
         */
        switch (*curr) {
            case 'T':
            case 'F':
            case 'N':
            case 'I':
                /* true, false, nil and infinitum allocate no bytes in argument data */
                break;
            
            case 'i': { WRITE_FIXED(int32_t, int, 32);          break; /* int32 */ }
            case 'h': { WRITE_FIXED(int64_t, int64_t, 64);      break; /* 64-bit big-endian signed integer */ }
            case 't': { WRITE_FIXED(uint64_t, uint64_t, 64);    break; /* OSC-timetag */ }
            case 'f': { WRITE_FIXED(float, double, 32);         break; /* float32 */ }
            case 'd': { WRITE_FIXED(double, double, 64);        break; /* double */ }
            case 's': /* OSC-string */
            case 'S': /* alternate string */
            {
                const char *string_val = va_arg(args, char*);
                pos = osc_write_aligned(pos, end - pos, string_val, strlen(string_val) + 1, 0);
                if (!pos) return -1;
                break;
            }
            case 'b': /* OSC-blob */
            {
                WRITE_FIXED(int32_t, int, 32); /* write the blob length; the value is stored in `real_val` */
                char *blob_ptr = va_arg(args, char*);
                pos = osc_write_aligned(pos, end - pos, blob_ptr, real_val, 0);
                if (!pos) return -1;
                break;
            }
            default:
                return -1;
        }
        curr++;
    }
        
    return (int) (pos - buffer);
    
}

int osc_write(char *buffer, size_t buffer_len, const char *address, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int written = osc_writev(buffer, buffer_len, address, format, args);
    va_end(args);
    
    return written;
}

void osc_bundle_init(osc_bundle_t *bundle, char *buffer, size_t buffer_sz) {
    bundle->buffer      = buffer;
    bundle->buffer_pos  = buffer;
    bundle->buffer_end  = buffer + buffer_sz;
    bundle->len         = 0;
}

int osc_bundle_start(osc_bundle_t *bundle, osc_timetag_t when) {
    bundle->buffer_pos  = bundle->buffer;
    bundle->len         = 0;
    
    bundle->buffer_pos  = osc_write_aligned(bundle->buffer_pos,
                                            bundle->buffer_end - bundle->buffer_pos,
                                            "#bundle",
                                            8,
                                            0);
    
    if (!bundle->buffer_pos) return 0;
    if (bundle->buffer_end - bundle->buffer_pos < 8) return 0;
    
    uint64_t when_nbo = osc_hton64(when);
    memcpy(bundle->buffer_pos, &when_nbo, 8);
    bundle->buffer_pos += 8;
    
    bundle->len = bundle->buffer_pos - bundle->buffer;
    
    return 1;
}

int osc_bundle_writev(osc_bundle_t *bundle, const char *address, const char *format, va_list args) {
    if (bundle->buffer_end - bundle->buffer_pos < 4) return 0;
    
    char *buffer_before = bundle->buffer_pos;
    bundle->buffer_pos += 4;
    
    int written = osc_writev(bundle->buffer_pos,
                             bundle->buffer_end - bundle->buffer_pos,
                             address,
                             format,
                             args);
    
    if (written < 0) return 0;
    
    bundle->buffer_pos += written;
    
    uint32_t written_nbo = osc_hton32(written);
    memcpy(buffer_before, &written_nbo, 4);
    
    bundle->len += written + 4;
    
    return 1;
}

int osc_bundle_write(osc_bundle_t *bundle, const char *address, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int retval = osc_bundle_writev(bundle, address, format, args);
    va_end(args);
    
    return retval;
}

#pragma mark -
#pragma mark Reading

int osc_reader_init(osc_reader_t *reader, char *buffer, size_t packet_len) {
    reader->buffer      = buffer;
    reader->buffer_end  = buffer + packet_len;
    
    reader->msg_ptr     = NULL;
    reader->msg_end     = NULL;
    reader->type_ptr    = NULL;
    reader->arg_ptr     = NULL;
    
    if (strcmp("#bundle", reader->buffer) == 0) {
        /* traverse the buffer and convert each message length to host byte order,
         * ensuring the reported message lengths do not cause a buffer overflow. */
        /* TODO: is HBO conversion a bad optimisation? it makes message parsing non-reentrant */
        char *msg_start = reader->buffer + 20; /* "#bundle" (8) + timetag (8) + msg size (4) */
        while (msg_start < reader->buffer_end) {
            char *msg_len = msg_start - 4;
            int32_t hb_len = (int32_t) osc_ntoh32(*((uint32_t*)msg_len));
            *((int32_t*)msg_len) = hb_len;
            msg_start += hb_len;
        }
        
        if (msg_start != reader->buffer_end) {
            return OSC_ERROR;
        }
        
        reader->is_bundle = 1;
    } else {
        reader->is_bundle = 0;
    }
    
    return OSC_OK;
}

int osc_reader_is_bundle(osc_reader_t *reader) {
    return reader->is_bundle;
}

osc_timetag_t osc_reader_get_timetag(osc_reader_t *reader) {
    if (reader->is_bundle) {
        return *((osc_timetag_t*)(reader->buffer + 8));
    } else {
        return OSC_NOW;
    }
}

int osc_reader_start_msg(osc_reader_t *reader) {
    if (reader->is_bundle) {
        if (!reader->msg_ptr && !reader->msg_end) {
            reader->msg_ptr = reader->buffer + 20; /* "#bundle" (8) + timetag (8) + msg size (4) */
            reader->msg_end = reader->msg_ptr + *((int32_t*)(reader->msg_ptr - 4));
        } else if (reader->msg_ptr) {
            if (reader->msg_end == reader->buffer_end) {
                reader->msg_ptr = NULL;
            } else {
                reader->msg_ptr = reader->msg_end + 4;
                reader->msg_end = reader->msg_ptr + *((int32_t*)(reader->msg_ptr - 4));
            }
        }
    } else {
        if (!reader->msg_ptr && !reader->msg_end) {
            reader->msg_ptr = reader->buffer;
            reader->msg_end = reader->buffer_end;
        } else {
            reader->msg_ptr = NULL;
        }
    }
    
    if (reader->msg_ptr) {
        if ((reader->msg_end - reader->msg_ptr) & 0x03) {
            return OSC_ERROR; /* message length is not multiple of 4 */
        }
        
        size_t addr_len = ROUND32(strlen(reader->msg_ptr) + 1);
        const char *type_ptr = reader->msg_ptr + addr_len;
        
        if (type_ptr > reader->msg_end) {
            return OSC_ERROR;
        } else if (type_ptr == reader->msg_end) {
            reader->type_ptr = NULL;
            reader->arg_ptr = NULL;
        } else if (*type_ptr == ',') {
            const char *arg_ptr = type_ptr + ROUND32(strlen(type_ptr) + 1);
            if (arg_ptr > reader->msg_end) {
                return OSC_ERROR;
            } else {
                reader->type_ptr = type_ptr + 1; /* skip leading ',' */
                reader->arg_ptr = arg_ptr;
            }
        } else {
            reader->type_ptr = NULL;
            reader->arg_ptr = type_ptr;
        }
    }
    
    return reader->msg_ptr ? OSC_OK : OSC_END;
}

/* returns 1 if the current message has a type tag, 0 otherwise */
int osc_reader_msg_is_typed(osc_reader_t *reader) {
    return reader->type_ptr != NULL;
}

/* returns the address of the current message. */
const char* osc_reader_get_msg_address(osc_reader_t *reader) {
    return reader->msg_ptr;
}

int osc_reader_get_arg(osc_reader_t *reader, osc_arg_t *arg) {
    char t = osc_reader_next_arg(reader);
    if (t < 0) return t;
    arg->type = t;
    switch (t) {
        case 'T':   /* fall through */
        case 'F':   /* fall through */
        case 'N':   /* fall through */
        case 'I':   return OSC_OK;
        case 'i':   return osc_reader_get_arg_int32(reader, &arg->val.val_int32);
        case 'h':   return osc_reader_get_arg_int64(reader, &arg->val.val_int64);
        case 't':   return osc_reader_get_arg_timetag(reader, &arg->val.val_timetag);
        case 'f':   return osc_reader_get_arg_float(reader, &arg->val.val_float);
        case 'd':   return osc_reader_get_arg_double(reader, &arg->val.val_double);
        case 's':   /* fall through */
        case 'S':   return osc_reader_get_arg_str(reader, &arg->val.val_str);
        case 'b':   return osc_reader_get_arg_blob(reader, &arg->val.val_blob.data, &arg->val.val_blob.len);
        default:    return OSC_ERROR;
    }
}

char osc_reader_next_arg(osc_reader_t *reader) {
    if (reader->type_ptr) {
        char curr = *reader->type_ptr;
        if (curr) {
            reader->type_ptr++;
            return curr;
        } else {
            return OSC_END;
        }
    } else {
        return OSC_ERROR;
    }
}

#define READ_FIXED(type, target, bits) \
    if (reader->msg_end - reader->arg_ptr < sizeof(type)) return OSC_ERROR; \
    uint##bits##_t raw_val = osc_ntoh##bits(*((uint##bits##_t*)reader->arg_ptr)); \
    *target = *(type*)(&raw_val); \
    reader->arg_ptr += sizeof(type); 
    
int osc_reader_get_arg_int32(osc_reader_t *reader, int32_t *val) {
    READ_FIXED(int32_t, val, 32);
    return OSC_OK;
}

int osc_reader_get_arg_int64(osc_reader_t *reader, int64_t *val) {
    READ_FIXED(int64_t, val, 64);
    return OSC_OK;
}

int osc_reader_get_arg_timetag(osc_reader_t *reader, osc_timetag_t *val) {
    READ_FIXED(osc_timetag_t, val, 64);
    return OSC_OK;
}

int osc_reader_get_arg_float(osc_reader_t *reader, float *val) {
    READ_FIXED(float, val, 32);
    return OSC_OK;
}

int osc_reader_get_arg_double(osc_reader_t *reader, double *val) {
    READ_FIXED(double, val, 64);
    return OSC_OK;
}

int osc_reader_get_arg_str(osc_reader_t *reader, const char **val) {
    size_t len = ROUND32(strlen(reader->arg_ptr) + 1);
    if (reader->arg_ptr + len > reader->msg_end) return OSC_ERROR;
    *val = reader->arg_ptr;
    reader->arg_ptr += len;
    return OSC_OK;
}

int osc_reader_get_arg_blob(osc_reader_t *reader, void **val, int32_t *sz) {
    READ_FIXED(int32_t, sz, 32);
    int32_t rounded_sz = ROUND32(*sz);
    if (reader->arg_ptr + rounded_sz > reader->msg_end) return OSC_ERROR;
    *val = (void*) reader->arg_ptr;
    reader->arg_ptr += rounded_sz;
    return OSC_OK;
}

#pragma mark -
#pragma mark TUIO

static const char* profile_addresses[] = {
    NULL,
    "/tuio/2Dobj",
    "/tuio/2Dcur",
    "/tuio/2Dblb"
};

int tuio_bundle_start(tuio_bundle_t *bundle, osc_timetag_t when, tuio_profile_t type) {
    if (!osc_bundle_start(&bundle->osc_bundle, when)) return 0;
    bundle->type = type;
    return 1;
}

int tuio_bundle_source(tuio_bundle_t *bundle, const char *source) {
    return osc_bundle_write(&bundle->osc_bundle, profile_addresses[bundle->type], "ss", "source", source);
}

int tuio_bundle_alive(tuio_bundle_t *bundle, int32_t *s_ids, size_t count) {
    
    char *pos       = bundle->osc_bundle.buffer_pos,
         *end       = bundle->osc_bundle.buffer_end;
         
    if (end - pos < sizeof(int32_t)) return 0;
    
    char *before        = pos;
    const char *addr    = profile_addresses[bundle->type];
    
    pos += sizeof(int32_t);
    pos = osc_write_aligned(pos, end - pos, addr, strlen(addr) + 1, 0);
    if (!pos) return 0;
    
    int i;
    char *type_end = pos + ROUND32(count + 2);
    
    if (type_end + sizeof(int32_t) * count > end) {
        return 0;
    }
    
    *(pos++) = ',';
    for (i = 0; i < count; i++) *(pos++) = 'i';
    while (pos < type_end) *(pos++) = 0;
    
    for (i = 0; i < count; i++) {
        uint32_t id_nbo = osc_hton32(s_ids[i]);
        memcpy(pos, &id_nbo, sizeof(int32_t));
        pos += 4;
    }
    
    int32_t written = pos - before + sizeof(int32_t);
    uint32_t written_nbo = osc_hton32(written);
    memcpy(before, &written_nbo, sizeof(uint32_t));
    
    bundle->osc_bundle.buffer_pos = pos;
    
    return 1;
    
}

int tuio_bundle_set(tuio_bundle_t *bundle, tuio_msg_t *msg) {
    switch (bundle->type) {
        case TUIO_2D_OBJ:
            return osc_bundle_write(&bundle->osc_bundle, profile_addresses[bundle->type],
                                                         "siiffffffff", "set",
                                                         msg->session_id,
                                                         msg->class_id,
                                                         msg->x, msg->y, msg->a,
                                                         msg->vx, msg->vy, msg->va,
                                                         msg->motion_acceleration,
                                                         msg->rotation_acceleration);
        case TUIO_2D_CUR:
            return osc_bundle_write(&bundle->osc_bundle, profile_addresses[bundle->type],
                                                         "sifffff", "set",
                                                         msg->session_id,
                                                         msg->x, msg->y,
                                                         msg->vx, msg->vy,
                                                         msg->motion_acceleration);
        case TUIO_2D_BLB:
            return osc_bundle_write(&bundle->osc_bundle, profile_addresses[bundle->type],
                                                         "sfffffffffff", "set",
                                                         msg->x, msg->y, msg->a,
                                                         msg->width, msg->height, msg->area,
                                                         msg->vx, msg->vy, msg->va,
                                                         msg->motion_acceleration,
                                                         msg->rotation_acceleration);
        default:
            return 0;
    }
}

int tuio_bundle_fseq(tuio_bundle_t *bundle, int32_t fseq) {
    return osc_bundle_write(&bundle->osc_bundle, profile_addresses[bundle->type], "si", "fseq", fseq);
}

