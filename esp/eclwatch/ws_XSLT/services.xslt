<?xml version="1.0" encoding="UTF-8"?>
<!--

    Copyright (C) 2011 HPCC Systems.

    All rights reserved. This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
-->

<!DOCTYPE xsl:stylesheet [
    <!--uncomment these for production-->
    <!ENTITY filePathEntity "/esp/files_">
    <!ENTITY xsltPathEntity "/esp/xslt">
    
    <!--uncomment these for debugging and change it to valid path-->
    <!--!ENTITY filePathEntity "d:/development/esp/files">
    <!ENTITY xsltPathEntity "d:/development/esp/services"-->
]>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:output method="html"/>
    
<xsl:include href="&xsltPathEntity;/ws_machine/preflightControls.xslt"/>

<xsl:variable name="autoRefresh" select="5"/>

<xsl:variable name="memThreshold" select="/TpServiceQueryResponse/MemThreshold"/>
<xsl:variable name="diskThreshold" select="/TpServiceQueryResponse/DiskThreshold"/>
<xsl:variable name="cpuThreshold" select="/TpServiceQueryResponse/CpuThreshold"/>
<xsl:variable name="memThresholdType" select="/TpServiceQueryResponse/MemThresholdType"/><!-- % -->
<xsl:variable name="diskThresholdType" select="/TpServiceQueryResponse/DiskThresholdType"/><!-- % -->
<xsl:variable name="encapsulatedSystem" select="/TpServiceQueryResponse/EncapsulatedSystem"/>
<xsl:variable name="enableSNMP" select="/TpServiceQueryResponse/EnableSNMP"/>
<xsl:variable name="addProcessesToFilter" select="/TpServiceQueryResponse/PreflightProcessFilter"/>


  <xsl:template match="/TpServiceQueryResponse">
    <script type="text/javascript" language="javascript">
      <![CDATA[
      function getConfigXML(url) {
          document.location.href = url;
      }                     
    ]]>
    </script>
    <html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en">
        <xsl:choose>
            <xsl:when test="not(ServiceList/*)">
                <head>
                    <title>System Services</title>
          <link rel="stylesheet" type="text/css" href="/esp/files/yui/build/fonts/fonts-min.css" />
          <link rel="stylesheet" type="text/css" href="/esp/files/css/espdefault.css" />
          <script type="text/javascript" src="/esp/files/scripts/espdefault.js">&#160;</script>
        </head>
        <body class="yui-skin-sam" onload="nof5();">
                    <h3>No system services defined!</h3>
                </body>
            </xsl:when>
            <xsl:otherwise>
                <xsl:apply-templates select="ServiceList"/>
            </xsl:otherwise>
        </xsl:choose>
    </html>
</xsl:template>

