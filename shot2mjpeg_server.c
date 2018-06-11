#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>
#include "httpd.h"
#include "httpd_priv.h"
#include "shot2jpeg.h"


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
        fprintf(stderr, "Unable to open file %s", name);
        return;
    }

    //Get file length
    fseek(file, 0, SEEK_END);
    result.length=(ftell(file));
    printf("file length: %d\n", result.length);
    fseek(file, 0, SEEK_SET);

    //Allocate memory
    result.buffer=(char *)malloc((result.length)+1);
    if (!result.buffer) {
        fprintf(stderr, "Memory error!");
        fclose(file);
        return;
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
    printf("buffer length: %d\n", buffer.length);
    request->response.responseLength += buffer.length;
    _httpd_net_write(request->clientSock, buffer.buffer, buffer.length);
    // free(&buffer); // *** Error in `./http_example_server': double free or corruption (out): 0x00007ffd72cd9580 ***
    return;
}

void mjpeg_handler(httpd *server, httpReq *request) {
    printf(">>>>>> mjpeg_handler start\n");
    httpdSetContentType(server, request, "multipart/x-mixed-replace;boundary=\"mjpegstream\"");
    httpdAddHeader(server, request, "Cache-Control: no-store, no-cache, must-revalidate, pre-check=0, post-check=0, max-age=0");
    // httpdAddHeader(server, request, "Content-Type: multipart/x-mixed-replace;boundary=--boundary");
    httpdAddHeader(server, request, "Pragma: no-cache");
    // httpdAddHeader(server, request, "Connection: close");
    httpdSendHeaders(server, request);

    int quality = 75;
    printf("Quality: %d\n", quality);

    struct timeval s, ss;
    struct FileContent buffer;

    xcb_connection_t *conn = xcb_connect(NULL, NULL);

    const xcb_setup_t *setup = xcb_get_setup(conn);
    xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
    xcb_screen_t *screen = iter.data;

    for (int i = 0; 1; i++) {
        gettimeofday(&s, NULL);
        printf("frame: %04d\n", i);
        xcb_image_t *screenshot;
        screenshot = take_screenshot(conn, screen);
        xcb_pixmap_t pixmap = image_to_pixmap(conn, screen, screenshot);
        printf("screenshot: width: %d, height: %d, size: %d\n", screenshot->width, screenshot->height, screenshot->size);
        // printf("pixmap: %d\n", pixmap);

        char *bp;
        size_t size;
        FILE *stream;
        stream = open_memstream(&bp, &size);
        write_to_jpeg_buffer(stream, quality, screenshot);
        buffer.buffer = bp;
        buffer.length = size;

        char *header = (char *)malloc((150)+1);
        sprintf(header, "--mjpegstream\r\nContent-Type: image/jpeg\r\nContent-Length: %d\r\n\r\n", buffer.length);
        int r;
        r = _httpd_net_write(request->clientSock, header, strlen(header));
        printf("socket r: %d\n", r);
        if (r == 0) {
            r = _httpd_net_write(request->clientSock, buffer.buffer, buffer.length);
        } else {
            break;
        }

        xcb_image_destroy(screenshot);
        xcb_free_pixmap(conn, pixmap);
        free(bp);
        free(header);
        // // printf("buffer: size: %d\n", size);
        gettimeofday(&ss, NULL);
        long interval = 40 - (((ss.tv_sec - s.tv_sec) * 1000000 + (ss.tv_usec - s.tv_usec))/1000); // ms
        if (interval > 0) {
            nsleep(interval);
            printf("sleep: %dms\n", interval);
        }
    }
    free(buffer.buffer);
    printf(">>>>>> mjpeg_handler end\n");
}

void sig_ign(int s) {
    printf("Caught SIGPIPE\n");
}

int main(argc, argv)
    int argc;
    char *argv[];
{
    httpd   *server;
    httpReq *request;
    struct timeval timeout;
    int result;

    signal(SIGPIPE, SIG_IGN);
    /*
    ** Create a server and setup our logging
    */
    server = httpdCreate(NULL, 8080);
    if (server == NULL)
    {
        perror("Can't create server");
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
    printf("mjpeg server start\n");
    while (1) {
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        request = httpdReadRequest(server, &timeout, &result);
        if (request == NULL && result == 0)
        {
            /* Timed out. Go do something else if needed */
            continue;
        }

        if (result < 0)
        {
            /* Error occurred */
            continue;
        }
        httpdProcessRequest(server, request);
        httpdEndRequest(server, request);
    }
    printf("mjpeg server end\n");
}