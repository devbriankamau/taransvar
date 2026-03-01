
function handleWikiRequest(cCommand)
{
    var szCategory = getIfSet(cCommand, "category");
    var szId = getIfSet(cCommand, "id");
    var szDivId = getIfSet(cCommand, "divid");
    var szTitle = decodeURIComponent(getIfSet(cCommand, "title"));
    requestData('tools:wikiRequest','cat='+szCategory+'&id='+szId+'&divid='+szDivId+'&title='+'');
   //Check: requestData("getWiki", "id="+nWiki);

}

function makeBlk(szId)
{
    var i = document.getElementById(szId);
    if(!i || !i.style) 
        return; //Moved to other window?
    var nSecs = 0.5;
/*    if(i.style.visibility=='hidden') 
    {
        i.style.visibility='visible';
        nSecs = 5;
    }
    else
        i.style.visibility='hidden';
        */
    
    if (i.style.width.localeCompare('16px'))
    {
        i.style.height = '16px';
        i.style.width = '16px';
        nSecs = 5;
    }     
    else
    {
        i.style.height = '14px';
        i.style.width = '14px';
    }   
    
    setTimeout("makeBlk('"+szId+"')",nSecs*1000);
    return true;
}   
 
function makeBlink(cCommand)
{
    var szId = getIfSet(cCommand, "id");
    makeBlk(szId);
}

var szCurrentJsVer = "";
function checkJsVer(szWhat, cCommand)
{
    if (!szWhat.localeCompare("jsVersion"))
    {
        var szVer = getIfSet(cCommand, "version");

        if (!szCurrentJsVer.length)
            szCurrentJsVer = szVer;
        else
            if (szCurrentJsVer.localeCompare(szVer))
                alert("Vi har oppdatert programmet. Vennlist trykk F5 eller pÃ¥ annen mÃ¥te refresh for Ã¥ fÃ¥ siste versjon");
            
    }
    else
        requestData('sys:currentJsVer','');
    
    //140118
}

            
function submenu(cCommand)
{
    var szId = getIfSet(cCommand, "id");
    var szItems = getIfSet(cCommand, "items");
    
    var cItemCollection = cCommand.getElementsByTagName("items")[0];
        
    var cItems = cItemCollection.getElementsByTagName("item");
    //alert(cCommands.length+" commands");

    var cDiv = document.getElementById(szId);
    //cDiv.append('<table width="320" border="1"></table>');
    
    // create elements <table> and a <tbody>
    var tbl     = document.createElement("table");
    tbl.align="right";
    var tblBody = document.createElement("tbody");
    var row = document.createElement("tr");
    
    tblBody.appendChild(row);
    
    for (var n=0; n<cItems.length; n++)
    {
        var cItem = cItems[n];
        var szScript = decodeURIComponent(cItem.getElementsByTagName("script")[0].textContent);
        var szText = decodeURIComponent(cItem.getElementsByTagName("text")[0].textContent);
        //var cNewRow = cTbl.insertRow(0);
        //cNewRow.innerHTML = szCode;        

         var cell = document.createElement("td");   
         var cellText = document.createTextNode(szText); 
         //cell.appendChild(cellText);
         
         var a = document.createElement('a');
         a.setAttribute('href', szScript);
         a.appendChild(cellText);
         cell.appendChild(a);
         row.appendChild(cell);
    }

    tbl.appendChild(tblBody);
    cDiv.appendChild(tbl);
    tbl.setAttribute("border", "2");    
}//140118

function initDatePick(szId)
{
  /*  var pDP = new JsDatePick({
        useMode:2,
        target:szId 
    }); */
  
    var pDP = new JsDatePick({
        useMode:2,
        target:szId,        
        isStripped:false
        ,yearsRange: new Array(1971,2100)
        //,limitToToday:true
        ,dateFormat:"%d.%m.%Y"
            //,imgPath:"jsdatepick/img/"
    });                          
    
    if (!pDP)
        alert("Error initializing");

        /*,selectedDate:{
            year:2009,
            month:4,
            day:16
           }*/
}

function pickDate(cCommand)
{
    var szId = getIfSet(cCommand, "id");
                      
    var cFld = document.getElementById(szId);
    if (!cFld)
        alert("date field not found..");

    initDatePick(szId);
}

function setClass(cCommand)
{
    var szId = getIfSet(cCommand, "id");
    var szClass = getIfSet(cCommand, "class");
    var cElement = document.getElementById(szId);
    if (cElement)
        cElement.className = szClass;
}

function ticking(cCommand)
{
    var szVal = getIfSet(cCommand, "ticking");
    setTicking(szVal);
    
    var cTicking = document.getElementById("ticking");
    if (cTicking)
        cTicking.innerHTML = szVal;
}

function editInline(cCommand)
{
    var szId = getIfSet(cCommand, "id");
    var cDiv = document.getElementById(szId);
    var szVal = getFieldVal(szId);        
    //var szVal = cDiv.innerHTML;
    var szFldType = getIfSet(cCommand, "fldType");
    var szCategory = getIfSet(cCommand, "category");

    var szHtml = decodeURIComponent(getIfSet(cCommand, "html"));
    cDiv.innerHTML = szHtml;
    var cEdit = document.getElementById("putItHere");
    cEdit.value = szVal;
    cEdit.id = "edit";
    
    if (!szFldType.localeCompare("datetime"))
    {
        var szOrigId = szCategory+"OrigVal";
        var cOrig = document.getElementById(szOrigId);
        if (cOrig)
       {
            var szOrig = cOrig.innerHTML;
            var cParts = szOrig.split(" ");
            cEdit.value = cParts[0];
            document.getElementById("time").value = cParts[1];
            initDatePick("edit");
       }            
    }
    
    if (!szFldType.localeCompare("date"))
        initDatePick("edit");
    
}

function setTooltip(cCommand)
{
    var szId = getIfSet(cCommand, "id");
    var cFld = document.getElementById(szId);
    if (cFld)
    {
        var szTip = decodeURIComponent(getIfSet(cCommand, "tip"));  //140206
        cFld.onmouseover = "tooltip.show('"+szTip+"', 200);";
        cFld.onmouseout="tooltip.hide();";
    }
    else
        if (bDebugging)
            alert("Fld not found..:"+szId);    
    //    else
    //        alert("Not debugging so shouldn't display....");
}

function submitForm(cCommand)
{
    var szFormsFound = "";
    var cForm = false;

    var szFormId = getIfSet(cCommand, "id");
    var szFldId = getIfSet(cCommand, "fldId");
    if (szFormId.length)
        cForm = document.getElementById(szFormId);
    else
        if (szFldId.length)
        {
            var cFld = document.getElementById(szFldId);
            cForm = findForm(cFld);
        }
            
    if (cForm)
    {
        //cForm.submit();
        cForm.onsubmit();
        return;
    }
        
    alert("Form not found");
    return;
    
    for (var n=0; n< document.forms.length;n++)
    {
        var cForm = document.forms[n];
        szFormsFound += ", "+cForm.Id;
    }
        alert("Forms: "+szFormsFound);
}

function setTextOnRow(cCommand)
{
    var szTbl = getIfSet(cCommand, "tbl");
    var szRow = getIfSet(cCommand, "row");
    var szFld = getIfSet(cCommand, "fld");
    var szTxt = decodeURIComponent(getIfSet(cCommand, "txt"));  //140622 decode 
    var szAdd = getIfSet(cCommand, "add");
            
    var cRow = document.getElementById(szRow);
    var cFld = getFieldSameRow(cRow, szFld, "DIV");
    if (!szAdd.localeCompare("1"))
        cFld.innerHTML = cFld.innerHTML + szTxt;
    else
        cFld.innerHTML = szTxt;
    
}

