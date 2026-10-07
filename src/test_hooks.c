#include "test_hooks.h"
#include "log.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <orbis/libkernel.h>

static int s_sock = -1;
static pthread_t s_thr;
static atomic_bool s_run;
static int s_tap_ms;

/* Last command sender; replies go there. Written by the hook thread,
 * read by the render/input threads under s_reply_lock. */
static pthread_mutex_t s_reply_lock = PTHREAD_MUTEX_INITIALIZER;
static struct sockaddr_in s_reply_to;
static int s_have_reply_to;

/* Command receive times, 0 = nothing pending. */
static _Atomic uint64_t s_flash_cmd_us;
static _Atomic uint64_t s_flash_taken_us;
static _Atomic uint64_t s_tap_cmd_us;
static _Atomic uint64_t s_tap_until_us;
static atomic_bool s_tap_reported;

static uint64_t now_us(void) {
    return (uint64_t)sceKernelGetProcessTime();
}

static void reply(const char *msg) {
    pthread_mutex_lock(&s_reply_lock);
    if (s_sock >= 0 && s_have_reply_to)
        sendto(s_sock, msg, strlen(msg), 0,
               (const struct sockaddr *)&s_reply_to, sizeof(s_reply_to));
    pthread_mutex_unlock(&s_reply_lock);
}

static void *hook_thread_main(void *arg) {
    (void)arg;
    char buf[32];
    while (atomic_load(&s_run)) {
        struct sockaddr_in from;
        socklen_t flen = sizeof(from);
        ssize_t n = recvfrom(s_sock, buf, sizeof(buf) - 1, 0,
                             (struct sockaddr *)&from, &flen);
        if (n <= 0)
            continue; /* timeout: re-check s_run */
        uint64_t t = now_us();
        buf[n] = '\0';
        buf[strcspn(buf, " \r\n")] = '\0';

        pthread_mutex_lock(&s_reply_lock);
        s_reply_to = from;
        s_have_reply_to = 1;
        pthread_mutex_unlock(&s_reply_lock);

        if (!strcmp(buf, "flash")) {
            atomic_store(&s_flash_cmd_us, t);
        } else if (!strcmp(buf, "tap")) {
            atomic_store(&s_tap_reported, false);
            atomic_store(&s_tap_until_us, t + (uint64_t)s_tap_ms * 1000u);
            atomic_store(&s_tap_cmd_us, t);
        } else if (!strcmp(buf, "ping")) {
            reply("pong");
        } else {
            LOGW("hooks: unknown command '%s'", buf);
        }
    }
    return NULL;
}

int test_hooks_start(int port, int tap_ms) {
    if (port <= 0 || s_sock >= 0)
        return -1;
    s_tap_ms = tap_ms > 0 ? tap_ms : 120;
    s_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (s_sock < 0) {
        LOGE("hooks: socket errno=%d", errno);
        return -1;
    }
    struct timeval tv = { 0, 200 * 1000 };
    setsockopt(s_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons((uint16_t)port);
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s_sock, (struct sockaddr *)&a, sizeof(a)) != 0) {
        LOGE("hooks: bind :%d errno=%d", port, errno);
        close(s_sock);
        s_sock = -1;
        return -1;
    }
    atomic_store(&s_flash_cmd_us, 0);
    atomic_store(&s_tap_cmd_us, 0);
    atomic_store(&s_tap_until_us, 0);
    s_have_reply_to = 0;
    atomic_store(&s_run, true);
    if (pthread_create(&s_thr, NULL, hook_thread_main, NULL) != 0) {
        atomic_store(&s_run, false);
        close(s_sock);
        s_sock = -1;
        LOGE("hooks: pthread_create failed");
        return -1;
    }
    LOGW("hooks: TEST HOOKS ON, udp :%d (flash, tap %d ms, ping)", port, s_tap_ms);
    return 0;
}

void test_hooks_stop(void) {
    if (s_sock < 0)
        return;
    atomic_store(&s_run, false);
    pthread_join(s_thr, NULL);
    pthread_mutex_lock(&s_reply_lock);
    close(s_sock);
    s_sock = -1;
    s_have_reply_to = 0;
    pthread_mutex_unlock(&s_reply_lock);
    atomic_store(&s_tap_until_us, 0);
}

bool test_hooks_take_flash(void) {
    uint64_t t = atomic_exchange(&s_flash_cmd_us, 0);
    if (!t)
        return false;
    atomic_store(&s_flash_taken_us, t);
    return true;
}

void test_hooks_flash_flipped(void) {
    uint64_t t = atomic_exchange(&s_flash_taken_us, 0);
    if (!t)
        return;
    uint64_t d = now_us() - t;
    char msg[32];
    snprintf(msg, sizeof(msg), "flash %llu", (unsigned long long)d);
    reply(msg);
    LOGI("hooks: flash cmd_to_flip=%llu us", (unsigned long long)d);
}

bool test_hooks_tap_active(void) {
    uint64_t until = atomic_load(&s_tap_until_us);
    return until && now_us() < until;
}

void test_hooks_tap_sent(void) {
    if (atomic_exchange(&s_tap_reported, true))
        return;
    uint64_t t = atomic_load(&s_tap_cmd_us);
    if (!t)
        return;
    uint64_t d = now_us() - t;
    char msg[32];
    snprintf(msg, sizeof(msg), "tap %llu", (unsigned long long)d);
    reply(msg);
    LOGI("hooks: tap cmd_to_send=%llu us", (unsigned long long)d);
}