<xsl:template match="ServiceList">
    <head>
        <meta http-equiv="Content-Type" content="text/html; charset=utf-8"/>
        <title>System Servers</title>
    <link rel="stylesheet" type="text/css" href="/esp/files/yui/build/fonts/fonts-min.css" />
    <link rel="stylesheet" type="text/css" href="/esp/files/css/espdefault.css" />
    <link type="text/css" rel="StyleSheet" href="&filePathEntity;/css/sortabletable.css"/>
    <script type="text/javascript" src="/esp/files/scripts/espdefault.js">&#160;</script>
    <style type="text/css">
            table.C { border: 0; cellpadding:0; cellspacing:0}
            table.C th, table.C td {border: 0; }
            .grey { background-color: #eee}
            .blueline { 
                border-collapse: collapse;
                text-align: left; 
            }
            .blueline thead {
                background-color: #eee;
            }
            .blueline thead tr th {
                border: gray 1px solid;
                border-collapse: collapse;
            }
            .content th {
                border: lightgrey 1px solid;
                border-top:0;
                text-align:left; 
            }
        </style>
        <script language="javascript" src="&filePathEntity;/scripts/multiselect.js">
        </script>
        <script language="javascript">
            <xsl:text disable-output-escaping="yes"><![CDATA[
      var fromTargetClusterPage = false;
            function onRowCheck(checked)
            {
                document.forms[0].submitBtn.disabled = checkedCount == 0;
            }
            function onLoad()
            {
                initSelection('resultsTable');
                initPreflightControls();
                onRowCheck(true);
                //alert(totalItems);
            }
            function toggleDetails(id)
            {
                span = document.getElementById( 'div_' + id );
                img  = document.getElementById( 'toggle_' + id );
                var show = span.style.display == 'none';
                var row = document.getElementById('row_' + id);
                row.vAlign = show ? 'top' : 'middle';
                span.style.display = show ? 'block' : 'none';
                img.src = '/esp/files_/img/folder' + (show?'open':'') + '.gif';
            }
            function toggleEclAgent(id)
            {
                span = document.getElementById( 'div_' + id );
                img  = document.getElementById( 'toggle_' + id );
                var show = span.style.display == 'none';
                span.style.display = show ? 'block' : 'none';
                img.src = '/esp/files_/img/folder' + (show?'open':'') + '.gif';
            }
                        
            var browseUrl = null;
            var browsePath = null;
            var browseCaption = null;
            
            function popup(ip, path, url, caption, os)
            {
                browseUrl = url;
                browsePath = path;
                browseCaption = caption;
                mywindow = window.open ("/FileSpray/FileList?Mask=ECLAGENT*.log&Netaddr="+ip+"&OS="+os+"&Path="+path, 
                    "mywindow", "location=0,status=1,scrollbars=1,resizable=1,width=500,height=600");
                if (mywindow.opener == null)
                    mywindow.opener = window;
                mywindow.focus();
                return false;
            } 
            //note that the following function gets invoked from the file selection window
            //
            function setSelectedPath(path)
            {
                //document.location = browseUrl + path + browseCaption;
                document.location = "/esp/iframe?" + browseCaption + "&inner=" + browseUrl + path;
            }

            function setPath(protocol, port)
            {
                var url = protocol + "://" + window.location.hostname + ":" + port;
                parent.document.location.href = url;
            }
            allowReloadPage = false;
            ]]>
        </xsl:text>
        </script>
     <xsl:call-template name="GenerateScriptForPreflightControls"/>
    </head>
  <body class="yui-skin-sam" onload="nof5();onLoad()">
        <form id="listitems" action="/ws_machine/GetMachineInfo" method="post">
            <h2>System Servers</h2>

            <table id="resultsTable" class="sort-table" width="100%">
            <colgroup>
                <col width="1%"/>
                <col width="25%"/>
                <col width="18%"/>
                <col width="18%"/>
                <col width="19%"/>
                <col width="19%"/>
            </colgroup>
            <tr class="grey">
                <th id="selectAll1" align="left" width="1%">
                    <input type="checkbox" title="Select or deselect all machines" onclick="selectAll(this.checked)"/>
                </th>
                <th width="20%">Name</th>
                <th width="15%">Queue</th>
                <th>Computer</th>
                <th>Network Address</th>
                <th>Directory</th>
            </tr>
            
            <xsl:call-template name="showMachines">
                <xsl:with-param name="caption" select="'DALI Servers'"/>
                <xsl:with-param name="nodes" select="TpDalis/TpDali"/>
                <xsl:with-param name="compType" select="'DaliServerProcess'"/>
            </xsl:call-template>

            <xsl:call-template name="showMachines">
                <xsl:with-param name="caption" select="'DFU Servers'"/>
                <xsl:with-param name="showQueue" select="1"/>
                <xsl:with-param name="nodes" select="TpDfuServers/TpDfuServer"/>
                <xsl:with-param name="compType" select="'DfuServerProcess'"/>
            </xsl:call-template>
            
            <xsl:call-template name="showMachines">
                <xsl:with-param name="caption" select="'DKC Slaves'"/>
                <xsl:with-param name="showCheckbox" select="1"/>
                <xsl:with-param name="nodes" select="TpDkcSlaves/TpDkcSlave"/>
                <xsl:with-param name="checked" select="0"/>
            </xsl:call-template>                    
            
            <xsl:call-template name="showMachines">
                <xsl:with-param name="caption" select="'Drop Zones'"/>
                <xsl:with-param name="showCheckbox" select="1"/>
                <xsl:with-param name="nodes" select="TpDropZones/TpDropZone"/>
                <xsl:with-param name="checked" select="0"/>
            </xsl:call-template>                    
            
                        <xsl:call-template name="showMachines">
                          <xsl:with-param name="caption" select="'ECL Agents'"/>
                          <xsl:with-param name="showAgentExec" select="1"/>
                          <xsl:with-param name="nodes" select="TpEclAgents/TpEclAgent"/>
                          <xsl:with-param name="compType" select="'EclAgentProcess'"/>
                        </xsl:call-template>

                        <xsl:call-template name="showMachines">
                          <xsl:with-param name="caption" select="'ECL Servers'"/>
                          <xsl:with-param name="showQueue" select="1"/>
                          <xsl:with-param name="nodes" select="TpEclServers/TpEclServer"/>
                          <xsl:with-param name="compType" select="'EclServerProcess'"/>
                        </xsl:call-template>

                        <xsl:call-template name="showMachines">
                          <xsl:with-param name="caption" select="'ECL CC Servers'"/>
                          <xsl:with-param name="showQueue" select="1"/>
                          <xsl:with-param name="nodes" select="TpEclCCServers/TpEclServer"/>
                          <xsl:with-param name="compType" select="'EclCCServerProcess'"/>
                        </xsl:call-template>

                        <xsl:call-template name="showMachines">
                          <xsl:with-param name="caption" select="'ECL Schedulers'"/>
                          <xsl:with-param name="showQueue" select="1"/>
                          <xsl:with-param name="nodes" select="TpEclSchedulers/TpEclScheduler"/>
                          <xsl:with-param name="compType" select="'EclSchedulerProcess'"/>
                        </xsl:call-template>

                        <xsl:call-template name="showMachines">
                <xsl:with-param name="caption" select="'ESP Servers'"/>
                <xsl:with-param name="showBindings" select="1"/>
                <xsl:with-param name="nodes" select="TpEspServers/TpEspServer"/>
                <xsl:with-param name="compType" select="'EspProcess'"/>
            </xsl:call-template>
            
            <xsl:call-template name="showMachines">
                <xsl:with-param name="caption" select="'FT Slaves'"/>
                <xsl:with-param name="nodes" select="TpFTSlaves/TpFTSlave"/>
                <xsl:with-param name="checked" select="0"/>
            </xsl:call-template>                    
            
            <xsl:call-template name="showMachines">
                <xsl:with-param name="caption" select="'Genesis Servers'"/>
                <xsl:with-param name="nodes" select="TpGenesisServers/TpGenesisServer"/>
            </xsl:call-template>                    
            
            <xsl:call-template name="showMachines">
                <xsl:with-param name="caption" select="'LDAP Servers'"/>
                <xsl:with-param name="nodes" select="TpLdapServers/TpLdapServer"/>
                <xsl:with-param name="showDirectory" select="0"/>
                <xsl:with-param name="checked" select="0"/>
            </xsl:call-template>                    
            
            <xsl:call-template name="showMachines">
                <xsl:with-param name="caption" select="'MySQL Servers'"/>
                <xsl:with-param name="nodes" select="TpMySqlServers/TpMySqlServer"/>
                <xsl:with-param name="showDirectory" select="0"/>
                    <xsl:with-param name="checked" select="0"/>
            </xsl:call-template>                    
            
            <xsl:call-template name="showMachines">
                <xsl:with-param name="caption" select="'Sasha Servers'"/>
                <xsl:with-param name="nodes" select="TpSashaServers/TpSashaServer"/>
                <xsl:with-param name="compType" select="'SashaServerProcess'"/>
            </xsl:call-template>
            <tr class="content">
                <th colspan="7">
                    <br/>
                </th>
            </tr>
        </table>

        <table cellpadding="0" width="100%">
            <tr>
                <th id="selectAll2" width="1%" style="padding-left:4px">
                    <input type="checkbox" title="Select or deselect all machines" onclick="selectAll(this.checked)"></input>
                </th>
                <th colspan="5" align="left">Select All / None</th>
            </tr>
            <tr><td height="20"/></tr>
        </table>
        <xsl:call-template name="ShowPreflightControls">
          <xsl:with-param name="method" select="'GetMachineInfo'"/>
          <xsl:with-param name="getProcessorInfo" select="1"/>
          <xsl:with-param name="getSoftwareInfo" select="1"/>
          <xsl:with-param name="getStorageInfo" select="1"/>
          <xsl:with-param name="applyProcessFilter" select="1"/>
          <xsl:with-param name="securityString" select="''"/>
          <xsl:with-param name="command" select="''"/>
          <xsl:with-param name="userid" select="''"/>
          <xsl:with-param name="password" select="''"/>
          <xsl:with-param name="isLinux" select="0"/>
          <xsl:with-param name="waitUntilDone" select="1"/>
          <xsl:with-param name="autoRefresh" select="1"/>
          <xsl:with-param name="clusterType" select="''"/>
          <xsl:with-param name="addProcessesToFilter" select="$addProcessesToFilter"/>
          <xsl:with-param name="enableSNMP" select="$enableSNMP"/>
        </xsl:call-template>
    </form>
    </body>
</xsl:template>
        
<xsl:template name="showMachines">
    <xsl:param name="caption"/>
    <xsl:param name="showCheckbox" select="1"/>
    <xsl:param name="checked" select="1"/>
    <xsl:param name="showDirectory" select="1"/>
    <xsl:param name="showQueue" select="0"/>
    <xsl:param name="showBindings" select="0"/>
  <xsl:param name="showAgentExec" select="0"/>
  <xsl:param name="nodes"/>
    <xsl:param name="compType"/>
    
    <xsl:if test="$nodes/TpMachines/*">
        <tr class="content">
            <th colspan="7">
                <br/>
                <xsl:value-of select="$caption"/>
            </th>
        </tr>
        <xsl:for-each select="$nodes">
            <xsl:sort select="Name"/>
            <xsl:for-each select="TpMachines/*">
                <tr>
                    <xsl:variable name="compName" select="../../Name"/>
                    <xsl:if test="$showBindings">
                        <xsl:attribute name="id">
                            <xsl:value-of select="concat('row_', $compName, '_', position())"/>
                        </xsl:attribute>
                    </xsl:if>
                    <td width="1%" valign="top">
                        <xsl:if test="$showCheckbox">
                            <input type="checkbox" name="Addresses_i{count(preceding::TpMachine)}" 
                                value="{Netaddress}|{ConfigNetaddress}:{Type}:{$compName}:{OS}:{translate(Directory, ':', '$')}" onclick="return clicked(this, event)">
                                <xsl:if test="$checked">
                                    <xsl:attribute name="checked">true</xsl:attribute>
                                </xsl:if>
                            </input>
                        </xsl:if>
                    </td>
                    <td align="left" colspan="2" padding="0" margin="0">
                        <table class="C" width="100%">
                        <tbody>
                            <tr>
                                <xsl:choose>
                                    <xsl:when test="$showQueue">
                                        <td width="54%">
                                                <xsl:value-of select="../../Name"/>
                                                <xsl:if test="count(../TpMachine) &gt; 1"> (instance <xsl:value-of select="position()"/>)</xsl:if>
                                            </td>
                                        <td width="45%" align="center"><xsl:value-of select="../../Queue"/></td>
                                    </xsl:when>
                                    <xsl:otherwise>
                                        <td colspan="2" width="100%">
                                            <xsl:choose>
                                                <xsl:when test="$showBindings">
                                                    <a href="javascript:toggleDetails('{../../Name}_{position()}')">
                                                        <img id="toggle_{../../Name}_{position()}" border="0" src="&filePathEntity;/img/folder.gif" align="middle"/>
                                                        <xsl:value-of select="../../Name"/>
                                                        <xsl:if test="count(../TpMachine) &gt; 1"> (Instance <xsl:value-of select="position()"/>)</xsl:if>
                                                    </a>
                                                </xsl:when>
                        <xsl:when test="$showAgentExec">
                          <a href="javascript:toggleEclAgent('{../../Name}_{position()}')">
                            <img id="toggle_{../../Name}_{position()}" border="0" src="&filePathEntity;/img/folder.gif" align="middle"/>
                            <xsl:value-of select="../../Name"/>
                            <xsl:if test="count(../TpMachine) &gt; 1">
                              (Instance <xsl:value-of select="position()"/>)
                            </xsl:if>
                          </a>
                        </xsl:when>
                        <xsl:otherwise>
                                                    <xsl:value-of select="../../Name"/>
                                                    <xsl:if test="count(../TpMachine) &gt; 1"> (Instance <xsl:value-of select="position()"/>)</xsl:if>
                                                </xsl:otherwise>
                                            </xsl:choose>
                                        </td>
                                    </xsl:otherwise>
                                </xsl:choose>
                            </tr>
                        </tbody>
                     </table>
                    </td>
                    <td>
                        <xsl:value-of select="Name"/>
                    </td>
                    <td>
                        <xsl:value-of select="Netaddress"/>
                        <xsl:if test="not(Port=0)">:<xsl:value-of select="Port"/></xsl:if>
                    </td>
                    <td>
                        <xsl:if test="$showDirectory">
                            <xsl:variable name="absolutePath">
                                <xsl:call-template name="makeAbsolutePath">
                                    <xsl:with-param name="path" select="Directory"/>
                                    <xsl:with-param name="isLinuxInstance" select="OS!=0"/>
                                </xsl:call-template>
                            </xsl:variable>
                            <table width="100%" cellpadding="0" cellspacing="0" class="C">
                                <tbody>
                                    <tr>
                    <td align="left" width="19">
                                            <xsl:if test="$compType!=''">
                        <xsl:variable name="logDir">
                            <xsl:choose>
                            <xsl:when test="$compType!='EclAgentProcess'">
                              <xsl:value-of  select="string(../../LogDirectory)"/>
                            </xsl:when>
                            <xsl:otherwise>
                              <xsl:value-of  select="string(../../LogDir)"/>
                            </xsl:otherwise>
                          </xsl:choose>
                          </xsl:variable>
                          <xsl:variable name="logPath">
                                                    <xsl:choose>
                                                        <xsl:when test="$logDir!='' and $logDir!='.'">
                                                            <xsl:call-template name="makeAbsolutePath">
                                                                <xsl:with-param name="path">
                                                                    <xsl:choose>
                                                                        <xsl:when test="starts-with($logDir, '.')">
                                                                            <xsl:value-of select="Directory"/>
                                                                            <xsl:value-of select="substring($logDir, 2)"/>
                                                                        </xsl:when>
                                                                        <xsl:when test="starts-with($logDir, '\') and string(OS)='0'">
                                                                            <xsl:value-of select="substring(Directory, 1, 2)"/>
                                                                            <xsl:value-of select="$logDir"/>
                                                                        </xsl:when>
                                                                        <xsl:otherwise>
                                                                            <xsl:value-of select="$logDir"/>
                                                                        </xsl:otherwise>
                                                                    </xsl:choose>
                                                                </xsl:with-param>
                                                                <xsl:with-param name="isLinuxInstance" select="OS!=0"/>
                                                            </xsl:call-template>
                                                        </xsl:when>
                                                        <xsl:otherwise>
                                                            <xsl:value-of select="$absolutePath"/>
                            </xsl:otherwise>
                                                    </xsl:choose>
                                                </xsl:variable>
                                                <a style="padding-right:2">
                                                    <xsl:variable name="url">
                                                        <xsl:text>/WsTopology/TpGetComponentFile%3fNetAddress%3d</xsl:text>
                                                        <xsl:value-of select="Netaddress"/>
                                                        <xsl:text>%26FileType%3dlog%26CompType%3d</xsl:text>
                                                        <xsl:value-of select="$compType"/>
                                                        <xsl:text>%26OsType%3d</xsl:text>
                                                        <xsl:value-of select="OS"/>
                                                        <xsl:text>%26Directory%3d</xsl:text>
                                                    </xsl:variable>
                                                    <xsl:variable name="pageCaption">
                                                        <xsl:variable name="captionLen" select="string-length($caption)-1"/>
                                                        <xsl:text>esp_iframe_title=Log file for </xsl:text>
                                                        <xsl:value-of select="substring($caption, 1, $captionLen)"/>
                                                        <xsl:text disable-output-escaping="yes"> '</xsl:text>
                                                        <xsl:value-of select="../../Name"/>
                                                        <xsl:text disable-output-escaping="yes">'</xsl:text>
                                                    </xsl:variable>
                                                    <xsl:attribute name="href">
                                                        <xsl:if test="$compType!='EclAgentProcess'">
                                                            <xsl:text>/esp/iframe?</xsl:text>
                                                            <xsl:value-of select="$pageCaption"/>
                                                            <xsl:text disable-output-escaping="yes">&amp;inner=</xsl:text>
                                                            <xsl:value-of select="$url"/>
                                                            <xsl:value-of select="$logPath"/>
                                                        </xsl:if>
                                                    </xsl:attribute>
                                                    <xsl:if test="$compType='EclAgentProcess'">
                                                        <xsl:attribute name="onclick">
                                                            <xsl:text disable-output-escaping="yes">return popup('</xsl:text>
                                                            <xsl:value-of select="Netaddress"/>
                                                            <xsl:text disable-output-escaping="yes">', '</xsl:text>
                                                            <xsl:value-of select="translate($logPath, '\', '/')"/>
                                                            <xsl:text disable-output-escaping="yes">', '</xsl:text>
                                                            <xsl:value-of select="$url"/>
                                                            <xsl:text disable-output-escaping="yes">', "</xsl:text>
                                                            <xsl:value-of select="$pageCaption"/>
                                                            <xsl:text disable-output-escaping="yes">", </xsl:text>
                                                            <xsl:value-of select="OS"/>
                                                            <xsl:text>);</xsl:text>
                                                        </xsl:attribute>
                                                    </xsl:if>
                                                    <img border="0" src="&filePathEntity;/img/base.gif" alt="View log file" width="19" height="16"/>
                                                </a>
                                            </xsl:if>
                                        </td>
                                        <td width="14">
                                            <xsl:choose>
                                                <xsl:when test="$compType!='' and ($compType!='EclAgentProcess' or OS=0)">
                          <xsl:variable name="captionLen" select="string-length($caption)-1"/>
                          <xsl:variable name="href0">
                              <xsl:text disable-output-escaping="yes">/esp/iframe?esp_iframe_title=Configuration file for </xsl:text>
                              <xsl:value-of select="substring($caption, 1, $captionLen)"/>
                              <xsl:text> - </xsl:text>
                              <xsl:value-of select="../../Name"/>
                              <xsl:text></xsl:text>
                              <xsl:text disable-output-escaping="yes">&amp;inner=/WsTopology/TpGetComponentFile%3fNetAddress%3d</xsl:text>
                              <xsl:value-of select="Netaddress"/>
                              <xsl:text>%26FileType%3dcfg%26Directory%3d</xsl:text>
                              <xsl:value-of select="$absolutePath"/>
                              <xsl:text>%26CompType%3d</xsl:text>
                              <xsl:value-of select="$compType"/>
                              <xsl:text>%26OsType%3d</xsl:text>
                              <xsl:value-of select="OS"/>
                            </xsl:variable>
                          <img onclick="getConfigXML('{$href0}')" border="0" src="/esp/files_/img/config.png" alt="View deployed configuration file" width="14" height="14"/>
                        </xsl:when>
                                                <xsl:otherwise>
                                                    <img border="0" src="/esp/files_/img/blank.png" width="14" height="14"/>
                                                </xsl:otherwise>
                                            </xsl:choose>
                                        </td>
                                        <td align="left">
                                            <xsl:value-of select="$absolutePath"/>
                                        </td>
                                    </tr>
                                </tbody>
                            </table>
                        </xsl:if>
                    </td>
                </tr>               
                <xsl:if test="$showBindings">
                    <tr>
                        <td/>
                        <td colspan="6" align="left" style="padding-left=30px">
                            <span id="div_{../../Name}_{position()}" style="display:none">
                                <xsl:choose>
                                    <xsl:when test="../../TpBindings/TpBinding">
                                        <table class="blueline" cellspacing="0">
                                            <thead>
                                                <tr>
                                                    <th>Service Name</th>
                                                    <th>Service Type</th>
                                                    <th>Protocol</th>
                                                    <th>Port</th>
                                                </tr>
                                            </thead>
                                            <tbody>
                                                <xsl:variable name="netAddress" select="Netaddress"/>
                                                <xsl:for-each select="../../TpBindings/TpBinding">
                                                    <xsl:sort select="Service"/>
                                                    <tr>
                                                        <td>
                                                            <xsl:choose>
                                                                <xsl:when test="substring(Protocol, 1, 4)='http' and (not($encapsulatedSystem) or $encapsulatedSystem != 1)">
                                                                    <a href="{Protocol}://{$netAddress}:{Port}" target="_top">
                                                                        <xsl:value-of select="Service"/>
                                                                    </a>
                                                                </xsl:when>
                                                                <xsl:when test="substring(Protocol, 1, 4)='http'">
                                                                    <a href="javascript:void(0)" onclick="setPath('{Protocol}', '{Port}')" target="_top">
                                                                        <xsl:value-of select="Service"/>
                                                                    </a>
                                                                </xsl:when>
                                                                <xsl:otherwise>
                                                                    <xsl:value-of select="Service"/>
                                                                </xsl:otherwise>
                                                            </xsl:choose>
                                                        </td>
                                                        <td>
                                                            <xsl:value-of select="ServiceType"/>
                                                        </td>
                                                        <td>
                                                            <xsl:value-of select="Protocol"/>
                                                        </td>
                                                        <td>
                                                            <xsl:value-of select="Port"/>
                                                        </td>
                                                    </tr>
                                                </xsl:for-each>
                                            </tbody>
                                        </table>
                                    </xsl:when>
                                    <xsl:otherwise>No bindings defined!</xsl:otherwise>
                                </xsl:choose>
                            </span>
                        </td>
                    </tr>
                </xsl:if>
        <xsl:if test="$showAgentExec">
          <xsl:variable name="absolutePath">
            <xsl:call-template name="makeAbsolutePath">
              <xsl:with-param name="path" select="string(Directory)"/>
              <xsl:with-param name="isLinuxInstance" select="OS!=0"/>
            </xsl:call-template>
          </xsl:variable>
          <xsl:variable name="logDir" select="string(../../LogDir)"/>
          <xsl:variable name="logPath">
            <xsl:choose>
              <xsl:when test="$logDir!='' and $logDir!='.'">
                <xsl:call-template name="makeAbsolutePath">
                  <xsl:with-param name="path">
                    <xsl:choose>
                      <xsl:when test="starts-with($logDir, '.')">
                        <xsl:value-of select="Directory"/>
                        <xsl:value-of select="substring($logDir, 2)"/>
                      </xsl:when>
                      <xsl:when test="starts-with($logDir, '\') and string(OS)='0'">
                        <xsl:value-of select="substring(Directory, 1, 2)"/>
                        <xsl:value-of select="$logDir"/>
                      </xsl:when>
                      <xsl:otherwise>
                        <xsl:value-of select="$logDir"/>
                      </xsl:otherwise>
                    </xsl:choose>
                  </xsl:with-param>
                  <xsl:with-param name="isLinuxInstance" select="OS!=0"/>
                </xsl:call-template>
              </xsl:when>
              <xsl:otherwise>
                <xsl:value-of select="$absolutePath"/>
              </xsl:otherwise>
            </xsl:choose>
          </xsl:variable>
          <tr>
            <td/>
            <td colspan="6" align="left" style="padding-left=30px">
              <span id="div_{../../Name}_{position()}" style="display:none;">
                  <table class="blueline" cellspacing="0">
                    <thead>
                      <tr>
                        <th>Component</th>
                        <th>Dali Server</th>
                        <!--th>WUQueueName</th-->
                        <th>Configuration</th>
                      </tr>
                    </thead>
                    <tbody>
                      <xsl:variable name="netAddress" select="Netaddress"/>
                      <tr>
                        <td>AgentExec</td>
                        <td>
                          <xsl:value-of select="../../DaliServer"/>
                        </td>
                        <!--td>
                          <xsl:value-of select="../../WUQueueName"/>
                        </td-->
                        <td width="14">
                          <a>
                            <xsl:attribute name="href">
                              <xsl:variable name="captionLen" select="string-length($caption)-1"/>
                              <xsl:text disable-output-escaping="yes">/esp/iframe?esp_iframe_title=Configuration file for </xsl:text>
                              <xsl:value-of select="substring($caption, 1, $captionLen)"/>
                              <xsl:text disable-output-escaping="yes"> '</xsl:text>
                              <xsl:value-of select="../../Name"/>
                              <xsl:text disable-output-escaping="yes">'</xsl:text>
                              <xsl:text disable-output-escaping="yes">&amp;inner=/WsTopology/TpGetComponentFile%3fNetAddress%3d</xsl:text>
                              <xsl:value-of select="Netaddress"/>
                              <xsl:text>%26FileType%3dcfg%26Directory%3d</xsl:text>
                              <xsl:value-of select="$absolutePath"/>
                              <xsl:text>%26CompType%3dAgentExecProcess</xsl:text>
                              <xsl:text>%26OsType%3d</xsl:text>
                              <xsl:value-of select="OS"/>
                            </xsl:attribute>
                            <img border="0" src="/esp/files_/img/config.png" alt="View deployed configuration file" width="14" height="14"/>
                          </a>
                        </td>
                      </tr>
                    </tbody>
                  </table>
              </span>
            </td>
          </tr>
        </xsl:if>
      </xsl:for-each>
        </xsl:for-each>
    </xsl:if>
</xsl:template>

<xsl:template name="makeAbsolutePath">
<xsl:param name="path"/>
<xsl:param name="isLinuxInstance"/>
    <xsl:variable name="oldPathSeparator">
        <xsl:choose>
            <xsl:when test="$isLinuxInstance">'\:'</xsl:when>
            <xsl:otherwise>'/$'</xsl:otherwise>
        </xsl:choose>   
    </xsl:variable>
    
    <xsl:variable name="newPathSeparator">
        <xsl:choose>
            <xsl:when test="$isLinuxInstance">'/$'</xsl:when>
            <xsl:otherwise>'\:'</xsl:otherwise>
        </xsl:choose>   
    </xsl:variable>
    
    <xsl:variable name="newPath" select="translate($path, $oldPathSeparator, $newPathSeparator)"/>
    <xsl:if test="$isLinuxInstance and not(starts-with($newPath, '/'))">/</xsl:if>          
    <xsl:value-of select="$newPath"/>   
</xsl:template>


</xsl:stylesheet>