function makeMenuGroup(cCommand)
{
//    alert("Not yet learned how to make menu group");    

    var szId = getIfSet(cCommand, "id");
    var szName = getIfSet(cCommand, "name");
    var szNameScript = getIfSet(cCommand, "nameScript");
    var szCreate = getIfSet(cCommand, "create");
    var szCreateScript = getIfSet(cCommand, "createScript");
    
    var szHtml = '<div class="left_box" id="'+szId+'">'
            +'<span>'+szName+'</span><a class="create" href="javascript:'+szCreateScript+'">'+szCreate+'</a>';

    for (var n = 0; n < 1000; n++)
    {
        var szChoiceName = getIfSet(cCommand, "choice"+n);
        
        if (!szChoiceName.length)
            break;
            
        var szChoiceScript = getIfSet(cCommand, "choiceScript"+n);            
        szHtml += '<div onclick="'+szChoiceScript+'"><img src="images/groups&community/icon-umbrella.png" align="absmiddle" />'+szChoiceName+'</div>';
    }            

    //<div id="lm_pbook" style="display:inline"></div>                                                                                             
    szHtml += '</div>';
    
    var szMenuTbl = document.getElementById("menues");
    if (szMenuTbl) //False if calendar or full screen entry
    {
        var cRow = szMenuTbl.insertRow(szMenuTbl.rows.length);  
        cRow.insertCell(cRow.cells.length).innerHTML = szHtml;

    }
}

function getFldValIfSet(szFld, bFalseIfNotSet)
{
    if (globJsonArray[szFld] == undefined)
        if (bFalseIfNotSet)
            return false;
        else
            return "";
    
    return globJsonArray[szFld];
}

function addHidden(cToWhat, szName, szValue)
{
    var cFld = document.createElement('INPUT');
    cFld.type='HIDDEN';
    cFld.name = szName;
    cFld.value = szValue;
    cToWhat.appendChild(cFld);
}

function form(cCommand)
{
    //Global variable...
    globJsonArray = getJsonParamArrayFromCommand(cCommand);
//    alert('ferdig');
    var szFormId = globJsonArray["where"];
    var szFunc = globJsonArray["func"];
    var szParam = globJsonArray["param"];
    
    var cForm = document.createElement('FORM');
    //cForm.name='myForm';
    //cForm.method='POST';
    //cForm.action='http://www.another_page.com/index.htm';
    cTbl = document.createElement('table');
    //tab.className="mytable";
    var cBody=document.createElement('tbody');

    //May want to move below talbe later....    
    cTbl.appendChild(cBody);
    
    var cDiv = document.getElementById(szFormId);
    if (!cDiv)
    {
        reportError("",0,"Div not found: "+szFormId);
        return;
    }
    
    cForm.appendChild(cTbl);
    cDiv.innerHTML = "";
    cDiv.appendChild(cForm);
    var row;
    
    var szFlds = "submit";
    
    for (var n = 0; n < 1000; n++)
    {
        if (globJsonArray["t"+n] == undefined)
            break;
            
        var szLabel = getFldValIfSet("l"+n, false);
        var szId = getFldValIfSet("id"+n, false);
        var szType = getFldValIfSet("t"+n, false);
        var szSize = getFldValIfSet("s"+n, false);
        var szValue = getFldValIfSet("v"+n, false);
        
        var bExport = getFldValIfSet("e"+n, false);
        
        if (!bExport.localeCompare("1"))
            szFlds += ","+szId;

        switch(szType)
        {
            case "string":
            case "date":
                var cFld = document.createElement('INPUT');
                cFld.type='TEXT';
                cFld.size = szSize;
                cFld.value=szValue;
                break;

            case "div":
                var cFld = document.createElement('DIV');
                break;
            
            case "select":
                var cFld = document.createElement('select');
                //140708
                for (var m = 0; true; m++)
                {
                    var szValue = getFldValIfSet("o"+n+"_"+m, false);
                    if (!szValue.length)
                        break;
                         
                    var cOpt = document.createElement('option');
                    var cElems = szValue.split("^");
                    
                    if (cElems.length > 1)
                    {
                        cOpt.value = cElems[0];
                        cOpt.textContent = cElems[1]; 
                    }
                    else
                    {
                        cOpt.textContent = szValue; 
                        cOpt.value = szValue;
                    }
                    cFld.appendChild(cOpt);
                }
                break;
                
            default:
                reportError("",0,"Unknown fld type in form: "+szType);
                return;
        }

        row=document.createElement('tr');
        
        if (szLabel.length)
        {
            cell=document.createElement('td');
            cell.appendChild(document.createTextNode(szLabel))
            row.appendChild(cell);
        }
        
        cFld.name=szId;
        cFld.id = szId;

        cell=document.createElement('td');
        
        if (!szLabel.length)
            cell.colSpan="2";

        cell.appendChild(cFld);
        row.appendChild(cell);
        cBody.appendChild(row);
        
        if (!szType.localeCompare("date"))
            initDatePick(szId);
    }
    
    row=document.createElement('tr');
    cell=document.createElement('td');
    cell.innerHTML = "&nbsp;";
    row.appendChild(cell);
    cell=document.createElement('td');
    cell.innerHTML = '<input type="submit" id="submit" value="Submit">';
    szFlds += ","+szParam;
    addHidden(cell, "formsettings_func", szFunc);
    addHidden(cell, "formsettings_flds", szFlds);
    row.appendChild(cell);
    cBody.appendChild(row);
    
    cForm.onsubmit = function (cEvent) 
        { 
            var cForm = cEvent.target;
            var szFunc = cForm.elements["formsettings_func"].value;//[4];//cForm.formsettings_func;//'notYetSet';
            var szFlds = cForm.elements["formsettings_flds"].value;//[5];//cForm.formsettings_flds;//'notYetSet=1';
            //alert('submitted...');
            formSubmitted(cForm,szFunc,0,szFlds,'');
            return false;
        };
    
    globJsonArray = null;   
    //140704
}

function getLocSelDrop(szCategory, szDefault)
{
    document.getElementById(szCategory+"div").innerHTML = "";
    var cDrop = document.createElement('select');
    cDrop.setAttribute('id',szCategory);
    var cArray;
    var bCodesDivBy100Only = false;//Used when selecting counties...
    var option = document.createElement("option");
    option.value = 0;

    switch (szCategory)
    {
        case "country":
            if(typeof cGlobalCountryArr === 'undefined')
                fillGlobalCountryArray();
            cArray = cGlobalCountryArr;    
            option.textContent = "---- Velg land ----";
            break;
        case "county":
            if (szDefault.localeCompare("NO"))
                return cDrop;   //No municipality info for other countries.
            
            if(typeof cGlobalMunicipalityArr === 'undefined')
                fillGlobalMunicipalityArray();
            cArray = cGlobalMunicipalityArr;    
            bCodesDivBy100Only = true;
            option.textContent = "---- Velg fylke ----";
            break;
        case "muni":
        case "municipality":
            if(typeof cGlobalMunicipalityArr === 'undefined')
                fillGlobalMunicipalityArray();
            cArray = cGlobalMunicipalityArr;    
            option.textContent = "---- Velg kommune ----";
            break;
        case "subdiv":
            var szCountry = document.getElementById("country").value;
            cArray = getCachedSubdivArray(szCountry, szDefault,0);    
            option.textContent = "---- Velg bydel/omrÃ¥de ----";
            option.value = 0;
            cDrop.appendChild(option);
            var option = document.createElement("option");
            option.value = -1;
            option.textContent = "---- Legg til ----";
            break;
            
        default:
            alert("Unknown category: "+szCategory);
            return;
    }

    cDrop.appendChild(option);
    
    var at;
    var id;
    var nId;
    var name;
    
    if (!cArray)
        return cDrop;   //Was not able to fill the array now... Hopefully it will be filled later.. by getCachedSubdivArray() calling fillSubdivList()
    
    for (var i = 0; i < cArray.DATA.length; i++) 
    {
        at = cArray.DATA[i];
        id = at[0];
        nId = parseInt(id);
        name = at[1];

        if (bCodesDivBy100Only) //County selector (municipality id is 100, 200, etc..
        {
            if (nId % 100 != 0)
                continue;
        }
        else
            if (!szCategory.localeCompare("muni"))
            {
                if (nId <= parseInt(szDefault))
                    continue;   //Not yet correct county.
                else
                    if (nId >= parseInt(szDefault)+100)
                        break; //Next county.. Break loop
            }        

        var option = document.createElement("option");
        option.value = id;
        option.textContent = name;
        cDrop.appendChild(option);
    };
    return cDrop;
}

