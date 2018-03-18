var localStream, peerConnection, dataChannel, wss, localID, remoteID=0;
var gstServerON = false;

//JavaScript variables associated with demo buttons
var dataChannelSend = document.getElementById("dataChannelSend");
var sendButton = document.getElementById("sendButton");

// JavaScript variables associated with HTML5 video elements in the page
var localVideo = document.getElementById("localVideo");
var remoteVideo = document.getElementById("remoteVideo");

// JavaScript variables assciated with call management buttons in the page
var startButton = document.getElementById("startButton");
var callButton = document.getElementById("callButton");
var hangupButton = document.getElementById("hangupButton");


// Just allow the user to click on the 'Call' button at start-up
startButton.disabled = false;
callButton.disabled = true;
hangupButton.disabled = true;

//On startup, just the 'Start' button must be enabled
////sendButton.disabled = true;


// Associate JavaScript handlers with click events on the buttons
startButton.onclick = start;
callButton.onclick = call;
hangupButton.onclick = hangup;

//Associate handlers with buttons
////sendButton.onclick = sendData;

var web_socket_sign_url = 'ws://127.0.0.1:3434';

var configuration = {
	'iceServers': [{
		'urls': 'stun:stun.l.google.com:19302'
	}]
};
             


//////////// Media ///////////////////////////////////////////////////////////////////////////////////////////////

function start() {
  console.log("Requesting local stream");
  // First of all, disable the 'Start' button on the page
  startButton.disabled = true;
  

  var constraints = {video: true, audio: true};

  // Add local stream
  navigator.mediaDevices.getUserMedia(constraints).then((stream) => { 

    localStream = stream 

    if (window.URL) localVideo.srcObject = stream;
    else localVideo.src = stream;
  });

  connectServer();
}

function call(){

  callButton.disabled = true;

  createPeerConnection();

  //---------- Create data channel -------------
  console.log('Creating data channel');
  dataChannel = peerConnection.createDataChannel("dataChannel",{reliable: true});
  handleDataChannel();
  //--------------------------------------------

  peerConnection.createOffer().then(function(description){

    console.log('Setting local description');
    peerConnection.setLocalDescription(description);

    console.log("Calling, sending offer: \n" + description.sdp);
    wss.send(JSON.stringify({type:"offer", data:description, to:remoteID, from:localID}));
  });
}

function hangup(){

  console.log("Ending call");

  peerConnection.close();
  peerConnection = null;

  hangupButton.disabled = true;
  startButton.disabled = false;
  sendButton.disabled = true;

  socket.disconnect();

  location.reload(true);
}


function connectServer(){

  // Connect to signalling server
  wss = new WebSocket(web_socket_sign_url);


  wss.onmessage = function(msg){

    var data = JSON.parse(msg.data);

    console.log("------------------------------------------");
    console.log("<<<Type:"+data.type+" from:"+data.from+" to:"+data.to);

    if(data.type=="txt") console.log(data.data);
    else if(data.type=="id"){

      localID = data.data;

      console.log("My id is:"+localID);
    }else if(data.type=="gstServerON"){

      gstServerON = true;
      callButton.disabled = false;

      console.log(data.data);
    }else if(data.type=="socketON"){

      callButton.disabled = false;

      console.log("^^^ New conected "+data.data.id+" = "+data.data.ip);
    }else if(data.type=="socketOFF"){

      console.log("vvv Disconnected "+data.data.id+" = "+data.data.ip);
    }else if(data.type=="offer"){

      createPeerConnection();

      console.log('Setting remote Description');
      peerConnection.setRemoteDescription(new RTCSessionDescription(data.data));

      answer();

    }else if(data.type=="answer"){

      console.log("Answer received");

      peerConnection.setRemoteDescription(new RTCSessionDescription(data.data));

    }else if(data.type=="candidate"){

      console.log("Candidate received");

      peerConnection.addIceCandidate(new RTCIceCandidate(data.data));

    }else console.log("Unkown type! "+data);

  }
}

function createPeerConnection(){

  console.log('Creating peer connection');
  peerConnection = new RTCPeerConnection(configuration);

  peerConnection.addStream(localStream);

  peerConnection.onicecandidate = function(ev){

    if (ev.candidate) {
      console.log("ICE candidate: \n" + ev.candidate.candidate);
      wss.send(JSON.stringify({type:"candidate", data:ev.candidate, to:remoteID, from: localID}));
    }
  }

  peerConnection.onaddstream = function(ev){

    videoTracks = ev.stream.getVideoTracks();
    audioTracks = ev.stream.getAudioTracks();

    console.log('Incoming stream: ' + videoTracks.length + ' video tracks and ' + audioTracks.length + ' audio tracks');
    remoteVideo.srcObject = ev.stream;
    

    callButton.disabled = true;
    hangupButton.disabled = false;
  }

  //----------- on data channel--------------
  peerConnection.ondatachannel = function(ev){

    console.log('Got Channel Callback: event --> ' + ev);
    // Retrieve channel information
    dataChannel = ev.channel;

    handleDataChannel();
  }
}

function answer(){

  console.log('Sending answer');

  peerConnection.createAnswer().then(function(description){

    console.log('Setting local description');
    peerConnection.setLocalDescription(description);

    wss.send(JSON.stringify({type:"answer", data:description, to:remoteID, from:localID}));

  });
}


//////////// Data Channel ////////////////////////////////////////////////////////////////

function handleDataChannel(){

  dataChannel.onmessage = function(ev) {
    console.log('Received message: ' + ev.data);
    // Show message in the HTML5 page
    document.getElementById("dataChannelReceive").value = ev.data;
    // Clean 'Send' text area in the HTML page
    dataChannelSend.value = '';
  };


  dataChannel.onopen = dataChannelStateChange;
  dataChannel.onclose = dataChannelStateChange;

  function dataChannelStateChange(){

    var readyState = dataChannel.readyState;
    console.log('Send channel state is: ' + readyState);
    if (readyState == "open") {
      // Enable 'Send' text area and set focus on it
      //xxxxxxxdataChannelSend.disabled = false;
      //xxxxxxxdataChannelSend.focus();
      //xxxxxxxdataChannelSend.placeholder = "";
      // Enable both 'Send' and 'Close' buttons  
      //xxxxxxxsendButton.disabled = false;
    } else { // event MUST be 'close', if we are here...
      // Disable 'Send' text area
      //xxxxxxxdataChannelSend.disabled = true;
      // Disable both 'Send' and 'Close' buttons
      //xxxxxxxsendButton.disabled = true;
    }
  }
}

function sendData() {
  var data = dataChannelSend.value;
  dataChannel.send(data);
  console.log('Sent data: ' + data);

  dataChannelSend.value = "";
}