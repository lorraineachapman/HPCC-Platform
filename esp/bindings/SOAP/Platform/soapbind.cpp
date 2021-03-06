/*##############################################################################

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
############################################################################## */

#pragma warning( disable : 4786 )

#ifdef WIN32
#ifdef ESPHTTP_EXPORTS
    #define esp_http_decl __declspec(dllexport)
#endif
#endif

//Jlib
#include "jliball.hpp"

//SCM Interfaces
#include "esp.hpp"

//ESP Core
#include "espthread.hpp"

//ESP Bindings
#include "SOAP/Platform/soapbind.hpp"
#include "SOAP/client/soapclient.hpp"
#include "http/platform/httpprot.hpp"
#include "http/platform/httpservice.hpp"
#include "SOAP/Platform/soapservice.hpp"

#ifdef WIN32
#define ESP_FACTORY __declspec(dllexport)
#else
#define ESP_FACTORY
#endif

CSoapBinding::CSoapBinding()
{
}

CSoapBinding::~CSoapBinding()
{
}

//IEspRpcBinding
const char * CSoapBinding::getRpcType()
{
   return "soap";
}


const char * CSoapBinding::getTransportType()
{
   return "unknown";
}

int CSoapBinding::processRequest(IRpcMessage* rpc_call, IRpcMessage* rpc_response)
{
    return 0;
}

CHttpSoapBinding::CHttpSoapBinding():EspHttpBinding(NULL, NULL, NULL)
{
    log_level_=hsl_none;
}

CHttpSoapBinding::CHttpSoapBinding(IPropertyTree* cfg, const char *bindname, const char *procname, http_soap_log_level level)
: EspHttpBinding(cfg, bindname, procname)
{
    log_level_=level;
}

CHttpSoapBinding::~CHttpSoapBinding()
{
}

const char * CHttpSoapBinding::getTransportType()
{
    return "http";
}

static CSoapFault* makeSoapFault(CHttpRequest* request, IMultiException* me, const char *ns)
{
    const char* svcName = request->queryServiceName();
    if (svcName && *svcName)
    {
        const char* method = request->queryServiceMethod();
        StringBuffer host;
        const char* wsdlAddr = request->queryParameters()->queryProp("__wsdl_address");
        if (wsdlAddr && *wsdlAddr)
            host.append(wsdlAddr);
        else
        {
            host.append(request->queryHost());
            if (request->getPort()>0)
              host.append(":").append(request->getPort());
        }
        
        VStringBuffer ns_ext("xmlns=\"%s\""
            " xsi:schemaLocation=\"%s %s/%s/%s?xsd\"", 
            ns, ns, host.str(), svcName, method ? method : "");
        return new CSoapFault(me, ns_ext);
    }

    return new CSoapFault(me);
}

int CHttpSoapBinding::onSoapRequest(CHttpRequest* request, CHttpResponse* response)
{
    Owned<CSoapFault> soapFault;
    try
    {
        return HandleSoapRequest(request,response);
    }
    catch (IMultiException* mex)
    {
        StringBuffer ns;
        soapFault.setown(makeSoapFault(request,mex, generateNamespace(*request->queryContext(), request, request->queryServiceName(), request->queryServiceMethod(), ns).str()));
        //SetHTTPErrorStatus(mex->errorCode(),response);
        SetHTTPErrorStatus(500,response);
        mex->Release();
    }
    catch (IException* e)
    {
        StringBuffer ns;
        Owned<IMultiException> mex = MakeMultiException("Esp");
        mex->append(*e); // e is owned by mex 
        soapFault.setown(makeSoapFault(request,mex, generateNamespace(*request->queryContext(), request, request->queryServiceName(), request->queryServiceMethod(), ns).str()));
        SetHTTPErrorStatus(500,response);
    }
    catch (...)
    { 
        soapFault.setown(new CSoapFault(500,"Internal Server Error"));
        SetHTTPErrorStatus(500,response);
    }
    //response->setContentType(soapFault->get_content_type());
    response->setContentType(HTTP_TYPE_TEXT_XML_UTF8);
    response->setContent(soapFault->get_text());
    DBGLOG("Sending SOAP Fault(%u): %s", response->getContentLength(), response->queryContent());
    {
        EspTimeSection sendtime("send fault [CHttpSoapBinding::onSoapRequest]");
        response->send();
    }
    return -1;
}