function checkLoadJsonLibrary()
{
    //This library is not always loaded... Don't know why...
    if (!window.JSON) {
        //load JSON libaray
        head =  document.getElementsByTagName("head")[0];
        script = document.createElement("script");
        script.src = "path/to/json2.js";
        script.onload = script.onreadystatechange = function () {
            if (!this.readyState || this.readyState == "loaded" || this.readyState == "complete") {
                head.removeChild(script); //remove the script tag we dynamically created
                alert(JSON);
            }
        }
        head.append(script);
    }    
}
            
function locationSelector(cCommand)
{
    var szJson = decoded(getIfSet(cCommand, "json"));
    globJsonArray = window.JSON.parse(szJson);

    var szWhere = globJsonArray["where"];
    var szCountryCode = getFldValIfSet("cCode", false);
    var szCountryName = getFldValIfSet("cName", false);
    
    var szMuniCode = getFldValIfSet("mCode", false);
    var szMuniName = getFldValIfSet("mName", false);

    var szSubDivCode = getFldValIfSet("sCode", false);
    var szSubDivName = getFldValIfSet("sName", false);
        
    var cDiv = document.getElementById(szWhere);
    var cCountryDiv = document.createElement("div");
    cCountryDiv.id = "countrydiv";
    cDiv.appendChild(cCountryDiv);

    if (!szCountryCode.length)
    {
        var cCountryDrop = getLocSelDrop("country");
        cCountryDiv.appendChild(cCountryDrop);
        
        cCountryDrop.onchange = function(cEvent)
                {
                  alert('Selected (ogsÃ¥)');  
                  fillCountyList();
                  //fillMunicipalityList();
                };
    }
    else
    {
        //Country code er satt.. Ingen drop list...
        //cCountryDiv.innerHTML += '<input type="hidden" id="country" value = "'+szCountryCode+'">'+szCountryName;
        setStaticLocationSelectorTxt("country", szCountryCode, szCountryName);        
    }
    
    //Legg til div for County:
    var cCountyDiv = document.createElement('div');
    cCountyDiv.setAttribute('id',"countydiv");
    cCountyDiv.innerHTML = '&nbsp;';
    cDiv.appendChild(cCountyDiv);

    //Legg til div for Municipality:
    var cMuniDiv = document.createElement('div');
    cMuniDiv.setAttribute('id',"munidiv");
    cMuniDiv.innerHTML = '&nbsp;';
    cDiv.appendChild(cMuniDiv);

    //Legg til div for SubDivision:
    var cSubDivDiv = document.createElement('div');
    cSubDivDiv.setAttribute('id',"subdivdiv");
    cSubDivDiv.innerHTML = '&nbsp;';
    cDiv.appendChild(cSubDivDiv);
    
    if (!szMuniCode.length)
    {
        fillCountyList(cCountyDiv);
    }
    else
    {
        //140708
        //cMuniDiv.innerHTML += '<input type="hidden" id="muni" value = "'+szMuniCode+'">'+szMuniName;
        setStaticLocationSelectorTxt("muni", szMuniCode, szMuniName);        
        
        if (!szSubDivCode.length)
            fillSubdivList();
        else
            setStaticLocationSelectorTxt("subdiv", szSubDivCode, szSubDivName);        
    }
}

function fbPost(cCommand)
{   
    var szJson = decoded(getIfSet(cCommand, "json"));
    //checkLoadJsonLibrary();    
    //var cJsonArray = window.JSON.parse(szJson);
    fbWallPost(szJson);

    /*
    fbWallPost(
                {
                    method: 'feed', 
                    name: getIfSet(cCommand, "name"),
                    link: getIfSet(cCommand, "link"),
                    picture: getIfSet(cCommand, "pic"),
                    caption: getIfSet(cCommand, "cap"),
                    description: getIfSet(cCommand, "desc")
                }
             );*/
}

function getWpCmtDivId(szCat, szId, szDateTime)
{
    var szDatePart = (!szCat.localeCompare("Ac")?"_"+szDateTime.substring(5,7)+szDateTime.substring(8,10):"");
    return 'wpC'+szCat+szId+szDatePart;
}

function appendImgCellFromArr(cCell,cArr)
{
    if (cArr.length<3)
        return;
    var cImg=document.createElement("img");
    cImg.setAttribute('src', cArr[1]);
    //141024
    //var cCell = cRow.insertCell(cRow.cells.length);
    if (!cArr[2].localeCompare("T"))
        adjustImgObject(cImg);
    else
        adjustImgObj(cImg,cArr[2],cArr[3]);//235);
    cCell.appendChild(cImg);  //mouseover
}


