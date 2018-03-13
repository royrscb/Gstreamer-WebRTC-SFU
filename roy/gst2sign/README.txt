# gcc gst.c $(pkg-config --cflags --libs gstreamer-webrtc-1.0 gstreamer-sdp-1.0 libsoup-2.4 json-glib-1.0) -o 


This will be the first prove where the gstreamer app just have to connect by sockets.io to the
already done and totally normal signaling server made with JS to exghange some txt msg by sockets.

