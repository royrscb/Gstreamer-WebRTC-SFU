# Author: Roy Ros Cobo
# Gstreamer
Gstreamer WebRTC SFU

Compile gstRoy.c: gcc gstRoy.c $(pkg-config --cflags --libs gstreamer-webrtc-1.0 gstreamer-sdp-1.0 libsoup-2.4 json-glib-1.0) -o gstRoy
Run signalling server: nodejs sign.js

This is my university final project.


The project consists in building a basic SFU (Selective Forward Unit) server to serve WebRTC (Web Real Time Communications) peers media streams. 

The WebRTC is an open project to integrate RTC on browsers via a simple API. WebRTC is meant to be used directly from one browser peer to another. The problem is when the number of peers increase, then probably  browsers may not support that much media fluxes due used to be mobile phones, PCs and other not powerful devices.
With a SFU this problem has less impact. With a SFU (depending on the hardware they are running on) the number of supported peers without problems is much bigger than without. A multi-party WebRTC app without SFU can support around 5 peers maximum, meanwhile the same with a SFU can support more than 20.

The idea of this SFU is that the peers don’t have to send their streams to all of the other peers. They just communicate with the SFU server and it spreads the media to all the other peers. Each peer will be sending 1 stream and receiving N-1 streams ( where N is the total number of peers in the conversation ).

The SFU server is built with a new GStreamer plug-in GstWebRTC running on Linux OS. GStreamer is a pipeline-based multimedia framework that links together media processing systems to complete complex work-flows.
This frameworks is written on C as the SFU server. 

To make it work properly needs a JSON protocol to negotiate the RTC capabilities and 2 more components: a Node.js signalling server to handle the web-socket negotiations and a JS browser app to be a peer.
