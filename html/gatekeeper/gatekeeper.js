
//gatekeeper.js

function debugging()
{
    return true;
}

function xmlHandled(xmlDoc)
{
    if (!xmlDoc)
        return false;   //Handle as text...
        
    var cCommands = xmlDoc.getElementsByTagName("command");
    if (!cCommands.length)
        return false;
        
    //alert(cCommands.length+" commands");

    for (var n=0; n<cCommands.length; n++)
        handleCommand(cCommands[n]);
    return true;
}


function partnerscan()
{
    var cRes = document.getElementById("scanresult"); 
    cRes.innerHTML = "Scanning network... Please wait";

    request('partnerscan','');
    //alert("Parterscan will be performed");
}