function wp(cCommand)
{
//    var szJson = decoded(getIfSet(cCommand, "json"));
//    checkLoadJsonLibrary();    
//    var cJsonArray = window.JSON.parse(szJson);
    var cJsonArray = getJsonParamArrayFromCommand(cCommand);
    var szTblName = cJsonArray["Tbl"];
    var cTbl = document.getElementById(szTblName);
    if (!cTbl)
        return;

    var szWhere = cJsonArray["where"];
    var nRowNo = (szWhere && !szWhere.localeCompare("end")?nRowNo = cTbl.rows.length:0); //141031
                
    var cRow = cTbl.insertRow(nRowNo);
    cRow.innerHTML = '<td><table></table></td>';//szHtml;
    cTbl = cRow.getElementsByTagName('table')[0];
        
    /*var cRows = cTbl.getElementsByTagName('tr');
    var iRowCount = cRows.length;    
    
    if (iRowCount)
    {   
        var cRow = cTbl.insertRow(0);
        cRow.innerHTML = '<td colspan="2">&nbsp;</td>';
    } */

    var szCat = cJsonArray["Cat"];
    
    if (!szCat.localeCompare())
        alert("Txt received!");
    
    var szId = cJsonArray["ID"];
    
    var szHtml = '<td rowspan="'+(cJsonArray["Pic"]==undefined?"4":"5")+'" valign="top">';
    if (cJsonArray["Thmb"] != undefined)
        szHtml += cJsonArray["Thmb"];
    else
        if (cJsonArray["thmb"] != undefined && cJsonArray["thmb"][1] != undefined)
            szHtml += '<span>&nbsp;</span>';    //Pic to be put here..
        else
            szHtml += "&nbsp;";
    
    szHtml += '</td>';
    
    //Print from to and time here... (1st row, 2nd column)
    var szNames = (cJsonArray["BNm"] == undefined?"":cJsonArray["BNm"]) + 
            (cJsonArray["RNm"] != undefined || cJsonArray["BNm"] != undefined ?' > ':"") + (cJsonArray["RNm"] == undefined?"":cJsonArray["RNm"]); 
            
    var cDate = convertToDate(cJsonArray["DT"].substring(0,10));
    
    szDisplayDate = (cDate !== null?getDisplayDate(cDate):"");
    szDisplayTime = cJsonArray["DT"].substring(11,16);
            
    szHtml += '<td>'+ (szNames.length?', ':"") + szDisplayDate + " "+ szDisplayTime + '</td>';  //140831
    var nRowNo = 0;
    
    var cRow = cTbl.insertRow(nRowNo++);
    cRow.innerHTML = szHtml;//szHtml;

    if (cJsonArray["thmb"] != undefined && cJsonArray["thmb"][1] != undefined)
    {
        //szHtml += "PIC HERE";   //141024 aaaaaa
        var cSpan = cRow.getElementsByTagName("SPAN")[0];  
        appendImgCellFromArr(cSpan,cJsonArray["thmb"]);
    }
    var szTxt = cJsonArray["Txt"];
    if (!szCat.localeCompare("Ma"))
    {
        //szTxt += " "+szId+" "+szCat+'[TEKSTEN]';
        szTxt = '<a href="javascript:menu(\'club:showMatch\',\'id='+szId+'\')">'+szTxt+'</a>';
    }
    else
        szTxt += " ";

    //Picture here...
    if (cJsonArray["pic"]!=undefined)
    {
        //Never happens?
        //141024
        cRow = cTbl.insertRow(nRowNo++);
        appendImgCellFromArr(cRow,cJsonArray["pic"])
    }
    else
        if (cJsonArray["Pic"]!=undefined)
        {
            //Never happens?
            cRow = cTbl.insertRow(nRowNo++);
            cRow.innerHTML = '<td>'+cJsonArray["Pic"]+'</td>';
        }
    
    //Text here...
    var cRow = cTbl.insertRow(nRowNo++);
        
    cRow.innerHTML = '<td>'+szTxt+'</td>';

    if (parseInt(szId)>0)
    {
        //List comments
        var cRow = cTbl.insertRow(nRowNo++);
        cRow.innerHTML = '<td>'+cJsonArray["Cmt0"]+'</td>';//<div id="wpCmnt'+szCat+szId+'">&nbsp;</div>

        //List comments
        cRow = cTbl.insertRow(nRowNo++);
        var szDivId = getWpCmtDivId(szCat, szId, cJsonArray["DT"]);
        cRow.innerHTML = '<td><div class="wall_sub"><table id="'+szDivId+'" cellpadding="1"></table></div></td>';//<div id="wpCmnt'+szCat+szId+'">&nbsp;</div>
        requestData("wall:getCmnt","cat="+szCat+"&id="+szId+"&dt="+cJsonArray["DT"].substring(0,10));
    }
    else
    {
        //Empty rows bcoz rowspan = 4
        for (var p = 0;p<2;p++)
        {
            var cRow = cTbl.insertRow(nRowNo++);
            cRow.innerHTML = '<td>&nbsp;</td>';
        }
    }
}

function WPCmnt(cCommand)
{
    var szCat = decoded(getIfSet(cCommand, "cat"));
    var szId = decoded(getIfSet(cCommand, "id"));

    //var szJson = decoded(getIfSet(cCommand, "json"));
    //checkLoadJsonLibrary();    
    //var cJsonArray = window.JSON.parse(szJson);
    var cJsonArray = getJsonParamArrayFromCommand(cCommand);
    
    var szTblName = cJsonArray["Tbl"];
    var szTblId = getWpCmtDivId(szCat, szId, cJsonArray["Date"]);// 'wpCmnts'+szCat+szId; 
    var cTbl = document.getElementById(szTblId);
    if (cTbl)
    {
        var cRow = cTbl.insertRow(0);
        if (cJsonArray["pic"] != undefined && cJsonArray["pic"][1] != undefined)
            appendImgCellFromArr(cRow,cJsonArray["pic"])
        else
            cRow.innerHTML = '<td>&nbsp;</td>';
        //141024
        //cCell.innerHTML = cJsonArray["Pic"];

    //    cCell = cRow.insertCell(cRow.cells.length);
    //    cCell.innerHTML = cJsonArray["BNm"];

    //    cCell = cRow.insertCell(cRow.cells.length);
    //    cCell.innerHTML = cJsonArray["DT"];

        cCell = cRow.insertCell(cRow.cells.length);
        cCell.innerHTML = cJsonArray["Txt"];
        //140825        
    }
    else
        alert("Comments tbl not found: "+szTblId);
}

function setEventHandler(szId,szEvent,szHandler)
{
    var cElement = document.getElementById(szId);
    if (!cElement)
    {
        alert("element not found!");
        return;
    }
    switch (szEvent)
    {
        case "onsubmit":
            cElement.onsubmit = szHandler;
            break;
        default:
            alert(szEvent+" not yet handled!");
    }
}

var cJsonParam = new Object;

function setJsonParamFld(cCommand)
{
    //151221
    var szKey = decoded(getIfSet(cCommand, "key"));
    var szVal = decoded(getIfSet(cCommand, "val"));

    //cNew = {szKey:szVal};
    var szExpression = "cNew = {"+szKey+":'"+szVal+"'}";
    eval(szExpression);
    
    cJsonParam = Object.assign(cJsonParam,cNew);
    
    //var szJson = "&modjson="+JSON.stringify(cJson);
    //var cElement = document.getElementById(szId);
}

Date.millisecBetween = function( date1, date2 ) 
{
    // Convert both dates to milliseconds
    var date1_ms = date1.getTime();
    var date2_ms = date2.getTime();

    // Calculate the difference in milliseconds
    return difference_ms = date2_ms - date1_ms;
}

function getTimeRemaining(endtime){
    //var t = Date.parse(endtime) - Date.now();
    var arr = endtime.split(/[- :]/),
    date = new Date(arr[0], arr[1]-1, arr[2], arr[3], arr[4], arr[5]);
    var t = Date.millisecBetween(new Date, date);//new Date(Date.parse(endtime)))
    var seconds = Math.floor( (t/1000) % 60 );
    var minutes = Math.floor( (t/1000/60) % 60 );
    var hours = Math.floor( (t/(1000*60*60)) % 24 );
    var days = Math.floor( t/(1000*60*60*24) );
    return {
    'total': t,
    'days': days,
    'hours': hours,
    'minutes': minutes,
    'seconds': seconds
    };
}



function handleCountDown(szDivId)
{
    var cElem = document.getElementById(szDivId+"val");
    if (!cElem)
        return;
        
    var szTm = cElem.innerHTML;
    
    if (!szTm.length)
        return;
        
    var cRemaining = getTimeRemaining(szTm);
    
    var szHtml = "";//"Fant: "+szTm+" ";
    if (cRemaining.total >=0)
    {
        if (cRemaining.days)
            szHtml += cRemaining.days+" dager, ";
        if (cRemaining.hours)
        {
            var nHours = cRemaining.hours % 24;
            szHtml += nHours+":";
        }
            
        szHtml += (cRemaining.minutes<10?"0":"")+cRemaining.minutes+":";
        szHtml += (cRemaining.seconds<10?"0":"")+cRemaining.seconds;
    }
    else
    {
        var currentDate = new Date(szTm);
        szHtml = "Kommer snart";//<br><br>"+szTm;//+szHome
        //var szCurTime = (currentDate.getHours()<10?"0":"")+currentDate.getHours()+":";
        //szCurTime += (currentDate.getMinutes()<10?"0":"")+currentDate.getMinutes();
        //szHtml += "<br>Nå: "+szCurTime;
        //Html += "<br>Gjenst: "+ cRemaining.total;
        var cHiddenInfo = document.getElementById("addEntyLinkDiv");
        if (cHiddenInfo)
            cHiddenInfo.style.display = "";
    }
        
    cElem = document.getElementById(szDivId+"cnt");
    cElem.innerHTML = szHtml;
}

var cIntervalHandler = 0;

