//Roy Ros Cobo

//---------REQUIRES--------------
const WebSocket = require('ws');//web socket


//------CONSTS-------------------
const GST_SERVER = 0;
const SIGNALLING_SERVER = -1;
const BROADCAST = -2;


//-------vars----------------------
var port = 3434;
var sockets = [], ids=1;//sockets ids starts at 1
//this signalling server id is -1
//id of gst server is allways 0
var gstSocketServer = {socket:null, ip: null, id:GST_SERVER};



const wss = new WebSocket.Server({ port: port });
console.log("Signalling server on port "+port);


//--------------txt processing---------------------------------------------
process.openStdin().addListener("data", function(d) {

	var txt = d.toString().trim();

	if(txt==""){
		console.log("LIST OF SOCKETS ("+Object.keys(sockets).length+")\n--------------------------------------------");
    console.log(gstSocketServer.id+" = "+gstSocketServer.ip+"\t\t(socket gst server)");
		Object.keys(sockets).forEach(function(key){ if(key!=GST_SERVER) console.log(key+" = "+sockets[key].ip); }); 

	}else if("0"<=txt[0] && txt[0]<="9") {

    var id = txt[0];

    if(sockets[id]!=undefined){

      var txt = txt.substring(1,txt.length);
      var msg = JSON.stringify({type:"txt",data:"~~~From sign server: "+txt, from:SIGNALLING_SERVER, to:id});

      sockets[id].socket.send(msg);
      console.log(">>>From:-1 to:"+id+" type:txt data: "+"~~~From sign server: "+txt);

    }else console.log("Socket doesnt exist");

	}else wss.broadcast("From sign server: "+txt);
});


//----------------------Connections handle------------------
wss.on('connection', function(socket, req) {

  var ip = req.connection.remoteAddress;
  var id = ids; ids++;
  sockets[id]={socket:socket, ip:ip, id:id};

  wss.broadcast("^^^New conected "+id+" = "+ip, socket); console.log("^^^New conected "+id+" = "+ip); 
  socket.send(JSON.stringify({type:"text", data:"Your id is "+id, from: SIGNALLING_SERVER, to:id}));
  


  socket.on('message', function(msg) {
      
    var data;

    try{ data = JSON.parse(msg); }
    catch(ex){ data = {type:"wrongJSON", data:msg, from:undefined, to:undefined} }

    console.log("________________________________________________________________");
    console.log("<<<Type:"+data.type+" from:"+data.from+" to:"+data.to+" data: "+data.data);
    console.log("----------------------------------------------------------------");

    if(data.to>=0 && sockets[data.to]!=undefined){

      console.log(">>>Type:"+data.type+" from:"+data.from+" to:"+data.to+" data: "+data.data);

      sockets[data.to].socket.send(msg);

    }else if(data.to==SIGNALLING_SERVER && data.type=="gstServerON"){

      wss.broadcast("Gst server active!"); console.log("Gst server active!");

      sockets[GST_SERVER] = {socket:socket, ip:ip, id:GST_SERVER};
      gstSocketServer = sockets[GST_SERVER];

      delete sockets[id];

    }else if(data.to==BROADCAST) wss.broadcast(")))"+data.data,socket,id);
    console.log("________________________________________________________________");
  });


  socket.on('close', function(){

  	console.log("vvvDisconected "+id+"="+ip);
  	wss.broadcast("vvvDisconected "+id+"="+ip);

  	delete sockets[id];
  })

});



//----------Private Functions------------------
//entry normal text
wss.broadcast = function(msg, exception = null, from=SIGNALLING_SERVER) {
  wss.clients.forEach(function (client) {
    if (client.readyState === WebSocket.OPEN) {
      
      //to -2 means broadcast
      if(client!=exception) client.send(JSON.stringify({type:"text", data:msg, from: from, to:BROADCAST}));
    }
  });
  console.log(")))Broadcast: "+msg);
};
