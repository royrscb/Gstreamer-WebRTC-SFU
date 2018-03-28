// Author: Roy Ros Cobo

var localVideo = document.getElementById("localVideo");
var remoteVideos = [];
remoteVideos[0] = document.getElementById("remoteVideo");
remoteVideos[1] = document.getElementById("remoteVideo2");

var startButton = document.getElementById("startButton");
var negotiateButton = document.getElementById("negotiateButton");
var hangupButton = document.getElementById("hangupButton");

startButton.onclick = start;
negotiateButton.onclick = negotiate;
hangupButton.onclick = hangup;


//////// Variables //////////////////////////////////////////////////
const GST_SERVER_ID = 0;
const BROADCAST=-2;

var localStream, pcs = [], wss, localID, remoteID=GST_SERVER_ID, gstServerON = false;
var web_socket_sign_url = 'wss://'+window.location.host;

var configuration = {
	'iceServers': [{
		'urls': 'stun:stun.l.google.com:19302'
	}]
};
             

//////////// Media ///////////////////////////////////////////////////////////////////////////////////////////////

function start() {

  startButton.disabled = true;
  
  createPeerConnection(0);

  var constraints = {video: true, audio: false};

  // Add local stream
  navigator.mediaDevices.getUserMedia(constraints).then(function(stream){ 

    console.log("Requesting local media");
    localStream = stream;

    stream.getTracks().forEach(track => pcs[0].addTrack(track, stream));

    connectSignServer();

  });
}

function negotiate(){
  /*

  negotiateButton.disabled = true;

  peerConnection.createOffer().then(function(description){

    console.log('Setting local description');
    peerConnection.setLocalDescription(description);

    console.log("%c>>>", 'color: red'," negotiating, sending offer:"); console.log(description);
    wss.send(JSON.stringify({type:"offer", data:description, to:remoteID, from:localID}));
  });
  */
}

function hangup(){
  /*

  console.log("Hanging up");

  peerConnection.close();
  peerConnection = null;

  remoteID = 0;

  hangupButton.disabled = true;
  negotiateButton.disabled = false;
  */
}

function connectSignServer(){

  console.log("Connecting to the signalling server");
  wss = new WebSocket(web_socket_sign_url);


  wss.onmessage = function(msg){

    var data = JSON.parse(msg.data);

    console.log("------------------------------------------");
    console.log("%c<<< ", 'color: green', "Type:"+data.type+" index:"+data.index+" from:"+data.from+" to:"+data.to);

    if(data.type=="txt") console.log(data.data);
    else if(data.type=="id"){

      localID = data.data;
      document.getElementById("id").innerHTML = "ID: "+localID;

      console.log('%c My id is:'+localID+' ', 'background: black; color: white');

    }else if(data.type=="gstServerON"){

      gstServerON = true;
      negotiateButton.disabled = false;

      console.log(data.data);
    }else if(data.type=="socketON"){

      //negotiateButton.disabled = false;
      //if(data.from==-1) wss.send(JSON.stringify({type:"socketON",data:{id:localID},to:data.data.id}));//ultraMegaMasterPROVI
      //remoteID = data.data.id;

      console.log("^^^ New conected "+data.data.id+" = "+data.data.ip);
    }else if(data.type=="socketOFF"){

      console.log("vvv Disconnected "+data.data.id+" = "+data.data.ip);
    }else if(data.type=="offer"){

      console.log('<<< OFFER '+data.index+' received:'); console.log(data.data);

      if(data.index > 0) createPeerConnection(data.index);

      pcs[data.index].setRemoteDescription(new RTCSessionDescription(data.data));

      pcs[data.index].createAnswer().then(function(description){

        pcs[data.index].setLocalDescription(description);

        console.log('%c>>>', 'color: red','Sending answer '+data.index+':'); console.log(description);
        wss.send(JSON.stringify({type:"answer", index:data.index, data:description, to:remoteID, from:localID}));
      });

    }else if(data.type=="answer"){

      //console.log("<<< ANSWER received:"); console.log(data.data);

      //peerConnection.setRemoteDescription(new RTCSessionDescription(data.data));

    }else if(data.type=="candidate"){

      console.log(data.data);

      pcs[data.index].addIceCandidate(new RTCIceCandidate(data.data));

    }else{ console.log("Type ERROR: "); console.log(data); } 


  wss.onclose = function(){

    document.getElementById("id").innerHTML = "ID: undefined";
    localID = undefined;
    remoteID = 0;

    startButton.disabled = false;
    negotiateButton.disabled = true;
    hangupButton.disabled = true;

    wss.close();

    console.log("Sign server disconnected!");
  }

  }
}

function createPeerConnection(index){

  console.log('Creating peer connection '+index);
  pcs[index] = new RTCPeerConnection();


  pcs[index].onicecandidate = function(ev){

    if (ev.candidate){

      console.log("Sending candidate: "+index); console.log(ev.candidate);

      wss.send(JSON.stringify({type:"candidate", index:index, data:ev.candidate, to:remoteID, from: localID}));
    }
  }

  //pcs[index].onnegotiationneeded = negotiate;

  pcs[index].ontrack = function(ev){

    remoteVideos[index].srcObject = ev.streams[0];
    

    negotiateButton.disabled = true;
    hangupButton.disabled = false;
  }
}



/////////// Send text //////////////////////////////////
const inputSend = document.getElementById("inputSend");
inputSend.addEventListener("keyup", function(event) {
  if (localID!=undefined && event.key === "Enter") {
      
      var txt = inputSend.value;
      inputSend.value = '';

      var isTxt = txt[0]!="{";
   

      if(isTxt){ 

        var to = BROADCAST;

        if(!isNaN(txt[0])) { to=txt[0]; txt = txt.substring(1,txt.length); }

        console.log('%c>>>', 'color: red','Type:txt from:'+localID+' to:'+to); 
        console.log(txt);
        
        wss.send(JSON.stringify({type:"txt", data:txt, from:localID, to:to })); 

      }else{

        var data = JSON.parse(txt);

        console.log('%c>>>', 'color: red','Type:'+data.type+' from:'+data.from+' to:'+data.to); 
        console.log(data.data);
        
        wss.send(txt);
      }
  }
});