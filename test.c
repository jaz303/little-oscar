#include "osc.h"

#include <stdlib.h>
#include <stdio.h>

#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <netdb.h>

#define BUFFER_LEN 1536

int main(int argc, char *argv[]) {
    
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    int port = 9000;
    
    struct sockaddr_in addr;
    struct hostent *hp = gethostbyname("127.0.0.1");
  
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    memcpy(&addr.sin_addr, hp->h_addr, hp->h_length);
    addr.sin_port = htons(port);

    char buffer[BUFFER_LEN];
    osc_bundle_t bundle;
    
    osc_bundle_init(&bundle, buffer, BUFFER_LEN);
    
    int i = 0;
    float f = 0.0;
    
    while (1) {
        osc_bundle_start(&bundle, OSC_NOW);
        osc_bundle_write(&bundle, "/foo/bar", "iifs", i, 2, f, "string string");
        osc_bundle_write(&bundle, "/baz/bleem", "ifs", i * 2, f / 2, "another string string");
        osc_bundle_write(&bundle, "/boof/barf", "iii", i * 3, i * 4, i * 5);
        
        int written = sendto(sock, bundle.buffer, bundle.len, 0, (struct sockaddr *)&addr, sizeof(addr));
        printf("sent %d bytes...\n", written);
        i += 1;
        f += 0.05;
        sleep(1);
    }
    
    return 0;   
    
}


