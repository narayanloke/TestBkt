//sudo sysctl -w net.core.rmem_max=26214400
//sudo sysctl -w net.core.wmem_max=26214400
//Optimizations to be done on core
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>

#define BUFLEN 512
#define PORT 9000


volatile int keep_running = 1;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

struct receiver_args {
    int sock;
    struct sockaddr_in *si_other;
};

void handle_signal(int sig) {
    pthread_mutex_lock(&mutex);
    keep_running = 0;
    pthread_mutex_unlock(&mutex);
}

struct __attribute__((packed)) TestStruct
{
    uint8_t x;
    uint32_t len;
    float_t temp;
};

typedef struct TestStruct TestADT;

char *value_int;

struct sockaddr_in si_me, si_other;
int s=0;


int createUDPSocket(struct sockaddr_in *si_me, int *s, const char *interface_name, const char *ip_address,int port)
{
    // Create UDP socket
    if ((*s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        return -1;
    }

    // Setup address structure
    memset((char *)si_me, 0, sizeof(struct sockaddr_in));
    si_me->sin_family = AF_INET;
    si_me->sin_port = htons(port);

    // Set specific IP if provided, otherwise use INADDR_ANY
    if (ip_address != NULL) {
        si_me->sin_addr.s_addr = inet_addr(ip_address);
    } else {
        si_me->sin_addr.s_addr = htonl(INADDR_ANY);
    }

    // Bind to specific interface if provided
    if (interface_name != NULL) {
        #ifdef __APPLE__
            // macOS specific code
            struct ifreq ifr;
            memset(&ifr, 0, sizeof(ifr));
            strncpy(ifr.ifr_name, interface_name, IFNAMSIZ-1);
            if (ioctl(*s, SIOCGIFINDEX, &ifr) < 0) {
                return -3;
            }
            if (setsockopt(*s, IPPROTO_IP, IP_BOUND_IF, &ifr.ifr_ifindex, sizeof(ifr.ifr_ifindex)) < 0) {
                return -4;
            }
        #else
            // Linux specific code
            struct ifreq ifr;
            memset(&ifr, 0, sizeof(ifr));
            strncpy(ifr.ifr_name, interface_name, IFNAMSIZ-1);
            if (setsockopt(*s, SOL_SOCKET, SO_BINDTODEVICE, (void *)&ifr, sizeof(ifr)) < 0) {
                return -3;
            }
        #endif
    }

    // Bind socket
    if (bind(*s, (struct sockaddr*)si_me, sizeof(struct sockaddr_in)) == -1)
    {
        return -2;
    }
    return 0;
}




// Receiver thread function
void* receive_thread(void* arg) {
    struct receiver_args *args = (struct receiver_args*)arg;
    int sock = args->sock;
    struct sockaddr_in *si_other = args->si_other;

    char buf[BUFLEN];
    socklen_t slen = sizeof(struct sockaddr_in);
    int recv_len;

    printf("Receiver thread started...\n");

    while (1) {
        pthread_mutex_lock(&mutex);
        if (!keep_running) {
            pthread_mutex_unlock(&mutex);
            break;
        }
        pthread_mutex_unlock(&mutex);

        // Clear buffer
        memset(buf, 0, BUFLEN);

        // Try to receive data
        if ((recv_len = recvfrom(sock, buf, BUFLEN, MSG_DONTWAIT,
                                (struct sockaddr*)si_other, &slen)) == -1) {
            // If no data, sleep briefly to prevent CPU spinning
            usleep(1000);  // 1ms sleep
            continue;
        }

        // Print received message
        printf("Received packet from %s:%d\n",
               inet_ntoa(si_other->sin_addr), ntohs(si_other->sin_port));
        printf("Data: %s\n", buf);
    }

    printf("Receiver thread ending...\n");
    return NULL;
}



// Send function
int send_message(int sock, struct sockaddr_in *si_other, const char *message) {
    socklen_t slen = sizeof(struct sockaddr_in);

    if (sendto(sock, message, strlen(message), 0,
               (struct sockaddr*)si_other, slen) == -1) {
        return -1;
    }
    return 0;
}



char* serialize(TestADT at)
{
    value_int = (char*)malloc(sizeof(at));  // Allocate memory for the serialized data
    if (!value_int) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    memcpy(value_int, &at, sizeof(at));  // Copy the structure data into the allocated memory
    return value_int;
}

TestADT deserialize(char* str)
{
    TestADT ta;
    memcpy(&ta,str,sizeof(ta));
    return ta;
}


pthread_t recv_thread;

int main(int argc,char** argv)
{
    TestADT aa;
    aa.len=10;
    aa.temp=12.12;
    aa.x=2;

    char message[BUFLEN];

    char* btl;
    printf("\nPrior to Serialization\n");
    printf("\nLen: %u \n",aa.len);
    printf("\nX:%d \n",aa.x);
    printf("\nTemp: %f \n",aa.temp);

    btl=serialize(aa);

    TestADT ttl = deserialize(btl);

    printf("\nPost to Serialization\n");
    printf("\nLen: %u \n",ttl.len);
    printf("\nX:%d \n",ttl.x);
    printf("\nTemp: %f \n",ttl.temp);

    // Setup signal handling for graceful shutdown
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Create and bind socket
    if (createUDPSocket(&si_me, &s, NULL, NULL,atoi(argv[1])) != 0) {
        fprintf(stderr, "Cannot create socket\n");
        return 1;
    }


    // Setup receiver thread arguments
    struct receiver_args args;
    args.sock = s;
    args.si_other = &si_other;

    // Create receiver thread
    if (pthread_create(&recv_thread, NULL, receive_thread, &args) != 0) {
        fprintf(stderr, "Cannot create receiver thread\n");
        close(s);
        return 1;
    }

    printf("UDP System Ready - Enter messages to send (Ctrl+C to quit)\n");

    // Main sending loop
    while (keep_running) {
        // Get input from user
       // if (fgets(message, BUFLEN, stdin) != NULL) {
            // Remove trailing newline
            message[strcspn(message, "\n")] = 0;

            // Setup destination address (example: sending to localhost)
            memset((char *)&si_other, 0, sizeof(si_other));
            si_other.sin_family = AF_INET;
            si_other.sin_port = htons(atoi(argv[2]));
            si_other.sin_addr.s_addr = inet_addr("127.0.0.1");  // Change this for different destination

            // Send message
            if (send_message(s, &si_other, btl) == -1) {
                fprintf(stderr, "Error sending message\n");
            }
        //}
    }

    // Wait for receiver thread to finish
    pthread_join(recv_thread, NULL);

    // Cleanup
    pthread_mutex_destroy(&mutex);
    close(s);

    return 0;
}
