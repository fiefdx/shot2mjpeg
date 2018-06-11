// screenshot to mjpeg for linux

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>
#include "httpd.h"
#include "httpd_priv.h"
#include "shot2jpeg.h"
#include "log.h"
#include "config.h"

struct config CONFIG;

struct FileContent {
    unsigned long length;
    char *buffer;
};

struct FileContent read_file(char *name) {
    FILE *file;
    struct FileContent result;

    //Open file
    file = fopen(name, "rb");
    if (!file) {
        log_error("Unable to open file %s", name);
        return result;
    }

    //Get file length
    fseek(file, 0, SEEK_END);
    result.length=(ftell(file));
    log_debug("file length: %d", result.length);
    fseek(file, 0, SEEK_SET);

    //Allocate memory
    result.buffer=(char *)malloc((result.length)+1);
    if (!result.buffer) {
        log_error("Memory error!");
        fclose(file);
        return result;
    }

    //Read file contents into buffer
    fread(result.buffer, result.length, 1, file);
    fclose(file);

    return result;
}

int nsleep(long miliseconds) {
    struct timespec req, rem;

    if(miliseconds > 999) {
        req.tv_sec = (int)(miliseconds / 1000);                            /* Must be Non-Negative */
        req.tv_nsec = (miliseconds - ((long)req.tv_sec * 1000)) * 1000000; /* Must be in range of 0 to 999999999 */
    } else {
        req.tv_sec = 0;                         /* Must be Non-Negative */
        req.tv_nsec = miliseconds * 1000000;    /* Must be in range of 0 to 999999999 */
    }

    return nanosleep(&req, &rem);
}

void jpeg_handler(httpd *server, httpReq *request) {
    httpdSetContentType(server, request, "image/jpeg");
    httpdSendHeaders(server, request);
    struct FileContent buffer;
    buffer = read_file("../example.jpeg");
    log_debug("buffer length: %d", buffer.length);
    request->response.responseLength += buffer.length;
    _httpd_net_write(request->clientSock, buffer.buffer, buffer.length);
    // free(&buffer); // *** Error in `./http_example_server': double free or corruption (out): 0x00007ffd72cd9580 ***
    return;
}

void mjpeg_handler(httpd *server, httpReq *request) {
    log_debug("mjpeg_handler start");
    httpdSetContentType(server, request, "multipart/x-mixed-replace;boundary=\"mjpegstream\"");
    httpdAddHeader(server, request, "Cache-Control: no-store, no-cache, must-revalidate, pre-check=0, post-check=0, max-age=0");
    // httpdAddHeader(server, request, "Content-Type: multipart/x-mixed-replace;boundary=--boundary");
    httpdAddHeader(server, request, "Pragma: no-cache");
    // httpdAddHeader(server, request, "Connection: close");
    httpdSendHeaders(server, request);

    log_debug("Quality: %d", CONFIG.quality);

    struct timeval s, ss;
    struct FileContent buffer;

    xcb_connection_t *conn = xcb_connect(NULL, NULL);

    const xcb_setup_t *setup = xcb_get_setup(conn);
    xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
    xcb_screen_t *screen = iter.data;

    for (int i = 0; 1; i++) {
        gettimeofday(&s, NULL);
        log_debug("frame: %04d", i);
        xcb_image_t *screenshot;
        screenshot = take_screenshot(conn, screen);
        xcb_pixmap_t pixmap = image_to_pixmap(conn, screen, screenshot);
        log_debug("screenshot: width: %d, height: %d, size: %d", screenshot->width, screenshot->height, screenshot->size);
        log_debug("pixmap: %d", pixmap);

        char *bp;
        size_t size;
        FILE *stream;
        stream = open_memstream(&bp, &size);
        write_to_jpeg_buffer(stream, CONFIG.quality, screenshot);
        buffer.buffer = bp;
        buffer.length = size;

        char *header = (char *)malloc((150)+1);
        sprintf(header, "--mjpegstream\r\nContent-Type: image/jpeg\r\nContent-Length: %lu\r\n\r\n", buffer.length);
        int r;
        r = _httpd_net_write(request->clientSock, header, strlen(header));
        log_debug("socket r: %d", r);
        if (r == 0) {
            r = _httpd_net_write(request->clientSock, buffer.buffer, buffer.length);
        } else {
            break;
        }

        xcb_image_destroy(screenshot);
        xcb_free_pixmap(conn, pixmap);
        free(bp);
        free(header);
        log_debug("buffer: size: %d", size);
        gettimeofday(&ss, NULL);
        long interval = 40 - (((ss.tv_sec - s.tv_sec) * 1000000 + (ss.tv_usec - s.tv_usec))/1000); // ms
        if (interval > 0) {
            nsleep(interval);
            log_debug("sleep: %dms", interval);
        }
    }
    free(buffer.buffer);
    log_debug("mjpeg_handler end");
}

void sig_ign(int s) {
    log_warn("Caught SIGPIPE");
}

int main(argc, argv)
    int argc;
    char *argv[];
{
    httpd   *server;
    httpReq *request;
    struct timeval timeout;
    int result, port = 8080;

    FILE *log_file;

    CONFIG = get_config("./configuration.conf");

    //Open file
    log_file = fopen("./server.log", "a");
    if (!log_file) {
        fprintf(stderr, "Unable to open file ./server.log");
        exit(1);
    }
    log_set_fp(log_file);
    log_set_level(LOG_TRACE);
    // log_set_level(LOG_INFO);
    log_set_quiet(0);

    signal(SIGPIPE, SIG_IGN);
    /*
    ** Create a server and setup our logging
    */
    server = httpdCreate(NULL, CONFIG.port);
    if (server == NULL)
    {
        log_error("Can't create server at port: %d", CONFIG.port);
        exit(1);
    }
    httpdSetAccessLog(server, stdout);
    httpdSetErrorLog(server, stderr);

    /*
    ** Setup some content for the server
    */
    httpdAddCContent(server, "/", "mjpeg", HTTP_TRUE, NULL, mjpeg_handler);
    httpdAddCContent(server, "/", "jpeg", HTTP_TRUE, NULL, jpeg_handler);

    /*
    ** Go into our service loop
    */
    log_info("mjpeg server start at port: %d, quality: %d", CONFIG.port, CONFIG.quality);
    while (1) {
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        request = httpdReadRequest(server, &timeout, &result);
        if (request == NULL && result == 0) {
            /* Timed out. Go do something else if needed */
            continue;
        }

        if (result < 0) {
            /* Error occurred */
            continue;
        }
        httpdProcessRequest(server, request);
        httpdEndRequest(server, request);
    }
    log_info("mjpeg server end");
}