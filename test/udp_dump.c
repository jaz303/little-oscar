#include <stdio.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFFER_SIZE 1536

#include "little-oscar/osc.h"

void dump_osc_message(osc_reader_t *reader, int indent);
void dump_osc_packet(char *buffer, int len, int indent);

int main(int argc, char *argv[]) {
    
    int sock_fd, n;
    struct sockaddr_in server_address, client_address;
    socklen_t len;
    
    // last byte set to NUL; ensures unterminated strings in message payload
    // cannot break out of the buffer.
    char buffer[BUFFER_SIZE];
    buffer[BUFFER_SIZE - 1] = '\0';
    
    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(9000);
    bind(sock_fd, (struct sockaddr *)&server_address, sizeof(server_address));
    
    for (;;) {
        len = sizeof(client_address);
        n = recvfrom(sock_fd, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr *)&client_address, &len);
        dump_osc_packet(buffer, n, 0);
    }
    
    return 0;

}

void p_indent(int c) {
    while (c--) { putchar(' '); putchar(' '); }
}


void dump_osc_message(osc_reader_t *reader, int indent) {
    p_indent(indent);
    printf("%s\n", osc_reader_get_msg_address(reader));
    
    indent++;
    
    if (osc_reader_msg_is_typed(reader)) {
        osc_arg_t arg;
        while (osc_reader_get_arg(reader, &arg) == OSC_OK) {
            p_indent(indent);
            putchar(arg.type);
            putchar(':');
            putchar(' ');
            switch (arg.type) {
                case 'T': puts("true"); break;
                case 'F': puts("false"); break;
                case 'N': puts("null"); break;
                case 'I': puts("infinity"); break;
                case 'i': printf("%d\n", arg.val.val_int32); break;
                case 'h': printf("%ld\n", arg.val.val_int64); break;
                case 't': printf("timetag: %ld\n", arg.val.val_timetag); break;
                case 'f': printf("%f\n", arg.val.val_float); break;
                case 'd': printf("%f\n", arg.val.val_double); break;
                case 's': /* fall through */
                case 'S': /* fall through */
                case 'k': printf("%s\n", arg.val.val_str); break;
                case 'b': printf("blob, %d byte(s)\n", arg.val.val_blob.len); break;
                default:  printf("(unknown argument type)\n"); break;
            }
        }
    } else {
        p_indent(indent);
        printf("(message has no type tag)\n");
    }
}

void dump_osc_packet(char *buffer, int len, int indent) {
    
    osc_reader_t reader;
    osc_reader_init(&reader, buffer, len);
    
    if (osc_reader_is_bundle(&reader)) {
        p_indent(indent);
        printf("#bundle (timetag=xxx,sz=xxx)\n");
        while (osc_reader_start_msg(&reader) == OSC_OK) {
            if (0) {
                
            } else {
                dump_osc_message(&reader, indent + 1);
            }
        }
        // get message length
        // start iteratng over messages and dump
    } else {
        if (osc_reader_start_msg(&reader) == OSC_OK) {
            dump_osc_message(&reader, indent);
        }
    }

}
