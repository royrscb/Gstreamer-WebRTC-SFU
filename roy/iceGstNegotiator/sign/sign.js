//Roy Ros Cobo

//////////////REQUIRES//////////////////////////////////////////////////////
const WebSocket = require('ws');//web socket


//////////////CONSTS//////////////////////////////////////////////////////
const GST_SERVER_ID = 0;
const SIGNALLING_SERVER_ID = -1;

const BROADCAST = -2;


//////////////vars//////////////////////////////////////////////////////
var port = 3434;
var sockets = [], ids=1;//sockets ids starts at 1


//////////////run the server//////////////////////////////////////////////////////
const wss = new WebSocket.Server({ port: port });
console.log("Signalling server on port "+port);


//////////////txt processing//////////////////////////////////////////////////////
process.openStdin().addListener("data", function(d) {

	var txt = d.toString().trim();

	if(txt==""){

		console.log("LIST OF SOCKETS ("+Object.keys(sockets).length+")\n--------------------------------------------");
    if(sockets[GST_SERVER_ID]==undefined) console.log(GST_SERVER_ID+" = null\t(GST server OFF)");
    else console.log(GST_SERVER_ID+" = "+sockets[GST_SERVER_ID].ip+"\t(GST server ON)");      
		Object.keys(sockets).forEach(function(id){ if(id!=GST_SERVER_ID) console.log(id+" = "+sockets[id].ip); }); 

	}else if("0"<=txt[0] && txt[0]<="9") {

    var id = txt[0];

    if(sockets[id]!=undefined){

      var txt = txt.substring(1,txt.length);
      var msg = JSON.stringify({type:"txt",data:"~~~From sign server: "+txt, from:SIGNALLING_SERVER_ID, to:id});

      sockets[id].socket.send(msg);
      console.log(">>> From:-1 to:"+id+" type:txt data: "+"~~~From sign server: "+txt);

    }else console.log("Socket doesnt exist");

	}else wss.broadcast("))) From sign server: "+txt);
});


//////////////Connections handle//////////////////////////////////////////////////////
wss.on('connection', function(socket, req) {

  var id, ip = req.connection.remoteAddress;

  if(req.headers.origin=="gstServer"){

    id = GST_SERVER_ID;
    sockets[GST_SERVER_ID] = {socket:socket, ip:ip, id:GST_SERVER_ID};

    console.log  ("Gst server active!");
    wss.broadcast("Gst server active!", "gstServerON", GST_SERVER_ID); 
  }else{

    id = ids; ids++;
    sockets[id]={socket:socket, ip:ip, id:id};

    console.log  ("^^^ New conected "+id+" = "+ip); 
    wss.broadcast({id:id,ip:ip},"socketON", id); 

    console.log(">>> Type:id from:-1 to:"+id+" data: "+id);
    socket.send(JSON.stringify({type:"id", data:id, from: SIGNALLING_SERVER_ID, to:id}));
    if(sockets[GST_SERVER_ID]!=undefined) socket.send(JSON.stringify({type:"gstServerON", data:"Gst server active!", from: SIGNALLING_SERVER_ID, to:id}));
  }

  socket.on('message', function(msg) {
      
    var data;

    try{ data = JSON.parse(msg); }
    catch(ex){ data = {type:"JSON_ERROR", data:msg, from:undefined, to:undefined} }

    console.log("________________________________________________________________");
    console.log("<<< Type:"+data.type+" from:"+data.from+" to:"+data.to+" data: "+data.data);
    console.log("----------------------------------------------------------------");

    if(data.to>=0 && sockets[data.to]!=undefined){

      console.log(">>> Type:"+data.type+" from:"+data.from+" to:"+data.to+" data: "+data.data);

      sockets[data.to].socket.send(msg);

    }else if(data.to==SIGNALLING_SERVER_ID) console.log(data.data);
    else if(data.to==BROADCAST) wss.broadcast("))) "+data.data,"txt",id,id);
    else console.log("Destination ERROR");
    
    console.log("________________________________________________________________");
  });


  socket.on('close', function(){

    delete sockets[id];

  	console.log("vvv Disconected "+id+"="+ip);
  	wss.broadcast({id:id, ip:ip}, "socketOFF");
  })

  console.log("");
});



//////////////Private Functions//////////////////////////////////////////////////////


wss.broadcast = function(msg, type = "txt", exception = null, from=SIGNALLING_SERVER_ID) {
  Object.keys(sockets).forEach(function(id){ 
      
      //to -2 means broadcast
      if(id!=exception) sockets[id].socket.send(JSON.stringify({type:type, data:msg, from: from, to:BROADCAST}));

  });
  console.log("))) Broadcast: "+msg);
};
