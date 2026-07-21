/* GStreamer test-launch example (gst-rtsp-server 1.16) */

#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>

#define DEFAULT_RTSP_PORT "8554"

static char *port = (char *) DEFAULT_RTSP_PORT;

static GOptionEntry entries[] = {
    {"port", 'p', 0, G_OPTION_ARG_STRING, &port,
     "Port to listen on (default: " DEFAULT_RTSP_PORT ")", "PORT"},
    {NULL}
};

int main(int argc, char *argv[])
{
    GMainLoop *loop;
    GstRTSPServer *server;
    GstRTSPMountPoints *mounts;
    GstRTSPMediaFactory *factory;
    GOptionContext *optctx;
    GError *error = NULL;

    if (argc < 2) {
        g_printerr("Usage: %s \"( pipeline-with-pay0 )\" [-p port]\n", argv[0]);
        return 1;
    }

    optctx = g_option_context_new("- Test RTSP Server");
    g_option_context_add_main_entries(optctx, entries, NULL);
    g_option_context_add_group(optctx, gst_init_get_option_group());
    if (!g_option_context_parse(optctx, &argc, &argv, &error)) {
        g_printerr("Error parsing options: %s\n", error->message);
        g_option_context_free(optctx);
        g_clear_error(&error);
        return -1;
    }
    g_option_context_free(optctx);

    loop = g_main_loop_new(NULL, FALSE);

    server = gst_rtsp_server_new();
    g_object_set(server, "service", port, NULL);

    mounts = gst_rtsp_server_get_mount_points(server);

    factory = gst_rtsp_media_factory_new();
    gst_rtsp_media_factory_set_launch(factory, argv[1]);
    gst_rtsp_media_factory_set_shared(factory, TRUE);
    gst_rtsp_mount_points_add_factory(mounts, "/test", factory);

    g_object_unref(mounts);

    gst_rtsp_server_attach(server, NULL);

    g_print("stream ready at rtsp://0.0.0.0:%s/test\n", port);
    g_main_loop_run(loop);

    return 0;
}