function timeCountDown(cCommand)
{
    var szDiv = decoded(getIfSet(cCommand, "div"));
    var szTime = decoded(getIfSet(cCommand, "tm"));
    cElem = document.getElementById(szDiv);
    if (cElem)
        if (cElem.innerHTML.length < 10)
        {
            var szHtml = '<div id="'+szDiv+'cnt"></div><div id="'+szDiv+'val" style="display:none">'+szTime+'</div>';
            cElem.innerHTML = szHtml;
        }
        else
        {
            cElem = document.getElementById(szDiv+"val");
            cElem.innerHTML = szTime;
        }
    //151222 
    if (!cIntervalHandler)
        cIntervalHandler = setInterval(function(){handleCountDown(szDiv)},1000);   
        
}

function handleCommand(cCommand)
{
    var cWhat = cCommand.getElementsByTagName("what");
    
    var szWhat = cWhat[0].textContent;
    handleDivs("data", szWhat);
    
    switch (szWhat)
    {
        case "alert":
            alert(decoded(cCommand.getElementsByTagName("msg")[0].textContent));
            //alert(decodeURIComponent(cMsgs));
            break;
        case "prompt":
            handlePrompt(cCommand);
            break;
 
        case "addTableRow":
            handleAddTableRow(cCommand);
            break;
 
        case "addTableCol":
            handleAddTableCol(cCommand);
            break;
               
        case "setInnerHTML":
        case "setValue":
             handleSetValue(cCommand, false);
            break;

        case "addToInnerHTML":
        case "addToValue":
             handleSetValue(cCommand, true);
            break;
            
        case "ajaxRequest":
            handleAjaxRequest(cCommand);
            break;
            
        case "stdAjaxRequest":
            handleStdAjaxRequest(cCommand);
            break;

        case "javascript":
            handleJavaScriptRequest(cCommand);
            return;
            
        case "emptyTable":
            emptyTable(cCommand);
            break;
            
        case "deleteRow":
            deleteRow(cCommand);
            break;
            
        case "deleteColumnContainingId":
            deleteColumnContainingId(cCommand);
            break;

         case "deleteRowContainingId":
            deleteRowContainingId(cCommand);
            break;
            
         case "nothing":
            //return;   
            break;         

         case "setDisplay":
            setDisplay(cCommand);
            break;
            
         case "initchat":
            jqcc.cometchat.reinitialize(); 
            break;

         case "chatWith":
            chatWith();
            break;

         case "moveToTop":
            xmlCmdMoveToTop(cCommand);            
            break;         

         case "displayPerson":
            displayPerson(cCommand);
            break;

         case "moduleData":
            moduleData(cCommand);
            break;
            
         case "phonebook":
            phonebook(cCommand);
            break;
            
         case "userConfirmed":
            userConf(cCommand);
            break;
            
         case "memSave":
            //alert("About to call memorySave..");
            memorySave(cCommand);
            break;
            
         case "setMemVal":
            //140425
            szTag = getIfSet(cCommand, "tag");
            szVal = getIfSet(cCommand, "value");
            cStoredVals[szTag] = szVal;
            break;
            
         case "memGet":
            memoryGet(cCommand);
            break;
            
         case "memRequest":
            memoryRequest(cCommand);
            break;
            
         case "changeElementId":
            changeElementId(cCommand);
            break;
            
         case "getContents":
            getContents(cCommand);
            break;

         case "informLoggedIn":
            var szLoggedIn = getIfSet(cCommand, "loggedIn");
            bLoggedIn = (!szLoggedIn.localeCompare("Yes")?true:false);  //Set global status variable.
            
            if (!bLoggedIn)
            {
                var szForcedLoggedOut = getIfSet(cCommand, "forcedLoggedOut");
                if (!szForcedLoggedOut.localeCompare("Yes"))
                {
                    bFormRequested = false;
                    szLoginCategory = "";
                    //alert("Login category reset..");

                    requestLogin(); //141215 - Moved from outside if below... To allow remain logged out...
                }
                else
                {
                    var cPhonebk = document.getElementById("phonebktbl");
                    if (cPhonebk)
                        cPhonebk.innerHTML = "<tbody></tbody>";
                    //alert("Session probably expired...");   //160520
                }
            }
            //else
            cStoredVals["loggedIn"] = bLoggedIn+0;  //160704 - check...

            break;

         case "setFlag":
            var szFlag = getIfSet(cCommand, "flag");
            var szValue = getIfSet(cCommand, "value");
            cStoredVals[szFlag] = szValue;
            break;

         case "waitCursor":
            var szWait = getIfSet(cCommand, "wait");
            var bWait = (!szWait.localeCompare("1")?true:false);
            
      //      if (!bWait)
      //      {
      //          if (hasClass(document.body, "wait"))
      //              alert("Wait cursor turned off with: "+szPrevCommand);   //NOTE! Only for test
      //      }
            
            waitCursor(bWait);//.parseInt());
            break;

        case "calendarEvents":
            calendarEvents(cCommand);
            checkJsVer(szWhat, cCommand);
            break;
            
        case "jsVersion":            
            checkJsVer(szWhat, cCommand);
            break;

        case "wiki":
            handleWikiRequest(cCommand);
            break;
            
        case "resetDropList":
            resetDropList(cCommand);
            break;

        case "makeBlink":
            makeBlink(cCommand);
            break;

        case "submenu":
            submenu(cCommand);
            break;
        
        case "pickDate":
            pickDate(cCommand);
            break;

        case "setClass":
            setClass(cCommand);
            break;

        case "ticking":
            ticking(cCommand);
            break;
            
        case "editInline":
            editInline(cCommand);
            break;

        case "tooltip":
            setTooltip(cCommand);
            break;

        case "submitForm":
            submitForm(cCommand);
            break;

        case "debug":
            //Do nothing as of now....
            break;

        case "setTextOnRow":
            setTextOnRow(cCommand);
            break;      
            
        case "breakPoint":
            //alert("Breakpoint...");
            break;
            
        case "makeMenuGroup":
            makeMenuGroup(cCommand);
            break;
            
        case "form":
            form(cCommand);
            break;
            
        case "locSel":
            locationSelector(cCommand);
            break;
            
        case "fbPost":
            fbPost(cCommand);
            break;
            
        case "WP":
            wp(cCommand);
            break;
            
        case "WPCmnt":
            WPCmnt(cCommand);
            break;
        
        case "transLink":
            transportLink(cCommand);
            break;
            
        case "responseBtn":
            responseBtn(cCommand);
            break;

        case "chatMsg":
            chatMsg(cCommand);
            break;

        case "ad":
            ad(cCommand);
            break;

        case "noti":
            notification(cCommand);
            break;

        case "cart":
            cart(cCommand);
            break;

        case "adjustImg":
            var cJson = getJsonParamArrayFromCommand(cCommand);
            adjustImg(cJson[0],cJson[1],cJson[2]);
            break;
            
        case "adjustThumbs":
            var cJson = getJsonParamArrayFromCommand(cCommand);
            for (var n=0;n<cJson.length;n++)
            {
                var cImg = document.getElementById(cJson[n]);
                adjustImgObject(cImg);
            }
            break;        

        case "adjustImgs":
            var cJson = getJsonParamArrayFromCommand(cCommand);
            var szWorH = getIfSet(cCommand, "XorH");
            var nPx = getIfSet(cCommand, "px");
            for (var n=0;n<cJson.length;n++)
            {
                var cImg = document.getElementById(cJson[n]);
                adjustImgObj(cImg, szWorH, nPx)
            }
            break;        
            
        case "enable":
            var szId = getIfSet(cCommand, "id");
            document.getElementById(szId).disabled = false;
            break;

        case "disable":
            var szId = getIfSet(cCommand, "id");
            document.getElementById(szId).disabled = true;
            break;

        case "adInterest":
            adInterest(cCommand);
            break;

        case "withTable":
            withTable(cCommand);
            break;

        case "timers":
            handleTimers(cCommand);
            break;
 
        case "check":
            //150808
            var szId = getIfSet(cCommand, "id");
            var nOn = parseInt(getIfSet(cCommand, "on"));
            document.getElementById(szId).checked = (nOn?true:false);
            break;
            
        case "setEventHandler":
            var szId = getIfSet(cCommand, "id");
            var szEvent = getIfSet(cCommand, "event");
            var szHandler = getIfSet(cCommand, "handler");
            setEventHandler(szId,szEvent,szHandler);
            break;
         
        case "jsonParam":
            setJsonParamFld(cCommand);
            break; 
        
        case "timeCountDown":
            timeCountDown(cCommand);
            break;
            
        case "refresh":
            //alert("Refreshing...");   //160522
            location.reload();    //160704 - check if reload works if not logged in...
            break;
            
        default: 
            if (szWhat)
            {
                alert("This page is changed and you should press F5 to load updated scripts.");
                reportError("",0,"Unhandled xml what: "+szWhat);
            }
                //alert("Unknowns command: "+szWhat);
            //NOTE! Should be removed anyway....
    }
    
    szPrevCommand = szWhat; //To test cursor...
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

function dbg(szTxt)
{
    var cDebug = document.getElementById("debugLog");
    cDebug.innerHTML = cDebug.innerHTML + szTxt + "<br>";
    
}

function handleDivs(szCat, szWhat)
{
    return;
    var cSkipArr = ["moveToTop","addToInnerHTML","waitCursor","setDisplay","nothing"];

    var cArr = [];
            //["","calendarEvents"],  //func,div,hide other,reveal
            //"",""];
    for (var n=0;n<cArr.length;n++)
    {
        
    }
    
    if (n == cArr.length)
    {
        //Not found
        dbg("Div not in arr: "+ szWhat);
    }
}

function scrollIntoView(eleID) {
   var e = document.getElementById(eleID);
   if (!!e && e.scrollIntoView) {
       e.scrollIntoView();
   }
}

function profilePic(nId, szFileUrl)
{
    cProfilePics[""+nId+""] = szFileUrl;
    var cImgs = document.getElementsByTagName("img");
    for (var n=0; n < cImgs.length; n++)
    {
        var cImg = cImgs[n];
        var szMsgId = cImg.id;
        
        if (!cImg.id.localeCompare("pndng"+nId))
        {
            //var szFile = cImg.src;
            cImg.src = szFileUrl;
            adjustImgObject(cImg);
            //alert("Pic to replace found.. ;)");
        }
    }
}

function withElement(szId, szTag, szWhat, szJson)
{
    var cElem = document.getElementById(szId);
    var cOrigElem = cElem;//In case not found parent...

    while (cElem && (typeof(cElem.tagName) !== 'undefined'))
        if (cElem.tagName.localeCompare(szTag))
            cElem = cElem.parentNode;
        else
            break;
        
    if (!cElem || (typeof(cElem.tagName) == 'undefined'))
    {
        if (cOrigElem)
        {
            var cElems = cOrigElem.getElementsByTagName(szTag);
            if (cElems && cElems.length)
                cElem = cElems[0];
            else
                cElem = false;
        }
    }    
        
    if (!cElem)
    {
        reportError('N/A',0, szTag+' with id '+szId+' not found in withElement');
        return;        
    }   
         
    switch (szWhat)
    {
        case "show":
            cElem.style.display = "";
            break;
        case "hide":
            cElem.style.display = "none";
            break;
        case "focus":
            cElem.focus();
            break;
        default:
            //Look for seteventhandler_onchange
            if (szWhat.substring(0,16)=="seteventhandler_")
            {
                var cFld = document.getElementById(szId);
                var szHandler = szWhat.substring(16);
                switch (szHandler)
                {
                    case "onclick":
                        cFld.onclick = new Function(szJson);
                        break;
                    case "onchange":
                        cFld.onchange = new Function(szJson);
                        break;
                    default:
                        reportError("N/A",0,"Unknown handler in js:withElement()...");
                }
                //alert(szHandler+": "+szJson);
            }
            else
                reportError('N/A',0,'Unknown what in js:withElement(): '+szWhat);
    }
}

function cart(cCommand)
{
    //141023 toTop(
    document.getElementById("tempbox").innerHTML = '<div id="cartHere" class="ad_big"></div>';
    moveToTop("tempbox");
    showCartAt("cartHere", getJsonParamArrayFromCommand(cCommand));
}

function bought(cFld)
{
    var cForm = findForm(cFld);
    if (!cForm)
    {
        reportError("","","Form not found in bought()");
        return;
    }
    var nPrice = cForm.price.value;
    var nCount = cForm.count.value;
    //var nId = cForm.parentNode.parentNode.id.substring(2,100);
    var nId = cForm.id.substring(1,100);
    //141022
    doRequestData('orders:bought','id='+nId+'&count='+nCount+'&price='+nPrice);
    
}

function ad(cCommand)
{
    var cJson = getJsonParamArrayFromCommand(cCommand);    
    var nId = cJson["Id"];
    var szFormat = cJson["Frmt"];
    var bIsBig = (!szFormat.localeCompare("big"));
    var bIsSmall = (!szFormat.localeCompare("small"));
    var cContainer;
//    var cAd = document.createElement("div");
//    cAd.id = "Ad"+nId;
/*    cAd.onclick = function()
                {
                    menu('ad:clicked','id='+this.id);
                };
  */          
    //141021 onmouse
    
    //cAd.className = "ad";
    var szAdDivFormat = 'id="Ad'+nId+'" class="ad_'+szFormat+'"';   
    
    if (bIsSmall)
    {
        withRowContainingId("adContainer", "show");
        cContainer = document.getElementById("adContainer");
        //cContainer.appendChild(cAd);    
        szAdDivFormat += 'onclick="menu(\'ad:clicked\',\'id='+nId+'\');"'; 
    }
    else
        if (bIsBig)
        {
            cContainer = document.getElementById("tempbox");    //141023
            cContainer.innerHTML = "";
        }
        else
            return;

    cContainer.innerHTML += '<div '+szAdDivFormat+'</div>';
    var cAd = document.getElementById("Ad"+nId);

    //Pictures    
    var cTbl = document.createElement("table");
    cAd.appendChild(cTbl);
    var cRow = document.createElement("tr");
    cTbl.appendChild(cRow);
    var cCell = document.createElement("td");
    cCell.colSpan="2";
    cRow.appendChild(cCell);
    var cPics = cJson["Pics"];
    for(var n=0;n<cPics.length;n++)
    {
        var cPic = cPics[n];
        var szImgId = "ad"+nId+"Img"+n;
        var cImg=document.createElement("img");
        cImg.id = szImgId;
        cImg.setAttribute('src', cPic["url"]);
        cCell.appendChild(cImg);
        var nWidth = (!szFormat.localeCompare("small")?195:350);
        adjustImgObj(cImg,"W",nWidth);//235);
    }    
    
    //Heading
    var szHead = cJson["Hd"];
    var cHd = document.createElement("div");
    cHd.innerHTML = '<h1>'+szHead+'</h1>';
    cCell.appendChild(cHd);

    if (bIsBig)
    {
        //make new cell for this..
        cRow = document.createElement("tr");
        cTbl.appendChild(cRow);
        cCell = document.createElement("td");
        cCell.style.width = "50%";
        cRow.appendChild(cCell);
    }
        
    //Txt
    var cTxt = document.createElement("div");
    cTxt.className="adTxt";
    var szDesc = cJson["Ds"];
    //cTxt.appendChild(document.createTextNode(szDesc));- doesn't display html..
    cTxt.innerHTML = szDesc;
    cCell.appendChild(cTxt);

    if (bIsBig)
    {
        //150119
        if (!cJson["Terms"])
            cJson["Terms"] = "";
            
        cCell = document.createElement("td");
        cCell.innerHTML = cJson["Terms"];
        cRow.appendChild(cCell);

        cRow = document.createElement("tr");
        cTbl.appendChild(cRow);
        cCell = document.createElement("td");
        cCell.colSpan = "2";
        cRow.appendChild(cCell);
    }
    
    var cPrc = document.createElement("div");
    cCell.appendChild(cPrc);
    cPrc.className = "adBuySection";
    var cForm = document.createElement("form");
    cForm.id = "f"+nId;
    cPrc.appendChild(cForm);
    var szOnClick = (bIsBig?' onclick="bought(this)"':'');
/*    cForm.innerHTML = "<table><tr><td>Pris:</td><td><div class=\"price\">"+cJson["MP"]+'</div><input type="hidden" name="price" value="'+cJson["MP"]+'"></td></tr>'+
                '<tr><td>Antall:</td><td><div><div id="buyCount" style="display:inline">1</div></div><input type="hidden" name="count" value="1"></td></tr>'+
                '<tr><td colspan="2"><div class="buy"'+szOnClick+'><span>KjÃ¸p!</span></div><//tr></table>';*/
    cForm.innerHTML = "<table><tr><td>Pris:</td><td><div class=\"price\">"+cJson["MP"]+'</div><input type="hidden" name="price" value="'+cJson["MP"]+'"></td></tr>'+
                '<tr><td>Antall:</td><td><div><div id="buyCount" style="display:inline"><input type="text" name="count" value="1" size="1"></div></div></td></tr>'+
                '<tr><td colspan="2"><div class="buy"'+szOnClick+'><span>KjÃ¸p!</span></div><//tr></table>';                
    //cForm.appendChild(cPrc);
    //var cPrice = cForm.getElementsByTagName("span")[0];
/*    cPrice.onclick = 'alert("Tekst")';
    if (!cPrice.onclick)
    {
        cPrice.onclick = function(){alert("Tekst");};
    }
    if (!cPrice.onclick)
    {
        cPrice.setAttribute('onclick', 'alert("testing testing")');
    } */

    if (bIsBig)
    {
        var cOrderDiv = document.createElement("div");
        cOrderDiv.id = "adCart";
        cCell.appendChild(cOrderDiv);
        showCartAt("adCart", cJson["Ordr"]);

        var cInterestDiv = document.createElement("div");
        cInterestDiv.id = "adInterest";
        cCell.appendChild(cInterestDiv);
        cInterestDiv.innerHTML = '<form onsubmit="return formSubmitted(this,\'ad:adInterest\','+nId+',\'submit,adIntrTxt,anonymous\',\'\')">'+
            '<table><tr><td>Kommentar:</td><td><input id="adIntrTxt" type="text"></td></td><td><label><input type="checkbox" id="anonymous" value="1"> Anonym</label></td><td><input type="submit" value="Lagre" id="submit"></td><td>NB! Offentlig kommentar synlig for alle</td></tr></table></form><div id="adIntrListHere">&nbsp;</div>';    //</tr><tr><td>&nbsp;

        doRequestData('ad:getInterest', 'id='+nId);
        //141118
        //if (typeof cJson["Intrst"] !== 'undefined')
    }

}


function notification(cCommand)
{
    var cJson = getJsonParamArrayFromCommand(cCommand);    
    var cTbl = document.getElementById("notitable");
    
    if (!cTbl)
        return; //Calendar??

    var szDisp = cTbl.style.display;
    if (!szDisp.localeCompare("none"))
    {
        cTbl.style.display = "";
        var cNoti = document.getElementById("notifications");
        cNoti.style.display = "";
    }

    var szId = cJson["id"];
    
    for (var n=0; n<cTbl.rows.length; n++)
    {
        if (!cTbl.rows[n].id.localeCompare(szId))
        {
            //Row already exist..
            return;
        }
    }

    var cRow = document.createElement("tr");
    cRow.id = szId;
    cTbl.appendChild(cRow);
    var szLbl = (cJson["lbl"]?cJson["lbl"]:(cJson["png"]?'<img src="pics/'+cJson["png"]+'.png" width="16" height="16" align="absmiddle" />':"&nbsp"));
    var cCells = new Array(szLbl, cJson["txt"],"");
    var cCell;
    for (var m=0;m<cCells.length; m++)
    {
        cCell = document.createElement("td");
        cCell.innerHTML = cCells[m];
        cRow.appendChild(cCell);
        
        if (m <2)
            cCell.onclick = function(){menu('noti:clicked','id='+this.parentNode.id);};
        else
        {
            var szHandled = cJson["handled"];
            if (szHandled && szHandled.length)
                cCell.innerHTML = '<img src="pics/stop.png" width="16" hight="16" onclick="menu(\'noti:handled\',\'id='+szId+'\')" title="'+szHandled+'">';
            else
            {
                var szExtra = cJson["xtra"];
                if (szExtra && szExtra.length)
                {
                    var re = /(count\#\d*)/i;
                    var found = szExtra.match(re);
                    if (found)
                    {
                        var nCount = found[0].split("#")[1];
                        cCell.innerHTML = getPendingMsgCode("msgCount", true, nCount);
                    }
                    else
                        alert("Some unhandled code..");
                }
                else
                    cCell.innerHTML = "&nbsp;";
                
            }
        }        
        
    }
    
        
}

function sendId(cElem, szParent,szFunc, szParams)
{
    while (cElem && cElem.tagName.localeCompare(szParent))
        cElem = cElem.parentNode;
        
    if (cElem)
    {
        var szSelected = cElem.id;
        
        var pParent = cElem.parentNode;
        while (!pParent.id.length)
            pParent = pParent.parentNode;
        doRequestData(szFunc, szParams+"&selected="+szSelected+"&parent="+pParent.id);
    }
}

function showCartAt(szDiv, cOrders)
{
    var cContainer = document.getElementById(szDiv);
    var cTbl = document.createElement("table");
    //cTbl.className = "cartTbl";
    cContainer.appendChild(cTbl);
    var cRow = document.createElement("tr");
    cTbl.appendChild(cRow);
    var cLabels = new Array("Dato","Kl","Vare","Pris","Antall","Betalt","Status","Kundemelding","LeverandÃ¸rmelding");
    for (var n = 0; n<cLabels.length;n++)
    {
        if (n==2 && cOrders.length && (typeof cOrders[0]["prn"] == 'undefined'))
            continue;
            
        var cCell = document.createElement("th");
        cCell.innerHTML = cLabels[n];
        cRow.appendChild(cCell);
    }
    //
    for (var n = 0; n < cOrders.length; n++)
    {
        var cOrder = cOrders[n];
        var cRow = document.createElement("tr");
        cRow.id = "ordr"+cOrder["oid"];
        cRow.onclick = function(){sendId(this,"TR","orders:show","");};
        cTbl.appendChild(cRow);
        var cDT = cOrder["tm"].split(" ");
        var cFlds = new Array(cDT[0],cDT[1],cOrder["prc"],cOrder["cnt"]);
        for (var m = 0; m<cFlds.length; m++)
        {
            var cCell = document.createElement("td");
            cCell.innerHTML = cFlds[m];
            cRow.appendChild(cCell);
        }

        if (typeof cOrder["prn"] !== 'undefined')
        {
            var cCell = document.createElement("td");
            cCell.innerHTML = cOrder["prd"];
            cRow.insertBefore(cCell, cRow.cells[2]);
        }
        
        var szPaid = cOrder["pd"];
        if (szPaid && szPaid.length)
        {
            var cCell = document.createElement("td");
            cCell.innerHTML = szPaid;
            cRow.appendChild(cCell);

            cCell = document.createElement("td");
            cCell.innerHTML = (typeof cOrder["hnd"] == 'undefined'?"&nbsp;":cOrder["hnd"]);
            cRow.appendChild(cCell);
        }
        else
        {
            var cCell = document.createElement("td");
            cCell.colSpan=2;
            cCell.innerHTML = '..klikk for betaling..';
            cRow.appendChild(cCell);
        }

        cArr = new Array("cmC","cmP");
        for (var m=0;m<cArr.length;m++)
        {
            var cCell = document.createElement("td");
            cCell.innerHTML = (typeof cOrder[cArr[m]] == 'undefined'?"&nbsp;":cOrder[cArr[m]]);
            cRow.appendChild(cCell);
        }
        
        //Delete not working.. need to remove onclick on record first... 
/*        if (typeof cOrder["cnf"] == 'undefined' || !cOrder["cnf"])
        {
            //asdfasdf
            var cCell = document.createElement("td");

            cCell.innerHTML = '<img src="pics/stop.png" width="16" height="16" border="0" title="Slett ordren" onclick="menu(\'orders:delete\',\'id='+cOrder["oid"]+'\')">';

            cRow.appendChild(cCell);
        }
 */       
    }
    
}

function displayDateTime(szDatetime)
{
    var cParts = szDatetime.split(" ");
    var cDate = convertToDate(cParts[0]);
    
    if(cDate !== null)
        szTxt = getDisplayDate(cDate);
    else
        szTxt = cParts[0];
        
    szTxt += " "+cParts[1].substring(0,5);
    return szTxt;
}

function adInterest(cCommand)
{
    var szId = getIfSet(cCommand, "id");
    //141118
    //alert("Received ad interest...");    
    
    var cContainer = document.getElementById("adIntrListHere");
    cContainer.innerHTML = "";//"Would have put ad interest here....";

    var tbl     = document.createElement("table");
    //tbl.align="right";
    cContainer.appendChild(tbl);
    
    var tblBody = document.createElement("tbody");
    tbl.appendChild(tblBody);

    var cJson = getJsonParamArrayFromCommand(cCommand);
    for (var n=0; n<cJson.length;n++)
    {
        if (n==0)
            tblBody.innerHTML="<tr><th>Postet</th><th>Av</th><th>Tekst</th></tr>";

        cJRec = cJson[n];
        var row = document.createElement("tr");
        tblBody.appendChild(row);

        var cell = document.createElement("td");   
        row.appendChild(cell);
        cell.innerHTML = displayDateTime(cJRec["time"]);

        cell = document.createElement("td");   
        row.appendChild(cell);
        cell.innerHTML = cJRec["by"];

        cell = document.createElement("td");   
        row.appendChild(cell);
        var cellText = document.createTextNode(cJRec["txt"]);//"Teksten her..."); 
        cell.appendChild(cellText);

        if (cJRec["me"]==1)
        {
            cell = document.createElement("td");   
            row.appendChild(cell);
            cell.innerHTML = '<img src="pics/stop.png" onclick="confirmMenu(\'ad:delInterest\',\'id='+cJRec["id"]+'\',\'Ã˜nsker du Ã¥ slette denne kommentaren?\')" width="16" height="16">';
        }
    
    }
    
    //Blank the input field
    var cInputFld = document.getElementById("adIntrTxt");
    cInputFld.value = "";
}

function putTblSubRow(szId, bReplace, szHtml)
{
    var cRow = document.getElementById(szId);
    if (!cRow)
    {
        alert("Row not found!");
        return;
    }
    
    //Expand the rowspan of the first column
    cCol = cRow.cells[0];
    if (bReplace)
    {
        var nRows = cCol.rowSpan;
        
        if (nRows > 1)
            while (nRows-- > 1)
            {
                cRow.parentNode.deleteRow(cRow.rowIndex+1);
            }
        
    }
    
    cCol.rowSpan = cCol.rowSpan+1;
    
    var cNewRow = document.createElement('tr');

    cRow.parentNode.insertBefore(cNewRow, cRow.nextSibling );
    cNewRow.innerHTML = szHtml;
}

function getClipboard(szFunc)
{

    if (!window.clipboardData)
    {
        alert("This function is not working on this browser");
        return;    
    }
    
    var szTxt = window.clipboardData.getData('Text');
    requestData(szFunc,'txt='+encodeURIComponent(szTxt));
}

function getFbChats(nMax,szDebug)
{
    if (!window.clipboardData)
    {
        alert("This function is not working on this browser. (It probably only works on Internet Exploerer)");
        requestData("chat:fbChatPals",'chats=N/A');
        return;    
    }
    
    var szTxt = window.clipboardData.getData('Text');
    var szSearchFor = '"ShortProfiles","setMulti"';
    var nPos = szTxt.search(szSearchFor);
    var nPosEnd = szTxt.indexOf('}}]],',nPos)+5;
    nMax = parseInt(nMax);
    var szSub = szTxt.substring(nPos, nPosEnd);//nPos+1000);
    var nLen = nPosEnd - nPos-10;
    
    var cJson = false;
    
    while (!cJson && nLen < 100000)
    {
        var szJson = szSub.substring(szSearchFor.length+4,nLen);
        var szEnd = szJson.substring(szJson.length-10,999);
        try
        {
            cJson = JSON.parse(szJson);
        }
        catch(e)
        {
            nLen++;
        }
    }    
    
    if (!cJson)
    {
        alert("Unable to find chat info. View facebook soucre and then copy all to clipboard and try again.");
        requestData("chat:fbChatPals",'chats=N/A');
        return;
    }
    
    var cNew = new Array();
    var cChats = cJson["0"];

    var cIds = Object.getOwnPropertyNames(cChats);
    for (var n = 0; n < cIds.length; n++)
    {
        var cChat = cChats[cIds[n]];
        cNew.push(new Array(cChat.id,cChat.name,cChat.vanity,cChat.uri));
    }

    var szParam = 'chats='+encodeURIComponent(JSON.stringify(cNew));
    requestData("chat:fbChatPals",szParam);
}

function withTbl(cCommand)
{
    var szId = getIfSet(cCommand, "id");
    var szCmd = getIfSet(cCommand, "cmd");
    var szParam = getIfSet(cCommand, "param");
    withTable(szId, szCmd, szParam);
}

function withTable(szId, szCmd, szParam)
{
    var cTbl = document.getElementById(szId);
    switch (szCmd)
    {
        case "showNMore":
            var nShow = parseInt(szParam);
            for (var n=0; n<cTbl.rows.length && nShow > 0;n++)
            {
                var cRow = cTbl.rows[n];
                var szDisplay = cRow.style.display;
                if (szDisplay == "none")
                {
                    //szVisible.style.visibility = "visible";
                    cRow.style.display = "";
                    nShow--;
                }
            }
            break;

        case "hideNfLast":
            var nHide = parseInt(szParam);
            for (var n=cTbl.rows.length-1; n>=0 && nHide > 0;n--)
            {
                var cRow = cTbl.rows[n];
                var szDisplay = cRow.style.display;
                if (szDisplay == "")
                {
                    //szVisible.style.visibility = "visible";
                    cRow.style.display = "none";
                    nHide--;
                }
            }
            break;

        default:
            reportError("",0,"Unknown withTable command");
    }
}

function handleTimers(cCommand)
{
    var nOn = parseInt(getIfSet(cCommand, "on"));
    var nAlert = parseInt(getIfSet(cCommand, "alert"));

    if (bGlobalPolling != nOn)
    {
        bGlobalPolling = nOn;

        if (nAlert)
            alert("Timer has been turned "+(nOn?"on":"off"));
    }
}

function getValue(cFld, szFunc, szParam)
{
    var szVal = cFld.value;
    requestData(szFunc, szParam+"&val="+szVal);
}