int CHttpSoapBinding::HandleSoapRequest(CHttpRequest* request, CHttpResponse* response)
{

    ESP_TIME_SECTION("CHttpSoapBinding::onSoapRequest");

    StringBuffer requeststr;
    request->getContent(requeststr);
    
    if (log_level_>hsl_none)
        DBGLOG("Received request, requestlen = %d request=\n%s", requeststr.length(), requeststr.str());
    if(requeststr.length() == 0)
        throw MakeStringException(-1, "Content read is empty");

    Owned<CSoapService> soapservice;
    Owned<CSoapRequest> soaprequest;
    Owned<CSoapResponse> soapresponse;

    //soapservice.setown(new CSoapService((IEspSoapBinding*)(this)));
    soapservice.setown(new CSoapService(this));
    soaprequest.setown(new CSoapRequest);
    soaprequest->set_text(requeststr.str());

    StringBuffer contenttype;
    request->getContentType(contenttype);
    soaprequest->set_content_type(contenttype.str());
    soaprequest->setContext(request->queryContext());

    CMimeMultiPart* multipart = request->queryMultiPart();
    if(multipart != NULL)
    {
        soaprequest->setOwnMultiPart(LINK(multipart));
    }
    
    soapresponse.setown(new CSoapResponse);

    StringBuffer reqPath;
    request->getPath(reqPath);
    setRequestPath(reqPath.str());
    
    soapservice->processRequest(*soaprequest.get(), *soapresponse.get());

    response->setVersion(HTTP_VERSION);
    int status = soapresponse->get_status();
    if(status == SOAP_OK)
        response->setStatus(HTTP_STATUS_OK);
    else if(status == SOAP_SERVER_ERROR || status == SOAP_RPC_ERROR || status == SOAP_CONNECTION_ERROR)
    {
        StringBuffer msg("Internal Server Error");
        const char* detail = soapresponse->get_err();
        if (detail && *detail)
            msg.appendf(" [%s]", detail);
        throw MakeStringException(500, "%s", msg.str());
    }
    else if(status == SOAP_CLIENT_ERROR || status == SOAP_REQUEST_TYPE_ERROR)
    {
        StringBuffer msg("Bad Request");
        const char* detail = soapresponse->get_err();
        if (detail && *detail)
            msg.appendf(" [%s]", detail);
        throw MakeStringException(400, "%s", msg.str());
    }
    else if(status == SOAP_AUTHENTICATION_REQUIRED)
        response->sendBasicChallenge(m_challenge_realm.str(), false);
    else if(status == SOAP_AUTHENTICATION_ERROR)
    {
        throw MakeStringException(401,"Unauthorized Access");
    }
    else
        response->setStatus(HTTP_STATUS_OK);

    response->setContentType(soapresponse->get_content_type());
    response->setContent(soapresponse->get_text());
    
    DBGLOG("Sending SOAP Response(%u)", response->getContentLength());
    {
        EspTimeSection sendtime("send response [CHttpSoapBinding::HandleSoapRequest]");
        response->send();
    }
    
    return 0;
}


void CSoapRequestBinding::post(const char *proxy, const char* url, IRpcResponseBinding& response, const char *soapaction)
{
    CRpcCall rpccall;
    CRpcResponse rpcresponse;

    rpccall.set_url(url);
    rpccall.setProxy(proxy);

    serialize(*static_cast<IRpcMessage*>(&rpccall));
    
    CSoapClient soapclient; //to add support for handling cookies soapclient(false);
    soapclient.setUsernameToken(getUserId(), getPassword(), getRealm());
    int result = soapclient.postRequest(soapaction, rpccall, rpcresponse);

    if(result == SOAP_OK)
    {
        response.setRpcState(RPC_MESSAGE_OK);
        response.unserialize(rpcresponse, NULL, NULL);
    }
    else if(result == SOAP_CONNECTION_ERROR)
    {
        response.setRpcState(RPC_MESSAGE_CONNECTION_ERROR);
    }
    else
    {
        response.setRpcState(RPC_MESSAGE_ERROR);
    }
}

void CSoapResponseBinding::handleExceptions(IMultiException *me, const char *serv, const char *meth)
{
    if (me->ordinality() > 0)
    {
        StringBuffer text;
        me->errorMessage(text);
        text.append('\n');
        WARNLOG("Exception(s) in %s::%s - %s", serv, meth, text.str());

        IArrayOf<IException>& exceptions = me->getArray();
        ForEachItemIn(i, exceptions)
            noteException(*LINK(&exceptions.item(i)));
    }
}


void SetHTTPErrorStatus(int ErrorCode,CHttpResponse* response)
{
    switch(ErrorCode)
    {
    case 204:
        response->setStatus(HTTP_STATUS_NO_CONTENT);
        break;
    case 301:
        response->setStatus(HTTP_STATUS_MOVED_PERMANENTLY);
        break;
    case 302:
        response->setStatus(HTTP_STATUS_REDIRECT);
        break;
    case 303:
        response->setStatus(HTTP_STATUS_REDIRECT_POST);
        break;
    case 400:
        response->setStatus(HTTP_STATUS_BAD_REQUEST);
        break;
    case 401:
        response->setStatus(HTTP_STATUS_UNAUTHORIZED);
        break;
    case 403:
        response->setStatus(HTTP_STATUS_FORBIDDEN);
        break;
    case 404:
        response->setStatus(HTTP_STATUS_NOT_FOUND);
        break;
    case 405:
        response->setStatus(HTTP_STATUS_NOT_ALLOWED);
        break;
    case 501:
        response->setStatus(HTTP_STATUS_NOT_IMPLEMENTED);
        break;
    default:
        response->setStatus(HTTP_STATUS_INTERNAL_SERVER_ERROR);
    }
}
