//#include <stdio.h>
//#include <stdlib.h>
//#include <string.h>
//#include <unistd.h>
//#include <pthread.h>
//#include <sys/socket.h>
//#include <netinet/in.h>
//#include <arpa/inet.h>
//#include <net/if.h>
//#include <sys/ioctl.h>
//#include <signal.h>
//#include <time.h>
//#include <sys/time.h>
//#include <errno.h>
//#include <fcntl.h>
//
//#define PACKET_SIZE 128
//#define SEND_RATE 333    // packets per second
//#define RECV_RATE 100    // expected packets per second
//#define SEND_BUFFER_SIZE (PACKET_SIZE * SEND_RATE)
//#define RECV_BUFFER_SIZE (PACKET_SIZE * RECV_RATE)
//#define PORT 8888
//
//// Global variables for graceful shutdown
//volatile int keep_running = 1;
//pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
//
//// Statistics structure
//struct stats {
//    unsigned long packets_sent;
//    unsigned long packets_received;
//    unsigned long bytes_sent;
//    unsigned long bytes_received;
//    struct timespec start_time;
//};
//
//struct stats statistics = {0};
//
//// Structure to pass multiple parameters to threads
//struct thread_args {
//    int sock;
//    struct sockaddr_in *si_other;
//};
//
//void handle_signal(int sig) {
//    pthread_mutex_lock(&mutex);
//    keep_running = 0;
//    pthread_mutex_unlock(&mutex);
//}
//
//// Function to print statistics
//void print_stats() {
//    struct timespec current_time;
//    clock_gettime(CLOCK_MONOTONIC, &current_time);
//    double elapsed = (current_time.tv_sec - statistics.start_time.tv_sec) +
//                    (current_time.tv_nsec - statistics.start_time.tv_nsec) / 1e9;
//
//    printf("\nStatistics:\n");
//    printf("Packets sent: %lu (%.2f packets/sec)\n",
//           statistics.packets_sent, statistics.packets_sent / elapsed);
//    printf("Packets received: %lu (%.2f packets/sec)\n",
//           statistics.packets_received, statistics.packets_received / elapsed);
//    printf("Bytes sent: %lu (%.2f KB/sec)\n",
//           statistics.bytes_sent, (statistics.bytes_sent / 1024.0) / elapsed);
//    printf("Bytes received: %lu (%.2f KB/sec)\n",
//           statistics.bytes_received, (statistics.bytes_received / 1024.0) / elapsed);
//}
//
//void* receive_thread(void* arg) {
//    struct thread_args *args = (struct thread_args*)arg;
//    int sock = args->sock;
//    struct sockaddr_in *si_other = args->si_other;
//
//    char buf[PACKET_SIZE];
//    socklen_t slen = sizeof(struct sockaddr_in);
//    int recv_len;
//    struct timeval tv;
//
//    // Set socket receive timeout
//    tv.tv_sec = 0;
//    tv.tv_usec = 1000;  // 1ms timeout
//    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
//
//    while (1) {
//        pthread_mutex_lock(&mutex);
//        if (!keep_running) {
//            pthread_mutex_unlock(&mutex);
//            break;
//        }
//        pthread_mutex_unlock(&mutex);
//
//        memset(buf, 0, PACKET_SIZE);
//
//        recv_len = recvfrom(sock, buf, PACKET_SIZE, 0,
//                           (struct sockaddr*)si_other, &slen);
//
//        if (recv_len > 0) {
//            statistics.packets_received++;
//            statistics.bytes_received += recv_len;
//        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
//            perror("recvfrom failed");
//        }
//    }
//
//    return NULL;
//}
//
//void* send_thread(void* arg) {
//    struct thread_args *args = (struct thread_args*)arg;
//    int sock = args->sock;
//    struct sockaddr_in *si_other = args->si_other;
//
//    char packet[PACKET_SIZE];
//    struct timespec sleep_time;
//    sleep_time.tv_sec = 0;
//    sleep_time.tv_nsec = 1000000000 / SEND_RATE;  // Calculate nanosleep interval
//
//    while (1) {
//        pthread_mutex_lock(&mutex);
//        if (!keep_running) {
//            pthread_mutex_unlock(&mutex);
//            break;
//        }
//        pthread_mutex_unlock(&mutex);
//
//        // Fill packet with some data (example: packet number)
//        snprintf(packet, PACKET_SIZE, "Packet %lu", statistics.packets_sent);
//
//        if (sendto(sock, packet, PACKET_SIZE, 0,
//                   (struct sockaddr*)si_other, sizeof(struct sockaddr_in)) != -1) {
//            statistics.packets_sent++;
//            statistics.bytes_sent += PACKET_SIZE;
//        }
//
//        nanosleep(&sleep_time, NULL);
//    }
//
//    return NULL;
//}
//
//int createUDPSocket(struct sockaddr_in *si_me, int *s, const char *interface_name,
//                   const char *ip_address) {
//    if ((*s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
//        return -1;
//    }
//
//    // Set socket buffers for high-performance
//    int send_buf = SEND_BUFFER_SIZE;
//    int recv_buf = RECV_BUFFER_SIZE;
//    setsockopt(*s, SOL_SOCKET, SO_SNDBUF, &send_buf, sizeof(send_buf));
//    setsockopt(*s, SOL_SOCKET, SO_RCVBUF, &recv_buf, sizeof(recv_buf));
//
//    // Set socket to non-blocking mode
//    int flags = fcntl(*s, F_GETFL, 0);
//    fcntl(*s, F_SETFL, flags | O_NONBLOCK);
//
//    memset((char *)si_me, 0, sizeof(struct sockaddr_in));
//    si_me->sin_family = AF_INET;
//    si_me->sin_port = htons(PORT);
//
//    if (ip_address != NULL) {
//        si_me->sin_addr.s_addr = inet_addr(ip_address);
//    } else {
//        si_me->sin_addr.s_addr = htonl(INADDR_ANY);
//    }
//
//    if (interface_name != NULL) {
//        struct ifreq ifr;
//        memset(&ifr, 0, sizeof(ifr));
//        strncpy(ifr.ifr_name, interface_name, IFNAMSIZ-1);
//        if (setsockopt(*s, SOL_SOCKET, SO_BINDTODEVICE, (void *)&ifr, sizeof(ifr)) < 0) {
//            return -3;
//        }
//    }
//
//    if (bind(*s, (struct sockaddr*)si_me, sizeof(struct sockaddr_in)) == -1) {
//        return -2;
//    }
//    return 0;
//}
//
//int main2(void) {
//    struct sockaddr_in si_me, si_other;
//    int s;
//    pthread_t recv_thread_id, send_thread_id;
//
//    // Initialize statistics
//    clock_gettime(CLOCK_MONOTONIC, &statistics.start_time);
//
//    // Setup signal handling
//    signal(SIGINT, handle_signal);
//    signal(SIGTERM, handle_signal);
//
//    if (createUDPSocket(&si_me, &s, NULL, NULL) != 0) {
//        fprintf(stderr, "Cannot create socket\n");
//        return 1;
//    }
//
//    // Setup thread arguments
//    struct thread_args args;
//    args.sock = s;
//    args.si_other = &si_other;
//
//    // Setup destination address
//    memset((char *)&si_other, 0, sizeof(si_other));
//    si_other.sin_family = AF_INET;
//    si_other.sin_port = htons(PORT);
//    si_other.sin_addr.s_addr = inet_addr("127.0.0.1");  // Change for different destination
//
//    // Create threads
//    if (pthread_create(&recv_thread_id, NULL, receive_thread, &args) != 0 ||
//        pthread_create(&send_thread_id, NULL, send_thread, &args) != 0) {
//        fprintf(stderr, "Cannot create threads\n");
//        close(s);
//        return 1;
//    }
//
//    printf("UDP System Running - Press Ctrl+C to stop\n");
//
//    // Main loop for printing statistics
//    while (keep_running) {
//        sleep(1);
//        print_stats();
//    }
//
//    // Wait for threads to finish
//    pthread_join(recv_thread_id, NULL);
//    pthread_join(send_thread_id, NULL);
//
//    // Final statistics
//    print_stats();
//
//    // Cleanup
//    pthread_mutex_destroy(&mutex);
//    close(s);
//    return 0;
//}
