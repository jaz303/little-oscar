#include "osc.h"

#define osc_hton32(i)   (htobe32(*((uint32_t*)(&i))))
#define osc_hton64(i)   (htobe64(*((uint64_t*)(&i))))

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
    size_t rounded      = (unrounded + 3) & ~0x03;
    
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
    /* this is a slippery one */
    return 0;
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

