// JavaScript variables associated with HTML5 video elements in the page
var localVideo = document.getElementById("localVideo");
var remoteVideo = document.getElementById("remoteVideo");

// JavaScript variables assciated with call management buttons in the page
var startButton = document.getElementById("startButton");
var callButton = document.getElementById("callButton");
var hangupButton = document.getElementById("hangupButton");

// Associate JavaScript handlers with click events on the buttons
startButton.onclick = start;
callButton.onclick = call;
hangupButton.onclick = hangup;

//////// Variables //////////////////////////////////////////////////
var localStream, peerConnection, wss, localID, remoteID=0, gstServerON = false;
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

  peerConnection.createOffer().then(function(description){

    console.log('Setting local description');
    peerConnection.setLocalDescription(description);

    console.log("Calling, sending offer:"); console.log(description);
    wss.send(JSON.stringify({type:"offer", data:description, to:remoteID, from:localID}));
  });
}

function hangup(){

  console.log("Ending call");

  peerConnection.close();
  wss.close();
  peerConnection = null;

  hangupButton.disabled = true;
  startButton.disabled = false;
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

      console.log('%c My id is:'+localID, 'background: #222; color: #bada55');
      
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

      console.log('Offer received:'); console.log(data.data);
      peerConnection.setRemoteDescription(new RTCSessionDescription(data.data));

      answer();

    }else if(data.type=="answer"){

      console.log("Answer received:"); console.log(data.data);

      peerConnection.setRemoteDescription(new RTCSessionDescription(data.data));

    }else if(data.type=="candidate"){

      console.log("Candidate received:"); console.log(data.data);

      peerConnection.addIceCandidate(new RTCIceCandidate(data.data));

    }else console.log("Unkown type! "+data);

  }
}

function createPeerConnection(){

  console.log('Creating peer connection');
  peerConnection = new RTCPeerConnection(configuration);

  peerConnection.addStream(localStream);

  peerConnection.onicecandidate = function(ev){

    if (ev.candidate){

      console.log("Sending candidate:"); console.log(ev.candidate);

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
}

function answer(){

  peerConnection.createAnswer().then(function(description){

    console.log('Setting local description');
    peerConnection.setLocalDescription(description);

    console.log('Sending answer:'); console.log(description);
    wss.send(JSON.stringify({type:"answer", data:description, to:remoteID, from:localID}));

  });
}
