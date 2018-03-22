// JavaScript variables associated with HTML5 video elements in the page
var localVideo = document.getElementById("localVideo");
var remoteVideo = document.getElementById("remoteVideo");

// JavaScript variables assciated with call management buttons in the page
var startButton = document.getElementById("startButton");
var callButton = document.getElementById("callButton");
var hangupButton = document.getElementById("hangupButton");

startButton.onclick = start;
callButton.onclick = call;
hangupButton.onclick = hangup;


//////// Variables //////////////////////////////////////////////////
var localStream, peerConnection, wss, localID, remoteID=0, gstServerON = false;
var web_socket_sign_url = 'wss://130.159.242.82:3434';

var configuration = {
	'iceServers': [{
		'urls': 'stun:stun.l.google.com:19302'
	}]
};
             


//////////// Media ///////////////////////////////////////////////////////////////////////////////////////////////

function start() {
  console.log("Creating peerConnection, requesting local stream and connecting to the signalling server");
  startButton.disabled = true;
  
  createPeerConnection();

  var constraints = {video: true, audio: false};

  // Add local stream
  //navigator.mediaDevices.getUserMedia(constraints).then(function(stream){ 

    //localStream = stream;

    //stream.getTracks().forEach(track => peerConnection.addTrack(track, stream));

    connectSignServer();

  //});
}

function call(){

  callButton.disabled = true;

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



function connectSignServer(){

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
      
      //if(data.from==-1) wss.send(JSON.stringify({type:"socketON",data:{id:localID},to:data.data.id}));//ultraMegaMasterPROVI
      //remoteID = data.data.id;

      console.log("^^^ New conected "+data.data.id+" = "+data.data.ip);
    }else if(data.type=="socketOFF"){

      console.log("vvv Disconnected "+data.data.id+" = "+data.data.ip);
    }else if(data.type=="offer"){

      console.log('Offer received:'); console.log(data.data);
      peerConnection.setRemoteDescription(new RTCSessionDescription(data.data));

      peerConnection.createAnswer().then(function(description){

        peerConnection.setLocalDescription(description);

        console.log('Sending answer:'); console.log(description);
        wss.send(JSON.stringify({type:"answer", data:description, to:remoteID, from:localID}));
      });

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
  peerConnection = new RTCPeerConnection();


  peerConnection.onicecandidate = function(ev){

    if (ev.candidate){

      console.log("Sending candidate:"); console.log(ev.candidate);

      wss.send(JSON.stringify({type:"candidate", data:ev.candidate, to:remoteID, from: localID}));
    }
  }

  //peerConnection.onnegotiationneeded = call;

  peerConnection.ontrack = function(ev){

    remoteVideo.srcObject = ev.streams[0];
    

    callButton.disabled = true;
    hangupButton.disabled = false;
  }
}
