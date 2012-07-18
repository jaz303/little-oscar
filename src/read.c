#include "little-oscar/osc_internal.h"

int osc_packet_get_type(const char *buffer, int len) {
    if ((len < 4) || (len & 0x03)) return OSC_ERROR;
    return buffer[0] == '#' ? OSC_BUNDLE : OSC_MESSAGE;
}

int osc_bundle_reader_init(osc_bundle_reader_t *reader, const char *buffer, int len) {
    
    if (len < 20) return OSC_ERROR;
    
    reader->bundle_ptr  = buffer;
    reader->bundle_end  = buffer + len;
    reader->msg_ptr     = buffer + 16;
    
    return OSC_OK;
    
}

osc_timetag_t osc_bundle_reader_get_timetag(osc_bundle_reader_t *reader) {
    return *((osc_timetag_t*)(reader->bundle_ptr + 8));
}

int osc_bundle_reader_next(osc_bundle_reader_t *reader, const char **start, int32_t *len) {
    
    if (reader->msg_ptr == reader->bundle_end) return OSC_END;
    
    osc_v32_t msg_len;
    msg_len.u32 = osc_ntoh32(*((uint32_t*)reader->msg_ptr));
    
    *start  = (reader->msg_ptr + 4);
    *len    = msg_len.i32;
    
    reader->msg_ptr = (*start) + (*len);
    if (reader->msg_ptr > reader->bundle_end) return OSC_ERROR;
    
    /* checks message length & alignment */
    return osc_packet_get_type(*start, *len);
    
}

int osc_msg_reader_init(osc_msg_reader_t *reader, const char *buffer, int len) {
    
    reader->msg_ptr = buffer;
    reader->msg_end = buffer + len;
    
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
}

int osc_msg_reader_is_typed(osc_msg_reader_t *reader) {
    return reader->type_ptr != NULL;
}

const char *osc_msg_reader_get_address(osc_msg_reader_t *reader) {
    return reader->msg_ptr;
}

int osc_msg_reader_get_arg(osc_msg_reader_t *reader, osc_arg_t *arg) {
    char t = osc_msg_reader_next_arg(reader);
    if (t < 0) return t;
    arg->type = t;
    switch (t) {
        case 'T':   /* fall through */
        case 'F':   /* fall through */
        case 'N':   /* fall through */
        case 'I':   return OSC_OK;
        case 'i':   return osc_msg_reader_get_arg_int32(reader, &arg->val.val_int32);
        case 'h':   return osc_msg_reader_get_arg_int64(reader, &arg->val.val_int64);
        case 't':   return osc_msg_reader_get_arg_timetag(reader, &arg->val.val_timetag);
        case 'f':   return osc_msg_reader_get_arg_float(reader, &arg->val.val_float);
        case 'd':   return osc_msg_reader_get_arg_double(reader, &arg->val.val_double);
        case 'k':   /* fall through */
        case 's':   /* fall through */
        case 'S':   return osc_msg_reader_get_arg_str(reader, &arg->val.val_str);
        case 'b':   return osc_msg_reader_get_arg_blob(reader, &arg->val.val_blob.data, &arg->val.val_blob.len);
        default:    return OSC_ERROR;
    }
}

char osc_msg_reader_next_arg(osc_msg_reader_t *reader) {
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
    if (reader->msg_end - reader->arg_ptr < (int)sizeof(raw_val.union_member)) return OSC_ERROR; \
    raw_val.u##bits = osc_ntoh##bits(*((uint##bits##_t*)reader->arg_ptr)); \
    *target = raw_val.union_member; \
    reader->arg_ptr += sizeof(raw_val.union_member);
    
int osc_msg_reader_get_arg_int32(osc_msg_reader_t *reader, int32_t *val) {
    READ_FIXED(val, 32, i32);
    return OSC_OK;
}

int osc_msg_reader_get_arg_int64(osc_msg_reader_t *reader, int64_t *val) {
    READ_FIXED(val, 64, i64);
    return OSC_OK;
}

int osc_msg_reader_get_arg_timetag(osc_msg_reader_t *reader, osc_timetag_t *val) {
    READ_FIXED(val, 64, timetag);
    return OSC_OK;
}

int osc_msg_reader_get_arg_float(osc_msg_reader_t *reader, float *val) {
    READ_FIXED(val, 32, fl);
    return OSC_OK;
}

int osc_msg_reader_get_arg_double(osc_msg_reader_t *reader, double *val) {
    READ_FIXED(val, 64, fl);
    return OSC_OK;
}

int osc_msg_reader_get_arg_str(osc_msg_reader_t *reader, const char **val) {
    int len = ROUND32(strlen(reader->arg_ptr) + 1);
    if (reader->arg_ptr + len > reader->msg_end) return OSC_ERROR;
    *val = reader->arg_ptr;
    reader->arg_ptr += len;
    return OSC_OK;
}

int osc_msg_reader_get_arg_blob(osc_msg_reader_t *reader, void **val, int32_t *sz) {
    READ_FIXED(sz, 32, i32);
    int32_t rounded_sz = ROUND32(*sz);
    if (reader->arg_ptr + rounded_sz > reader->msg_end) return OSC_ERROR;
    *val = (void*) reader->arg_ptr;
    reader->arg_ptr += rounded_sz;
    return OSC_OK;
}
