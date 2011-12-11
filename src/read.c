#include "little-oscar/osc_internal.h"

int osc_reader_init(osc_reader_t *reader, char *buffer, int len) {
    reader->buffer      = buffer;
    reader->buffer_end  = buffer + len;
    
    reader->msg_ptr     = NULL;
    reader->msg_end     = NULL;
    reader->msg_len     = 0;
    reader->type_ptr    = NULL;
    reader->arg_ptr     = NULL;
    
    if (*reader->buffer == '#') {
        
        if (len < 20) {
            return OSC_ERROR;
        }
        
        char *pos = reader->buffer + 16; /* "#bundle" (8) + timetag (8) */
        
        do {
            osc_v32_t msg_len;
            msg_len.u32 = osc_ntoh32(*((uint32_t*)pos));
            if (msg_len.i32 & 0x03) {
                return OSC_ERROR;
            }
            pos = pos + 4 + msg_len.i32;
        } while (pos < reader->buffer_end);
        
        if (pos != reader->buffer_end) {
            return OSC_ERROR;
        }
        
        reader->is_bundle = 1;
    
    } else {
        if ((reader->buffer_end - reader->buffer) & 0x03) {
            return OSC_ERROR;
        }
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
        osc_v32_t msg_len;
        if (!reader->msg_ptr && !reader->msg_end) { /* first message */
            char *header = reader->buffer + 16; /* "#bundle" (8) + timetag (8) */
            msg_len.u32 = osc_ntoh32(*((uint32_t*)header));
            reader->msg_ptr = header + 4;
            reader->msg_end = reader->msg_ptr + msg_len.i32;
        } else if (reader->msg_ptr) {
            if (reader->msg_end == reader->buffer_end) {
                reader->msg_ptr = NULL;
            } else {
                msg_len.u32 = osc_ntoh32(*((uint32_t*)reader->msg_end));
                reader->msg_ptr = reader->msg_end + 4;
                reader->msg_end = reader->msg_ptr + msg_len.i32;
            }
        } else {
            /* No-op - no messages remain */
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
        int addr_len = ROUND32(strlen(reader->msg_ptr) + 1);
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
        
        return OSC_OK;
        
    } else {
        
        return OSC_END;
    
    }
}

int osc_reader_msg_get_len(osc_reader_t *reader) {
    return reader->msg_len;
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
        case 'k':   /* fall through */
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

#define READ_FIXED(target, bits, union_member) \
    osc_v##bits##_t raw_val; \
    if (reader->msg_end - reader->arg_ptr < sizeof(raw_val.union_member)) return OSC_ERROR; \
    raw_val.u##bits = osc_ntoh##bits(*((uint##bits##_t*)reader->arg_ptr)); \
    *target = raw_val.union_member; \
    reader->arg_ptr += sizeof(raw_val.union_member);
    
int osc_reader_get_arg_int32(osc_reader_t *reader, int32_t *val) {
    READ_FIXED(val, 32, i32);
    return OSC_OK;
}

int osc_reader_get_arg_int64(osc_reader_t *reader, int64_t *val) {
    READ_FIXED(val, 64, i64);
    return OSC_OK;
}

int osc_reader_get_arg_timetag(osc_reader_t *reader, osc_timetag_t *val) {
    READ_FIXED(val, 64, timetag);
    return OSC_OK;
}

int osc_reader_get_arg_float(osc_reader_t *reader, float *val) {
    READ_FIXED(val, 32, fl);
    return OSC_OK;
}

int osc_reader_get_arg_double(osc_reader_t *reader, double *val) {
    READ_FIXED(val, 64, fl);
    return OSC_OK;
}

int osc_reader_get_arg_str(osc_reader_t *reader, const char **val) {
    int len = ROUND32(strlen(reader->arg_ptr) + 1);
    if (reader->arg_ptr + len > reader->msg_end) return OSC_ERROR;
    *val = reader->arg_ptr;
    reader->arg_ptr += len;
    return OSC_OK;
}

int osc_reader_get_arg_blob(osc_reader_t *reader, void **val, int32_t *sz) {
    READ_FIXED(sz, 32, i32);
    int32_t rounded_sz = ROUND32(*sz);
    if (reader->arg_ptr + rounded_sz > reader->msg_end) return OSC_ERROR;
    *val = (void*) reader->arg_ptr;
    reader->arg_ptr += rounded_sz;
    return OSC_OK;
}
