# Roy Ros Cobo
# Gstreamer-WebRTC-SFU
SFU with WebRTC usign Gstreamer

Compile gstRoy.c: ```gcc gst.c $(pkg-config --cflags --libs gstreamer-webrtc-1.0 gstreamer-sdp-1.0 libsoup-2.4 json-glib-1.0) -o gst```

Run signalling server: ```nodejs sign.js```

```diff
- This project is not completely done.
```
**When new src pad appears because a new user is trying to connect to an existent user already connected to the server, this function should link the stream from the new peer to the old peer but the connection is not working.
At line 339 is created the element where it should be connected the new src pad with the stream of the new peer and connect it to the a new WebRTCbin, creted in the same line, to send it to the old peer but this element is not created correctly so it doesn't works.  
At lines 344 and 345 there is 2 options to get the sink pad from the element created previously to connect it later with the new src pad with the new peer stream but i dont know wich one could work since the element is not created correctly.
After this and if this would work at line 347 is linked the new src pad with the sink pad of the element and in the end of the function the pipe with all this elements in it is synced and started.
I think the problem is on line 339 at this code: "rtpvp8depay name=dp ! queue ! rtpvp8pay ! queue ! application/x-rtp,media=video,payload=96,encoding-name=VP8 ! webrtcbin name=newBin" where the elements are not in the right order and not the right ones.  
At line 350 starts the debuging i used to fix this but i never ended it so if you know if some way it can works please let me know it would be so helpful since im not working anymore on this project.**
     
    
     
This is my university final project.

The project consists in building a basic SFU (Selective Forward Unit) server to serve WebRTC (Web Real Time Communications) peers media streams. 

The WebRTC is an open project to integrate RTC on browsers via a simple API. WebRTC is meant to be used directly from one browser peer to another. The problem is when the number of peers increase, then probably  browsers may not support that much media fluxes due used to be mobile phones, PCs and other not powerful devices.
With a SFU this problem has less impact. With a SFU (depending on the hardware they are running on) the number of supported peers without problems is much bigger than without. A multi-party WebRTC app without SFU can support around 5 peers maximum, meanwhile the same with a SFU can support more than 20.

The idea of this SFU is that the peers don’t have to send their streams to all of the other peers. They just communicate with the SFU server and it spreads the media to all the other peers. Each peer will be sending 1 stream and receiving N-1 streams ( where N is the total number of peers in the conversation ).

The SFU server is built with a new GStreamer plug-in GstWebRTC running on Linux OS. GStreamer is a pipeline-based multimedia framework that links together media processing systems to complete complex work-flows.
This frameworks is written on C as the SFU server. 

To make it work properly needs a JSON protocol to negotiate the RTC capabilities and 2 more components: a Node.js signalling server to handle the web-socket negotiations and a JS browser app to be a peer.
